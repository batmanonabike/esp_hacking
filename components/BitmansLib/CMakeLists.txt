idf_component_register(
    SRCS "bitmans_lib.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver"
    PRIV_REQUIRES "esp_wifi" "nvs_flash" "esp_netif" 
)
