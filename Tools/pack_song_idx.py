#!/usr/bin/env python3
"""Pack Song Ci text into a single STM32-friendly binary index file."""

from __future__ import annotations

import argparse
import binascii
import struct
from pathlib import Path


MAGIC = b"SNGI"
VERSION = 1
HEADER_SIZE = 32
ENTRY_SIZE = 8
FLAG_UTF8 = 1 << 0
FLAG_LF_LINE_ENDING = 1 << 1
SEPARATOR = "###_END_OF_CI_###"


def parse_poems(text: str) -> list[str]:
    poems: list[str] = []

    for part in text.split(SEPARATOR):
        poem = part.strip(" \t\r\n")
        if not poem:
            continue

        poem = poem.replace("\r\n", "\n").replace("\r", "\n")
        poems.append(poem)

    return poems


def build_idx(poems: list[str]) -> tuple[bytes, dict[str, int]]:
    payload = bytearray()
    entries = bytearray()
    max_poem_size = 0

    for poem in poems:
        data = poem.encode("utf-8")
        offset = len(payload)
        length = len(data)
        entries += struct.pack("<II", offset, length)
        payload += data
        max_poem_size = max(max_poem_size, length)

    data_offset = HEADER_SIZE + len(entries)
    crc32 = binascii.crc32(payload) & 0xFFFFFFFF
    flags = FLAG_UTF8 | FLAG_LF_LINE_ENDING

    header = struct.pack(
        "<4sHHIIIIII",
        MAGIC,
        VERSION,
        HEADER_SIZE,
        len(poems),
        ENTRY_SIZE,
        data_offset,
        len(payload),
        crc32,
        flags,
    )

    blob = header + entries + payload
    stats = {
        "poem_count": len(poems),
        "data_offset": data_offset,
        "data_size": len(payload),
        "max_poem_size": max_poem_size,
        "crc32": crc32,
        "file_size": len(blob),
    }
    return blob, stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(r"C:\Users\Administrator\Desktop\ziyuan\ok\song_300.txt"),
        help="UTF-8 source text separated by ###_END_OF_CI_###",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "USER" / "Res" / "song_300.idx",
        help="Output .idx file",
    )
    args = parser.parse_args()

    text = args.input.read_text(encoding="utf-8")
    poems = parse_poems(text)
    if not poems:
        raise SystemExit("no poems found")

    blob, stats = build_idx(poems)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(blob)

    print(f"input          : {args.input}")
    print(f"output         : {args.output}")
    print(f"poem_count     : {stats['poem_count']}")
    print(f"data_offset    : {stats['data_offset']}")
    print(f"data_size      : {stats['data_size']}")
    print(f"max_poem_size  : {stats['max_poem_size']}")
    print(f"crc32          : 0x{stats['crc32']:08X}")
    print(f"file_size      : {stats['file_size']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
