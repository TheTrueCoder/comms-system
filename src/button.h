#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

/**
 * Configure a GPIO as an active-low, pulled-up button input.
 *
 * No interrupt or task is used. Call button_update() regularly (every few
 * milliseconds) to sample and debounce it.
 */
esp_err_t button_init(gpio_num_t gpio);

/**
 * Sample the button, apply debouncing, and return the stable state.
 *
 * @return true while the button is held down.
 */
bool button_update(void);
