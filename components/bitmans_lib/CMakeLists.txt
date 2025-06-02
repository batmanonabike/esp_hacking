idf_component_register(
    SRCS "bitmans_wifi_logging.c" "bitmans_lib.c" "bitmans_blink.c" "bitmans_ble_client.c" "bitmans_wifi_connect.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver" "nvs_flash" "esp_wifi" "esp_netif" "bt"
    PRIV_REQUIRES "esp_timer" "esp_driver_ledc"
)
