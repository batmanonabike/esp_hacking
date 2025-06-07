#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

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

#define ADVERTISED_NAME_BUFFER_LEN 32

typedef struct
{
    char name[ADVERTISED_NAME_BUFFER_LEN];
} advertised_name_t;

// `GCC typeof` because the header files doesn\'t expose scan_rst structure directly.
typedef typeof(((esp_ble_gap_cb_param_t *)0)->scan_rst) ble_scan_result_t;

esp_err_t bitmans_ble_client_init();
esp_err_t bitmans_ble_client_term();
esp_err_t bitmans_ble_client_stop_scan();
esp_err_t bitmans_ble_client_register_gattc(gattc_app_id_t app_id);
esp_err_t bitmans_ble_client_unregister_gattc(gattc_app_id_t app_id);
esp_err_t bitmans_ble_client_start_scan(uint32_t scan_duration_secs);
esp_err_t bitmans_ble_client_get_advertised_name(ble_scan_result_t *pScanResult, advertised_name_t *pAdvertisedName);

#ifdef __cplusplus
}
#endif

