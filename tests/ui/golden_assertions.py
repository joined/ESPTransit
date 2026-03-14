from __future__ import annotations

import os
from pathlib import Path

from PIL import Image, ImageChops

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCREENSHOTS_DIR = PROJECT_ROOT / ".artifacts" / "ui-smoke-screenshots"
SCREENSHOTS_DIFF_DIR = SCREENSHOTS_DIR / "diff"
GOLDEN_SCREENSHOTS_DIR = Path(__file__).resolve().parent / "golden"


def should_update_golden() -> bool:
    value = os.environ.get("UPDATE_GOLDEN", "0").strip().lower()
    return value in {"1", "true", "yes", "on"}


def assert_screenshot_matches_golden(
    screenshot_file: Path,
    output: str,
    *,
    golden_subdir: str,
) -> None:
    assert screenshot_file.exists(), (
        f"Expected screenshot not found: {screenshot_file}\n{output}"
    )

    screenshot_name = screenshot_file.name
    golden_file = GOLDEN_SCREENSHOTS_DIR / golden_subdir / screenshot_name
    diff_path = SCREENSHOTS_DIFF_DIR / golden_subdir / screenshot_name
    if diff_path.exists():
        diff_path.unlink()

    if should_update_golden():
        golden_file.parent.mkdir(parents=True, exist_ok=True)
        with Image.open(screenshot_file) as actual_png:
            actual = actual_png.convert("RGBA")

        if golden_file.exists():
            with Image.open(golden_file) as golden_png:
                expected = golden_png.convert("RGBA")
                if (
                    actual.size == expected.size
                    and ImageChops.difference(actual, expected).getbbox(
                        alpha_only=False
                    )
                    is None
                ):
                    return

        actual.save(golden_file, format="PNG", optimize=False, compress_level=9)
        return

    assert golden_file.exists(), (
        f"Missing golden screenshot: {golden_file}\n"
        "Run `mise run ui-test-regen` to generate committed screenshot baselines."
    )

    with (
        Image.open(screenshot_file) as actual_png,
        Image.open(golden_file) as golden_png,
    ):
        actual = actual_png.convert("RGBA")
        expected = golden_png.convert("RGBA")

        assert actual.size == expected.size, (
            f"Screenshot size mismatch for {screenshot_name}: "
            f"actual={actual.size}, golden={expected.size}"
        )

        diff = ImageChops.difference(actual, expected)
        diff_bbox = diff.getbbox(alpha_only=False)
        if diff_bbox is None:
            return

        # Keep diff artifacts opaque so they are readable in CI artifact viewers.
        diff_rgb = ImageChops.difference(actual.convert("RGB"), expected.convert("RGB"))
        diff = diff_rgb.convert("RGBA")
        diff.putalpha(255)

        diff_path.parent.mkdir(parents=True, exist_ok=True)
        diff.save(diff_path)

    assert False, (
        f"Screenshot mismatch for {screenshot_name}\n"
        f"actual: {screenshot_file}\n"
        f"golden: {golden_file}\n"
        f"diff: {diff_path}\n"
        "If the visual change is intentional, run `mise run ui-test-regen` and commit the updated golden PNGs."
    )
