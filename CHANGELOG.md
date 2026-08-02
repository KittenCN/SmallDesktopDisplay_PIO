# Changelog

## 1.5.0 - 2026-08-02

### Added

- Device status (`0x00`) and immediate network refresh (`0x08`) serial commands.
- Persistent weather interval and safe one-line forms for every parameterized serial command.
- CRC16/version validation for the legacy-compatible Wi-Fi EEPROM layout.
- Compile-only Unity tests for display/configuration boundary logic and a feature build matrix.
- GitHub Actions coverage for clean Linux builds, feature variants, tests, and repository hygiene.
- Deterministic, current-Pillow animation generator and strict font conversion test suite.

### Fixed

- Declared and pinned every direct PlatformIO dependency and moved the complete TFT setup into project build flags.
- Bounded Wi-Fi, HTTP, configuration portal, and NTP waits; unified periodic network ownership and radio sleep cleanup.
- Removed plaintext Wi-Fi password/API-key logs and insecure HTTP time fallback.
- Replaced hand-written HTTPS response parsing with `HTTPClient`; TianAPI now fails closed unless a TLS fingerprint is configured.
- Validated NTP source, mode, stratum, timestamp range, and request cookie.
- Rejected incomplete weather responses before drawing; fixed 100% humidity, negative temperature bars, three-digit weather codes, and AQI thresholds.
- Made brightness endpoints persistent, validated every Web/serial setting, and repaired immediate city switching.
- Made Web-disabled, DHT-enabled, and animation-disabled builds compile again.
- Fixed animation PROGMEM access, frame-count drift, and two out-of-bounds astronaut frame lengths.
- Fixed Linux case-sensitive `weatherNum.h` include.

### Changed

- Button click now cycles brightness instead of rebooting; long click still resets Wi-Fi.
- Default weather interval is 10 minutes and is restored from EEPROM when valid.
- Removed obsolete test sketches, empty Wi-Fi module, broken Wokwi placeholders, downloaded dependencies, CMake scratch trees, and machine-specific editor files.
- Repository-generated dependencies and build products are ignored rather than committed.
