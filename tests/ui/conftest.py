from __future__ import annotations

from collections.abc import Iterator
from contextlib import contextmanager
from pathlib import Path

import pytest

from tests.ui.simulator_runner import (
    BOARD_IDS,
    ORIENTATIONS,
    SimulatorSession,
    run_simulator,
)

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SIMULATOR_BINARY = PROJECT_ROOT / "simulator" / "build" / "bin" / "esptransit_sim"


def _require_simulator_binary_or_exit() -> None:
    if SIMULATOR_BINARY.exists():
        return
    pytest.exit(
        "Simulator binary not found.\n"
        f"path: {SIMULATOR_BINARY}\n"
        "Build it first with `mise run sim-build` (or run `mise run ui-test`, which depends on sim-build).",
        returncode=1,
    )


@pytest.hookimpl(tryfirst=True)
def pytest_sessionstart(session: pytest.Session) -> None:
    """Verify simulator binary exists once on controller at session start."""
    # In xdist, only the controller process checks.
    if getattr(session.config, "workerinput", None) is not None:
        return

    _require_simulator_binary_or_exit()


@pytest.fixture(scope="session")
def simulator_binary() -> Path:
    """Return simulator binary path."""
    return SIMULATOR_BINARY


@pytest.fixture(params=BOARD_IDS)
def board(request: pytest.FixtureRequest) -> str:
    return str(request.param)


@pytest.fixture(params=ORIENTATIONS)
def orientation(request: pytest.FixtureRequest) -> str:
    return str(request.param)


@pytest.fixture
def run_sim(simulator_binary: Path, board: str, orientation: str):
    """Return a context manager factory with board/orientation pre-bound."""

    @contextmanager
    def _run(*args: str, **kwargs) -> Iterator[SimulatorSession]:
        with run_simulator(
            simulator_binary, *args, board=board, orientation=orientation, **kwargs
        ) as session:
            yield session

    return _run
