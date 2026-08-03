import base64
import hashlib
import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


class FirmwareAssetTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
