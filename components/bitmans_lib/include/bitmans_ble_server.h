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
esp_err_t bitmans_ble_server_unregister_gatts(bitmans_gatts_app_id app_id);
esp_err_t bitmans_ble_server_register_gatts(bitmans_gatts_app_id app_id, bitmans_gatts_callbacks_t *);

#ifdef __cplusplus
}
#endif
