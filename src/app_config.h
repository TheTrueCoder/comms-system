#pragma once

/*
 * Compile-time defaults. Every value can be overridden from platformio.ini
 * with -DAPP_...=... in build_flags.
 */

/* On-board BOOT button (active low). GPIO0 on the ESP32-S3-DevKitC-1. */
#ifndef APP_BUTTON_GPIO
#define APP_BUTTON_GPIO 0
#endif

/* On-board WS2812 data pin. GPIO48 on the initial DevKitC-1, GPIO38 on v1.1.
 * Check the marking on your own board, as clones vary. */
#ifndef APP_RGB_GPIO
#define APP_RGB_GPIO 48
#endif

/* LED brightness in percent (0-100) applied to the configured colour. */
#ifndef APP_LED_BRIGHTNESS
#define APP_LED_BRIGHTNESS 32
#endif

/* Colour used while either unit's button is held. */
#ifndef APP_LED_ACTIVE_R
#define APP_LED_ACTIVE_R 0
#endif
#ifndef APP_LED_ACTIVE_G
#define APP_LED_ACTIVE_G 255
#endif
#ifndef APP_LED_ACTIVE_B
#define APP_LED_ACTIVE_B 0
#endif

/* Both units must use the same Wi-Fi channel and group number. */
#ifndef APP_ESPNOW_CHANNEL
#define APP_ESPNOW_CHANNEL 1
#endif
#ifndef APP_ESPNOW_GROUP
#define APP_ESPNOW_GROUP 1
#endif

/* Main loop / link timing. */
#ifndef APP_LOOP_PERIOD_MS
#define APP_LOOP_PERIOD_MS 10
#endif
#ifndef APP_BUTTON_DEBOUNCE_MS
#define APP_BUTTON_DEBOUNCE_MS 30
#endif
#ifndef APP_HEARTBEAT_MS
#define APP_HEARTBEAT_MS 250
#endif
#ifndef APP_PRESS_REFRESH_MS
#define APP_PRESS_REFRESH_MS 100
#endif
#ifndef APP_LINK_TIMEOUT_MS
#define APP_LINK_TIMEOUT_MS 1000
#endif
