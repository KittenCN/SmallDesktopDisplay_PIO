# TJpg_Decoder Array-Only

This is the `src/` directory from Bodmer's `TJpg_Decoder` 1.1.0, pinned in
the project so isolated builds remain reproducible. The upstream license is in
`LICENSE.txt`.

SmallDesktopDisplay only calls the `const uint8_t[]` JPEG API. Upstream 1.1.0
unconditionally enables LittleFS/SPIFFS on ESP8266 and also enables SD in
`User_Config.h`. Removing those unused paths saves about 21 KiB in the current
default firmware.
This local variant removes the filesystem/SD members and overloads from the
small Arduino wrapper. The `tjpgd.c`, `tjpgd.h`, and `tjpgdcnf.h` decode core
remains byte-for-byte identical to 1.1.0, while the retained array input,
callback, scale, size, and byte-swap behavior is unchanged.

When updating the upstream decoder, preserve the array-only configuration and
run the complete PlatformIO environment matrix plus the firmware asset tests.
