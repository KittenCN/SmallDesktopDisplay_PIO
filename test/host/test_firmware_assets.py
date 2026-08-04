import base64
import hashlib
import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


class FirmwareAssetTests(unittest.TestCase):
    def test_all_jpeg_assets_have_expected_frames_and_dimensions(self):
        source_root = REPOSITORY_ROOT / "src"
        paths = sorted((source_root / "weatherNum" / "img" / "tianqi").glob("*.h"))
        paths += [source_root / "img" / "temperature.h", source_root / "img" / "humidity.h"]
        paths += sorted((source_root / "Animate" / "img").glob("*.h"))

        dimensions = {}
        frame_count = 0
        array_pattern = re.compile(
            r"const uint8_t\s+\w+\[\]\s+PROGMEM\s*=\s*\{(.*?)\};",
            re.DOTALL,
        )
        for path in paths:
            source = path.read_text(encoding="utf-8")
            for body in array_pattern.findall(source):
                data = bytes(
                    int(value, 16)
                    for value in re.findall(r"0x([0-9A-Fa-f]{2})", body)
                )
                self.assertTrue(data.startswith(b"\xff\xd8"), path)
                self.assertTrue(data.endswith(b"\xff\xd9"), path)
                size = None
                for index in range(len(data) - 8):
                    if data[index] == 0xFF and data[index + 1] in (0xC0, 0xC1, 0xC2):
                        height = int.from_bytes(data[index + 5 : index + 7], "big")
                        width = int.from_bytes(data[index + 7 : index + 9], "big")
                        size = (width, height)
                        break
                self.assertIsNotNone(size, path)
                dimensions[size] = dimensions.get(size, 0) + 1
                frame_count += 1

        self.assertEqual(71, frame_count)
        self.assertEqual({(60, 60): 23, (24, 24): 2, (70, 70): 46}, dimensions)

    def test_array_only_decoder_core_is_pinned_and_has_no_file_api(self):
        decoder = REPOSITORY_ROOT / "lib" / "TJpg_Decoder_ArrayOnly"
        expected_hashes = {
            "tjpgd.c": "1321092606c3dbf4bb57e9e858c2db39c350c79c0075b13ada8b48c2cd9387bc",
            "tjpgd.h": "5f210a6a04b6f98aefa5fd8bb8cfbb977f154e77daf949ee93d251bc5c13437a",
            "tjpgdcnf.h": "a5f1349225d969f6930f78fde300dde4bb5d2dea027d71664cff3bf9edc7330e",
        }
        for name, expected in expected_hashes.items():
            self.assertEqual(
                expected,
                hashlib.sha256((decoder / "src" / name).read_bytes()).hexdigest(),
            )

        wrapper = "\n".join(
            (decoder / "src" / name).read_text(encoding="utf-8")
            for name in ("TJpg_Decoder.h", "TJpg_Decoder.cpp")
        )
        for unused_api in ("drawFsJpg", "drawSdJpg", "<FS.h>", "<SD.h>"):
            self.assertNotIn(unused_api, wrapper)

    def test_clock_line_font_is_flash_resident_and_pixel_identical(self):
        header = (REPOSITORY_ROOT / "src" / "font" / "timeClockFont.h").read_text(
            encoding="utf-8"
        )
        arrays = []
        pattern = re.compile(
            r"const LineAtom (_(?:small|middle|large)LineFont_[0-9]+)"
            r"\[\] PROGMEM = \{(.*?)\n\};",
            re.DOTALL,
        )
        for match in pattern.finditer(header):
            atoms = [
                tuple(map(int, values))
                for values in re.findall(
                    r"\{([0-9]+),\s*([0-9]+),\s*([0-9]+)\}", match.group(2)
                )
            ]
            arrays.append((match.group(1), atoms))
        self.assertEqual(30, len(arrays))
        self.assertEqual(1333, sum(len(atoms) for _, atoms in arrays))
        canonical = "\n".join(
            name + ":" + ";".join(",".join(map(str, atom)) for atom in atoms)
            for name, atoms in arrays
        ).encode()
        self.assertEqual(
            "e1ab04dfc6f02d7153586291bb9340a60168909ec7683d5641d40b2cee911b33",
            hashlib.sha256(canonical).hexdigest(),
        )

        source = (REPOSITORY_ROOT / "src" / "SmallDesktopDisplay.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("pgm_read_ptr(&largeLineFont[_num])", source)
        self.assertIn("memcpy_P(&atom, &fontOne[i], sizeof(atom))", source)

    def test_bundled_digicert_root_matches_official_sha256(self):
        header = (REPOSITORY_ROOT / "src" / "core" / "TlsTrust.h").read_text(
            encoding="utf-8"
        )
        match = re.search(
            r"-----BEGIN CERTIFICATE-----\s*(.*?)\s*-----END CERTIFICATE-----",
            header,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "bundled TLS root certificate is missing")
        der = base64.b64decode("".join(match.group(1).split()), validate=True)
        self.assertEqual(
            "CB3CCBB76031E5E0138F8DD39A23F9DE47FFC35E43C1144CEA27D46A5AB1CB5F",
            hashlib.sha256(der).hexdigest().upper(),
        )

    def test_root_validation_receives_timelib_utc_time(self):
        source = (REPOSITORY_ROOT / "src" / "SmallDesktopDisplay.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("client.setTrustAnchors(&tianApiTrustAnchor);", source)
        self.assertIn(
            "client.setX509Time(now() - timeZone * SECS_PER_HOUR);", source
        )

    def test_firmware_does_not_require_float_stdio_formats(self):
        source = (REPOSITORY_ROOT / "src" / "SmallDesktopDisplay.cpp").read_text(
            encoding="utf-8"
        )
        format_calls = re.findall(
            r"(?:printf|sprintf|snprintf|scanf|sscanf)\s*\([^;]+;", source
        )
        self.assertTrue(format_calls)
        self.assertFalse(
            any(re.search(r"%[-+ #0-9.*hlL]*[aAeEfFgG]", call) for call in format_calls)
        )

        build_script = (REPOSITORY_ROOT / "extra_script.py").read_text(
            encoding="utf-8"
        )
        self.assertIn('("_printf_float", "_scanf_float")', build_script)


if __name__ == "__main__":
    unittest.main()
