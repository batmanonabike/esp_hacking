#pragma once

#include "esp_err.h"
#include "esp_gatts_api.h"
#include "bat_ble.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef uint16_t bat_gatts_app_id;
    typedef uint16_t bat_gatts_service_handle;

    typedef struct bat_gatts_callbacks_t
    {
        void *pContext;
        esp_gatt_if_t gatts_if;
        bat_gatts_service_handle service_handle;

        void (*on_reg)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_create)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_add_char)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_add_char_descr)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_start)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_stop)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_connect)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_disconnect)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_read)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_write)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
        void (*on_unreg)(struct bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);

    } bat_gatts_callbacks_t;    
	
	esp_err_t bat_ble_server_init();
    esp_err_t bat_ble_server_deinit();

    void bitman_gatts_no_op(bat_gatts_callbacks_t *, esp_ble_gatts_cb_param_t *);
    void bat_ble_gatts_callbacks_init(bat_gatts_callbacks_t *, void *pContext);

    esp_err_t bat_gatts_stop_advertising();
    esp_err_t bat_gatts_start_advertising();
    esp_err_t bat_gatts_start_service(bat_gatts_service_handle);
    esp_err_t bat_gatts_stop_service(bat_gatts_service_handle);
    esp_err_t bat_gatts_add_cccd(uint16_t service_handle, uint16_t char_handle);
    esp_err_t bat_gatts_begin_advert_data_set128(const char *, bat_ble_uuid128_t *pId);
    esp_err_t bat_gatts_create_service128(esp_gatt_if_t gatts_if, bat_ble_uuid128_t *pId);
    esp_err_t bat_gatts_begin_advert_data_set(const char *pszAdvertisedName, uint8_t *pId, uint8_t idLen);
    
    esp_err_t bat_gatts_register(bat_gatts_app_id app_id, bat_gatts_callbacks_t *, void *pContext);
    esp_err_t bat_gatts_unregister(bat_gatts_app_id app_id);

    esp_err_t bat_gatts_send_response(
        esp_gatt_if_t gatts_if, uint16_t conn_id, uint32_t trans_id,
        esp_gatt_status_t status, esp_gatt_rsp_t *pResponse);
    esp_err_t bat_gatts_send_uint8(
        esp_gatt_if_t gatts_if, uint16_t handle, uint16_t conn_id, uint32_t trans_id,
        esp_gatt_status_t status, uint8_t value);
    esp_err_t bat_gatts_create_char128(esp_gatt_if_t, bat_gatts_service_handle,
        bat_ble_uuid128_t *, esp_gatt_char_prop_t, esp_gatt_perm_t);

    typedef struct bat_gaps_callbacks_t
    {
        void *pContext;
        void (*on_advert_data_set)(struct bat_gaps_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_advert_start)(struct bat_gaps_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_advert_stop)(struct bat_gaps_callbacks_t *, esp_ble_gap_cb_param_t *);

    } bat_gaps_callbacks_t;
    void bat_ble_gaps_callbacks_init(bat_gaps_callbacks_t *, void *pContext);

#ifdef __cplusplus
}
#endif
