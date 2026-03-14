from __future__ import annotations

import http.client
import json
import time
from collections.abc import Mapping
from pathlib import Path

from tests.ui.golden_assertions import SCREENSHOTS_DIR, assert_screenshot_matches_golden

DEFAULT_WAIT_FOR_ID_TIMEOUT_SECONDS = 5.0


def _http_request(
    host: str,
    port: int,
    method: str,
    path: str,
    body: str | None = None,
    content_type: str | None = None,
    timeout_seconds: float = 4.0,
) -> tuple[int, str]:
    connection = http.client.HTTPConnection(host, port, timeout=timeout_seconds)
    try:
        headers: dict[str, str] = {}
        if body is not None:
            if content_type is not None:
                headers["Content-Type"] = content_type
            connection.request(method, path, body=body.encode("utf-8"), headers=headers)
        else:
            connection.request(method, path, headers=headers)
        response = connection.getresponse()
        payload = response.read().decode("utf-8", errors="replace").strip()
        return response.status, payload
    finally:
        connection.close()


def wait_for_control_server(host: str, port: int, timeout_seconds: float = 8.0) -> None:
    deadline = time.monotonic() + timeout_seconds
    last_error: Exception | None = None

    while time.monotonic() < deadline:
        try:
            status, payload = _http_request(
                host, port, "GET", "/health", timeout_seconds=1.0
            )
            health = json.loads(payload)
            if (
                status == 200
                and isinstance(health, Mapping)
                and health.get("ok") is True
            ):
                return
        except (OSError, json.JSONDecodeError) as exc:
            last_error = exc

        time.sleep(0.05)

    details = f"\nLast connection error: {last_error}" if last_error is not None else ""
    raise AssertionError(
        f"Timed out waiting for HTTP control server at {host}:{port}.{details}"
    )


def send_control_command(
    host: str,
    port: int,
    command: str,
    args: Mapping[str, object] | None = None,
    timeout_seconds: float = 4.0,
) -> str:
    request_payload = {"cmd": command}
    if args is not None:
        request_payload["args"] = dict(args)

    try:
        status, payload = _http_request(
            host,
            port,
            "POST",
            "/control",
            body=json.dumps(request_payload, separators=(",", ":")),
            content_type="application/json",
            timeout_seconds=timeout_seconds,
        )
    except OSError as exc:
        raise AssertionError(
            f"Control command request failed: {command}\n{exc}"
        ) from exc

    try:
        response = json.loads(payload)
    except json.JSONDecodeError as exc:
        raise AssertionError(
            "Control command returned invalid JSON.\n"
            f"command: {command}\n"
            f"status: {status}\n"
            f"response: {payload}"
        ) from exc

    if not isinstance(response, Mapping):
        raise AssertionError(
            "Control command returned unexpected response type.\n"
            f"command: {command}\n"
            f"status: {status}\n"
            f"response: {payload}"
        )

    if status != 200:
        error = response.get("error")
        error_message = (
            error.get("message")
            if isinstance(error, Mapping) and isinstance(error.get("message"), str)
            else payload
        )
        raise AssertionError(
            "Control command failed.\n"
            f"command: {command}\n"
            f"status: {status}\n"
            f"response: {error_message}"
        )

    if response.get("ok") is True:
        result = response.get("result")
        if isinstance(result, Mapping) and isinstance(result.get("message"), str):
            return result["message"]
        return json.dumps(response, sort_keys=True)

    error = response.get("error")
    error_message = (
        error.get("message")
        if isinstance(error, Mapping) and isinstance(error.get("message"), str)
        else payload
    )
    raise AssertionError(
        f"Control command failed.\ncommand: {command}\nerror: {error_message}"
    )


class SimControlClient:
    def __init__(
        self,
        host: str,
        port: int,
        orientation: str | None = None,
        golden_subdir: str | None = None,
    ) -> None:
        self._host = host
        self._port = port
        self._orientation = orientation
        self._golden_subdir = golden_subdir

    def command(
        self,
        command: str,
        args: Mapping[str, object] | None = None,
        timeout_seconds: float = 4.0,
    ) -> str:
        return send_control_command(
            self._host,
            self._port,
            command,
            args=args,
            timeout_seconds=timeout_seconds,
        )

    def wait_for_id(
        self,
        test_id: str,
        timeout_seconds: float = DEFAULT_WAIT_FOR_ID_TIMEOUT_SECONDS,
    ) -> str:
        timeout_ms = int(timeout_seconds * 1000)
        return self.command(
            "wait_for_id",
            {"id": test_id, "timeout_ms": timeout_ms},
            timeout_seconds=timeout_seconds,
        )

    def click_id(self, test_id: str, timeout_seconds: float = 4.0) -> str:
        return self.command(
            "click_id", {"id": test_id}, timeout_seconds=timeout_seconds
        )

    def scroll_to_view_id(self, test_id: str, timeout_seconds: float = 4.0) -> str:
        return self.command(
            "scroll_to_view_id",
            {"id": test_id},
            timeout_seconds=timeout_seconds,
        )

    def type_text(self, text: str, timeout_seconds: float = 6.0) -> str:
        return self.command(
            "type_text", {"text": text}, timeout_seconds=timeout_seconds
        )

    def set_dropdown_id(
        self, test_id: str, index: int, timeout_seconds: float = 4.0
    ) -> str:
        return self.command(
            "set_dropdown_id",
            {"id": test_id, "index": index},
            timeout_seconds=timeout_seconds,
        )

    def screenshot(self, file_path: str | Path, timeout_seconds: float = 8.0) -> str:
        return self.command(
            "screenshot", {"path": str(file_path)}, timeout_seconds=timeout_seconds
        )

    def sleep_ms(self, milliseconds: int, timeout_seconds: float | None = None) -> str:
        effective_timeout = (
            max(4.0, (milliseconds / 1000.0) + 2.0)
            if timeout_seconds is None
            else timeout_seconds
        )
        return self.command(
            "sleep_ms",
            {"milliseconds": milliseconds},
            timeout_seconds=effective_timeout,
        )

    def compare_and_save_screenshot(
        self, screenshot_name: str, timeout_seconds: float = 8.0
    ) -> None:
        if self._orientation is not None:
            screenshot_name = f"{screenshot_name}_{self._orientation}"
        screenshots_dir = (
            SCREENSHOTS_DIR / self._golden_subdir
            if self._golden_subdir is not None
            else SCREENSHOTS_DIR
        )
        screenshots_dir.mkdir(parents=True, exist_ok=True)
        screenshot_file = screenshots_dir / f"{screenshot_name}.png"
        if screenshot_file.exists():
            screenshot_file.unlink()
        self.screenshot(screenshot_file, timeout_seconds=timeout_seconds)
        assert_screenshot_matches_golden(
            screenshot_file,
            "",
            golden_subdir=self._golden_subdir,
        )

    def quit(self, timeout_seconds: float = 4.0) -> str:
        try:
            return self.command("quit", timeout_seconds=timeout_seconds)
        except AssertionError:
            # Simulator may close before the HTTP response is fully observed.
            return "quit"
