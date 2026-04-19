"""Exception hierarchy for the griz package.

The worker-layer exceptions (`WorkerError`, `GrizCommandError`) continue
to live in `griz.worker` for backward compatibility and are re-exported
from the top-level package alongside the session-level exceptions
defined here.
"""

from __future__ import annotations


class GrizError(Exception):
    """Base class for all griz package errors."""


class GrizConnectionError(GrizError):
    """Raised when the Worker subprocess cannot be started or reached."""


class UnknownFieldError(GrizError):
    """Raised when a (field, component) pair is not in the results map."""


class DatabaseError(GrizError):
    """Raised when open/close of a Mili database fails."""


class RenderingError(GrizError):
    """Raised when a screenshot or render command fails."""
