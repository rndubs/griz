"""RPC transport worker for griz-server.

Drop-in replacement for `griz.worker.Worker` that speaks the length-
framed JSON protocol described in planning/ui-design/02-protocol.md §2
instead of newline-delimited stdio. Public API matches `Worker` so it
can be plugged into `Griz(worker_factory=RpcWorker)` without changing
any caller.

Wire sequence driven by this class:

1. Spawn `griz-server --transport=rpc --rendezvous=<tempfile> -i <db>`.
2. Poll for the rendezvous file (written 0600, atomic rename) with
   {host, port, token, ...}.
3. TCP connect to 127.0.0.1:<port>.
4. Send `kind=0x01` hello frame carrying the token; read `hello_ack`.
5. Read the `ready` event that the server emits post-startup.
6. Accept `cmd()` calls — each sends a kind=0x01 request frame and
   blocks until the matching response arrives (id-correlated).

Frame layout: `uint32 big-endian N` + `uint8 kind` + `N bytes` payload.
Kinds: 0x01 JSON, 0x02 binary (ignored in this class for now), 0x03
heartbeat (echoed through the same infrastructure so RTT works).
"""

from __future__ import annotations

import itertools
import json
import os
import shutil
import socket
import struct
import subprocess
import tempfile
import threading
import time
from collections import deque
from pathlib import Path

from griz.worker import GRIZ_PROTOCOL_VERSION, GrizCommandError, WorkerError


FRAME_HEADER_STRUCT = struct.Struct(">IB")  # uint32 len + uint8 kind
FRAME_KIND_JSON      = 0x01
FRAME_KIND_BINARY    = 0x02
FRAME_KIND_HEARTBEAT = 0x03
FRAME_MAX_PAYLOAD    = 16 * 1024 * 1024


class RpcWorker:
    """Manage a `griz-server --transport=rpc` subprocess over TCP."""

    def __init__(
        self,
        database: str | os.PathLike[str],
        *,
        griz_bin: str | os.PathLike[str] | None = None,
        width: int = 1024,
        height: int = 1024,
        ready_timeout: float = 30.0,
        hello_timeout: float = 10.0,
        shutdown_timeout: float = 5.0,
        rendezvous_timeout: float = 10.0,
        log_buffer: int = 200,
    ) -> None:
        self._database = Path(database)
        if not self._database.exists():
            raise FileNotFoundError(f"database not found: {self._database}")

        self._griz_bin = str(griz_bin) if griz_bin else _find_griz_server()
        self._width = int(width)
        self._height = int(height)
        self._ready_timeout = float(ready_timeout)
        self._hello_timeout = float(hello_timeout)
        self._shutdown_timeout = float(shutdown_timeout)
        self._rendezvous_timeout = float(rendezvous_timeout)

        self._stderr_log: deque[str] = deque(maxlen=log_buffer)
        self._stderr_lock = threading.Lock()

        self._cond = threading.Condition()
        self._responses_by_id: dict[str, dict] = {}
        # kind=0x02 bodies keyed by request_id in their sub-header.
        # Bodies without a request_id (render-stream frames) land in
        # _anon_binaries for callers that only care about the latest.
        self._binaries_by_id: dict[str, dict] = {}
        self._anon_binaries: deque[dict] = deque(maxlen=log_buffer)
        self._anon_responses: deque[dict] = deque(maxlen=log_buffer)
        self._events: list[dict] = []
        self._reader_alive = False

        self._id_counter = itertools.count(1)
        self._sock: socket.socket | None = None
        self._send_lock = threading.Lock()
        self._proc: subprocess.Popen[str] | None = None
        self._reader_thread: threading.Thread | None = None
        self._stderr_thread: threading.Thread | None = None
        self._rv_path: Path | None = None
        self._final_returncode: int | None = None

        self.server_info: dict | None = None

        self._spawn()

    # ------------------------------------------------------------------ #
    # Lifecycle
    # ------------------------------------------------------------------ #

    def _spawn(self) -> None:
        rv_dir = Path(tempfile.mkdtemp(prefix="griz-rpc-"))
        self._rv_path = rv_dir / "rendezvous.json"

        cmd = [
            self._griz_bin,
            "--transport=rpc",
            f"--rendezvous={self._rv_path}",
            "-i", str(self._database),
            "-w", str(self._width), str(self._height),
        ]
        self._proc = subprocess.Popen(
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

        self._stderr_thread = threading.Thread(
            target=self._drain_stderr,
            daemon=True,
            name="griz-server-stderr",
        )
        self._stderr_thread.start()

        try:
            rv = self._wait_for_rendezvous()
            self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self._sock.connect((rv["host"], int(rv["port"])))

            # Handshake + ready come back on the same socket; start the
            # reader thread first so frames are routed as they arrive.
            self._reader_alive = True
            self._reader_thread = threading.Thread(
                target=self._reader_loop,
                daemon=True,
                name="griz-server-rpc",
            )
            self._reader_thread.start()

            self._do_handshake(rv["token"])
            self._wait_for_ready()
        except Exception:
            self.cleanup()
            raise

    def _wait_for_rendezvous(self) -> dict:
        assert self._rv_path is not None
        deadline = time.monotonic() + self._rendezvous_timeout
        while True:
            if self._rv_path.exists():
                try:
                    return json.loads(self._rv_path.read_text())
                except (OSError, json.JSONDecodeError):
                    # File is being written — retry.
                    pass
            if self._proc is not None and self._proc.poll() is not None:
                raise WorkerError(
                    f"griz-server exited before writing rendezvous "
                    f"(rc={self._proc.returncode}); "
                    f"stderr tail: {self._stderr_tail()}"
                )
            if time.monotonic() >= deadline:
                raise WorkerError(
                    f"rendezvous file did not appear within "
                    f"{self._rendezvous_timeout}s; "
                    f"stderr tail: {self._stderr_tail()}"
                )
            time.sleep(0.05)

    def _drain_stderr(self) -> None:
        assert self._proc is not None and self._proc.stderr is not None
        try:
            for line in self._proc.stderr:
                with self._stderr_lock:
                    self._stderr_log.append(line.rstrip("\r\n"))
        except (ValueError, OSError):
            pass

    # ------------------------------------------------------------------ #
    # Frame I/O
    # ------------------------------------------------------------------ #

    def _read_exact(self, n: int) -> bytes | None:
        """Read exactly n bytes from the socket; None on clean close."""
        assert self._sock is not None
        buf = bytearray()
        while len(buf) < n:
            try:
                chunk = self._sock.recv(n - len(buf))
            except (OSError, ValueError):
                return None
            if not chunk:
                return None
            buf.extend(chunk)
        return bytes(buf)

    def _write_frame(self, kind: int, payload: bytes) -> None:
        if len(payload) > FRAME_MAX_PAYLOAD:
            raise WorkerError("frame payload exceeds 16 MiB cap")
        header = FRAME_HEADER_STRUCT.pack(len(payload), kind)
        sock = self._sock
        if sock is None:
            raise WorkerError("rpc socket is not connected")
        with self._send_lock:
            try:
                sock.sendall(header + payload)
            except (BrokenPipeError, OSError, ValueError) as exc:
                raise WorkerError(f"failed to write frame: {exc}") from exc

    def _send_json(self, obj: dict) -> None:
        self._write_frame(FRAME_KIND_JSON, json.dumps(obj).encode("utf-8"))

    def _reader_loop(self) -> None:
        try:
            while True:
                header = self._read_exact(FRAME_HEADER_STRUCT.size)
                if header is None:
                    return
                length, kind = FRAME_HEADER_STRUCT.unpack(header)
                if length > FRAME_MAX_PAYLOAD:
                    return
                payload = self._read_exact(length) if length else b""
                if payload is None:
                    return

                if kind == FRAME_KIND_HEARTBEAT:
                    # Server-initiated heartbeat — echo back so RTT
                    # instrumentation is symmetric with what the spec
                    # says a client should do. Protocol-wise the server
                    # accepts echoes the same way it echoes ours.
                    try:
                        self._write_frame(FRAME_KIND_HEARTBEAT, payload)
                    except WorkerError:
                        return
                    continue
                if kind == FRAME_KIND_BINARY:
                    frame = _parse_binary_frame(payload)
                    if frame is not None:
                        self._route_binary(frame)
                    continue
                if kind != FRAME_KIND_JSON:
                    continue

                try:
                    obj = json.loads(payload.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError):
                    continue
                if isinstance(obj, dict):
                    self._route_object(obj)
        finally:
            with self._cond:
                self._reader_alive = False
                self._cond.notify_all()

    def _route_object(self, obj: dict) -> None:
        obj_type = obj.get("type")
        with self._cond:
            if obj_type == "response":
                rid = obj.get("id")
                if isinstance(rid, str):
                    self._responses_by_id[rid] = obj
                else:
                    self._anon_responses.append(obj)
            elif obj_type in ("event", "hello_ack"):
                self._events.append(obj)
            else:
                # Errors without a matching id fall through to anon.
                self._anon_responses.append(obj)
            self._cond.notify_all()

    def _route_binary(self, frame: dict) -> None:
        with self._cond:
            rid = frame.get("header", {}).get("request_id")
            if isinstance(rid, str):
                self._binaries_by_id[rid] = frame
            else:
                self._anon_binaries.append(frame)
            self._cond.notify_all()

    # ------------------------------------------------------------------ #
    # Handshake / ready
    # ------------------------------------------------------------------ #

    def _do_handshake(self, token: str) -> None:
        hello = {
            "type": "hello",
            "version": GRIZ_PROTOCOL_VERSION,
            "client": "griz-python-rpc",
            "token": token,
        }
        self._send_json(hello)

        deadline = time.monotonic() + self._hello_timeout

        def _find_ack() -> dict | None:
            for evt in self._events:
                if evt.get("type") == "hello_ack":
                    return evt
            return None

        with self._cond:
            while True:
                ack = _find_ack()
                if ack is not None:
                    if ack.get("compatible") is False:
                        raise WorkerError(
                            f"protocol version mismatch: {ack.get('message')!r}"
                        )
                    self.server_info = dict(ack)
                    return
                if not self._reader_alive:
                    raise WorkerError(
                        "griz-server closed socket before hello_ack; "
                        f"stderr tail: {self._stderr_tail()}"
                    )
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WorkerError(
                        f"griz-server did not emit hello_ack within "
                        f"{self._hello_timeout}s; "
                        f"stderr tail: {self._stderr_tail()}"
                    )
                self._cond.wait(timeout=remaining)

    def _wait_for_ready(self) -> None:
        deadline = time.monotonic() + self._ready_timeout

        def _find_ready() -> dict | None:
            for evt in self._events:
                if evt.get("type") == "event" and evt.get("event") == "ready":
                    return evt
            return None

        with self._cond:
            while True:
                evt = _find_ready()
                if evt is not None:
                    self.server_info = {**(self.server_info or {}), **evt}
                    return
                if not self._reader_alive:
                    raise WorkerError(
                        "griz-server closed socket before ready; "
                        f"stderr tail: {self._stderr_tail()}"
                    )
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WorkerError(
                        f"griz-server did not emit ready within "
                        f"{self._ready_timeout}s; "
                        f"stderr tail: {self._stderr_tail()}"
                    )
                self._cond.wait(timeout=remaining)

    # ------------------------------------------------------------------ #
    # Public command surface
    # ------------------------------------------------------------------ #

    def cmd(self, command: str, *, timeout: float = 30.0) -> dict:
        request_id = f"req_{next(self._id_counter)}"
        self._send_json({"type": "request", "id": request_id, "cmd": command})

        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                if request_id in self._responses_by_id:
                    response = self._responses_by_id.pop(request_id)
                    break
                if not self._reader_alive:
                    raise WorkerError(
                        f"griz-server closed socket before response to "
                        f"{request_id!r}; stderr tail: {self._stderr_tail()}"
                    )
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WorkerError(
                        f"command {command!r} timed out after {timeout}s"
                    )
                self._cond.wait(timeout=remaining)

        if response.get("status") == "error":
            err = response.get("error") or {}
            raise GrizCommandError(
                err.get("code") or "internal_error",
                err.get("message") or "",
                response=response,
            )
        return response

    def screenshot(
        self,
        *,
        alpha: bool = False,
        timeout: float = 30.0,
    ) -> dict:
        """Capture the current view as an inline PNG binary frame.

        Sends a `screenshot` request, waits for the paired response and
        the correlated `kind=0x02` binary frame, and returns a dict:

            {
                "seq": int,
                "w": int,
                "h": int,
                "fmt": "png",
                "bytes": bytes,      # raw PNG bytes
                "header": {...},     # JSON sub-header from the binary frame
            }

        Raises WorkerError if the binary frame never arrives (e.g. the
        server rejected the command and emitted only an error response).
        """
        request_id = f"req_{next(self._id_counter)}"
        payload: dict = {"type": "request", "id": request_id, "cmd": "screenshot"}
        if alpha:
            payload["alpha"] = True
        self._send_json(payload)

        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                if request_id in self._responses_by_id:
                    response = self._responses_by_id.pop(request_id)
                    break
                if not self._reader_alive:
                    raise WorkerError(
                        f"griz-server closed socket before response to "
                        f"{request_id!r}; stderr tail: {self._stderr_tail()}"
                    )
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WorkerError(
                        f"screenshot timed out after {timeout}s"
                    )
                self._cond.wait(timeout=remaining)

            if response.get("status") == "error":
                # Discard any stray binary buffered for this id — the
                # server only emits one on the success path.
                self._binaries_by_id.pop(request_id, None)
                err = response.get("error") or {}
                raise GrizCommandError(
                    err.get("code") or "internal_error",
                    err.get("message") or "",
                    response=response,
                )

            frame = self._binaries_by_id.pop(request_id, None)

        if frame is None:
            raise WorkerError(
                f"screenshot response arrived without a binary frame "
                f"for {request_id!r}; stderr tail: {self._stderr_tail()}"
            )

        data = response.get("data") or {}
        return {
            "seq":    int(data.get("seq", frame["header"].get("seq", 0))),
            "w":      int(data.get("w",   frame["header"].get("w",   0))),
            "h":      int(data.get("h",   frame["header"].get("h",   0))),
            "fmt":    data.get("fmt") or frame["header"].get("fmt", "png"),
            "bytes":  frame["body"],
            "header": frame["header"],
        }

    def send_command(self, command: str) -> None:
        """Fire-and-forget raw command — posts via the JSON envelope.

        The RPC transport has no "raw bytes on stdin" escape hatch like
        the stdio worker, so raw commands still go through the JSON
        envelope but without id correlation. Matches `Worker.send_command`
        semantics closely enough for the pygriz call-sites that use it.
        """
        self._send_json({"type": "request", "cmd": command})

    def drain_anon_responses(self) -> list[dict]:
        with self._cond:
            out = list(self._anon_responses)
            self._anon_responses.clear()
            return out

    def recent_stderr(self) -> list[str]:
        with self._stderr_lock:
            return list(self._stderr_log)

    def recent_events(self) -> list[dict]:
        with self._cond:
            return list(self._events)

    def _stderr_tail(self, n: int = 5) -> str:
        with self._stderr_lock:
            tail = list(self._stderr_log)[-n:]
        return " | ".join(tail) if tail else "(empty)"

    @property
    def pid(self) -> int | None:
        return self._proc.pid if self._proc else None

    @property
    def returncode(self) -> int | None:
        if self._proc is not None:
            return self._proc.poll()
        return self._final_returncode

    def cleanup(self) -> None:
        """Send quit, close the socket, wait for the subprocess to exit."""
        proc = self._proc
        self._proc = None

        sock = self._sock
        if sock is not None and proc is not None and proc.poll() is None:
            try:
                payload = json.dumps({"type": "request", "cmd": "quit"})
                header = FRAME_HEADER_STRUCT.pack(len(payload), FRAME_KIND_JSON)
                with self._send_lock:
                    sock.sendall(header + payload.encode("utf-8"))
            except (BrokenPipeError, OSError, ValueError):
                pass

        self._sock = None
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass

        if proc is not None:
            try:
                proc.wait(timeout=self._shutdown_timeout)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
            self._final_returncode = proc.returncode

        for thread in (self._reader_thread, self._stderr_thread):
            if thread is not None:
                thread.join(timeout=1.0)
        self._reader_thread = None
        self._stderr_thread = None

        if self._rv_path is not None:
            rv = self._rv_path
            self._rv_path = None
            try:
                rv.unlink()
            except OSError:
                pass
            try:
                rv.parent.rmdir()
            except OSError:
                pass

    def __enter__(self) -> "RpcWorker":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.cleanup()

    def __del__(self) -> None:
        try:
            self.cleanup()
        except Exception:
            pass


def _parse_binary_frame(payload: bytes) -> dict | None:
    """Decode a kind=0x02 payload per 02-protocol.md §3.

    Returns a dict with `subtype`, `codec`, `flags`, `header` (parsed
    JSON sub-header or {}), and `body` (raw bytes). Returns None on
    malformed input so the reader loop can drop the frame.
    """
    if len(payload) < 6:
        return None
    subtype = payload[0]
    codec   = payload[1]
    flags   = payload[2]
    # payload[3] reserved
    hdr_len = (payload[4] << 8) | payload[5]
    if 6 + hdr_len > len(payload):
        return None
    header: dict = {}
    if hdr_len > 0:
        try:
            decoded = json.loads(payload[6:6 + hdr_len].decode("utf-8"))
            if isinstance(decoded, dict):
                header = decoded
        except (UnicodeDecodeError, json.JSONDecodeError):
            header = {}
    body = bytes(payload[6 + hdr_len:])
    return {
        "subtype": subtype,
        "codec":   codec,
        "flags":   flags,
        "header":  header,
        "body":    body,
    }


def _find_griz_server() -> str:
    env_bin = os.environ.get("GRIZ_BIN")
    if env_bin:
        path = Path(env_bin)
        if path.is_file() and os.access(path, os.X_OK):
            return str(path)
        raise FileNotFoundError(
            f"GRIZ_BIN does not point at an executable: {env_bin}"
        )

    on_path = shutil.which("griz-server")
    if on_path:
        return on_path

    repo_root = Path(__file__).resolve().parents[3]
    candidates = sorted(
        repo_root.glob("Src/GRIZ4-*/bin_server_opt/griz-server"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)

    raise FileNotFoundError(
        "griz-server binary not found. "
        "Set GRIZ_BIN or build with `./build.sh server`."
    )
