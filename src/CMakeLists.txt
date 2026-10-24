idf_component_register(
    SRCS
        "main.c"
        "button.c"
        "ws2812.c"
        "espnow_link.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        esp_timer
        nvs_flash
        driver
)
