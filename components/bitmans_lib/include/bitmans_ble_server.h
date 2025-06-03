#pragma once

#include "esp_err.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t bitmans_gatts_app_id;

typedef struct bitmans_gatts_callbacks_t
{
    esp_gatt_if_t gatts_if;
    uint16_t service_handle;

    void (*on_reg)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_create)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_add_char)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_start)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_connect)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_disconnect)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_read)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_write)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_unreg)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);

} bitmans_gatts_callbacks_t;

void bitman_gatts_no_op(bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);

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
