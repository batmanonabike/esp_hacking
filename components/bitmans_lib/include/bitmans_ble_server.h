#pragma once

#include "esp_err.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t bitmans_gatts_app_id;
typedef uint16_t bitmans_ble_uuid16_t;
typedef uint16_t bitmans_gatts_service_handle;

typedef struct bitmans_gatts_callbacks_t
{
    void * pContext;
    esp_gatt_if_t gatts_if;
    bitmans_gatts_service_handle service_handle;

    void (*on_reg)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_create)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_add_char)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_add_char_descr)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_start)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_connect)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_disconnect)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_read)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_write)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void (*on_unreg)(struct bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);

} bitmans_gatts_callbacks_t;

esp_err_t bitmans_ble_server_init();
esp_err_t bitmans_ble_server_term();
esp_err_t bitmans_ble_gatts_unregister(bitmans_gatts_app_id app_id);
esp_err_t bitmans_ble_gatts_register(bitmans_gatts_app_id app_id, bitmans_gatts_callbacks_t *, void * pContext);

void bitman_gatts_no_op(bitmans_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
void bitmans_ble_gatts_callbacks_init(bitmans_gatts_callbacks_t *, void * pContext);

esp_err_t bitmans_gatts_stop_advertising();
esp_err_t bitmans_gatts_start_service(bitmans_gatts_service_handle);
esp_err_t bitmans_gatts_begin_advertise128(const char *, bitmans_ble_uuid128_t *pId);
esp_err_t bitmans_gatts_begin_advertise(const char *pszAdvertisedName, const uint8_t *pId, uint8_t idLen);
esp_err_t bitmans_gatts_create_service128(esp_gatt_if_t gatts_if, bitmans_ble_uuid128_t *pId);
esp_err_t bitmans_gatts_create_char128(esp_gatt_if_t, bitmans_gatts_service_handle, 
    bitmans_ble_uuid128_t *, esp_gatt_char_prop_t, esp_gatt_perm_t);
esp_err_t bitmans_gatts_add_cccd(uint16_t service_handle, uint16_t char_handle);

esp_err_t bitmans_gatts_send_response(
    esp_gatt_if_t gatts_if, uint16_t conn_id, uint32_t trans_id,
    esp_gatt_status_t status, esp_gatt_rsp_t *pResponse);
esp_err_t bitmans_gatts_send_uint8(
    esp_gatt_if_t gatts_if, uint16_t handle, uint16_t conn_id, uint32_t trans_id,
    esp_gatt_status_t status, uint8_t value);
    
#ifdef __cplusplus
}
#endif
