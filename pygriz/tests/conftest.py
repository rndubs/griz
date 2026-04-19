import os
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DB = REPO_ROOT / "Src" / "test" / "image" / "bar71" / "bar71.pltA"

# Pick the most recently built binary. Plain glob+next is not deterministic
# and will happily pick up stale `GRIZ4-*-old` build trees that shadow the
# current configure output.
_server_binaries = sorted(
    REPO_ROOT.glob("Src/GRIZ4-*/bin_server_opt/griz-server"),
    key=lambda p: p.stat().st_mtime,
    reverse=True,
)
DEFAULT_BIN = _server_binaries[0] if _server_binaries else None


@pytest.fixture(scope="session")
def griz_bin() -> str:
    binary = os.environ.get("GRIZ_BIN") or (str(DEFAULT_BIN) if DEFAULT_BIN else None)
    if not binary or not Path(binary).is_file():
        pytest.skip("griz-server binary not built; run ./build.sh server")
    return binary


@pytest.fixture(scope="session")
def sample_database() -> Path:
    if not DEFAULT_DB.exists():
        pytest.skip(f"sample database not found at {DEFAULT_DB}")
    return DEFAULT_DB
