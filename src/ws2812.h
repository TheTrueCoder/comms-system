#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

/**
 * Initialise an RMT channel for a single WS2812/SK6812 style RGB LED.
 * The LED is left off.
 */
esp_err_t led_init(gpio_num_t gpio);

/** Set the LED colour (0-255 per channel). Global brightness is applied. */
esp_err_t led_set_rgb(uint8_t red, uint8_t green, uint8_t blue);

/** Turn the LED on with the configured "active" colour, or off. */
esp_err_t led_set_active(bool active);

/** Briefly flash red, green and blue to prove the LED is wired correctly. */
void led_self_test(void);
