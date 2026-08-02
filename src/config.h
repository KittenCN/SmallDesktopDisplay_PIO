#ifndef SDD_CONFIG_H
#define SDD_CONFIG_H

// Feature switches (0 = disabled).
#ifndef ANIMATION_CHOICE
#define ANIMATION_CHOICE 3  // 1: astronaut, 2: Hu Tao, 3: Hatsune Miku
#endif
#ifndef WEB_CONFIG_ENABLED
#define WEB_CONFIG_ENABLED 1
#endif
#ifndef DHT_ENABLED
#define DHT_ENABLED 0
#endif

// Compatibility aliases used by the original modules.
#define Animate_Choice ANIMATION_CHOICE
#define WM_EN WEB_CONFIG_ENABLED
#define DHT_EN DHT_ENABLED

#if ANIMATION_CHOICE < 0 || ANIMATION_CHOICE > 3
#error "ANIMATION_CHOICE must be between 0 and 3"
#endif

#define TMS 1000UL
#define SD_FONT_YELLOW 0xD404
#define SD_FONT_WHITE 0xFFFF
#define timeY 82

// Network behavior.
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#define CONFIG_PORTAL_TIMEOUT_SECONDS 180
#define WEATHER_HTTP_TIMEOUT_MS 10000
#define DEFAULT_WEATHER_INTERVAL_MINUTES 10

// TianAPI is disabled when this is empty. Set the current SHA-1 certificate
// fingerprint to authenticate the server; never silently fall back to insecure TLS.
#define TIANAPI_TLS_FINGERPRINT ""

#endif
