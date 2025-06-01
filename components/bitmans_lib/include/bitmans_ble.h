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

typedef struct
{
    char service_uuid[ESP_UUID_LEN_128];
} service_uuid_t;

// `GCC typeof` because the header files doesn\'t expose scan_rst structure directly.
typedef typeof(((const esp_ble_gap_cb_param_t *)0)->scan_rst) ble_scan_result_t;

esp_err_t bitmans_ble_init();
esp_err_t bitmans_ble_term();
esp_err_t bitmans_ble_stop_scan();
esp_err_t bitmans_ble_register_gattc(gattc_app_id_t app_id);
esp_err_t bitmans_ble_unregister_gattc(gattc_app_id_t app_id);
esp_err_t bitmans_ble_start_scan(uint32_t scan_duration_secs);
esp_err_t bitmans_ble_uuid_to_service_uuid_t(const char *pszUUID, service_uuid_t *out_struct);
esp_err_t bitmans_ble_get_advertised_name(const ble_scan_result_t *pScanResult, advertised_name_t *pAdvertisedName);

#ifdef __cplusplus
}
#endif

