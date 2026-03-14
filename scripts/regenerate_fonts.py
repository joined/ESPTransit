#!/usr/bin/env python3

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent

FIRA_SANS_REGULAR = "development/fonts/FiraSans-Regular.ttf"
FIRA_SANS_BOLD = "development/fonts/FiraSans-Bold.ttf"
NERD_FONT = "development/fonts/FiraCodeNerdFontMono-Regular.ttf"
FONT_AWESOME = "development/fonts/FontAwesome5-Solid+Brands+Regular.woff"
FONTS_OUTPUT_DIR = "shared/ui/fonts"

LATIN_RANGE = (
    "0x20-0x5F,0x61-0x7E,0xA0,0xA7,0xA9,0xAB,0xBB,0xC4,0xD6,0xDC,0xDF,0xE4,"
    "0xE9,0xF6,0xFC,0x2013-0x2014,0x2018,0x201A,0x201C,0x201E,0x2026,0x2030,"
    "0x20AC"
)
NERD_RANGE = "0xB0,0x27F2,0x27F3,0x25CB,0x25CF,0x2713"
FA_RANGE = (
    "0xF001,0xF008,0xF00B,0xF00C,0xF00D,0xF011,0xF013,0xF015,0xF019,0xF01C,"
    "0xF021,0xF023,0xF026,0xF027,0xF028,0xF03E,0xF048,0xF04B,0xF04C,0xF04D,"
    "0xF051,0xF052,0xF053,0xF054,0xF067,0xF068,0xF06E,0xF070,0xF071,0xF074,"
    "0xF077,0xF078,0xF079,0xF07B,0xF093,0xF095,0xF0C4,0xF0C5,0xF0C7,0xF0E7,"
    "0xF0EA,0xF0F3,0xF11C,0xF124,0xF15B,0xF1EB,0xF240,0xF241,0xF242,0xF243,"
    "0xF244,0xF287,0xF293,0xF2ED,0xF304,0xF55A,0xF7C2,0xF8A2"
)

COMMON_OPTS = [
    "--bpp",
    "4",
    "--format",
    "lvgl",
    "--no-compress",
    "--lv-include",
    "lvgl.h",
]


def run_cmd(cmd: list[str]) -> None:
    subprocess.run(cmd, cwd=ROOT_DIR, check=True)


def ensure_lv_font_conv_installed() -> None:
    if shutil.which("lv_font_conv") is not None:
        return

    print("Error: lv_font_conv is not installed or not in PATH", file=sys.stderr)
    print("", file=sys.stderr)
    print("To install lv_font_conv, run:", file=sys.stderr)
    print("  npm install -g lv_font_conv", file=sys.stderr)
    print("", file=sys.stderr)
    raise SystemExit(1)


def generate_regular_font(size: int) -> None:
    print(f"Generating fira_sans_{size}...")
    run_cmd(
        [
            "lv_font_conv",
            "--size",
            str(size),
            "--font",
            FIRA_SANS_REGULAR,
            "--range",
            LATIN_RANGE,
            "--font",
            NERD_FONT,
            "--range",
            NERD_RANGE,
            "--font",
            FONT_AWESOME,
            "--range",
            FA_RANGE,
            *COMMON_OPTS,
            "-o",
            f"{FONTS_OUTPUT_DIR}/fira_sans_{size}.c",
        ]
    )


def generate_24_bold_font() -> None:
    print("Generating fira_sans_24_bold...")
    run_cmd(
        [
            "lv_font_conv",
            "--size",
            "24",
            "--font",
            FIRA_SANS_BOLD,
            "--range",
            LATIN_RANGE,
            "--font",
            NERD_FONT,
            "--range",
            NERD_RANGE,
            "--font",
            FONT_AWESOME,
            "--range",
            FA_RANGE,
            *COMMON_OPTS,
            "-o",
            f"{FONTS_OUTPUT_DIR}/fira_sans_24_bold.c",
        ]
    )


def generate_20_bold_font() -> None:
    print("Generating fira_sans_20_bold...")
    run_cmd(
        [
            "lv_font_conv",
            "--size",
            "20",
            "--font",
            FIRA_SANS_BOLD,
            "--range",
            LATIN_RANGE,
            "--font",
            NERD_FONT,
            "--range",
            NERD_RANGE,
            "--font",
            FONT_AWESOME,
            "--range",
            FA_RANGE,
            *COMMON_OPTS,
            "-o",
            f"{FONTS_OUTPUT_DIR}/fira_sans_20_bold.c",
        ]
    )


def generate_bold_font(size: int) -> None:
    print(f"Generating fira_sans_{size}_bold...")
    run_cmd(
        [
            "lv_font_conv",
            "--size",
            str(size),
            "--font",
            FIRA_SANS_BOLD,
            "--range",
            LATIN_RANGE,
            "--font",
            NERD_FONT,
            "--range",
            NERD_RANGE,
            "--font",
            FONT_AWESOME,
            "--range",
            FA_RANGE,
            *COMMON_OPTS,
            "-o",
            f"{FONTS_OUTPUT_DIR}/fira_sans_{size}_bold.c",
        ]
    )


def generate_48_bold_font() -> None:
    print("Generating fira_sans_48_bold...")
    run_cmd(
        [
            "lv_font_conv",
            "--size",
            "48",
            "--font",
            FIRA_SANS_BOLD,
            "--range",
            LATIN_RANGE,
            *COMMON_OPTS,
            "-o",
            f"{FONTS_OUTPUT_DIR}/fira_sans_48_bold.c",
        ]
    )


def main() -> int:
    ensure_lv_font_conv_installed()

    print("Regenerating LVGL fonts...")
    print("")

    for size in (14, 16, 18, 20, 22, 24, 28):
        generate_regular_font(size)

    generate_20_bold_font()
    generate_24_bold_font()
    generate_bold_font(28)
    generate_48_bold_font()
    generate_bold_font(56)

    print("")
    print("Font regeneration complete!")
    print(f"Generated fonts in: {FONTS_OUTPUT_DIR}/")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
