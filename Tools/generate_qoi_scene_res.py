#!/usr/bin/env python3
"""Generate headerless QOI scene resources for EmbeddedGUI.

The project QOI decoder expects the QOI opcode stream only: no 14-byte QOI
header and no 8-byte end marker. This script keeps the image metadata in C.
"""

from __future__ import annotations

import argparse
import re
from collections import deque
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageFilter


CLOUD_TARGET_SIZE = (229, 112)
SPECIAL_CLOUDS = {"cloud3", "cloud4", "cloud5"}


@dataclass
class GeneratedImage:
    name: str
    width: int
    height: int
    has_alpha: bool
    stream: bytes


def c_identifier(stem: str) -> str:
    value = re.sub(r"[^0-9A-Za-z_]", "_", stem)
    if not value or value[0].isdigit():
        value = "_" + value
    return value


def qoi_color_hash(px: tuple[int, int, int, int]) -> int:
    r, g, b, a = px
    return (r * 3 + g * 5 + b * 7 + a * 11) & 63


def qoi_encode_headerless_rgba(image: Image.Image) -> bytes:
    rgba = image.convert("RGBA")
    raw = rgba.tobytes()
    out = bytearray()
    index = [(0, 0, 0, 0)] * 64
    prev = (0, 0, 0, 255)
    run = 0
    pixel_count = rgba.width * rgba.height
    last_index = pixel_count - 1

    for i in range(pixel_count):
        pos = i * 4
        px = (raw[pos], raw[pos + 1], raw[pos + 2], raw[pos + 3])
        if px == prev:
            run += 1
            if run == 62 or i == last_index:
                out.append(0xC0 | (run - 1))
                run = 0
            continue

        if run:
            out.append(0xC0 | (run - 1))
            run = 0

        index_pos = qoi_color_hash(px)
        if index[index_pos] == px:
            out.append(index_pos)
        else:
            index[index_pos] = px
            pr, pg, pb, pa = prev
            r, g, b, a = px

            if a == pa:
                dr = r - pr
                dg = g - pg
                db = b - pb
                dr_dg = dr - dg
                db_dg = db - dg

                if -2 <= dr <= 1 and -2 <= dg <= 1 and -2 <= db <= 1:
                    out.append(0x40 | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2))
                elif -32 <= dg <= 31 and -8 <= dr_dg <= 7 and -8 <= db_dg <= 7:
                    out.append(0x80 | (dg + 32))
                    out.append(((dr_dg + 8) << 4) | (db_dg + 8))
                else:
                    out.extend((0xFE, r, g, b))
            else:
                out.extend((0xFF, r, g, b, a))

        prev = px

    return bytes(out)


def largest_component(mask: Image.Image) -> Image.Image:
    width, height = mask.size
    src = mask.load()
    seen = bytearray(width * height)
    best: list[tuple[int, int]] = []

    for y in range(height):
        for x in range(width):
            pos = y * width + x
            if seen[pos] or src[x, y] == 0:
                continue

            seen[pos] = 1
            queue: deque[tuple[int, int]] = deque([(x, y)])
            points: list[tuple[int, int]] = []

            while queue:
                cx, cy = queue.popleft()
                points.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < width and 0 <= ny < height:
                        npos = ny * width + nx
                        if not seen[npos] and src[nx, ny] != 0:
                            seen[npos] = 1
                            queue.append((nx, ny))

            if len(points) > len(best):
                best = points

    dst = Image.new("L", mask.size, 0)
    pix = dst.load()
    for x, y in best:
        pix[x, y] = 255
    return dst


def remove_watermark_regions(mask: Image.Image) -> Image.Image:
    # The source cloud images have small bright watermark blocks near corners.
    # Clearing these bands before connected-component selection keeps them out
    # of the final alpha while leaving the large cloud body untouched.
    width, height = mask.size
    pix = mask.load()
    for y in range(0, int(height * 0.16)):
        for x in range(0, int(width * 0.18)):
            pix[x, y] = 0
    for y in range(int(height * 0.91), height):
        for x in range(int(width * 0.86), width):
            pix[x, y] = 0
    return mask


def fit_to_canvas(rgba: Image.Image, size: tuple[int, int]) -> Image.Image:
    image = rgba.copy()
    image.thumbnail(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.paste(image, ((size[0] - image.width) // 2, size[1] - image.height), image)
    return canvas


def process_cloud(path: Path, preview_dir: Path | None) -> Image.Image:
    source = Image.open(path).convert("RGB")
    work_size = (max(1, source.width // 4), max(1, source.height // 4))
    work = source.resize(work_size, Image.Resampling.LANCZOS)
    lum = work.convert("L")

    mask = lum.point(lambda value: 255 if value >= 82 else 0)
    mask = remove_watermark_regions(mask)
    mask = mask.filter(ImageFilter.MaxFilter(9))
    mask = mask.filter(ImageFilter.MinFilter(5))
    mask = largest_component(mask)
    mask = mask.filter(ImageFilter.MaxFilter(7))
    mask = mask.filter(ImageFilter.GaussianBlur(2.0))

    scale_x = source.width / work_size[0]
    scale_y = source.height / work_size[1]
    bbox = mask.point(lambda value: 255 if value >= 24 else 0).getbbox()
    if bbox is None:
        raise RuntimeError(f"cloud mask is empty: {path}")

    pad_x = int(source.width * 0.012)
    pad_y = int(source.height * 0.012)
    box = (
        max(0, int(bbox[0] * scale_x) - pad_x),
        max(0, int(bbox[1] * scale_y) - pad_y),
        min(source.width, int(bbox[2] * scale_x) + pad_x),
        min(source.height, int(bbox[3] * scale_y) + pad_y),
    )

    full_mask = mask.resize(source.size, Image.Resampling.LANCZOS)
    cropped_rgb = source.crop(box)
    cropped_alpha = full_mask.crop(box).point(lambda value: 0 if value < 18 else min(255, value + 24))
    cropped = cropped_rgb.convert("RGBA")
    cropped.putalpha(cropped_alpha)
    result = fit_to_canvas(cropped, CLOUD_TARGET_SIZE)

    if preview_dir is not None:
        preview_dir.mkdir(parents=True, exist_ok=True)
        result.save(preview_dir / path.name)

    return result


def load_image(path: Path, preview_dir: Path | None) -> Image.Image:
    if path.stem in SPECIAL_CLOUDS:
        return process_cloud(path, preview_dir)
    return Image.open(path).convert("RGBA")


def format_c_array(data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
    return "\n".join(lines)


def generate_header(images: list[GeneratedImage]) -> str:
    externs = "\n".join(f"extern const egui_image_qoi_t qoi_scene_{image.name};" for image in images)
    return f"""#ifndef __QOI_SCENE_RES_H__
#define __QOI_SCENE_RES_H__

#ifdef __cplusplus
extern "C" {{
#endif

#include "image/egui_image_qoi.h"

#if EGUI_CONFIG_FUNCTION_IMAGE_CODEC_QOI

{externs}

#endif /* EGUI_CONFIG_FUNCTION_IMAGE_CODEC_QOI */

#ifdef __cplusplus
}}
#endif

#endif /* __QOI_SCENE_RES_H__ */
"""


def generate_source(images: list[GeneratedImage]) -> str:
    body = [
        '#include "qoi_scene_res.h"',
        "",
        "#if EGUI_CONFIG_FUNCTION_IMAGE_CODEC_QOI",
        "",
        '#include "core/egui_common.h"',
        "",
        "// clang-format off",
        "",
    ]

    for image in images:
        data_name = f"qoi_scene_{image.name}_data"
        info_name = f"qoi_scene_{image.name}_info"
        alpha_type = "EGUI_IMAGE_ALPHA_TYPE_8" if image.has_alpha else "EGUI_IMAGE_ALPHA_TYPE_1"
        body.append(f"static const uint8_t {data_name}[] = {{")
        body.append(format_c_array(image.stream))
        body.append("};")
        body.append("")
        body.append(f"static const egui_image_qoi_info_t {info_name} = {{")
        body.append(f"    .data_buf = {data_name},")
        body.append("    .alpha_buf = NULL,")
        body.append("    .data_type = EGUI_IMAGE_DATA_TYPE_RGB565,")
        body.append(f"    .alpha_type = {alpha_type},")
        body.append("    .res_type = EGUI_RESOURCE_TYPE_INTERNAL,")
        body.append("    .channels = 4,")
        body.append(f"    .width = {image.width},")
        body.append(f"    .height = {image.height},")
        body.append(f"    .data_size = {len(image.stream)}U,")
        body.append(f"    .decompressed_size = {image.width * image.height * 4}U,")
        body.append("};")
        body.append("")
        body.append(f"extern const egui_image_qoi_t qoi_scene_{image.name};")
        body.append(f"EGUI_IMAGE_SUB_DEFINE_CONST(egui_image_qoi_t, qoi_scene_{image.name}, &{info_name});")
        body.append("")

    body.extend(["// clang-format on", "", "#endif /* EGUI_CONFIG_FUNCTION_IMAGE_CODEC_QOI */", ""])
    return "\n".join(body)


def build_resources(input_dir: Path, output_c: Path, output_h: Path, preview_dir: Path | None) -> list[GeneratedImage]:
    pngs = sorted(input_dir.glob("*.png"), key=lambda item: item.name.lower())
    if not pngs:
        raise RuntimeError(f"no PNG files found in {input_dir}")

    images: list[GeneratedImage] = []
    for path in pngs:
        rgba = load_image(path, preview_dir)
        alpha = rgba.getchannel("A")
        has_alpha = alpha.getextrema() != (255, 255)
        stream = qoi_encode_headerless_rgba(rgba)
        images.append(GeneratedImage(c_identifier(path.stem), rgba.width, rgba.height, has_alpha, stream))

    output_h.write_text(generate_header(images), encoding="utf-8", newline="\n")
    output_c.write_text(generate_source(images), encoding="utf-8", newline="\n")
    return images


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", default=r"C:\Users\Administrator\Desktop\ziyuan\tupian\a")
    parser.add_argument("--output-c", default=r"USER\APP\qoi_scene_res.c")
    parser.add_argument("--output-h", default=r"USER\APP\qoi_scene_res.h")
    parser.add_argument("--preview-dir", default=r"Tools\qoi_scene_preview")
    args = parser.parse_args()

    images = build_resources(
        Path(args.input_dir),
        Path(args.output_c),
        Path(args.output_h),
        Path(args.preview_dir) if args.preview_dir else None,
    )
    for image in images:
        print(
            f"{image.name}: {image.width}x{image.height}, "
            f"alpha={'yes' if image.has_alpha else 'no'}, qoi={len(image.stream)} bytes"
        )


if __name__ == "__main__":
    main()
