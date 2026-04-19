"""Phase 1 minimal worker: spawn griz-server and pipe plain-text commands.

The Phase 1 server prints a single `READY` sentinel on stdout, then reads
newline-terminated commands from stdin. There is no per-command response
framing yet (that arrives in Phase 2 with the JSON envelope). This worker
therefore treats post-READY stdout/stderr as opaque log streams and drains
them in background threads so the server never blocks on full pipes.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import threading
import time
from collections import deque
from pathlib import Path
from typing import Iterable


class WorkerError(RuntimeError):
    """Raised when the griz-server subprocess misbehaves."""


_READY_SENTINEL = "READY"


class Worker:
    """Manage a `griz-server --transport=stdio` subprocess.

    Phase 1 contract: caller sends plain-text commands via `send_command`
    and inspects side effects (e.g. files written by `outrgb`/`outpng`).
    Captured stdout/stderr lines are accessible via `recent_stdout()` and
    `recent_stderr()` for diagnostics, but the protocol does not correlate
    output to a specific command.
    """

    def __init__(
        self,
        database: str | os.PathLike[str],
        *,
        griz_bin: str | os.PathLike[str] | None = None,
        width: int = 1024,
        height: int = 1024,
        ready_timeout: float = 30.0,
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
        self._shutdown_timeout = float(shutdown_timeout)

        self._stdout_log: deque[str] = deque(maxlen=log_buffer)
        self._stderr_log: deque[str] = deque(maxlen=log_buffer)
        self._stdout_lock = threading.Lock()
        self._stderr_lock = threading.Lock()

        self._proc: subprocess.Popen[str] | None = None
        self._stdout_thread: threading.Thread | None = None
        self._stderr_thread: threading.Thread | None = None
        self._final_returncode: int | None = None

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
            target=self._drain, args=(self._proc.stderr, self._stderr_log, self._stderr_lock),
            daemon=True, name="griz-server-stderr",
        )
        self._stderr_thread.start()

        self._wait_for_ready()

        self._stdout_thread = threading.Thread(
            target=self._drain, args=(self._proc.stdout, self._stdout_log, self._stdout_lock),
            daemon=True, name="griz-server-stdout",
        )
        self._stdout_thread.start()

    def _wait_for_ready(self) -> None:
        assert self._proc is not None and self._proc.stdout is not None
        deadline = time.monotonic() + self._ready_timeout
        while True:
            if self._proc.poll() is not None:
                raise WorkerError(
                    f"griz-server exited before READY (rc={self._proc.returncode}); "
                    f"stderr tail: {self._stderr_tail()}"
                )
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise WorkerError(
                    f"griz-server did not emit READY within {self._ready_timeout}s; "
                    f"stderr tail: {self._stderr_tail()}"
                )
            line = self._proc.stdout.readline()
            if not line:
                if self._proc.poll() is not None:
                    raise WorkerError(
                        f"griz-server closed stdout before READY (rc={self._proc.returncode}); "
                        f"stderr tail: {self._stderr_tail()}"
                    )
                time.sleep(0.05)
                continue
            stripped = line.rstrip("\r\n")
            with self._stdout_lock:
                self._stdout_log.append(stripped)
            if stripped == _READY_SENTINEL:
                return

    @staticmethod
    def _drain(stream, sink: deque[str], lock: threading.Lock) -> None:
        try:
            for line in stream:
                with lock:
                    sink.append(line.rstrip("\r\n"))
        except (ValueError, OSError):
            pass

    def send_command(self, command: str) -> None:
        """Write a single plain-text command to the server's stdin."""
        if self._proc is None or self._proc.stdin is None:
            raise WorkerError("worker is not running")
        if self._proc.poll() is not None:
            raise WorkerError(
                f"griz-server has exited (rc={self._proc.returncode}); "
                f"stderr tail: {self._stderr_tail()}"
            )
        if "\n" in command or "\r" in command:
            raise ValueError("command must not contain newline characters")

        try:
            self._proc.stdin.write(command + "\n")
            self._proc.stdin.flush()
        except (BrokenPipeError, ValueError) as exc:
            raise WorkerError(f"failed to write command: {exc}") from exc

    def send_commands(self, commands: Iterable[str]) -> None:
        for cmd in commands:
            self.send_command(cmd)

    def recent_stdout(self) -> list[str]:
        with self._stdout_lock:
            return list(self._stdout_log)

    def recent_stderr(self) -> list[str]:
        with self._stderr_lock:
            return list(self._stderr_log)

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

        for thread in (self._stdout_thread, self._stderr_thread):
            if thread is not None:
                thread.join(timeout=1.0)
        self._stdout_thread = None
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
