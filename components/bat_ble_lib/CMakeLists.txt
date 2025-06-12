idf_component_register(
    SRCS "bat_ble_lib.c" 
    INCLUDE_DIRS "include"
    REQUIRES "driver" "nvs_flash" "esp_wifi" "esp_netif" "bt"
    PRIV_REQUIRES "esp_timer" "esp_driver_ledc"
)
