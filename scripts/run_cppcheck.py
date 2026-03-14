#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_BOARD = "jc8012p4a1c"


def run_cmd(cmd: list[str], cwd: Path | None = None) -> None:
    subprocess.run(cmd, cwd=str(cwd) if cwd else None, check=True)


def ensure_cppcheck_installed() -> None:
    if shutil.which("cppcheck") is None:
        print("cppcheck is required but not installed", file=sys.stderr)
        raise SystemExit(1)


def ensure_simulator_compile_db(refresh: bool) -> Path:
    compile_db = ROOT_DIR / "simulator" / "build" / "compile_commands.json"
    if refresh or not compile_db.exists():
        run_cmd(
            [
                "cmake",
                "-S",
                str(ROOT_DIR / "simulator"),
                "-B",
                str(ROOT_DIR / "simulator" / "build"),
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            ]
        )
    if not compile_db.exists():
        print(
            f"simulator compile_commands.json not found at {compile_db}",
            file=sys.stderr,
        )
        raise SystemExit(1)
    return compile_db


def get_board_names() -> list[str]:
    with open(ROOT_DIR / "boards.json") as f:
        return list(json.load(f).keys())


def ensure_esp_compile_db(board: str | None, refresh: bool) -> Path:
    # If a specific board was given, use its build dir. Otherwise try all board
    # build dirs in boards.json order and use the first one that already has a
    # compile_commands.json (avoids requiring IDF to be set up just to run cppcheck).
    boards_to_try = [board] if board else get_board_names()

    if not refresh:
        for b in boards_to_try:
            candidate = ROOT_DIR / "esp" / f"build_{b}" / "compile_commands.json"
            if candidate.exists():
                return candidate

    # Regenerate using the target board (specified or default).
    build_board = board or DEFAULT_BOARD
    build_dir = ROOT_DIR / "esp" / f"build_{build_board}"
    compile_db = build_dir / "compile_commands.json"

    run_cmd([str(ROOT_DIR / "scripts" / "idf.sh"), build_board, "reconfigure"])

    if not compile_db.exists():
        print(f"esp compile_commands.json not found at {compile_db}", file=sys.stderr)
        raise SystemExit(1)
    return compile_db


def run_project_cppcheck(
    label: str,
    project_json: Path,
    cache_dir: Path,
    file_filters: list[str],
    esp_build_dir: Path | None = None,
) -> None:
    cache_dir.mkdir(parents=True, exist_ok=True)
    print(f"Running cppcheck for {label}...")

    cmd = [
        "cppcheck",
        f"--project={project_json}",
        f"--cppcheck-build-dir={cache_dir}",
        "--enable=warning,performance,portability",
        "--inline-suppr",
        "--suppress=missingIncludeSystem",
        "--suppress=unusedFunction",
        "--suppress=normalCheckLevelMaxBranches",
        f"--suppress=*:{ROOT_DIR / 'simulator' / 'build' / '_deps'}/*",
        f"--suppress=*:{esp_build_dir / 'esp-idf'}/*"
        if esp_build_dir
        else "--suppress=*:*/build_*/esp-idf/*",
        f"--suppress=*:{ROOT_DIR / 'esp' / 'managed_components'}/*",
        "--suppress=*:*/idf/components/*",
        "--suppress=*:*/esp-idf/components/*",
        "--suppress=*:*/esp/managed_components/*",
        f"--suppress=*:{ROOT_DIR / 'esp' / 'components' / 'bsp_jc8012p4a1c'}/*",
        f"--suppress=*:{ROOT_DIR / 'esp' / 'components' / 'bsp_jc4880p443c'}/*",
        f"--suppress=*:{ROOT_DIR / 'esp' / 'components' / 'esp_lcd_touch_gsl3680'}/*",
        f"--suppress=badBitmaskCheck:{ROOT_DIR / 'shared' / 'app_config_codec.h'}",
        f"--suppress=badBitmaskCheck:{ROOT_DIR / 'shared' / 'http_parse.cpp'}",
        f"--suppress=constParameterPointer:{ROOT_DIR / 'simulator' / 'src' / 'freertos_hooks.c'}",
        f"--suppress=constVariablePointer:{ROOT_DIR / 'shared' / 'ui' / 'screens' / 'station_search.cpp'}",
        "-D__cplusplus=201703L",
        "-DLV_USE_OBJ_NAME=1",
        "-DconfigUSE_16_BIT_TICKS=0",
        "--error-exitcode=1",
        "--quiet",
    ]

    for file_filter in file_filters:
        cmd.append(f"--file-filter={file_filter}")

    run_cmd(cmd)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="scripts/run_cppcheck.py",
        description="Run cppcheck for simulator/ESP/shared code.",
    )
    parser.add_argument(
        "--target",
        choices=("all", "simulator", "esp"),
        default="all",
        help="Which target context to check.",
    )
    parser.add_argument(
        "--board",
        help="Board name to select the esp build dir (e.g. jc4880p443c). Defaults to searching all board build dirs.",
    )
    parser.add_argument(
        "--refresh",
        action="store_true",
        help="Regenerate compile_commands.json before checking.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    ensure_cppcheck_installed()

    if args.target in ("all", "simulator"):
        sim_db = ensure_simulator_compile_db(args.refresh)
        run_project_cppcheck(
            "simulator + shared",
            sim_db,
            ROOT_DIR / "build" / "cppcheck" / "simulator",
            file_filters=[
                str(ROOT_DIR / "shared" / "*"),
                str(ROOT_DIR / "simulator" / "src" / "*"),
            ],
        )

    if args.target in ("all", "esp"):
        esp_db = ensure_esp_compile_db(args.board, args.refresh)
        run_project_cppcheck(
            "esp + shared",
            esp_db,
            ROOT_DIR / "build" / "cppcheck" / "esp",
            file_filters=[
                str(ROOT_DIR / "shared" / "*"),
                str(ROOT_DIR / "esp" / "main" / "*"),
            ],
            esp_build_dir=esp_db.parent,
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
