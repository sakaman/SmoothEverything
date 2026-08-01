from __future__ import annotations

import shutil
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
BRANDING = ROOT / "assets" / "branding"
SOURCE = BRANDING / "SmoothEverything.cutout.png"
MASTER = BRANDING / "SmoothEverything.png"
ICO = BRANDING / "SmoothEverything.ico"
ENGINE_ICO = ROOT / "src" / "engine" / "assets" / "AppIcon.ico"
SETTINGS_ICO = ROOT / "src" / "settings" / "Assets" / "AppIcon.ico"
SETTINGS_PNG = ROOT / "src" / "settings" / "Assets" / "AppIcon.png"

ICON_SIZES = (
    (16, 16),
    (20, 20),
    (24, 24),
    (32, 32),
    (40, 40),
    (48, 48),
    (64, 64),
    (96, 96),
    (128, 128),
    (256, 256),
)


def normalize_master(source: Image.Image) -> Image.Image:
    rgba = source.convert("RGBA")
    bounds = rgba.getchannel("A").getbbox()
    if bounds is None:
        raise ValueError("The source image has no visible pixels.")

    mark = rgba.crop(bounds)
    padding = round(max(mark.size) * 0.08)
    side = max(mark.size) + (padding * 2)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    position = ((side - mark.width) // 2, (side - mark.height) // 2)
    canvas.alpha_composite(mark, position)
    return canvas.resize((1024, 1024), Image.Resampling.LANCZOS)


def main() -> None:
    if not SOURCE.exists():
        raise FileNotFoundError(f"Missing transparent source: {SOURCE}")

    with Image.open(SOURCE) as source:
        master = normalize_master(source)

    BRANDING.mkdir(parents=True, exist_ok=True)
    ENGINE_ICO.parent.mkdir(parents=True, exist_ok=True)
    SETTINGS_ICO.parent.mkdir(parents=True, exist_ok=True)

    master.save(MASTER, format="PNG", optimize=True)
    master.save(ICO, format="ICO", sizes=ICON_SIZES)
    master.resize((256, 256), Image.Resampling.LANCZOS).save(
        SETTINGS_PNG,
        format="PNG",
        optimize=True,
    )
    shutil.copy2(ICO, ENGINE_ICO)
    shutil.copy2(ICO, SETTINGS_ICO)

    print(f"Master PNG: {MASTER}")
    print(f"Multi-size ICO: {ICO}")
    print(f"Engine ICO: {ENGINE_ICO}")
    print(f"Control panel ICO: {SETTINGS_ICO}")


if __name__ == "__main__":
    main()
