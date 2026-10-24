#include "button.h"

#include "esp_timer.h"

#include "app_config.h"

static bool s_initialized;
static bool s_stable_pressed;
static bool s_raw_pressed;
static int64_t s_raw_changed_us;
static gpio_num_t s_gpio = GPIO_NUM_NC;

esp_err_t button_init(gpio_num_t gpio)
{
    if (!GPIO_IS_VALID_GPIO(gpio)) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        return err;
    }

    s_gpio = gpio;
    s_raw_pressed = gpio_get_level(gpio) == 0;
    s_stable_pressed = s_raw_pressed;
    s_raw_changed_us = esp_timer_get_time();
    s_initialized = true;
    return ESP_OK;
}

bool button_update(void)
{
    if (!s_initialized) {
        return false;
    }

    const int64_t now_us = esp_timer_get_time();
    const bool raw_pressed = gpio_get_level(s_gpio) == 0;

    if (raw_pressed != s_raw_pressed) {
        s_raw_pressed = raw_pressed;
        s_raw_changed_us = now_us;
    } else if (raw_pressed != s_stable_pressed &&
               (now_us - s_raw_changed_us) >= (APP_BUTTON_DEBOUNCE_MS * 1000LL)) {
        s_stable_pressed = raw_pressed;
    }

    return s_stable_pressed;
}
