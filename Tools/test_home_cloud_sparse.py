#!/usr/bin/env python3
"""Offline consistency checks for the Home Dynamic sparse cloud cache."""

from __future__ import annotations

import re
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))

from generate_home_cloud_sparse_rows import merge_runs, row_runs  # noqa: E402
from generate_qoi_scene_res import qoi_encode_headerless_rgba  # noqa: E402


def parse_qoi_array(source: str, name: str) -> bytes:
    match = re.search(
        rf"static const uint8_t {re.escape(name)}\[\] = \{{(.*?)\}};",
        source,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"missing resource array {name}")
    return bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", match.group(1)))


def alpha_mix(a: int, b: int) -> int:
    if a == 255:
        return b
    if b == 255:
        return a
    return (a * b + 128) >> 8


def compose_alpha(from_alpha: int, to_alpha: int, blend: int) -> int:
    to_effective = alpha_mix(to_alpha, blend)
    denominator = to_effective * 255 + from_alpha * (255 - to_effective)
    return (denominator + 127) // 255


def validate_chunks(address: int, length: int, transfer_size: int) -> None:
    end = address + length
    while address < end:
        chunk = min(end - address, transfer_size, 1024 - address % 1024)
        assert 0 < chunk <= transfer_size
        assert address // 1024 == (address + chunk - 1) // 1024
        address += chunk
    assert address == end


def main() -> None:
    resource_source = (ROOT / "USER" / "APP" / "home_theme2_cloud_res.c").read_text()
    sparse_bytes = 0
    full_bytes = 0
    shape_runs: dict[str, list[list[list[tuple[int, int]]]]] = {}
    shape_full_bytes: dict[str, int] = {}

    for shape in "abc":
        shape_runs[shape] = []
        for state in "123":
            path = ROOT / "USER" / "Res" / "yun" / "processed" / f"cloud_{shape}_{state}.png"
            image = Image.open(path).convert("RGBA")
            encoded = qoi_encode_headerless_rgba(image)
            actual = parse_qoi_array(resource_source, f"qoi_scene_cloud_{shape}_{state}_data")
            assert actual == encoded, f"QOI mismatch: cloud_{shape}_{state}"

            width, height, rows = row_runs(path)
            alpha = image.getchannel("A")
            for y, runs in enumerate(rows):
                values = alpha.crop((0, y, width, y + 1)).tobytes()
                covered = bytearray(width)
                for x_start, length in runs:
                    covered[x_start : x_start + length] = b"\1" * length
                assert all((value != 0) == bool(covered[x]) for x, value in enumerate(values))
            sparse_bytes += sum(length * 3 for runs in rows for _, length in runs)
            full_bytes += width * height * 3
            shape_runs[shape].append(rows)
            shape_full_bytes[shape] = width * height * 3

    largest_blend_payload = 0
    for shape in "abc":
        largest_shape_payload = 0
        for from_index in range(3):
            for to_index in range(3):
                payload = sum(
                    length * 3
                    for row_values in zip(shape_runs[shape][from_index], shape_runs[shape][to_index])
                    for _, length in merge_runs(row_values)
                )
                largest_shape_payload = max(largest_shape_payload, payload)
        largest_blend_payload += largest_shape_payload

    for transfer_size in (128, 256, 512, 1024):
        for address, length in ((0, 1), (511, 2048), (1023, 2049), (8 * 1024 * 1024 - 2048, 2048)):
            validate_chunks(address, length, transfer_size)

    for blend in (0, 1, 64, 128, 192, 254, 255):
        for from_alpha in (0, 1, 64, 128, 254, 255):
            for to_alpha in (0, 1, 64, 128, 254, 255):
                result = compose_alpha(from_alpha, to_alpha, blend)
                assert 0 <= result <= 255
                if blend == 0:
                    assert result == from_alpha
                if blend == 255:
                    expected = (to_alpha * 255 + from_alpha * (255 - to_alpha) + 127) // 255
                    assert result == expected

    saving = 100.0 * (full_bytes - sparse_bytes) / full_bytes
    full_two_layer_frame = sum(shape_full_bytes.values()) * 2
    blend_saving = 100.0 * (full_two_layer_frame - largest_blend_payload) / full_two_layer_frame
    print(f"9 QOI resources and sparse spans: OK")
    print(f"keyframe payload: {full_bytes} -> {sparse_bytes} bytes ({saving:.1f}% saved)")
    print(
        f"worst preblend frame payload: {full_two_layer_frame} -> "
        f"{largest_blend_payload} bytes ({blend_saving:.1f}% saved)"
    )
    print("128/256/512/1024-byte page-boundary chunk model: OK")
    print("blend alpha cases 0/1/64/128/192/254/255: OK")


if __name__ == "__main__":
    main()
