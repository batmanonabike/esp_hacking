#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// The GATT Client applications IDs, the application IDs are used to register GATT Client applications.
typedef enum {
    GATTC_APPFIRST = 0,
    GATTC_APP0 = GATTC_APPFIRST,       
    GATTC_APP1 = 1,       
    GATTC_APP2 = 2,       
    GATTC_APP3 = 3,       
    GATTC_APP4 = 4,
    GATTC_APPLAST = GATTC_APP4, // Last app ID       
} gattc_app_id_t;

esp_err_t bitmans_ble_init();
esp_err_t bitmans_ble_term();
esp_err_t bitmans_ble_stop_scan();
esp_err_t bitmans_ble_register_gattc(gattc_app_id_t app_id);
esp_err_t bitmans_ble_unregister_gattc(gattc_app_id_t app_id);
esp_err_t bitmans_ble_start_scan(uint32_t scan_duration_secs);

#ifdef __cplusplus
}
#endif

