"""Phase 2 worker: JSON protocol + handshake.

The server emits a single `{"type":"event","event":"ready",...}` line on
startup. The worker consumes it, performs the hello / hello_ack
handshake (05-protocol.md §3), and then accepts JSON requests via
`cmd()`. `send_command()` remains for fire-and-forget raw commands —
useful for crashy code paths (e.g. the outrgb abort) and for exercising
the server's raw-command backward-compat mode.

A single reader thread consumes stdout, classifies each JSON object as
response/event/hello_ack, and routes responses to pending `cmd()`
callers via a shared condition variable. Non-JSON lines flow to an
in-memory tail buffer for diagnostics. stderr is drained in its own
thread; we never inspect it for protocol content.
"""

from __future__ import annotations

import itertools
import json
import os
import shutil
import subprocess
import threading
import time
from collections import deque
from pathlib import Path
from typing import Any, Iterable


class WorkerError(RuntimeError):
    """Raised when the griz-server subprocess misbehaves."""


class GrizCommandError(RuntimeError):
    """Raised when a command returns status:"error"."""

    def __init__(self, code: str, message: str, response: dict | None = None):
        super().__init__(f"{code}: {message}" if code else message)
        self.code = code
        self.message = message
        self.response = response


GRIZ_PROTOCOL_VERSION = "1.0"


class Worker:
    """Manage a `griz-server --transport=stdio` subprocess.

    Primary API: `cmd(command)` sends a JSON request, blocks until the
    matching response arrives, and returns the parsed response dict (or
    raises `GrizCommandError` on status=="error"). `send_command(raw)`
    is a fire-and-forget raw-command mode retained for debugging.
    """

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

        self._stdout_log: deque[str] = deque(maxlen=log_buffer)
        self._stderr_log: deque[str] = deque(maxlen=log_buffer)
        self._stdout_lock = threading.Lock()
        self._stderr_lock = threading.Lock()

        self._cond = threading.Condition()
        self._responses_by_id: dict[str, dict] = {}
        self._anon_responses: deque[dict] = deque(maxlen=log_buffer)
        self._events: list[dict] = []
        self._reader_alive = False

        self._id_counter = itertools.count(1)

        self._proc: subprocess.Popen[str] | None = None
        self._reader_thread: threading.Thread | None = None
        self._stderr_thread: threading.Thread | None = None
        self._final_returncode: int | None = None

        self.server_info: dict | None = None

        self._spawn()

    def _spawn(self) -> None:
        cmd = [
            self._griz_bin,
            "--transport=stdio",
            "-i", str(self._database),
            "-w", str(self._width), str(self._height),
        ]
        self._proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
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

        self._reader_alive = True
        self._reader_thread = threading.Thread(
            target=self._reader_loop,
            daemon=True,
            name="griz-server-stdout",
        )
        self._reader_thread.start()

        try:
            self._wait_for_ready()
            self._do_handshake()
        except Exception:
            self.cleanup()
            raise

    def _drain_stderr(self) -> None:
        assert self._proc is not None and self._proc.stderr is not None
        try:
            for line in self._proc.stderr:
                with self._stderr_lock:
                    self._stderr_log.append(line.rstrip("\r\n"))
        except (ValueError, OSError):
            pass

    def _reader_loop(self) -> None:
        assert self._proc is not None and self._proc.stdout is not None
        try:
            for line in self._proc.stdout:
                stripped = line.rstrip("\r\n")
                if not stripped:
                    continue
                obj: Any = None
                if stripped.startswith("{"):
                    try:
                        obj = json.loads(stripped)
                    except json.JSONDecodeError:
                        obj = None
                if not isinstance(obj, dict):
                    with self._stdout_lock:
                        self._stdout_log.append(stripped)
                    continue
                self._route_object(obj, stripped)
        except (ValueError, OSError):
            pass
        finally:
            with self._cond:
                self._reader_alive = False
                self._cond.notify_all()

    def _route_object(self, obj: dict, raw: str) -> None:
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
                with self._stdout_lock:
                    self._stdout_log.append(raw)
            self._cond.notify_all()

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
                    self.server_info = evt
                    return
                if not self._reader_alive:
                    raise WorkerError(
                        "griz-server closed stdout before ready event; "
                        f"stderr tail: {self._stderr_tail()}"
                    )
                if self._proc is not None and self._proc.poll() is not None:
                    raise WorkerError(
                        f"griz-server exited before ready (rc={self._proc.returncode}); "
                        f"stderr tail: {self._stderr_tail()}"
                    )
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WorkerError(
                        f"griz-server did not emit ready event within {self._ready_timeout}s; "
                        f"stderr tail: {self._stderr_tail()}"
                    )
                self._cond.wait(timeout=remaining)

    def _do_handshake(self) -> None:
        hello = {
            "type": "hello",
            "version": GRIZ_PROTOCOL_VERSION,
            "client": "griz-python",
        }
        self._write_line(json.dumps(hello))

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
                    self.server_info = {**(self.server_info or {}), **ack}
                    return
                if not self._reader_alive:
                    raise WorkerError("griz-server closed stdout before hello_ack")
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WorkerError(
                        f"griz-server did not emit hello_ack within {self._hello_timeout}s"
                    )
                self._cond.wait(timeout=remaining)

    def _write_line(self, line: str) -> None:
        if self._proc is None or self._proc.stdin is None:
            raise WorkerError("worker is not running")
        if self._proc.poll() is not None:
            raise WorkerError(
                f"griz-server has exited (rc={self._proc.returncode}); "
                f"stderr tail: {self._stderr_tail()}"
            )
        if "\n" in line or "\r" in line:
            raise ValueError("line must not contain newline characters")
        try:
            self._proc.stdin.write(line + "\n")
            self._proc.stdin.flush()
        except (BrokenPipeError, ValueError) as exc:
            raise WorkerError(f"failed to write: {exc}") from exc

    def cmd(self, command: str, *, timeout: float = 30.0) -> dict:
        """Send a JSON request and return the matching response dict.

        Raises GrizCommandError on status=="error", WorkerError on
        transport failures or timeouts.
        """
        request_id = f"req_{next(self._id_counter)}"
        request = {"type": "request", "id": request_id, "cmd": command}
        self._write_line(json.dumps(request))

        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                if request_id in self._responses_by_id:
                    response = self._responses_by_id.pop(request_id)
                    break
                if not self._reader_alive:
                    raise WorkerError(
                        f"griz-server closed stdout before response to {request_id!r}; "
                        f"stderr tail: {self._stderr_tail()}"
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

    def send_command(self, command: str) -> None:
        """Write a raw command line (no envelope) to the server.

        Fire-and-forget: the response arrives asynchronously and can be
        retrieved via `drain_anon_responses()`. Used for debugging and
        for commands whose responses may never arrive (e.g. outrgb on
        builds where it aborts mid-flush).
        """
        self._write_line(command)

    def send_commands(self, commands: Iterable[str]) -> None:
        for c in commands:
            self.send_command(c)

    def drain_anon_responses(self) -> list[dict]:
        """Return and clear responses that arrived without a request id."""
        with self._cond:
            out = list(self._anon_responses)
            self._anon_responses.clear()
            return out

    def recent_stdout(self) -> list[str]:
        with self._stdout_lock:
            return list(self._stdout_log)

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
        """Send `quit`, close stdin, and wait for the subprocess to exit."""
        if self._proc is None:
            return
        proc = self._proc
        self._proc = None

        if proc.poll() is None and proc.stdin is not None:
            try:
                proc.stdin.write("quit\n")
                proc.stdin.flush()
            except (BrokenPipeError, ValueError, OSError):
                pass
            try:
                proc.stdin.close()
            except (BrokenPipeError, OSError):
                pass

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

    def __enter__(self) -> "Worker":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.cleanup()

    def __del__(self) -> None:
        try:
            self.cleanup()
        except Exception:
            pass


def _find_griz_server() -> str:
    env_bin = os.environ.get("GRIZ_BIN")
    if env_bin:
        path = Path(env_bin)
        if path.is_file() and os.access(path, os.X_OK):
            return str(path)
        raise FileNotFoundError(f"GRIZ_BIN does not point at an executable: {env_bin}")

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
        "griz-server binary not found. Set GRIZ_BIN or build with `./build.sh server`."
    )
