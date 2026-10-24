#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/** Start Wi-Fi in station mode and bring up ESP-NOW in broadcast mode. */
esp_err_t link_init(void);

/** Broadcast the local button state to every unit in range. */
esp_err_t link_send(bool pressed);

/** True if a valid packet from a peer arrived recently. */
bool link_remote_alive(int64_t now_us);

/** True if a peer is alive and its most recent packet says "pressed". */
bool link_remote_pressed(int64_t now_us);

/** Copy the most recently heard peer MAC. Returns false if none yet. */
bool link_remote_get_mac(uint8_t mac[6]);

/** RSSI of the most recent peer packet, in dBm. */
int8_t link_remote_rssi(void);
