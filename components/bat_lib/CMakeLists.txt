idf_component_register(
    SRCS "bat_ble.c" "bat_hash_table.c" "bat_wifi_logging.c" "bat_lib.c" "bat_blink.c" 
         "bat_ble_client.c" "bat_ble_client_logging.c" "bat_wifi_connect.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver" "nvs_flash" "esp_wifi" "esp_netif" "bt"
    PRIV_REQUIRES "esp_timer" "esp_driver_ledc"
)
