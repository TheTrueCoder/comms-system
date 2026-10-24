# arduino-espnow

Minimal Arduino/PlatformIO ESP-NOW test for two ESP32-S3 boards.

- Same firmware on both units, broadcasts to `FF:FF:FF:FF:FF:FF` (no pairing/routers).
- BOOT button (GPIO0, active low) held on either unit lights the on-board WS2812 (default GPIO48) on both.
- Hold-to-light: LED on while local OR remote button held. Heartbeat/refresh keeps state consistent if packets are lost.
- Uses [FastLED](https://github.com/FastLED/FastLED) to drive the WS2812 with minimal code.

## Hardware
- ESP32-S3-DevKitC-1 (initial rev uses GPIO48; v1.1 often uses GPIO38 — change `LED_PIN` if needed)
- Same Wi-Fi channel on both (default 1)

## Build/Flash
```bash
cd arduino-espnow
pio run -e esp32-s3-devkitc-1
pio run -e esp32-s3-devkitc-1 -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0
```
Flash the same firmware to both boards.

## Config
Edit `platformio.ini` build flags: `LED_PIN`, `BUTTON_PIN`, `ESPNOW_CHANNEL`, `ESPNOW_GROUP`.
