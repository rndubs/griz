"""Two-client attach test (planning/DEMO.md task D).

Spawns one griz-server in spawn mode, attaches a second RpcWorker via
`attach_rendezvous=`, and verifies:

* Both clients share the same Analysis (mutations made by client A are
  observed by client B).
* State-mutating commands broadcast `state_changed` events to every
  connected client (not just the sender).
* Auto-push JPEG frames broadcast to every connected client — this is
  what lets the Qt UI paint frames triggered by MCP in the demo.
* Detaching client B does not kill the server (client A keeps working).
"""

from __future__ import annotations

import json
import time
from pathlib import Path

import pytest

from griz.rpc_worker import RpcWorker
from griz.worker import WorkerError


def _drain_state_changed(w: RpcWorker, *, min_seq: int = 0,
                         deadline_s: float = 5.0) -> dict:
    """Wait for the next state_changed event with state_seq > min_seq."""
    end = time.monotonic() + deadline_s
    with w._cond:  # noqa: SLF001 — test introspects internals
        while True:
            for evt in list(w._events):  # noqa: SLF001
                if (
                    evt.get("type") == "event"
                    and evt.get("event") == "state_changed"
                    and evt.get("state_seq", -1) > min_seq
                ):
                    return evt
            remaining = end - time.monotonic()
            if remaining <= 0:
                raise AssertionError(
                    f"no state_changed event with seq > {min_seq} arrived "
                    f"within {deadline_s}s"
                )
            w._cond.wait(timeout=remaining)  # noqa: SLF001


def _drain_jpeg(w: RpcWorker, deadline_s: float = 5.0) -> dict:
    end = time.monotonic() + deadline_s
    with w._cond:  # noqa: SLF001
        while True:
            for frame in list(w._anon_binaries):  # noqa: SLF001
                if frame.get("codec") == 0x01:
                    w._anon_binaries.remove(frame)  # noqa: SLF001
                    return frame
            remaining = end - time.monotonic()
            if remaining <= 0:
                raise AssertionError("no JPEG frame arrived")
            w._cond.wait(timeout=remaining)  # noqa: SLF001


def _clear_events_and_frames(w: RpcWorker) -> None:
    with w._cond:  # noqa: SLF001
        w._events.clear()  # noqa: SLF001
        w._anon_binaries.clear()  # noqa: SLF001


def test_two_clients_share_state_and_receive_broadcasts(
    griz_bin: str, sample_database: Path, tmp_path: Path
) -> None:
    # Use an explicit rendezvous path so both workers agree.
    rv_path = tmp_path / "rv.json"

    # Client A spawns the server. Point the server at a rendezvous file
    # we control so client B can attach deterministically.
    import subprocess
    import socket
    proc = subprocess.Popen(
        [
            griz_bin,
            "--transport=rpc",
            f"--rendezvous={rv_path}",
            "-i", str(sample_database),
            "-w", "256", "256",
        ],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        # Wait for the rendezvous file.
        deadline = time.monotonic() + 10.0
        while not rv_path.exists():
            if time.monotonic() >= deadline:
                raise AssertionError("rendezvous file never appeared")
            if proc.poll() is not None:
                raise AssertionError(
                    f"server exited early (rc={proc.returncode}); "
                    f"stderr: {proc.stderr.read() if proc.stderr else ''}"
                )
            time.sleep(0.05)

        # Client A attaches (no spawn).
        client_a = RpcWorker(attach_rendezvous=rv_path)
        try:
            # Client B also attaches — shares the same server.
            client_b = RpcWorker(attach_rendezvous=rv_path)
            try:
                _clear_events_and_frames(client_a)
                _clear_events_and_frames(client_b)

                # Client A mutates state. Both clients should see a
                # state_changed event and an auto-push JPEG frame.
                client_a.cmd("state 1")

                evt_a = _drain_state_changed(client_a)
                evt_b = _drain_state_changed(client_b)
                assert evt_a["state_seq"] == evt_b["state_seq"], (
                    "state_changed state_seq should match across clients: "
                    f"A={evt_a['state_seq']} B={evt_b['state_seq']}"
                )

                frame_a = _drain_jpeg(client_a)
                frame_b = _drain_jpeg(client_b)
                assert frame_a["header"]["seq"] == frame_b["header"]["seq"]
                assert frame_a["body"] == frame_b["body"]

                # Sanity: both clients can run independent queries.
                resp_a = client_a.cmd("q_state")
                resp_b = client_b.cmd("q_state")
                data_a = resp_a.get("data") or {}
                data_b = resp_b.get("data") or {}
                # time.state_number should agree — shared Analysis.
                ta = (data_a.get("time") or {}).get("state_number")
                tb = (data_b.get("time") or {}).get("state_number")
                assert ta == tb, (
                    f"state_number should match across clients: A={ta} B={tb}"
                )

                # Detach B. A should keep working.
                client_b.cleanup()
                client_b = None

                # A mutating command still succeeds and still auto-pushes.
                _clear_events_and_frames(client_a)
                prev_seq = evt_a["state_seq"]
                client_a.cmd("state 2")
                evt_a2 = _drain_state_changed(client_a, min_seq=prev_seq)
                assert evt_a2["state_seq"] > prev_seq
            finally:
                if client_b is not None:
                    client_b.cleanup()
        finally:
            # Client A is an attach worker — cleanup closes its socket but
            # does NOT kill the server. We own the subprocess.
            client_a.cleanup()
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def test_attach_mode_does_not_kill_server(
    griz_bin: str, sample_database: Path, tmp_path: Path
) -> None:
    """Closing an attach worker must not quit the server."""
    rv_path = tmp_path / "rv.json"
    import subprocess
    proc = subprocess.Popen(
        [
            griz_bin,
            "--transport=rpc",
            f"--rendezvous={rv_path}",
            "-i", str(sample_database),
            "-w", "128", "128",
        ],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        deadline = time.monotonic() + 10.0
        while not rv_path.exists():
            if time.monotonic() >= deadline:
                raise AssertionError("rendezvous file never appeared")
            if proc.poll() is not None:
                raise AssertionError("server exited early")
            time.sleep(0.05)

        attached = RpcWorker(attach_rendezvous=rv_path)
        # Handshake worked.
        resp = attached.cmd("q_state")
        assert resp.get("status") == "ok"
        # Cleanup closes the socket but does not send `quit` and does not
        # reap the subprocess.
        attached.cleanup()

        # Server should still be running.
        time.sleep(0.2)
        assert proc.poll() is None, "server exited after attach cleanup"

        # We can attach again.
        attached2 = RpcWorker(attach_rendezvous=rv_path)
        resp = attached2.cmd("q_state")
        assert resp.get("status") == "ok"
        attached2.cleanup()
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
