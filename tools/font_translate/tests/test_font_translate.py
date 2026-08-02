from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "font_translate.py"
SPEC = importlib.util.spec_from_file_location("font_translate", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
font_translate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(font_translate)


class HeaderExtractionTests(unittest.TestCase):
    def test_comments_and_other_declarations_do_not_add_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            header = Path(temp_dir) / "font.h"
            header.write_text(
                "const uint8_t font[] PROGMEM = {\n"
                "  0x01, /* version 42 */ 2U, // build 2026\n"
                "  0b11, 010,\n"
                "};\n"
                "const int unrelated = 99;\n",
                encoding="utf-8",
            )
            self.assertEqual(
                font_translate.parse_c_header_array(str(header)),
                bytes([1, 2, 3, 8]),
            )

    def test_multiple_arrays_require_explicit_selection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            header = Path(temp_dir) / "fonts.h"
            header.write_text(
                "const uint8_t first[] = {1};\n"
                "static const unsigned char second[2] = {2, 3};\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "多个字节数组"):
                font_translate.parse_c_header_array(str(header))
            self.assertEqual(
                font_translate.parse_c_header_array(str(header), "second"),
                bytes([2, 3]),
            )

    def test_invalid_or_out_of_range_elements_are_rejected(self) -> None:
        cases = ("{256}", "{-1}", "{1 << 2}", "{1,,2}", "{}")
        with tempfile.TemporaryDirectory() as temp_dir:
            header = Path(temp_dir) / "font.h"
            for initializer in cases:
                with self.subTest(initializer=initializer):
                    header.write_text(
                        f"const uint8_t font[] = {initializer};", encoding="utf-8"
                    )
                    with self.assertRaises(ValueError):
                        font_translate.parse_c_header_array(str(header))

    def test_extract_creates_output_directories_and_splits_exactly(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            header = root / "font.h"
            header.write_text("const byte font[] = {1, 2, 3, 4};", encoding="utf-8")
            output = root / "nested" / "font.bin"
            glyph_dir = root / "nested" / "glyphs"
            font_translate.extract_header_to_bin_and_split(
                str(header), str(output), 2, str(glyph_dir)
            )
            self.assertEqual(output.read_bytes(), bytes([1, 2, 3, 4]))
            self.assertEqual((glyph_dir / "g_0000.bin").read_bytes(), bytes([1, 2]))
            self.assertEqual((glyph_dir / "g_0001.bin").read_bytes(), bytes([3, 4]))

    def test_incomplete_glyph_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "不是 glyph_size"):
            font_translate.split_glyphs(b"abc", 2)


class HzkGenerationTests(unittest.TestCase):
    def _hzk_with_glyph(self, char: str, width: int, height: int) -> tuple[bytes, bytes]:
        glyph_size = ((width + 7) // 8) * height
        area, index = font_translate.gb2312_pos(char)
        area_index = (area - 0xA1) * 94 + (index - 0xA1)
        glyph = bytes(range(1, glyph_size + 1))
        hzk = bytearray((area_index + 1) * glyph_size)
        offset = area_index * glyph_size
        hzk[offset : offset + glyph_size] = glyph
        return bytes(hzk), glyph

    def test_generates_selected_glyph_and_creates_parent(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            width, height = 8, 2
            hzk, glyph = self._hzk_with_glyph("中", width, height)
            hzk_path = root / "hzk.bin"
            txt_path = root / "chars.txt"
            output = root / "generated" / "font.h"
            hzk_path.write_bytes(hzk)
            txt_path.write_text("中中", encoding="utf-8")
            font_translate.build_h_from_hzk(
                str(hzk_path),
                str(txt_path),
                str(output),
                array_name="font_small",
                width=width,
                height=height,
            )
            generated = output.read_text(encoding="utf-8")
            self.assertIn("#pragma once", generated)
            self.assertIn("const uint8_t font_small[] PROGMEM", generated)
            for byte in glyph:
                self.assertIn(f"0x{byte:02X}", generated)

    def test_invalid_dimensions_and_identifier_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            hzk = root / "hzk.bin"
            chars = root / "chars.txt"
            hzk.write_bytes(b"")
            chars.write_text("中", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "必须大于 0"):
                font_translate.build_h_from_hzk(
                    str(hzk), str(chars), str(root / "out.h"), width=0
                )
            with self.assertRaisesRegex(ValueError, "C 标识符"):
                font_translate.build_h_from_hzk(
                    str(hzk),
                    str(chars),
                    str(root / "out.h"),
                    array_name="not-valid",
                )

    def test_missing_character_fails_without_partial_output(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            hzk = root / "hzk.bin"
            chars = root / "chars.txt"
            output = root / "font.h"
            hzk.write_bytes(b"")
            chars.write_text("中🙂", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "无法生成"):
                font_translate.build_h_from_hzk(str(hzk), str(chars), str(output))
            self.assertFalse(output.exists())


class CliAndTextTests(unittest.TestCase):
    def test_dedupe_creates_parent_and_preserves_order(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "chars.txt"
            output = root / "nested" / "dedup.txt"
            source.write_text("你 好你\n世好", encoding="utf-8")
            font_translate.dedupe_txt(str(source), str(output))
            self.assertEqual(output.read_text(encoding="utf-8"), "你好世")

    def test_cli_requires_subcommand(self) -> None:
        result = subprocess.run(
            [sys.executable, str(SCRIPT)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("required", result.stderr)

    def test_header2chars_does_not_extract_comment_delimiters(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            header = root / "font.h"
            output = root / "nested" / "chars.txt"
            header.write_text("// 中文 A1\n/* 天气! */", encoding="utf-8")
            font_translate.extract_chars_from_header(str(header), str(output))
            extracted = output.read_text(encoding="utf-8")
            self.assertEqual(extracted, "中文A1天气!")
            self.assertNotIn("/", extracted)
            self.assertNotIn("*", extracted)


if __name__ == "__main__":
    unittest.main()
