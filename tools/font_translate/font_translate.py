#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Utilities for extracting and generating compact bitmap fonts."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import List, Sequence


_C_COMMENT_RE = re.compile(r"//[^\r\n]*|/\*.*?\*/", re.S)
_BYTE_ARRAY_RE = re.compile(
    r"(?:\b(?:static|const|volatile)\b\s+)*"
    r"(?:uint8_t|unsigned\s+char|byte)\s+"
    r"(?P<name>[A-Za-z_]\w*)\s*"
    r"\[[^\]]*\]\s*(?:PROGMEM\s*)?=\s*\{"
)
_INTEGER_LITERAL_RE = re.compile(
    r"(?P<value>0[xX][0-9A-Fa-f]+|0[bB][01]+|0[0-7]+|[0-9]+)"
    r"(?P<suffix>[uUlL]*)"
)
_C_IDENTIFIER_RE = re.compile(r"[A-Za-z_]\w*\Z")


def _without_c_comments(source: str) -> str:
    """Remove C/C++ comments while preserving line breaks for diagnostics."""

    def replace(match: re.Match[str]) -> str:
        return "\n" * match.group(0).count("\n")

    return _C_COMMENT_RE.sub(replace, source)


def _find_matching_brace(source: str, opening: int) -> int:
    depth = 0
    for index in range(opening, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("数组初始化器缺少右大括号")


def _byte_arrays(source: str) -> list[tuple[str, str]]:
    clean_source = _without_c_comments(source)
    arrays: list[tuple[str, str]] = []
    for match in _BYTE_ARRAY_RE.finditer(clean_source):
        opening = match.end() - 1
        closing = _find_matching_brace(clean_source, opening)
        arrays.append((match.group("name"), clean_source[opening + 1 : closing]))
    return arrays


def _parse_byte_initializer(initializer: str, array_name: str) -> bytes:
    values = bytearray()
    elements = initializer.split(",")
    for index, raw_element in enumerate(elements, start=1):
        element = raw_element.strip()
        if not element:
            if index == len(elements):  # A trailing comma is valid C/C++.
                continue
            raise ValueError(f"数组 {array_name} 的第 {index} 个元素为空")
        literal = _INTEGER_LITERAL_RE.fullmatch(element)
        if literal is None:
            raise ValueError(
                f"数组 {array_name} 的第 {index} 个元素不是整数常量: {element!r}"
            )
        token = literal.group("value")
        if token.lower().startswith("0x"):
            base = 16
        elif token.lower().startswith("0b"):
            base = 2
        elif len(token) > 1 and token.startswith("0"):
            base = 8
        else:
            base = 10
        value = int(token, base)
        if not 0 <= value <= 0xFF:
            raise ValueError(
                f"数组 {array_name} 的第 {index} 个元素超出 uint8_t 范围: {value}"
            )
        values.append(value)
    if not values:
        raise ValueError(f"数组 {array_name} 没有字节数据")
    return bytes(values)


def parse_c_header_array(path: str, array_name: str | None = None) -> bytes:
    """Extract one uint8_t/byte array initializer from a C/C++ header.

    Comments are discarded before parsing, so numbers in comments cannot become
    output bytes. If the header contains multiple byte arrays, ``array_name`` is
    required to avoid silently extracting the wrong one.
    """

    source = Path(path).read_text(encoding="utf-8", errors="ignore")
    arrays = _byte_arrays(source)
    if not arrays:
        raise ValueError("未在 header 中找到 uint8_t/unsigned char/byte 数组")

    if array_name is None:
        if len(arrays) != 1:
            names = ", ".join(name for name, _ in arrays)
            raise ValueError(f"header 中有多个字节数组，请用 --array 指定: {names}")
        selected_name, initializer = arrays[0]
    else:
        matches = [(name, body) for name, body in arrays if name == array_name]
        if not matches:
            names = ", ".join(name for name, _ in arrays)
            raise ValueError(f"找不到数组 {array_name!r}；可选数组: {names}")
        if len(matches) > 1:
            raise ValueError(f"数组名 {array_name!r} 在 header 中重复")
        selected_name, initializer = matches[0]

    return _parse_byte_initializer(initializer, selected_name)


def _ensure_parent(path: str | Path) -> Path:
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    return output


def write_binary(path: str, data: bytes) -> None:
    _ensure_parent(path).write_bytes(data)


def split_glyphs(data: bytes, glyph_size: int) -> List[bytes]:
    if glyph_size <= 0:
        raise ValueError("glyph_size 必须大于 0")
    if len(data) % glyph_size != 0:
        raise ValueError(
            f"数据长度 {len(data)} 不是 glyph_size {glyph_size} 的整数倍"
        )
    return [data[i : i + glyph_size] for i in range(0, len(data), glyph_size)]


def save_glyph_files(
    glyphs: Sequence[bytes], out_dir: str, prefix: str = "g"
) -> None:
    output_dir = Path(out_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    for index, glyph in enumerate(glyphs):
        (output_dir / f"{prefix}_{index:04d}.bin").write_bytes(glyph)


def dedupe_txt(in_txt: str, out_txt: str) -> None:
    content = Path(in_txt).read_text(encoding="utf-8")
    seen: set[str] = set()
    unique_chars: list[str] = []
    for char in content:
        if char.isspace() or char in seen:
            continue
        seen.add(char)
        unique_chars.append(char)
    _ensure_parent(out_txt).write_text("".join(unique_chars), encoding="utf-8")
    print(f"dedup -> {out_txt}, kept {len(unique_chars)} chars")


def gb2312_pos(char: str) -> tuple[int, int]:
    """Return the two GB2312 bytes for one character."""

    encoded = char.encode("gb2312")
    if len(encoded) != 2:
        raise ValueError(f"字符 {char!r} 不是双字节 GB2312 字符")
    area, index = encoded
    if not (0xA1 <= area <= 0xFE and 0xA1 <= index <= 0xFE):
        raise ValueError(f"字符 {char!r} 不在 GB2312 HZK 区位范围内")
    return area, index


def _is_cjk(char: str) -> bool:
    codepoint = ord(char)
    return (
        0x4E00 <= codepoint <= 0x9FFF
        or 0x3400 <= codepoint <= 0x4DBF
        or 0x20000 <= codepoint <= 0x2A6DF
        or 0xF900 <= codepoint <= 0xFAFF
    )


def extract_chars_from_header(h_path: str, out_txt: str) -> None:
    """Extract unique display characters from C/C++ comments."""

    source = Path(h_path).read_text(encoding="utf-8", errors="ignore")
    comments = []
    for match in _C_COMMENT_RE.finditer(source):
        raw_comment = match.group(0)
        comments.append(
            raw_comment[2:] if raw_comment.startswith("//") else raw_comment[2:-2]
        )
    allowed_ascii_punctuation = "-_.:,;()[]{}?！!。，、·~+<>%#@*=/\\'\""
    seen: set[str] = set()
    output_chars: list[str] = []
    for comment in comments:
        for char in comment:
            keep = _is_cjk(char) or (
                ord(char) < 128
                and (char.isalnum() or char in allowed_ascii_punctuation)
            )
            if keep and char not in seen:
                seen.add(char)
                output_chars.append(char)

    if not output_chars:
        raise ValueError("header 注释中没有可提取的字符")
    _ensure_parent(out_txt).write_text("".join(output_chars), encoding="utf-8")
    print(f"Wrote {len(output_chars)} chars to {out_txt}")


def _validate_dimensions(width: int, height: int) -> int:
    if width <= 0 or height <= 0:
        raise ValueError("width 和 height 必须大于 0")
    return ((width + 7) // 8) * height


def build_h_from_hzk(
    hzk_path: str,
    txt_path: str,
    out_h: str,
    array_name: str = "font_small",
    width: int = 16,
    height: int = 16,
    allow_missing: bool = False,
) -> None:
    """Generate a compact header from a GB2312-ordered HZK font."""

    if _C_IDENTIFIER_RE.fullmatch(array_name) is None:
        raise ValueError(f"name 不是有效的 C 标识符: {array_name!r}")
    bytes_per_char = _validate_dimensions(width, height)
    print(f"bytes_per_char={bytes_per_char}")

    source_chars = Path(txt_path).read_text(encoding="utf-8")
    chars_unique = list(dict.fromkeys(char for char in source_chars if not char.isspace()))
    if not chars_unique:
        raise ValueError("txt 中没有可生成的字符")
    print(f"chars to include: {len(chars_unique)}")

    hzk = Path(hzk_path).read_bytes()
    if len(hzk) % bytes_per_char != 0:
        raise ValueError(
            f"HZK 文件大小 {len(hzk)} 不是单字形大小 {bytes_per_char} 的整数倍"
        )
    glyphs: list[tuple[str, bytes]] = []
    missing: list[tuple[str, str]] = []
    for char in chars_unique:
        try:
            area, index = gb2312_pos(char)
            area_index = (area - 0xA1) * 94 + (index - 0xA1)
            offset = area_index * bytes_per_char
            if offset + bytes_per_char > len(hzk):
                raise ValueError(
                    f"offset {offset} 超出 HZK 文件大小 {len(hzk)}"
                )
            glyphs.append((char, hzk[offset : offset + bytes_per_char]))
        except (UnicodeEncodeError, ValueError) as error:
            missing.append((char, str(error)))

    print(f"got glyphs: {len(glyphs)}, missing: {len(missing)}")
    for item in missing[:10]:
        print("missing:", item, file=sys.stderr)
    if missing and not allow_missing:
        missing_chars = "".join(char for char, _ in missing)
        raise ValueError(
            f"{len(missing)} 个字符无法生成: {missing_chars!r}；"
            "确认 HZK 文件/尺寸，或显式使用 --allow-missing"
        )
    if not glyphs:
        raise ValueError("没有成功生成任何字形")

    output = _ensure_parent(out_h)
    with output.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("// Generated by font_translate.py\n")
        stream.write("#pragma once\n")
        stream.write("#include <stdint.h>\n")
        stream.write("#include <pgmspace.h>\n")
        stream.write(f"const uint8_t {array_name}[] PROGMEM = {{\n")
        for char, glyph in glyphs:
            stream.write(f"    /* {char} */\n")
            byte_tokens = [f"0x{byte:02X}" for byte in glyph]
            for index in range(0, len(byte_tokens), 16):
                stream.write("    " + ", ".join(byte_tokens[index : index + 16]) + ",\n")
        stream.write("};\n")
    print("Wrote", out_h)


def extract_header_to_bin_and_split(
    h_path: str,
    out_bin: str,
    glyph_size: int | None = None,
    out_glyph_dir: str | None = None,
    array_name: str | None = None,
) -> None:
    data = parse_c_header_array(h_path, array_name=array_name)
    write_binary(out_bin, data)
    print(f"Wrote binary {out_bin}, size={len(data)} bytes")
    if glyph_size is not None:
        glyphs = split_glyphs(data, glyph_size)
        if out_glyph_dir:
            save_glyph_files(glyphs, out_glyph_dir)
            print(f"Wrote {len(glyphs)} glyph files to {out_glyph_dir}")
        else:
            print(f"Found {len(glyphs)} glyphs (size {glyph_size} each)")


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Font translate helper for Chinese fonts (HZK/headers)"
    )
    subcommands = parser.add_subparsers(dest="cmd", required=True)

    extract = subcommands.add_parser(
        "extract-h", help="Extract one byte array from a C header"
    )
    extract.add_argument("--input", required=True)
    extract.add_argument("--out", required=True)
    extract.add_argument(
        "--array", help="array name; required when the header has multiple byte arrays"
    )
    extract.add_argument(
        "--glyph-size", type=int, help="validate and split output into fixed-size glyphs"
    )
    extract.add_argument("--out-glyph-dir")

    txt2hzk = subcommands.add_parser(
        "txt2hzk", help="Generate a compact header from a GB2312 HZK font"
    )
    txt2hzk.add_argument("--hzk", required=True, help="path to HZK binary")
    txt2hzk.add_argument("--txt", required=True, help="UTF-8 character list")
    txt2hzk.add_argument("--out", required=True, help="output header")
    txt2hzk.add_argument("--name", default="font_small", help="C array name")
    txt2hzk.add_argument("--width", type=int, default=16)
    txt2hzk.add_argument("--height", type=int, default=16)
    txt2hzk.add_argument(
        "--allow-missing",
        action="store_true",
        help="generate available glyphs even if some requested characters are missing",
    )

    dedupe = subcommands.add_parser("dedupe", help="Dedupe non-whitespace characters")
    dedupe.add_argument("--txt", required=True)
    dedupe.add_argument("--out", required=True)

    header2chars = subcommands.add_parser(
        "header2chars", help="Extract unique display characters from header comments"
    )
    header2chars.add_argument("--input", required=True)
    header2chars.add_argument("--out", required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    try:
        if args.cmd == "extract-h":
            if args.out_glyph_dir and args.glyph_size is None:
                raise ValueError("使用 --out-glyph-dir 时必须同时指定 --glyph-size")
            extract_header_to_bin_and_split(
                args.input,
                args.out,
                glyph_size=args.glyph_size,
                out_glyph_dir=args.out_glyph_dir,
                array_name=args.array,
            )
        elif args.cmd == "txt2hzk":
            build_h_from_hzk(
                args.hzk,
                args.txt,
                args.out,
                array_name=args.name,
                width=args.width,
                height=args.height,
                allow_missing=args.allow_missing,
            )
        elif args.cmd == "dedupe":
            dedupe_txt(args.txt, args.out)
        elif args.cmd == "header2chars":
            extract_chars_from_header(args.input, args.out)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
