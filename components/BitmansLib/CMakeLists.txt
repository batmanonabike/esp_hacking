idf_component_register(
    SRCS "bitmans_lib.c" "bitmans_blink.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver"
    PRIV_REQUIRES "esp_wifi" "nvs_flash" "esp_netif" "esp_timer" esp_driver_ledc
)
