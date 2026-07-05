#!/usr/bin/env python3
"""Extract a de-duplicated charset from poetry source text files."""

from __future__ import annotations

import argparse
from pathlib import Path


DEFAULT_INPUTS = [
    Path(r"C:\Users\Administrator\Desktop\ziyuan\ok\song_300.txt"),
    Path(r"C:\Users\Administrator\Desktop\ziyuan\ok\tang_300.txt"),
    Path(r"C:\Users\Administrator\Desktop\ziyuan\ok\song_3000.txt"),
]

DEFAULT_EXTRA = (
    " "
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    ".,;:!?()[]{}<>/-_+=*&%#@'\"\\|"
    "，。！？；：、（）《》〈〉【】〔〕「」『』“”‘’·…—"
)

SEPARATOR = "###_END_OF_CI_###"


def char_sort_key(ch: str) -> tuple[int, int]:
    cp = ord(ch)
    if cp < 0x80:
        group = 0
    elif 0x3000 <= cp <= 0x303F:
        group = 1
    elif 0xFF00 <= cp <= 0xFFEF:
        group = 2
    elif 0x4E00 <= cp <= 0x9FFF:
        group = 3
    else:
        group = 4
    return group, cp


def collect_chars(paths: list[Path], extra: str) -> list[str]:
    chars: set[str] = set(extra)

    for path in paths:
        text = path.read_text(encoding="utf-8")
        text = text.replace(SEPARATOR, "")
        for ch in text:
            if ch in "\r\n\t":
                continue
            chars.add(ch)

    chars.discard("\ufeff")
    return sorted(chars, key=char_sort_key)


def write_wrapped(chars: list[str], output: Path, columns: int) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="\n") as fp:
        for i in range(0, len(chars), columns):
            fp.write("".join(chars[i:i + columns]))
            fp.write("\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", type=Path, dest="inputs", help="Poetry UTF-8 text file")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "USER" / "Res" / "poetry_charset.txt",
        help="Output charset text file",
    )
    parser.add_argument("--columns", type=int, default=80, help="Characters per line in output")
    args = parser.parse_args()

    inputs = args.inputs if args.inputs else DEFAULT_INPUTS
    chars = collect_chars(inputs, DEFAULT_EXTRA)
    write_wrapped(chars, args.output, args.columns)

    cjk_count = sum(1 for ch in chars if 0x4E00 <= ord(ch) <= 0x9FFF)
    print(f"output      : {args.output}")
    print(f"input_files : {len(inputs)}")
    print(f"chars       : {len(chars)}")
    print(f"cjk_chars   : {cjk_count}")
    print(f"utf8_bytes  : {args.output.stat().st_size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
