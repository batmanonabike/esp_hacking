#pragma once

#include "esp_err.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t bitmans_gatts_app_id;

typedef struct
{
    void (*on_reg)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    void (*on_read)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    void (*on_write)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    void (*on_create)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    void (*on_connect)(esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
    // ...add more as needed
} bitmans_gatts_callbacks_t;

/**
 * @brief Empty GATT server callbacks structure.
 *
 * This can be used as a default value when no callbacks are needed.
 */
extern bitmans_gatts_callbacks_t bitmans_gatts_default_callbacks;

esp_err_t bitmans_ble_server_init();
esp_err_t bitmans_ble_server_term();
esp_err_t bitmans_ble_gatts_unregister(bitmans_gatts_app_id app_id);
esp_err_t bitmans_ble_gatts_register(bitmans_gatts_app_id app_id, bitmans_gatts_callbacks_t *);
esp_err_t bitmans_gatts_create_service(esp_gatt_if_t gatts_if, bitmans_ble_uuid_t *pServiceUUID);
esp_err_t bitmans_gatts_advertise(const char *pszAdvertisedName, bitmans_ble_uuid_t *pServiceUUID);
esp_err_t bitmans_gatts_create_characteristic(
    esp_gatt_if_t gatts_if, uint16_t service_handle, 
    bitmans_ble_uuid_t *pCharUUID, esp_gatt_char_prop_t properties);

#ifdef __cplusplus
}
#endif
