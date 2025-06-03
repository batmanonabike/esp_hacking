#pragma once

#include "esp_err.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GATTS_APPFIRST = 0,
    GATTS_APP0 = GATTS_APPFIRST,       
    GATTS_APP1 = 1,       
    GATTS_APP2 = 2,       
    GATTS_APP3 = 3,       
    GATTS_APP4 = 4,
    GATTS_APPLAST = GATTS_APP4, // Last app ID       
    GATTS_APPCOUNT = GATTS_APPLAST - GATTS_APPFIRST + 1, 
} gatts_app_id_t;

typedef struct
{
    void (*on_reg)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    void (*on_create)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    void (*on_connect)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    void (*on_read)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    void (*on_write)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    // ...add more as needed
} bitmans_gatts_callbacks_t;

esp_err_t bitmans_ble_server_init();
esp_err_t bitmans_ble_server_term();
esp_err_t bitmans_ble_server_register_gatts();
esp_err_t bitmans_ble_server_unregister_gatts();
esp_err_t bitmans_ble_server_register_gatts(gatts_app_id_t app_id, bitmans_gatts_callbacks_t *);

#ifdef __cplusplus
}
#endif
