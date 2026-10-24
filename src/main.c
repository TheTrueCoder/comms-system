#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "button.h"
#include "espnow_link.h"
#include "ws2812.h"

static const char *TAG = "link-test";

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_nvs());

    ESP_LOGI(TAG, "ESP32-S3 ESP-NOW button/LED link test");
    ESP_LOGI(TAG, "button=GPIO%d, WS2812=GPIO%d, channel=%d, group=%d",
             APP_BUTTON_GPIO, APP_RGB_GPIO, APP_ESPNOW_CHANNEL, APP_ESPNOW_GROUP);

    ESP_ERROR_CHECK(button_init(APP_BUTTON_GPIO));
    ESP_ERROR_CHECK(led_init(APP_RGB_GPIO));
    led_self_test();

    ESP_ERROR_CHECK(link_init());
    ESP_LOGI(TAG, "press BOOT on either unit; both RGB LEDs should light");

    bool local_pressed = false;
    bool remote_pressed = false;
    bool remote_alive = false;
    bool led_on = false;
    int64_t last_send_us = esp_timer_get_time();

    /* Announce ourselves immediately so the peer sees us before the first
     * heartbeat is due. */
    (void)link_send(false);

    while (true) {
        const int64_t now_us = esp_timer_get_time();
        const bool local_now = button_update();

        if (local_now != local_pressed) {
            local_pressed = local_now;
            ESP_LOGI(TAG, "local BOOT %s", local_pressed ? "pressed" : "released");
            (void)link_send(local_pressed);
            last_send_us = now_us;
        }

        const bool alive_now = link_remote_alive(now_us);
        if (alive_now != remote_alive) {
            remote_alive = alive_now;
            if (remote_alive) {
                uint8_t mac[ESP_NOW_ETH_ALEN] = {0};
                (void)link_remote_get_mac(mac);
                ESP_LOGI(TAG, "peer online: " MACSTR " (%d dBm)", MAC2STR(mac),
                         link_remote_rssi());
            } else {
                ESP_LOGW(TAG, "peer offline (nothing heard for %d ms)",
                         APP_LINK_TIMEOUT_MS);
            }
        }

        const bool remote_now = link_remote_pressed(now_us);
        if (remote_now != remote_pressed) {
            remote_pressed = remote_now;
            ESP_LOGI(TAG, "remote BOOT %s", remote_pressed ? "pressed" : "released");
        }

        const bool active = local_pressed || remote_pressed;
        if (active != led_on) {
            led_on = active;
            const esp_err_t err = led_set_active(led_on);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "LED update failed: %s", esp_err_to_name(err));
            }
            ESP_LOGI(TAG, "RGB LED %s (local=%d remote=%d)", led_on ? "ON" : "off",
                     local_pressed, remote_pressed);
        }

        /* Send on every edge (above), refresh quickly while held so a lost
         * release frame cannot leave the peer stuck on, and send a slow
         * heartbeat otherwise. */
        const int64_t refresh_us =
            (local_pressed ? APP_PRESS_REFRESH_MS : APP_HEARTBEAT_MS) * 1000LL;
        if (now_us - last_send_us >= refresh_us) {
            (void)link_send(local_pressed);
            last_send_us = now_us;
        }

        vTaskDelay(pdMS_TO_TICKS(APP_LOOP_PERIOD_MS));
    }
}
