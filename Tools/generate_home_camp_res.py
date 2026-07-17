#!/usr/bin/env python3
"""Generate EmbeddedGUI RGB565 + Alpha8 resources for the Home campsite."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


HOUSE_NAME = "house"
FIRE_NAMES = ("fire1", "fire2", "fire3", "fire4")
HOUSE_TARGET_SIZE = (74, 48)
FIRE_CANVAS_SIZE = (22, 22)


@dataclass(frozen=True)
class CampImage:
    name: str
    width: int
    height: int
    pixels: tuple[int, ...]
    alpha: tuple[int, ...]


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def load_rgba(path: Path, normalize_fire: bool) -> Image.Image:
    image = Image.open(path).convert("RGBA")
    if not normalize_fire:
        return image.resize(HOUSE_TARGET_SIZE, Image.Resampling.LANCZOS)

    if image.width > FIRE_CANVAS_SIZE[0] or image.height > FIRE_CANVAS_SIZE[1]:
        raise ValueError(f"fire frame exceeds {FIRE_CANVAS_SIZE[0]}x{FIRE_CANVAS_SIZE[1]}: {path}")

    canvas = Image.new("RGBA", FIRE_CANVAS_SIZE, (0, 0, 0, 0))
    x = (FIRE_CANVAS_SIZE[0] - image.width) // 2
    y = FIRE_CANVAS_SIZE[1] - image.height
    canvas.alpha_composite(image, (x, y))
    return canvas


def build_image(name: str, path: Path, normalize_fire: bool) -> CampImage:
    image = load_rgba(path, normalize_fire)
    raw = image.tobytes()
    rgba = tuple(tuple(raw[offset : offset + 4]) for offset in range(0, len(raw), 4))
    pixels = tuple(rgb888_to_rgb565(r, g, b) for r, g, b, _a in rgba)
    alpha = tuple(a for _r, _g, _b, a in rgba)
    return CampImage(name, image.width, image.height, pixels, alpha)


def format_u16(values: tuple[int, ...]) -> str:
    lines = []
    for offset in range(0, len(values), 12):
        chunk = values[offset : offset + 12]
        lines.append("    " + ", ".join(f"0x{value:04X}" for value in chunk) + ",")
    return "\n".join(lines)


def format_u8(values: tuple[int, ...]) -> str:
    lines = []
    for offset in range(0, len(values), 12):
        chunk = values[offset : offset + 12]
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return "\n".join(lines)


def generate_header(images: list[CampImage]) -> str:
    externs = "\n".join(f"extern const egui_image_std_t home_camp_{image.name};" for image in images)
    return f"""#ifndef __HOME_CAMP_RES_H__
#define __HOME_CAMP_RES_H__

#ifdef __cplusplus
extern "C" {{
#endif

#include "image/egui_image_std.h"

#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565

{externs}

#endif /* EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565 */

#ifdef __cplusplus
}}
#endif

#endif /* __HOME_CAMP_RES_H__ */
"""


def generate_source(images: list[CampImage]) -> str:
    body = [
        '#include "home_camp_res.h"',
        "",
        '#include "core/egui_common.h"',
        "",
        "#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565",
        "",
        "// clang-format off",
        "",
    ]

    for image in images:
        prefix = f"home_camp_{image.name}"
        body.append(f"static const uint16_t {prefix}_data[] = {{")
        body.append(format_u16(image.pixels))
        body.append("};")
        body.append("")
        body.append(f"static const uint8_t {prefix}_alpha[] = {{")
        body.append(format_u8(image.alpha))
        body.append("};")
        body.append("")
        body.append(f"static const egui_image_std_info_t {prefix}_info = {{")
        body.append(f"    .data_buf = {prefix}_data,")
        body.append(f"    .alpha_buf = {prefix}_alpha,")
        body.append("    .alpha_type = EGUI_IMAGE_ALPHA_TYPE_8,")
        body.append("    .data_type = EGUI_IMAGE_DATA_TYPE_RGB565,")
        body.append("    .res_type = EGUI_RESOURCE_TYPE_INTERNAL,")
        body.append(f"    .width = {image.width},")
        body.append(f"    .height = {image.height},")
        body.append("};")
        body.append("")
        body.append(f"extern const egui_image_std_t {prefix};")
        body.append(f"EGUI_IMAGE_SUB_DEFINE_CONST(egui_image_std_t, {prefix}, &{prefix}_info);")
        body.append("")

    body.extend(["// clang-format on", "", "#endif /* EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565 */", ""])
    return "\n".join(body)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", type=Path, default=root / "USER" / "Res" / "zhangpeng")
    parser.add_argument("--output-c", type=Path, default=root / "USER" / "APP" / "home_camp_res.c")
    parser.add_argument("--output-h", type=Path, default=root / "USER" / "APP" / "home_camp_res.h")
    args = parser.parse_args()

    required = (HOUSE_NAME,) + FIRE_NAMES
    missing = [name for name in required if not (args.input_dir / f"{name}.png").is_file()]
    if missing:
        raise FileNotFoundError(f"missing campsite PNG files: {', '.join(missing)}")

    images = [build_image(HOUSE_NAME, args.input_dir / "house.png", False)]
    images.extend(build_image(name, args.input_dir / f"{name}.png", True) for name in FIRE_NAMES)

    args.output_h.write_text(generate_header(images), encoding="utf-8", newline="\n")
    args.output_c.write_text(generate_source(images), encoding="utf-8", newline="\n")

    for image in images:
        print(f"home_camp_{image.name}: {image.width}x{image.height}, RGB565 + Alpha8")


if __name__ == "__main__":
    main()
