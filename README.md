# comms-system

Making a wireless comms system for events/productions hopefully with custom packs.

This directory currently contains a **bench test** for the RF/link layer: two
ESP32-S3 dev boards exchange button state over ESP-NOW and light their on-board
RGB LEDs together.

## What the test does

- Both units run the same firmware and broadcast a small ESP-NOW packet.
- Pressing the **BOOT** button on *either* unit lights the RGB LED on **both**
  units while the button is held.
- Releasing it turns both LEDs off. If both buttons are held, the LEDs stay on
  until both are released.
- The link is completely symmetric: no master/slave role, no MAC addresses to
  configure, no router or Wi-Fi network.
- The boot sequence flashes red, green, then blue on each board, so a wiring or
  LED-type problem is obvious before any RF testing.

## Hardware

Tested configuration:

| Item | Setting |
| --- | --- |
| Board | Espressif ESP32-S3-DevKitC-1 (8 MB flash) |
| Button | On-board `BOOT` button, GPIO0, active low |
| RGB LED | On-board WS2812/SK6812, data on GPIO48 (initial DevKitC-1) |
| ESP-NOW channel | 1 |
| ESP-NOW group | 1 |

Espressif changed the RGB LED pin between board revisions: the initial
ESP32-S3-DevKitC-1 uses **GPIO48**, while **v1.1 uses GPIO38**. The default here
is GPIO48; for a v1.1 board, change `APP_RGB_GPIO` to `38` in `platformio.ini`.
Clones may differ again, so check your board's documentation. An external
WS2812 works too; connect its data input to the configured GPIO, plus 5 V (or
3.3 V for small parts) and ground.

Two boards and two USB cables are enough. Put them a few metres apart for the
first test.

## Build and flash

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/)
if needed, then:

```bash
pio run
```

Flash the **same firmware** to both boards, one at a time. Replace the port with
the one shown by `pio device list`:

```bash
pio run -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0
```

For the second board:

```bash
pio run -t upload --upload-port /dev/ttyUSB1
pio device monitor --port /dev/ttyUSB1
```

Use the board's **UART** USB port for upload and monitor (usually
`/dev/ttyUSB0` on Linux, the port next to the `UART` label). The other connector
is the native USB-Serial/JTAG port. If the monitor is silent, try the other
connector; the default ESP-IDF console for this project is UART0.

## Expected serial output

Unit A (button pressed first):

```text
I (421) link-test: ESP32-S3 ESP-NOW button/LED link test
I (422) link-test: button=GPIO0, WS2812=GPIO48, channel=1, group=1
I (431) espnow_link: ready: mac=7C:DF:A1:xx:xx:xx channel=1 group=1
I (435) link-test: press BOOT on either unit; both RGB LEDs should light
I (912) link-test: peer online: 7C:DF:A1:yy:yy:yy (-38 dBm)
I (2510) link-test: local BOOT pressed
I (2510) link-test: RGB LED ON (local=1 remote=0)
```

Unit B (same press, received over the air):

```text
I (2518) link-test: remote BOOT pressed
I (2518) link-test: RGB LED ON (local=0 remote=1)
```

Releasing the button on A produces `local BOOT released` on A,
`remote BOOT released` on B, and both logs show `RGB LED off (local=0 remote=0)`.
The `(local=…)` / `(remote=…)` suffix makes it easy to see whether the LED
state came from the local button or over the air.

## Configuration

All settings live in `src/app_config.h` and can be overridden with `build_flags`
in `platformio.ini`:

| Define | Default | Meaning |
| --- | --- | --- |
| `APP_BUTTON_GPIO` | `0` | BOOT button GPIO (active low) |
| `APP_RGB_GPIO` | `48` | WS2812 data GPIO (`38` on v1.1 boards) |
| `APP_LED_BRIGHTNESS` | `32` | LED brightness, percent |
| `APP_LED_ACTIVE_R/G/B` | `0/255/0` | Colour while active (green) |
| `APP_ESPNOW_CHANNEL` | `1` | Wi-Fi channel; must match on both units |
| `APP_ESPNOW_GROUP` | `1` | Logical group ID; filters other kits |
| `APP_HEARTBEAT_MS` | `250` | Idle state refresh period |
| `APP_PRESS_REFRESH_MS` | `100` | State refresh period while held |
| `APP_LINK_TIMEOUT_MS` | `1000` | Peer is considered gone after this silence |
| `APP_BUTTON_DEBOUNCE_MS` | `30` | Button debounce time |

For example, to use an ESP32-S3-DevKitC-1 v1.1:

```ini
build_flags =
    -DAPP_RGB_GPIO=38
    ...
```

After changing `sdkconfig.defaults` or the ESP-IDF version, delete the generated
`sdkconfig.esp32-s3-devkitc-1` and run `pio run -t fullclean`.

## How the link works

Each unit sends a 12-byte broadcast frame:

| Field | Size | Description |
| --- | --- | --- |
| magic | 1 | `0xB7`, rejects unrelated traffic |
| version | 1 | protocol version, currently 1 |
| group | 1 | `APP_ESPNOW_GROUP` |
| flags | 1 | bit 0 = local button pressed |
| seq | 4 | increments every transmitted frame |
| uptime_ms | 4 | sender uptime, for diagnostics |

- A frame is sent immediately on every button edge.
- While a button is held it is refreshed every 100 ms, so a lost release frame
  cannot leave the other LED stuck on.
- While idle a heartbeat is sent every 250 ms.
- A peer is considered present for 1 s after its last valid frame. If it
  disappears, its contribution to the LED state is dropped.

The link is **bi-directional broadcast**: every unit transmits and receives the
same packet type, and there is no connection or pairing process. ESP-NOW
broadcast frames are not acknowledged, so the serial log's `peer online/offline`
messages are the delivery indicator.

This is intentionally a test/demo: it is unencrypted, uses broadcast, and does
not authenticate peers. The `group` byte only prevents accidental cross-talk
with another kit on the same channel.

## Bench test checklist

1. Power both boards, open a monitor on each.
2. Confirm each board's LED does the red → green → blue self-test.
3. Wait for `peer online` on both monitors. RSSI should be roughly -30 to
   -70 dBm at short range.
4. Hold **BOOT** on unit A: both LEDs turn green within a few tens of ms;
   `local BOOT pressed` appears on A and `remote BOOT pressed` on B.
5. Release: both LEDs turn off.
6. Repeat from unit B.
7. Hold both buttons: both LEDs stay on while either is held.
8. Walk one unit away until `peer offline` appears; while offline, that unit can
   still light its own LED locally, but cannot control the other one.

## Troubleshooting

**No LED self-test / always dark**
: Check the WS2812 GPIO for your board revision (initial boards use `48`, v1.1
  uses `38`). Confirm the LED
  is a WS2812-style addressable part, not a plain LED or a PWM RGB LED. If the
  self-test shows red and green swapped, the part may use RGB rather than GRB
  order; swap the bytes in `write_grb()` in `src/ws2812.c`.

**LEDs do their self-test but react to nothing**
: Check both boards were flashed with this firmware, use the same
  `APP_ESPNOW_CHANNEL` and `APP_ESPNOW_GROUP`, and are within range. Look for
  `peer online` in the log. `peer offline` means no valid packets are arriving.

**Only one board's LED responds**
: Compare the two monitor logs. If one never prints `peer online`, that board is
  not receiving. Check antenna/board revision, channel, and that the firmware is
  identical. ESP-NOW is 2.4 GHz, so keep clear of USB 3 enclosures and crowded
  Wi-Fi channels.

**Button does nothing**
: Confirm `APP_BUTTON_GPIO` matches the board's BOOT button and that the log
  shows `local BOOT pressed` when you press it. An external momentary switch
  between the GPIO and GND works too.

**Build/download failures**
: This project pins PlatformIO platform `espressif32@8.0.0` and ESP-IDF 5.5.5
  for reproducible builds. Delete `.pio/` and `sdkconfig.esp32-s3-devkitc-1` to
  force a clean configure, then rebuild.
