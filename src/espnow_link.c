#include "espnow_link.h"

#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

#include "app_config.h"

static const char *TAG = "espnow_link";

#define LINK_MAGIC 0xB7
#define LINK_VERSION 1
#define LINK_FLAG_PRESSED (1U << 0)

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t version;
    uint8_t group;
    uint8_t flags;
    uint32_t seq;
    uint32_t uptime_ms;
} link_packet_t;

typedef struct {
    bool valid;
    bool pressed;
    int64_t last_rx_us;
    int8_t rssi;
    uint8_t mac[ESP_NOW_ETH_ALEN];
} remote_state_t;

static const uint8_t s_broadcast[ESP_NOW_ETH_ALEN] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static uint8_t s_own_mac[ESP_NOW_ETH_ALEN];
static uint32_t s_tx_seq;
static bool s_initialized;

static remote_state_t s_remote;
static portMUX_TYPE s_remote_mux = portMUX_INITIALIZER_UNLOCKED;

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (info == NULL || data == NULL || len != (int)sizeof(link_packet_t)) {
        return;
    }

    /* ESP-NOW data may not be aligned, so copy before dereferencing. */
    link_packet_t packet;
    memcpy(&packet, data, sizeof(packet));

    if (packet.magic != LINK_MAGIC || packet.version != LINK_VERSION ||
        packet.group != APP_ESPNOW_GROUP) {
        return;
    }

    if (memcmp(info->src_addr, s_own_mac, sizeof(s_own_mac)) == 0) {
        return;
    }

    const int64_t now_us = esp_timer_get_time();
    const bool pressed = (packet.flags & LINK_FLAG_PRESSED) != 0;

    /* Called from the Wi-Fi task, so keep this short and don't log here. */
    portENTER_CRITICAL(&s_remote_mux);
    s_remote.valid = true;
    s_remote.pressed = pressed;
    s_remote.last_rx_us = now_us;
    s_remote.rssi = (info->rx_ctrl != NULL) ? info->rx_ctrl->rssi : 0;
    memcpy(s_remote.mac, info->src_addr, sizeof(s_remote.mac));
    portEXIT_CRITICAL(&s_remote_mux);
}

esp_err_t link_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    const wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_config), TAG, "esp_wifi_init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG,
                        "set Wi-Fi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set Wi-Fi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "disable Wi-Fi power save");
    ESP_RETURN_ON_ERROR(esp_wifi_get_mac(WIFI_IF_STA, s_own_mac), TAG, "read MAC");
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(APP_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE),
                        TAG, "set channel %d", APP_ESPNOW_CHANNEL);

    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp_now_init");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(on_recv), TAG,
                        "register ESP-NOW receive callback");

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_broadcast, sizeof(peer.peer_addr));
    peer.channel = APP_ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "ready: mac=" MACSTR " channel=%d group=%d", MAC2STR(s_own_mac),
             APP_ESPNOW_CHANNEL, APP_ESPNOW_GROUP);
    return ESP_OK;
}

esp_err_t link_send(bool pressed)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const link_packet_t packet = {
        .magic = LINK_MAGIC,
        .version = LINK_VERSION,
        .group = APP_ESPNOW_GROUP,
        .flags = pressed ? LINK_FLAG_PRESSED : 0,
        .seq = ++s_tx_seq,
        .uptime_ms = (uint32_t)(esp_timer_get_time() / 1000),
    };

    /* Broadcast frames are unacknowledged: a successful return only means
     * they were queued for transmission. */
    return esp_now_send(s_broadcast, (const uint8_t *)&packet, sizeof(packet));
}

bool link_remote_alive(int64_t now_us)
{
    bool alive = false;

    portENTER_CRITICAL(&s_remote_mux);
    alive = s_remote.valid && (now_us >= s_remote.last_rx_us) &&
            ((now_us - s_remote.last_rx_us) <= ((int64_t)APP_LINK_TIMEOUT_MS * 1000));
    portEXIT_CRITICAL(&s_remote_mux);

    return alive;
}

bool link_remote_pressed(int64_t now_us)
{
    bool pressed = false;

    portENTER_CRITICAL(&s_remote_mux);
    pressed = s_remote.valid && s_remote.pressed && (now_us >= s_remote.last_rx_us) &&
              ((now_us - s_remote.last_rx_us) <= ((int64_t)APP_LINK_TIMEOUT_MS * 1000));
    portEXIT_CRITICAL(&s_remote_mux);

    return pressed;
}

bool link_remote_get_mac(uint8_t mac[ESP_NOW_ETH_ALEN])
{
    bool valid;

    portENTER_CRITICAL(&s_remote_mux);
    valid = s_remote.valid;
    if (valid) {
        memcpy(mac, s_remote.mac, sizeof(s_remote.mac));
    }
    portEXIT_CRITICAL(&s_remote_mux);

    return valid;
}

int8_t link_remote_rssi(void)
{
    int8_t rssi;

    portENTER_CRITICAL(&s_remote_mux);
    rssi = s_remote.rssi;
    portEXIT_CRITICAL(&s_remote_mux);

    return rssi;
}
