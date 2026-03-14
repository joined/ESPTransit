from __future__ import annotations

import json
import os
import socket
import subprocess
import tempfile
import threading
import time
from collections.abc import Iterator, Mapping, Sequence
from contextlib import contextmanager
from pathlib import Path
from typing import Any

from tests.ui.sim_control import SimControlClient, wait_for_control_server
from tests.ui.simulator_process import start_simulator_process, terminate_process

PROJECT_ROOT = Path(__file__).resolve().parents[2]
FIXTURES_DIR = Path(__file__).resolve().parent / "fixtures"
BOARDS: dict[str, dict] = json.loads((PROJECT_ROOT / "boards.json").read_text())
BOARD_IDS = list(BOARDS)
DEFAULT_BOARD = BOARD_IDS[0]
ORIENTATIONS = ["portrait", "landscape"]


def rotation_for_board(board: str, orientation: str) -> int:
    """Map a logical orientation name to rotation degrees for a specific board.

    Portrait-native boards (width < height) use 0° for portrait.
    Landscape-native boards (width > height) use 0° for landscape.
    """
    if orientation not in ORIENTATIONS:
        raise ValueError(f"Unsupported orientation: {orientation}")
    board_info = BOARDS.get(board, {})
    is_native_landscape = board_info.get("width", 0) > board_info.get("height", 0)
    if is_native_landscape:
        return 0 if orientation == "landscape" else 90
    return 0 if orientation == "portrait" else 90


def is_two_view_mode(board: str, orientation: str) -> bool:
    """Check if station search uses two-view mode for a board/orientation combo.

    Two-view mode activates when the effective orientation is landscape and the
    effective vertical resolution is <= 600px.
    """
    board_info = BOARDS.get(board, {})
    w, h = board_info.get("width", 0), board_info.get("height", 0)
    if orientation == "landscape":
        eff_w, eff_h = max(w, h), min(w, h)
    else:
        eff_w, eff_h = min(w, h), max(w, h)
    return eff_w > eff_h and eff_h <= 600


DEFAULT_WAIT_FOR_LOG_TIMEOUT_SECONDS = 5.0
DEFAULT_ASSERT_NO_LOG_TIMEOUT_SECONDS = 1.0
MAX_CONTROL_PORT_BIND_ATTEMPTS = 4


def _allocate_control_port(host: str) -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        _, port = sock.getsockname()
        return int(port)


def _is_bind_failure(startup_output: str) -> bool:
    return "Failed to bind HTTP control server on " in startup_output


def _validate_board(board: str) -> None:
    if board not in BOARDS:
        supported = ", ".join(BOARD_IDS)
        raise ValueError(f"Unknown board: {board} (available: {supported})")


class LogReader:
    """Reads stdout from a subprocess in a background thread."""

    def __init__(self, stdout_stream) -> None:  # noqa: ANN001
        self._lines: list[str] = []
        self._lock = threading.Lock()
        self._thread = threading.Thread(
            target=self._read_loop, args=(stdout_stream,), daemon=True
        )
        self._thread.start()

    def _read_loop(self, stream) -> None:  # noqa: ANN001
        try:
            for line in stream:
                with self._lock:
                    self._lines.append(line.rstrip("\n"))
        finally:
            stream.close()

    def get_lines(self) -> list[str]:
        with self._lock:
            return list(self._lines)

    def get_output(self) -> str:
        return "\n".join(self.get_lines())

    def join(self, timeout: float | None = None) -> None:
        self._thread.join(timeout=timeout)


class SimulatorSession:
    """Running simulator session with control helpers and live log assertions."""

    def __init__(self, client: SimControlClient, log_reader: LogReader) -> None:
        self._client = client
        self._log_reader = log_reader
        self._log_cursor = 0

    def __getattr__(self, name: str) -> Any:
        return getattr(self._client, name)

    @property
    def output(self) -> str:
        return self._log_reader.get_output()

    def wait_for_log(
        self,
        fragment: str,
        timeout_seconds: float = DEFAULT_WAIT_FOR_LOG_TIMEOUT_SECONDS,
    ) -> None:
        deadline = time.monotonic() + timeout_seconds
        while True:
            lines = self._log_reader.get_lines()
            for idx in range(self._log_cursor, len(lines)):
                if fragment in lines[idx]:
                    self._log_cursor = idx + 1
                    return
            if time.monotonic() >= deadline:
                break
            time.sleep(0.05)

        lines = self._log_reader.get_lines()
        recent = lines[self._log_cursor :]
        recent_text = "\n".join(recent) if recent else "(no output since last match)"
        raise AssertionError(
            f"Timed out waiting for log: {fragment!r}\n"
            f"--- Output since last match (cursor={self._log_cursor}) ---\n{recent_text}"
        )

    def assert_no_log(
        self,
        fragment: str,
        timeout_seconds: float = DEFAULT_ASSERT_NO_LOG_TIMEOUT_SECONDS,
        *,
        since_last_match: bool = True,
    ) -> None:
        self.assert_no_logs(
            [fragment],
            timeout_seconds=timeout_seconds,
            since_last_match=since_last_match,
        )

    def assert_no_logs(
        self,
        fragments: Sequence[str],
        timeout_seconds: float = DEFAULT_ASSERT_NO_LOG_TIMEOUT_SECONDS,
        *,
        since_last_match: bool = True,
    ) -> None:
        if not fragments:
            raise ValueError("assert_no_logs requires at least one fragment")

        start_index = self._log_cursor if since_last_match else 0
        scan_cursor = start_index
        deadline = time.monotonic() + timeout_seconds

        while True:
            lines = self._log_reader.get_lines()
            for idx in range(scan_cursor, len(lines)):
                line = lines[idx]
                for fragment in fragments:
                    if fragment in line:
                        checked_lines = lines[start_index : idx + 1]
                        checked_output = (
                            "\n".join(checked_lines)
                            if checked_lines
                            else "(no output checked)"
                        )
                        raise AssertionError(
                            f"Unexpected log fragment observed: {fragment!r}\n"
                            f"line index: {idx}\n"
                            f"line: {line}\n"
                            f"--- Output checked (from index={start_index}) ---\n{checked_output}"
                        )

            scan_cursor = len(lines)
            if time.monotonic() >= deadline:
                return
            time.sleep(0.05)


@contextmanager
def run_simulator(
    simulator_binary: Path,
    *args: str,
    orientation: str | None = None,
    board: str = DEFAULT_BOARD,
    fixture_name: str | None = None,
    env_overrides: Mapping[str, str] | None = None,
) -> Iterator[SimulatorSession]:
    timeout_seconds = float(os.environ.get("SIMULATOR_TEST_TIMEOUT", "8"))
    control_host = "127.0.0.1"
    env = os.environ.copy()
    env.setdefault("UI_TEST", "1")
    env.setdefault("SIMULATOR_DELAY_SCALE", "0.1")
    if env_overrides is not None:
        env.update(env_overrides)

    _validate_board(board)
    base_simulator_args = ["-m", "--board", board, *args]
    if orientation is not None:
        base_simulator_args.extend(["-r", str(rotation_for_board(board, orientation))])

    with tempfile.TemporaryDirectory(prefix="esptransit-ui-test-") as tmpdir:
        config_file = Path(tmpdir) / "simulator-config.json"
        if fixture_name is not None:
            fixture_path = FIXTURES_DIR / fixture_name
            config_file.write_bytes(fixture_path.read_bytes())

        proc: subprocess.Popen[str] | None = None
        log_reader: LogReader | None = None
        control_port = 0
        bind_retry_ports: list[int] = []
        for attempt in range(1, MAX_CONTROL_PORT_BIND_ATTEMPTS + 1):
            control_port = _allocate_control_port(control_host)
            simulator_args = [
                *base_simulator_args,
                "--control-http",
                "--control-http-port",
                str(control_port),
            ]
            proc = start_simulator_process(
                simulator_binary,
                *simulator_args,
                config_file=config_file,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            log_reader = LogReader(proc.stdout)

            try:
                wait_for_control_server(
                    control_host, control_port, timeout_seconds=timeout_seconds + 2
                )
                break
            except AssertionError as exc:
                terminate_process(proc)
                log_reader.join(timeout=2.0)
                startup_output = log_reader.get_output()
                bind_failed = _is_bind_failure(startup_output)
                if bind_failed and attempt < MAX_CONTROL_PORT_BIND_ATTEMPTS:
                    bind_retry_ports.append(control_port)
                    continue

                retry_details = (
                    "bind_retry_ports: "
                    f"{', '.join(str(port) for port in bind_retry_ports)}\n"
                    if bind_retry_ports
                    else ""
                )
                if startup_output:
                    raise AssertionError(
                        "Timed out waiting for simulator control server startup.\n"
                        f"port: {control_port}\n"
                        f"attempt: {attempt}/{MAX_CONTROL_PORT_BIND_ATTEMPTS}\n"
                        f"{retry_details}"
                        f"--- Captured startup output ---\n{startup_output}"
                    ) from exc

                if bind_retry_ports:
                    raise AssertionError(
                        "Timed out waiting for simulator control server startup after bind retries.\n"
                        f"port: {control_port}\n"
                        f"attempt: {attempt}/{MAX_CONTROL_PORT_BIND_ATTEMPTS}\n"
                        f"{retry_details}"
                    ) from exc
                raise

        if proc is None or log_reader is None:
            raise AssertionError("Failed to start simulator process")

        had_exception = False
        client = SimControlClient(
            control_host,
            control_port,
            orientation=orientation,
            golden_subdir=board,
        )
        session = SimulatorSession(client, log_reader)
        try:
            yield session
        except BaseException:
            had_exception = True
            raise
        finally:
            if proc.poll() is None:
                try:
                    client.quit(timeout_seconds=timeout_seconds + 2)
                except AssertionError:
                    if not had_exception:
                        raise

            log_reader.join(timeout=timeout_seconds + 5)

            if not had_exception:
                try:
                    return_code = proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    return_code = None
                if return_code is not None and return_code != 0:
                    raise AssertionError(
                        f"Simulator exited with code {return_code}\n"
                        f"--- Output ---\n{log_reader.get_output()}"
                    )

            terminate_process(proc)
