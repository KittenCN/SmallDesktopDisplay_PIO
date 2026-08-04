# Changelog

## 1.5.3 - 2026-08-04

### Changed

- Reduced the calendar VLW font from 245 to the exact 100-glyph runtime manifest while preserving every retained metric, bitmap, footer byte, spacing metric, and replacement glyph. The font payload fell from 69,384 B to 32,900 B.
- Replaced dynamically constructed clock line atoms with byte-identical POD tables in `PROGMEM`; all 1,333 drawing triples are protected by a canonical SHA-256 regression test.
- Pinned an array-only TJpg_Decoder 1.1.0 variant. The Tiny JPEG core is byte-identical to upstream, while unused LittleFS/SPIFFS/SD members, overloads, and dependencies are no longer linked.
- Stopped forcing unused newlib float `printf`/`scanf` implementations into the image; actual references would still be resolved normally.
- Removed dead lunar snapshot strings, duplicate JSON helper implementations, duplicate JPEG setup calls, unused banner state, and repeated serial log-prefix template bodies.

### Added

- Added strict VLW parser limits matching TFT_eSPI, an explicit calendar glyph manifest, exact font/clock asset checks, and validation for all 71 bundled JPEG frames.
- Added compile targets for the astronaut and Hu Tao animation variants and a 925,000 B default `firmware.bin` CI budget.

### Validation

- Default ESP8266 build changed from 983,012 B Flash / 47,624 B RAM to 907,464 B Flash / 41,760 B RAM: 75,548 B less Flash and 5,864 B less static RAM, with EEPROM, display, network, TLS, serial, button, and feature semantics unchanged.

## 1.5.2 - 2026-08-03

### Fixed

- Accepted decimal live temperatures returned by weather.com.cn (for example `33.9`) instead of rejecting the complete weather snapshot and leaving the top, temperature, and humidity areas blank.
- Made live weather text, weather code, city name, wind, and AQI resilient to optional-field variation while retaining strict validation for required numeric data.
- Replaced the disabled-by-default TianAPI TLS path with validation against the bundled DigiCert Global Root G2 and explicitly supplied the synchronized TimeLib UTC time to BearSSL; an explicitly configured leaf fingerprint still overrides the trust anchor.
- Removed the misleading `TLS未开` display state. A configured API key now attempts secure root-validated TLS and preserves the previous snapshot on failure.
- Accepted both padded Gregorian dates and TianAPI's documented non-padded lunar `YYYY-M-D` dates without weakening calendar-day validation.

### Added

- Added native decimal-temperature regression coverage and a SHA-256 integrity test for the bundled official TLS root certificate.
- Added field-specific serial diagnostics for rejected weather responses.
- Added a font-verified `WEATHER WAIT` first-start banner and detailed HTTPS failure diagnostics.

## 1.5.1 - 2026-08-03

### Fixed

- Fixed blank calendar frames after the lunar summary by selecting only non-empty carousel pages; the weather carousel now uses the same bounded state machine.
- Added width-aware horizontal paging for calendar and weather banners so valid long text is no longer clipped by the 150-pixel viewport.
- Rejected partial or incorrectly typed TianAPI results before updating any lunar display field, while keeping `jieqi` optional as documented by the provider.
- Preserved the last complete lunar snapshot across network, JSON, business-code, and date-format failures.
- Reported distinct unconfigured-key, unconfigured-TLS, and temporarily unavailable lunar states using only glyphs present in the compact calendar font.
- Rendered lunar months numerically, with `L` for leap months, avoiding missing 正/腊/闰 glyphs in the compact font.
- Rejected malformed weather values and missing/incorrectly typed required weather fields before drawing; wind remains an optional all-or-nothing page.
- Replaced unsupported dynamic weather glyphs with a visible marker instead of silently dropping them.
- Fetched lunar data during the first connected startup cycle instead of waiting for the next periodic refresh.
- Handled every TFT sprite allocation failure without drawing through a null buffer; banner failures also preserve carousel state.

### Changed

- Replaced rapid TianAPI retry behavior with bounded exponential backoff from 1 to 16 minutes.
- Split periodic NTP, weather, and lunar work into cooperative stages so UI tasks get control between blocking network operations.
- Rebuild calendar `String` pages only when the date, time status, or lunar snapshot changes; replaced the per-refresh weekday `String` array with static text.
- Released the DHT smooth font after each completed sensor frame to reduce retained heap pressure.
- Added maximum-heap-block and heap-fragmentation diagnostics to serial status output.
- Added executable native display-logic tests plus carousel, marquee, weather-range, and VLW glyph-coverage cases.
- Updated the official checkout and Python setup CI actions to their Node 24 releases.

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
