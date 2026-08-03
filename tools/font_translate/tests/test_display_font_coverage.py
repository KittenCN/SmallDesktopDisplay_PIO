import re
import struct
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]


def read_vlw_glyphs(header: Path):
    source = header.read_text(encoding="utf-8")
    data = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", source))
    if len(data) < 24:
        raise ValueError(f"{header} does not contain a VLW header")
    glyph_count = struct.unpack(">I", data[:4])[0]
    metrics_end = 24 + glyph_count * 28
    if len(data) < metrics_end:
        raise ValueError(f"{header} has a truncated VLW metrics table")

    glyphs = {}
    for index in range(glyph_count):
        record_start = 24 + index * 28
        codepoint, _, _, advance, _, _, _ = struct.unpack(
            ">7I", data[record_start : record_start + 28]
        )
        glyphs[chr(codepoint)] = advance
    ascent, descent = struct.unpack(">II", data[16:24])
    return glyphs, (ascent + descent) // 4


class DisplayFontCoverageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.calendar_glyphs, cls.calendar_space_width = read_vlw_glyphs(
            REPOSITORY_ROOT / "src" / "font" / "font_td_20.h"
        )
        cls.weather_glyphs, _ = read_vlw_glyphs(
            REPOSITORY_ROOT / "src" / "font" / "ZdyLwFont_20.h"
        )

    def assert_calendar_text_supported(self, text):
        missing = sorted({char for char in text if char != " " and char not in self.calendar_glyphs})
        self.assertEqual([], missing, f"missing calendar glyphs: {missing!r}")

    def calendar_width(self, text):
        return sum(
            self.calendar_space_width if char == " " else self.calendar_glyphs[char]
            for char in text
        )

    def test_fixed_calendar_pages_and_statuses_are_supported(self):
        self.assert_calendar_text_supported(
            "NTP WAIT TLS" "公历年月日周农历未开存" "L0123456789-"
        )
        for status in ("NTP WAIT", "农历未开", "TLS未开", "农历未存"):
            self.assertLessEqual(self.calendar_width(status), 150)

    def test_all_dynamic_lunar_characters_are_supported(self):
        heavenly_stems = "甲乙丙丁戊己庚辛壬癸"
        earthly_branches = "子丑寅卯辰巳午未申酉戌亥"
        zodiac = "鼠牛虎兔龙蛇马羊猴鸡狗猪"
        lunar_days = "初一二三四五六七八九十廿"
        solar_terms = (
            "立春雨水惊蛰春分清明谷雨立夏小满芒种夏至小暑大暑"
            "立秋处暑白露秋分寒露霜降立冬小雪大雪冬至"
        )
        self.assert_calendar_text_supported(
            heavenly_stems + earthly_branches + zodiac + lunar_days + solar_terms
        )

    def test_replacement_glyph_exists_in_both_smooth_fonts(self):
        self.assertIn("-", self.calendar_glyphs)
        self.assertIn("-", self.weather_glyphs)


if __name__ == "__main__":
    unittest.main()
