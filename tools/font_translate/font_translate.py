#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
font_translate.py

工具说明：
- 从 C header（类似 `const uint8_t font_td_20[] = { 0x.., ... };`）提取二进制并分割为字形
- 从 HZK（GB2312）二进制（例如 HZK16）按 txt 中的汉字生成精简 .h 字库（按 GB2312 编码索引）
- 将 txt 中的汉字去重并保存（作为映射/输入）

用法示例：
1) 从 header 提取 raw binary
   python font_translate.py extract-h --input src/font/font_td_20.h --out out.bin
   可选：--glyph-size 32

2) 把 HZK16 二进制中的字符集合（txt）生成 .h（仅包含这些字符的位图）
   python font_translate.py txt2hzk --hzk hzk16.bin --txt chars.txt --out font_small.h --name font_small --width 16 --height 16

3) 把 txt 去重
   python font_translate.py dedupe --txt input.txt --out dedup.txt

注：
- 生成 .h 依赖输入的 HZK 二进制（例如 HZK16），脚本将把 GB2312 编码的字符定位到 HZK 的区位并读取相应字模。
- UTF-8 -> GB2312 的转换使用 Python 内置编码 'gb2312'，若某字符无法编码为 gb2312 会被跳过并在日志中提示。

"""

from __future__ import annotations
import re
import os
import sys
import argparse
import textwrap
from typing import List


def parse_c_header_array(path: str) -> bytes:
    """解析 C header 中的 uint8_t 数组，返回 raw bytes"""
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        s = f.read()
    # 找到花括号中的内容
    m = re.search(r"\{(.*)\}", s, re.S)
    if not m:
        raise ValueError('未能在 header 中找到数组大括号')
    content = m.group(1)
    # 找到所有 0x.. 或 十进制数字
    tokens = re.findall(r"0x[0-9A-Fa-f]{1,2}|\d+", content)
    data = bytearray()
    for t in tokens:
        if t.startswith('0x'):
            data.append(int(t, 16) & 0xFF)
        else:
            data.append(int(t) & 0xFF)
    return bytes(data)


def write_binary(path: str, data: bytes):
    with open(path, 'wb') as f:
        f.write(data)


def split_glyphs(data: bytes, glyph_size: int) -> List[bytes]:
    if glyph_size <= 0:
        raise ValueError('glyph_size 必须大于 0')
    if len(data) % glyph_size != 0:
        print('警告: 数据长度不是 glyph_size 的整数倍，最后一个字形可能不完整', file=sys.stderr)
    glyphs = [data[i:i+glyph_size] for i in range(0, len(data), glyph_size)]
    return glyphs


def save_glyph_files(glyphs: List[bytes], out_dir: str, prefix='g'):
    os.makedirs(out_dir, exist_ok=True)
    for i, g in enumerate(glyphs):
        fname = os.path.join(out_dir, f"{prefix}_{i:04d}.bin")
        with open(fname, 'wb') as f:
            f.write(g)


def dedupe_txt(in_txt: str, out_txt: str):
    with open(in_txt, 'r', encoding='utf-8') as f:
        content = f.read()
    chars = list(content)
    seen = set()
    out = []
    for c in chars:
        if c.strip() == '':
            continue
        if c not in seen:
            seen.add(c)
            out.append(c)
    with open(out_txt, 'w', encoding='utf-8') as f:
        f.write(''.join(out))
    print(f'dedup -> {out_txt}, kept {len(out)} chars')


def gb2312_pos(ch: str):
    """将单个汉字转为 GB2312 区位 (area, index)。若不能编码，抛出 UnicodeEncodeError"""
    b = ch.encode('gb2312')  # 两字节
    if len(b) != 2:
        raise UnicodeEncodeError('gb2312', ch, 0, len(b), 'not 2 bytes')
    a = b[0]
    i = b[1]
    return a, i


def extract_chars_from_header(h_path: str, out_txt: str):
    """从 C header 中提取注释内的字符（优先 C 风格 /* ... */，也处理 // 注释），按出现顺序去重后写入 out_txt"""
    import io
    with open(h_path, 'r', encoding='utf-8', errors='ignore') as f:
        s = f.read()

    # 提取 /* ... */ 注释
    comments = re.findall(r'/\*([^*]*)\*/', s, re.S)
    # 提取 // 注释（按行）
    comments += re.findall(r'//([^\n]*)', s)

    def is_cjk(c):
        o = ord(c)
        return (
            (0x4E00 <= o <= 0x9FFF) or
            (0x3400 <= o <= 0x4DBF) or
            (0x20000 <= o <= 0x2A6DF) or
            (0xF900 <= o <= 0xFAFF)
        )

    seen = set()
    out_chars = []
    for com in comments:
        # 移除常见注释内的标记，如 0x00、数字、括号等
        # 逐字符检查，保留 CJK 与常见 ASCII 字母/数字/标点
        for ch in com:
            if ch.isspace():
                continue
            try:
                o = ord(ch)
            except Exception:
                continue
            keep = False
            if is_cjk(ch):
                keep = True
            elif o < 128 and (ch.isalnum() or ch in "-_.:,;()[]{}?！!。，、·~+<>%#@*=/\\'\""):
                # 包含 ASCII 字母/数字和一些符号
                keep = True
            if keep and ch not in seen:
                seen.add(ch)
                out_chars.append(ch)

    if not out_chars:
        print('No characters found in header comments')
    else:
        with open(out_txt, 'w', encoding='utf-8') as fo:
            fo.write(''.join(out_chars))
        print(f'Wrote {len(out_chars)} chars to {out_txt}')


def build_h_from_hzk(hzk_path: str, txt_path: str, out_h: str, array_name: str = 'font_small', width: int = 16, height: int = 16):
    """从 HZK 二进制（GB2312 顺序）及文本字符生成 .h 字库，仅包含这些字符的字模。
    width, height 指定字形尺寸（例如 HZK16: 16x16 -> bytes_per_char = 32）
    """
    bytes_per_row = (width + 7) // 8
    bytes_per_char = bytes_per_row * height
    print(f'bytes_per_char={bytes_per_char}')

    with open(txt_path, 'r', encoding='utf-8') as f:
        chars = [c for c in f.read() if c.strip() != '']
    # 去重并保持顺序
    seen = set()
    chars_unique = []
    for c in chars:
        if c not in seen:
            seen.add(c)
            chars_unique.append(c)
    print(f'chars to include: {len(chars_unique)}')

    with open(hzk_path, 'rb') as f:
        hzk = f.read()

    glyphs = []
    missing = []
    for ch in chars_unique:
        try:
            a, i = gb2312_pos(ch)
        except Exception as e:
            missing.append((ch, str(e)))
            continue
        areaIndex = (a - 0xA1) * 94 + (i - 0xA1)
        offset = areaIndex * bytes_per_char
        if offset + bytes_per_char > len(hzk):
            missing.append((ch, 'offset out of range'))
            continue
        glyph = hzk[offset:offset+bytes_per_char]
        glyphs.append((ch, glyph))

    print(f'got glyphs: {len(glyphs)}, missing: {len(missing)}')
    for m in missing[:10]:
        print('missing:', m)

    # 写 .h
    with open(out_h, 'w', encoding='utf-8') as f:
        f.write('// Generated by font_translate.py\n')
        f.write('#include <pgmspace.h>\n')
        f.write('const uint8_t %s[] PROGMEM = {\n' % array_name)
        # 写每个字的字节，带注释字符
        for ch, g in glyphs:
            # 注释（字符）
            safe = ch
            # 行格式化
            hexs = ', '.join(f'0x{b:02X}' for b in g)
            # split into multiple lines
            line = '    ' + hexs
            f.write(f'    /* {safe} */\n')
            # wrap long lines
            maxlen = 16
            bs = [f'0x{b:02X}' for b in g]
            for i in range(0, len(bs), maxlen):
                f.write('    ' + ', '.join(bs[i:i+maxlen]))
                if i + maxlen < len(bs):
                    f.write(',\n')
                else:
                    f.write(',\n')
        f.write('};\n')
    print('Wrote', out_h)


def extract_header_to_bin_and_split(h_path: str, out_bin: str, glyph_size: int = None, out_glyph_dir: str = None):
    data = parse_c_header_array(h_path)
    write_binary(out_bin, data)
    print(f'Wrote binary {out_bin}, size={len(data)} bytes')
    if glyph_size:
        glyphs = split_glyphs(data, glyph_size)
        if out_glyph_dir:
            save_glyph_files(glyphs, out_glyph_dir)
            print(f'Wrote {len(glyphs)} glyph files to {out_glyph_dir}')
        else:
            print(f'Found {len(glyphs)} glyphs (size {glyph_size} each)')


def main(argv=None):
    p = argparse.ArgumentParser(description='Font translate helper for Chinese fonts (HZK/headers)')
    sub = p.add_subparsers(dest='cmd')

    a = sub.add_parser('extract-h', help='Extract bytes from C header array to binary')
    a.add_argument('--input', required=True)
    a.add_argument('--out', required=True)
    a.add_argument('--glyph-size', type=int, default=0, help='if set, split binary into glyphs of this size')
    a.add_argument('--out-glyph-dir', default=None)

    b = sub.add_parser('txt2hzk', help='From HZK binary + txt chars generate a .h containing only those glyphs (GB2312)')
    b.add_argument('--hzk', required=True, help='path to HZK binary (e.g. hzk16.bin)')
    b.add_argument('--txt', required=True, help='path to txt file with characters (utf-8)')
    b.add_argument('--out', required=True, help='output .h path')
    b.add_argument('--name', default='font_small', help='array name in generated header')
    b.add_argument('--width', type=int, default=16)
    b.add_argument('--height', type=int, default=16)

    c = sub.add_parser('dedupe', help='dedupe chars in txt')
    c.add_argument('--txt', required=True)
    c.add_argument('--out', required=True)

    d = sub.add_parser('header2chars', help='从 C header 的注释中提取字符并生成 chars.txt')
    d.add_argument('--input', required=True, help='输入 header 文件路径')
    d.add_argument('--out', required=True, help='输出 txt 文件路径')

    args = p.parse_args(argv)
    if args.cmd == 'extract-h':
        gs = args.glyph_size if args.glyph_size > 0 else None
        extract_header_to_bin_and_split(args.input, args.out, gs, args.out_glyph_dir)
    elif args.cmd == 'txt2hzk':
        build_h_from_hzk(args.hzk, args.txt, args.out, array_name=args.name, width=args.width, height=args.height)
    elif args.cmd == 'dedupe':
        dedupe_txt(args.txt, args.out)
    elif args.cmd == 'header2chars':
        extract_chars_from_header(args.input, args.out)
    else:
        p.print_help()


if __name__ == '__main__':
    main()
