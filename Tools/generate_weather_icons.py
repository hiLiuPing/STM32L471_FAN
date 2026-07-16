#!/usr/bin/env python3
"""Generate 64x64 RGB565 + Alpha8 weather icon resources."""

from __future__ import annotations

import argparse
import re
from collections import deque
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


ICON_SIZE = 64
RGB565_COUNT = ICON_SIZE * ICON_SIZE
WHITE_THRESHOLD = 245
ALPHA_THRESHOLD = 128


@dataclass
class WeatherIcon:
    name: str
    icon_id: int
    rgb565: list[int]
    alpha8: list[int]


def is_white_background(pixel: tuple[int, int, int, int]) -> bool:
    r, g, b, _a = pixel
    return r >= WHITE_THRESHOLD and g >= WHITE_THRESHOLD and b >= WHITE_THRESHOLD


def remove_edge_white_background(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    pixels = rgba.load()
    seen = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def push_if_background(x: int, y: int) -> None:
        pos = y * width + x
        if seen[pos] == 0 and is_white_background(pixels[x, y]):
            seen[pos] = 1
            queue.append((x, y))

    for x in range(width):
        push_if_background(x, 0)
        push_if_background(x, height - 1)
    for y in range(height):
        push_if_background(0, y)
        push_if_background(width - 1, y)

    while queue:
        x, y = queue.popleft()
        r, g, b, _a = pixels[x, y]
        pixels[x, y] = (r, g, b, 0)
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < width and 0 <= ny < height:
                push_if_background(nx, ny)

    return rgba


def fit_icon_to_canvas(image: Image.Image) -> Image.Image:
    bbox = image.getchannel("A").getbbox()
    if bbox is not None:
        image = image.crop(bbox)

    image.thumbnail((ICON_SIZE, ICON_SIZE), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (0, 0, 0, 0))
    canvas.paste(image, ((ICON_SIZE - image.width) // 2, (ICON_SIZE - image.height) // 2), image)
    return canvas


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def convert_icon(path: Path) -> WeatherIcon:
    stem = path.stem
    if not stem.isdigit():
        raise ValueError(f"weather icon filename must be numeric: {path.name}")

    image = Image.open(path).convert("RGBA")
    image = remove_edge_white_background(image)
    image = fit_icon_to_canvas(image)

    data = image.tobytes()
    rgb565: list[int] = []
    alpha_values: list[int] = []
    for offset in range(0, len(data), 4):
        r = data[offset]
        g = data[offset + 1]
        b = data[offset + 2]
        a = data[offset + 3]
        rgb565.append(rgb888_to_rgb565(r, g, b) if a >= ALPHA_THRESHOLD else 0)
        alpha_values.append(a)

    return WeatherIcon(re.sub(r"[^0-9A-Za-z_]", "_", stem), int(stem), rgb565, alpha_values)


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


def generate_header(icons: list[WeatherIcon]) -> str:
    decls: list[str] = [
        "#ifndef __WEATHER_ICONS_H__",
        "#define __WEATHER_ICONS_H__",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
        "#include <stdint.h>",
        "",
        '#include "image/egui_image_std.h"',
        "",
        f"#define WEATHER_ICON_SIZE        {ICON_SIZE}U",
        f"#define WEATHER_ICON_RGB565_LEN {RGB565_COUNT}U",
        f"#define WEATHER_ICON_ALPHA8_LEN {RGB565_COUNT}U",
        "",
        "#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565",
        "",
    ]

    for icon in icons:
        decls.append(f"extern const uint16_t U16_IMG_{icon.name}_RGB565[WEATHER_ICON_RGB565_LEN];")
        decls.append(f"extern const uint8_t U8_IMG_{icon.name}_ALPHA[WEATHER_ICON_ALPHA8_LEN];")
        decls.append(f"extern const egui_image_std_t weather_icon_{icon.name};")
        decls.append("")

    decls.extend(
        [
            "const egui_image_std_t *ui_weather_icon_get(uint16_t icon_id);",
            "",
            "#endif /* EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565 */",
            "",
            "#ifdef __cplusplus",
            "}",
            "#endif",
            "",
            "#endif /* __WEATHER_ICONS_H__ */",
            "",
        ]
    )
    return "\n".join(decls)


def generate_source(icons: list[WeatherIcon]) -> str:
    body: list[str] = [
        '#include "icons.h"',
        "",
        '#include "core/egui_common.h"',
        "",
        "#if EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565",
        "",
        "// clang-format off",
        "",
    ]

    for icon in icons:
        body.append(f"const uint16_t U16_IMG_{icon.name}_RGB565[WEATHER_ICON_RGB565_LEN] = {{")
        body.append(c_array_u16(icon.rgb565))
        body.append("};")
        body.append("")
        body.append(f"const uint8_t U8_IMG_{icon.name}_ALPHA[WEATHER_ICON_ALPHA8_LEN] = {{")
        body.append(c_array_u8(icon.alpha8))
        body.append("};")
        body.append("")
        body.append(f"static const egui_image_std_info_t weather_icon_{icon.name}_info = {{")
        body.append(f"    .data_buf = U16_IMG_{icon.name}_RGB565,")
        body.append(f"    .alpha_buf = U8_IMG_{icon.name}_ALPHA,")
        body.append("    .alpha_type = EGUI_IMAGE_ALPHA_TYPE_8,")
        body.append("    .data_type = EGUI_IMAGE_DATA_TYPE_RGB565,")
        body.append("    .res_type = EGUI_RESOURCE_TYPE_INTERNAL,")
        body.append(f"    .width = {ICON_SIZE},")
        body.append(f"    .height = {ICON_SIZE},")
        body.append("};")
        body.append("")
        body.append(f"extern const egui_image_std_t weather_icon_{icon.name};")
        body.append(f"EGUI_IMAGE_SUB_DEFINE_CONST(egui_image_std_t, weather_icon_{icon.name}, &weather_icon_{icon.name}_info);")
        body.append("")

    body.extend(
        [
            "typedef struct",
            "{",
            "    uint16_t icon_id;",
            "    const egui_image_std_t *image;",
            "} weather_icon_map_t;",
            "",
            "static const weather_icon_map_t s_weather_icons[] = {",
        ]
    )
    for icon in icons:
        body.append(f"    {{ {icon.icon_id}U, &weather_icon_{icon.name} }},")
    body.extend(
        [
            "};",
            "",
            "const egui_image_std_t *ui_weather_icon_get(uint16_t icon_id)",
            "{",
            "    /* Prefer a dedicated resource before applying shared-icon ranges. */",
            "    for (uint32_t i = 0U; i < (sizeof(s_weather_icons) / sizeof(s_weather_icons[0])); i++)",
            "    {",
            "        if (s_weather_icons[i].icon_id == icon_id)",
            "        {",
            "            return s_weather_icons[i].image;",
            "        }",
            "    }",
            "",
            "    if ((icon_id >= 101U) && (icon_id <= 102U))",
            "    {",
            "        return &weather_icon_102;",
            "    }",
            "    if ((icon_id >= 103U) && (icon_id <= 104U))",
            "    {",
            "        return &weather_icon_104;",
            "    }",
            "    if ((icon_id >= 302U) && (icon_id <= 304U))",
            "    {",
            "        return &weather_icon_302;",
            "    }",
            "    if ((icon_id >= 307U) && (icon_id <= 399U))",
            "    {",
            "        return &weather_icon_307;",
            "    }",
            "    if ((icon_id >= 400U) && (icon_id <= 499U))",
            "    {",
            "        return &weather_icon_499;",
            "    }",
            "    if ((icon_id >= 500U) && (icon_id <= 515U))",
            "    {",
            "        return &weather_icon_509;",
            "    }",
            "",
            "    return &weather_icon_100;",
            "}",
            "",
            "// clang-format on",
            "",
            "#endif /* EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565 */",
            "",
        ]
    )
    return "\n".join(body)


def build_resources(input_dir: Path, output_c: Path, output_h: Path) -> list[WeatherIcon]:
    pngs = sorted(input_dir.glob("*.png"), key=lambda item: int(item.stem) if item.stem.isdigit() else item.name)
    if not pngs:
        raise RuntimeError(f"no PNG files found in {input_dir}")

    icons = [convert_icon(path) for path in pngs]
    output_h.write_text(generate_header(icons), encoding="utf-8", newline="\n")
    output_c.write_text(generate_source(icons), encoding="utf-8", newline="\n")
    return icons


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", default=r"USER\Res")
    parser.add_argument("--output-c", default=r"USER\APP\icons.c")
    parser.add_argument("--output-h", default=r"USER\APP\icons.h")
    args = parser.parse_args()

    icons = build_resources(Path(args.input_dir), Path(args.output_c), Path(args.output_h))
    for icon in icons:
        alpha_pixels = sum(1 for value in icon.alpha8 if value >= ALPHA_THRESHOLD)
        print(f"{icon.icon_id}: {ICON_SIZE}x{ICON_SIZE}, rgb565={len(icon.rgb565)}, alpha8={len(icon.alpha8)}, opaque_pixels={alpha_pixels}")


if __name__ == "__main__":
    main()
