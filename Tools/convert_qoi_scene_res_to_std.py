#!/usr/bin/env python3
"""Convert generated headerless QOI scene resources into std RGB565 resources.

The converter reads the existing qoi_scene_res.c/h pair, decodes each
headerless QOI stream, and rewrites the resource set as egui_image_std_t
resources backed by raw RGB565 pixels plus optional 8-bit alpha.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path


@dataclass
class SceneImage:
    name: str
    width: int
    height: int
    pixels: list[int]
    alpha: list[int] | None


def parse_uint8_array(block: str) -> bytes:
    values = re.findall(r"0x([0-9A-Fa-f]{2})", block)
    return bytes(int(value, 16) for value in values)


def qoi_decode_headerless_rgba(stream: bytes, pixel_count: int) -> list[tuple[int, int, int, int]]:
    data_len = len(stream)
    pos = 0
    prev_r = 0
    prev_g = 0
    prev_b = 0
    prev_a = 255
    index = [(0, 0, 0, 0)] * 64
    out: list[tuple[int, int, int, int]] = []
    run = 0

    while len(out) < pixel_count:
        if run > 0:
            out.append((prev_r, prev_g, prev_b, prev_a))
            run -= 1
            continue

        if pos >= data_len:
            raise ValueError("unexpected end of QOI stream")

        b1 = stream[pos]
        pos += 1

        if b1 == 0xFE:
            if pos + 3 > data_len:
                raise ValueError("unexpected end of QOI RGB op")
            prev_r = stream[pos]
            prev_g = stream[pos + 1]
            prev_b = stream[pos + 2]
            pos += 3
        elif b1 == 0xFF:
            if pos + 4 > data_len:
                raise ValueError("unexpected end of QOI RGBA op")
            prev_r = stream[pos]
            prev_g = stream[pos + 1]
            prev_b = stream[pos + 2]
            prev_a = stream[pos + 3]
            pos += 4
        elif b1 < 0x40:
            prev_r, prev_g, prev_b, prev_a = index[b1]
        elif b1 < 0x80:
            prev_r = (prev_r + ((b1 >> 4) & 0x03) - 2) & 0xFF
            prev_g = (prev_g + ((b1 >> 2) & 0x03) - 2) & 0xFF
            prev_b = (prev_b + (b1 & 0x03) - 2) & 0xFF
        elif b1 < 0xC0:
            if pos >= data_len:
                raise ValueError("unexpected end of QOI LUMA op")
            b2 = stream[pos]
            pos += 1
            vg = (b1 & 0x3F) - 32
            prev_r = (prev_r + vg - 8 + ((b2 >> 4) & 0x0F)) & 0xFF
            prev_g = (prev_g + vg) & 0xFF
            prev_b = (prev_b + vg - 8 + (b2 & 0x0F)) & 0xFF
        else:
            run = b1 & 0x3F

        if b1 < 0xC0 or b1 == 0xFE or b1 == 0xFF or b1 < 0x40 or b1 < 0x80 or b1 < 0xC0:
            hash_pos = (prev_r * 3 + prev_g * 5 + prev_b * 7 + prev_a * 11) & 63
            index[hash_pos] = (prev_r, prev_g, prev_b, prev_a)

        out.append((prev_r, prev_g, prev_b, prev_a))

    return out


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def c_array_u16(values: list[int]) -> str:
    lines: list[str] = []
    for offset in range(0, len(values), 12):
        chunk = values[offset : offset + 12]
        lines.append("    " + ", ".join(f"0x{value:04X}" for value in chunk) + ",")
    return "\n".join(lines)


def c_array_u8(values: list[int]) -> str:
    lines: list[str] = []
    for offset in range(0, len(values), 12):
        chunk = values[offset : offset + 12]
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return "\n".join(lines)


def extract_images(source_text: str) -> list[SceneImage]:
    data_pattern = re.compile(
        r"static const uint8_t qoi_scene_(?P<name>[A-Za-z0-9_]+)_data\[\] = \{\s*(?P<data>.*?)\};\s*"
        r"static const egui_image_qoi_info_t qoi_scene_(?P=name)_info = \{\s*(?P<info>.*?)\};",
        re.S,
    )
    images: list[SceneImage] = []

    for match in data_pattern.finditer(source_text):
        name = match.group("name")
        data = parse_uint8_array(match.group("data"))
        info = match.group("info")
        width_match = re.search(r"\.width = (\d+),", info)
        height_match = re.search(r"\.height = (\d+),", info)
        if width_match is None or height_match is None:
            raise ValueError(f"missing width/height for {name}")

        width = int(width_match.group(1))
        height = int(height_match.group(1))
        pixels_rgba = qoi_decode_headerless_rgba(data, width * height)

        pixels = [rgb888_to_rgb565(r, g, b) for r, g, b, _a in pixels_rgba]
        opaque = all(a == 255 for _r, _g, _b, a in pixels_rgba)
        alpha = None if opaque else [a for _r, _g, _b, a in pixels_rgba]
        images.append(SceneImage(name, width, height, pixels, alpha))

    if not images:
        raise ValueError("no scene images found")

    return images


def generate_header(images: list[SceneImage]) -> str:
    externs = "\n".join(f"extern const egui_image_std_t qoi_scene_{image.name};" for image in images)
    return f"""#ifndef __HOME_SCENE_RES_H__
#define __HOME_SCENE_RES_H__

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

#endif /* __HOME_SCENE_RES_H__ */
"""


def generate_source(images: list[SceneImage]) -> str:
    body = [
        '#include "home_scene_res.h"',
        "",
        '#include "core/egui_common.h"',
        "",
        "#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565",
        "",
        "// clang-format off",
        "",
    ]

    for image in images:
        data_name = f"qoi_scene_{image.name}_data"
        info_name = f"qoi_scene_{image.name}_info"
        alpha_name = f"qoi_scene_{image.name}_alpha"

        body.append(f"static const uint16_t {data_name}[] = {{")
        body.append(c_array_u16(image.pixels))
        body.append("};")
        body.append("")

        if image.alpha is not None:
            body.append(f"static const uint8_t {alpha_name}[] = {{")
            body.append(c_array_u8(image.alpha))
            body.append("};")
            body.append("")

        body.append(f"static const egui_image_std_info_t {info_name} = {{")
        body.append(f"    .data_buf = {data_name},")
        if image.alpha is None:
            body.append("    .alpha_buf = NULL,")
            body.append("    .alpha_type = EGUI_IMAGE_ALPHA_TYPE_1,")
        else:
            body.append(f"    .alpha_buf = {alpha_name},")
            body.append("    .alpha_type = EGUI_IMAGE_ALPHA_TYPE_8,")
        body.append("    .data_type = EGUI_IMAGE_DATA_TYPE_RGB565,")
        body.append("    .res_type = EGUI_RESOURCE_TYPE_INTERNAL,")
        body.append(f"    .width = {image.width},")
        body.append(f"    .height = {image.height},")
        body.append("};")
        body.append("")
        body.append(f"extern const egui_image_std_t qoi_scene_{image.name};")
        body.append(f"EGUI_IMAGE_SUB_DEFINE_CONST(egui_image_std_t, qoi_scene_{image.name}, &{info_name});")
        body.append("")

    body.extend(["// clang-format on", "", "#endif /* EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565 */", ""])
    return "\n".join(body)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    input_c = root / "USER" / "APP" / "qoi_scene_res.c"
    output_c = root / "USER" / "APP" / "home_scene_res.c"
    output_h = root / "USER" / "APP" / "home_scene_res.h"

    source_text = input_c.read_text(encoding="utf-8", errors="ignore")
    images = extract_images(source_text)

    output_h.write_text(generate_header(images), encoding="utf-8", newline="\n")
    output_c.write_text(generate_source(images), encoding="utf-8", newline="\n")
    print(f"converted {len(images)} scene images to std RGB565 resources")


if __name__ == "__main__":
    main()
