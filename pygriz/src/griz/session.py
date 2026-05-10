"""High-level `Griz` session class.

Wraps a `Worker` subprocess and exposes the namespaced APIs described in
planning/mcp/03-python-api.md. A `Griz` instance with no database is
inert — the worker spawns on the first `open()` call and is torn down
in `close()` / `__exit__`.

`screenshot()` uses `outrgb` to capture SGI RGB frames and converts
them to PNG in Python via the `_sgi` module (no Pillow required).
"""

from __future__ import annotations

import os
import tempfile
from pathlib import Path
from typing import TYPE_CHECKING, Any, Callable

from griz.exceptions import GrizConnectionError
from griz.field import FieldAPI
from griz.materials import MaterialsAPI
from griz.render import RenderAPI
from griz.selection import SelectionAPI
from griz.time_ import TimeAPI
from griz.view import ViewAPI
from griz.worker import Worker

if TYPE_CHECKING:
    WorkerFactory = Callable[..., Worker]


DEFAULT_WIDTH = 1024
DEFAULT_HEIGHT = 1024
DEFAULT_TIMEOUT = 30.0


class Griz:
    """Primary Python interface to the Griz visualization engine."""

    def __init__(
        self,
        database: str | os.PathLike[str] | None = None,
        *,
        griz_bin: str | os.PathLike[str] | None = None,
        width: int = DEFAULT_WIDTH,
        height: int = DEFAULT_HEIGHT,
        timeout: float = DEFAULT_TIMEOUT,
        worker_factory: "WorkerFactory | None" = None,
    ) -> None:
        self._griz_bin = griz_bin
        self._width = int(width)
        self._height = int(height)
        self._timeout = float(timeout)
        self._worker_factory: "WorkerFactory" = worker_factory or Worker

        self._database_path: Path | None = None
        self._worker: Any = None

        self._field = FieldAPI(self)
        self._view = ViewAPI(self)
        self._time = TimeAPI(self)
        self._materials = MaterialsAPI(self)
        self._selection = SelectionAPI(self)
        self._render = RenderAPI(self)

        if database is not None:
            self.open(database)

    # ------------------------------------------------------------------ #
    # Lifecycle
    # ------------------------------------------------------------------ #

    def open(self, path: str | os.PathLike[str]) -> None:
        """Open a database, spawning the worker if needed."""
        if self._worker is not None:
            raise GrizConnectionError(
                "a database is already open; call close() or reload() first"
            )

        db_path = Path(path)
        if not db_path.exists():
            raise FileNotFoundError(f"database not found: {db_path}")

        kwargs: dict[str, Any] = {
            "width": self._width,
            "height": self._height,
        }
        if self._griz_bin is not None:
            kwargs["griz_bin"] = self._griz_bin

        self._worker = self._worker_factory(db_path, **kwargs)
        self._database_path = db_path

    def attach(self, rendezvous_path: str | os.PathLike[str]) -> None:
        """Attach to a griz-server already spawned by another client.

        Reads the given rendezvous file, connects, and completes the
        standard hello/hello_ack/ready handshake. The Analysis and
        OSMesa context are shared with the spawning peer (DEMO.md) —
        this session's mutations are visible to the UI and vice versa.

        In attach mode `database_path` stays unset (we don't own the DB
        file path); call `state()` to discover what the peer has open.
        `close()` tears down the attached socket only; the underlying
        server keeps running for other clients.
        """
        if self._worker is not None:
            raise GrizConnectionError(
                "a database is already open; call close() or reload() first"
            )
        # Lazy import to avoid making RpcWorker a hard dep of Griz when
        # callers use the stdio Worker factory.
        from griz.rpc_worker import RpcWorker
        self._worker = RpcWorker(
            attach_rendezvous=rendezvous_path,
            width=self._width,
            height=self._height,
        )
        self._database_path = None

    def reload(self) -> None:
        """Close and reopen the current database."""
        if self._database_path is None:
            raise GrizConnectionError("no database is open")
        path = self._database_path
        self.close()
        self.open(path)

    def close(self) -> None:
        """Tear down the worker subprocess."""
        if self._worker is not None:
            try:
                self._worker.cleanup()
            finally:
                self._worker = None
        self._database_path = None

    def __enter__(self) -> "Griz":
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass

    # ------------------------------------------------------------------ #
    # Top-level operations
    # ------------------------------------------------------------------ #

    def state(self) -> dict:
        """Return the current viewer state via `q_state`."""
        resp = self._require_worker().cmd("q_state")
        data = resp.get("data")
        return dict(data) if isinstance(data, dict) else {}

    def raw(self, command: str, *, timeout: float | None = None) -> dict:
        """Escape hatch: send any raw command and return the full response."""
        worker = self._require_worker()
        kwargs: dict[str, Any] = {}
        if timeout is not None:
            kwargs["timeout"] = timeout
        return worker.cmd(command, **kwargs)

    def screenshot(
        self,
        path: str | os.PathLike[str] | None = None,
        *,
        format: str = "png",
    ) -> str | bytes:
        """Capture the current frame.

        If `path` is given, writes the image file there and returns the
        absolute path as a string. If `path` is None, returns the image
        as PNG bytes (default) or raw SGI RGB bytes if ``format="rgb"``.
        """
        from griz._sgi import sgi_to_png

        worker = self._require_worker()

        if path is not None:
            out_path = Path(path).resolve()
            worker.cmd(f"outrgb {out_path}")
            if format == "png" and not str(out_path).lower().endswith(".rgb"):
                with open(out_path, "rb") as fh:
                    rgb_data = fh.read()
                png_data = sgi_to_png(rgb_data)
                with open(out_path, "wb") as fh:
                    fh.write(png_data)
            return str(out_path)

        tmp_dir = _preferred_tmpdir()
        fd, tmp_path = tempfile.mkstemp(suffix=".rgb", dir=str(tmp_dir))
        os.close(fd)
        try:
            worker.cmd(f"outrgb {tmp_path}")
            with open(tmp_path, "rb") as fh:
                rgb_data = fh.read()
            if format == "png":
                return sgi_to_png(rgb_data)
            return rgb_data
        finally:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass

    # ------------------------------------------------------------------ #
    # Selection shortcuts (mirrored on selection namespace)
    # ------------------------------------------------------------------ #

    def select(self, klass: str, ids) -> dict:
        return self._selection.select(klass, ids)

    def highlight(self, klass: str, ids) -> dict:
        return self._selection.highlight(klass, ids)

    def clear_picks(self) -> dict:
        return self._selection.clear_picks()

    # ------------------------------------------------------------------ #
    # Namespaced APIs
    # ------------------------------------------------------------------ #

    @property
    def field(self) -> FieldAPI:
        return self._field

    @property
    def view(self) -> ViewAPI:
        return self._view

    @property
    def time(self) -> TimeAPI:
        return self._time

    @property
    def materials(self) -> MaterialsAPI:
        return self._materials

    @property
    def selection(self) -> SelectionAPI:
        return self._selection

    @property
    def render(self) -> RenderAPI:
        return self._render

    # ------------------------------------------------------------------ #
    # Introspection
    # ------------------------------------------------------------------ #

    @property
    def database_path(self) -> Path | None:
        return self._database_path

    @property
    def server_info(self) -> dict | None:
        worker = self._worker
        return getattr(worker, "server_info", None) if worker is not None else None

    @property
    def is_open(self) -> bool:
        return self._worker is not None

    # ------------------------------------------------------------------ #
    # Internal
    # ------------------------------------------------------------------ #

    def _require_worker(self):
        if self._worker is None:
            raise GrizConnectionError(
                "no database is open; call Griz.open(path) first"
            )
        return self._worker


def _preferred_tmpdir() -> Path:
    """Follow the project convention of scratch files under /tmp/<user>/."""
    user = os.environ.get("USER") or ""
    if user:
        candidate = Path("/tmp") / user
        try:
            candidate.mkdir(parents=True, exist_ok=True)
            if os.access(candidate, os.W_OK):
                return candidate
        except OSError:
            pass
    return Path(tempfile.gettempdir())
