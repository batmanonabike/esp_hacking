idf_component_register(
    SRCS "bat_ble_lib.c" "bat_ble_server.c" "bat_gatts_fsm.c" "bat_gatts_fsm_helpers.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver" "nvs_flash" "esp_wifi" "esp_netif" "bt" "bat_lib"
    PRIV_REQUIRES "esp_timer" "esp_driver_ledc"
)
