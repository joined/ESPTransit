#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import mimetypes
import struct
import urllib.parse
from collections import defaultdict
from dataclasses import asdict, dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parent.parent
GOLDEN_DIR = PROJECT_ROOT / "tests" / "ui" / "golden"
ASSETS_DIR = Path(__file__).resolve().parent / "ui_golden_viewer_assets"
ASSET_FILES = {"index.html", "app.css", "app.js"}
BOARDS_JSON = PROJECT_ROOT / "boards.json"


@dataclass(frozen=True)
class ViewerDefaults:
    monitor_diagonal_in: float
    monitor_width_px: int
    monitor_height_px: int


def load_boards() -> dict[str, Any]:
    return json.loads(BOARDS_JSON.read_text())


def read_png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as file:
        signature = file.read(8)
        if signature != b"\x89PNG\r\n\x1a\n":
            raise ValueError(f"{path} is not a PNG file")

        chunk_length_raw = file.read(4)
        chunk_type = file.read(4)
        if len(chunk_length_raw) != 4 or chunk_type != b"IHDR":
            raise ValueError(f"{path} has an invalid PNG header")

        chunk_length = struct.unpack(">I", chunk_length_raw)[0]
        ihdr = file.read(chunk_length)
        if len(ihdr) < 8:
            raise ValueError(f"{path} has a truncated IHDR chunk")

        width, height = struct.unpack(">II", ihdr[:8])
        return width, height


def list_screenshots(
    boards: dict[str, Any],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    groups: dict[str, dict[str, dict[str, Any]]] = defaultdict(dict)
    seen_boards: dict[str, dict[str, Any]] = {}

    if not GOLDEN_DIR.exists():
        return ([], [])

    for png in sorted(GOLDEN_DIR.rglob("*.png")):
        if not png.is_file():
            continue

        try:
            width, height = read_png_dimensions(png)
        except ValueError:
            continue

        relative_path = png.relative_to(GOLDEN_DIR)
        relative_parts = relative_path.parts
        group_name = relative_path.as_posix()
        board_id: str | None = None

        if len(relative_parts) > 1:
            folder = relative_parts[0]
            if folder in boards:
                board_id = folder
                group_name = Path(*relative_parts[1:]).as_posix()

        if board_id is None:
            continue

        seen_boards.setdefault(board_id, boards[board_id])

        screenshot_entry = {
            "name": group_name,
            "path": relative_path.as_posix(),
            "board": board_id,
            "width": width,
            "height": height,
            "orientation": "landscape" if width >= height else "portrait",
        }
        groups[group_name][board_id] = screenshot_entry

    board_items = []
    for board_id, info in seen_boards.items():
        board_items.append(
            {
                "id": board_id,
                "width": info["width"],
                "height": info["height"],
                "diagonal_inches": info["diagonal_inches"],
            }
        )

    board_items.sort(
        key=lambda item: (
            -(item["width"] * item["height"]),
            -item["width"],
            item["id"],
        )
    )

    grouped_items = [
        {"name": name, "by_board": groups[name]} for name in sorted(groups.keys())
    ]

    return grouped_items, board_items


class ViewerHandler(BaseHTTPRequestHandler):
    defaults: ViewerDefaults
    screenshot_groups: list[dict[str, Any]]
    boards: list[dict[str, Any]]

    def do_GET(self) -> None:  # noqa: N802
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/":
            self._serve_asset("index.html")
            return

        if path == "/api/config":
            self._serve_json({"defaults": asdict(self.defaults)})
            return

        if path == "/api/screenshots":
            self._serve_json(
                {
                    "groups": self.screenshot_groups,
                    "boards": self.boards,
                }
            )
            return

        if path.startswith("/golden/"):
            self._serve_golden(path.removeprefix("/golden/"))
            return

        if path == "/favicon.ico":
            self.send_response(HTTPStatus.NO_CONTENT)
            self.end_headers()
            return

        asset_name = path.removeprefix("/")
        if asset_name in ASSET_FILES:
            self._serve_asset(asset_name)
            return

        self.send_error(HTTPStatus.NOT_FOUND)

    def _serve_json(self, payload: dict[str, Any]) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _serve_asset(self, asset_name: str) -> None:
        if asset_name not in ASSET_FILES:
            self.send_error(HTTPStatus.NOT_FOUND)
            return

        asset_path = ASSETS_DIR / asset_name
        if not asset_path.exists() or not asset_path.is_file():
            self.send_error(HTTPStatus.NOT_FOUND, "asset not found")
            return

        data = asset_path.read_bytes()
        content_type, _ = mimetypes.guess_type(str(asset_path))
        self.send_response(HTTPStatus.OK)
        if asset_name == "index.html":
            content_type = "text/html; charset=utf-8"
        self.send_header("Content-Type", content_type or "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _serve_golden(self, encoded_path: str) -> None:
        relative_path = urllib.parse.unquote(encoded_path)
        if not relative_path or "\\" in relative_path:
            self.send_error(HTTPStatus.BAD_REQUEST, "invalid path")
            return

        requested_path = Path(relative_path)
        if requested_path.is_absolute() or any(
            part in {"", ".", ".."} for part in requested_path.parts
        ):
            self.send_error(HTTPStatus.BAD_REQUEST, "invalid path")
            return

        image_path = (GOLDEN_DIR / requested_path).resolve()
        golden_root = GOLDEN_DIR.resolve()
        try:
            image_path.relative_to(golden_root)
        except ValueError:
            self.send_error(HTTPStatus.BAD_REQUEST, "invalid path")
            return

        if (
            image_path.suffix.lower() != ".png"
            or not image_path.exists()
            or not image_path.is_file()
        ):
            self.send_error(HTTPStatus.NOT_FOUND, "image not found")
            return

        data = image_path.read_bytes()
        content_type, _ = mimetypes.guess_type(str(image_path))
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type or "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt: str, *args: object) -> None:
        return


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Serve tests/ui/golden/<resolution> screenshots in a physical-size-calibrated browser viewer."
    )
    parser.add_argument(
        "--host", default="127.0.0.1", help="HTTP bind host (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--port", type=int, default=8765, help="HTTP bind port (default: 8765)"
    )

    parser.add_argument("--monitor-diagonal-in", type=float, default=15.6)
    parser.add_argument("--monitor-width-px", type=int, default=1920)
    parser.add_argument("--monitor-height-px", type=int, default=1200)

    return parser.parse_args()


def validate_assets() -> None:
    missing = [name for name in sorted(ASSET_FILES) if not (ASSETS_DIR / name).exists()]
    if missing:
        raise SystemExit(
            f"Missing viewer asset files in {ASSETS_DIR}: {', '.join(missing)}"
        )


def main() -> int:
    args = parse_args()

    if not GOLDEN_DIR.exists():
        raise SystemExit(f"Golden screenshot directory not found: {GOLDEN_DIR}")

    validate_assets()

    boards = load_boards()
    screenshot_groups, board_items = list_screenshots(boards)
    if not screenshot_groups:
        raise SystemExit(f"No PNG screenshots found in: {GOLDEN_DIR}")
    if not board_items:
        raise SystemExit("No board directories found in golden screenshots.")

    defaults = ViewerDefaults(
        monitor_diagonal_in=args.monitor_diagonal_in,
        monitor_width_px=args.monitor_width_px,
        monitor_height_px=args.monitor_height_px,
    )

    handler = type(
        "ConfiguredViewerHandler",
        (ViewerHandler,),
        {
            "defaults": defaults,
            "screenshot_groups": screenshot_groups,
            "boards": board_items,
        },
    )

    server = ThreadingHTTPServer((args.host, args.port), handler)
    url = f"http://{args.host}:{args.port}/"
    print(
        f"Serving {len(screenshot_groups)} screenshot groups across "
        f"{len(board_items)} boards from {GOLDEN_DIR}"
    )
    print(f"Open {url}")
    print("Press Ctrl+C to stop")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
