#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bat_ble_gatts_app_register(uint16_t appId);
esp_err_t bat_ble_gap_set_device_name(const char *pName);
esp_err_t bat_ble_gap_config_adv_data(esp_ble_adv_data_t *pAdvData);
esp_err_t bat_ble_gatts_create_service(esp_gatt_if_t gattsIf, esp_gatt_srvc_id_t *pServiceId, uint16_t numHandle);
esp_err_t bat_ble_gatts_add_char(uint16_t serviceHandle, esp_bt_uuid_t *pCharUuid, esp_gatt_perm_t perm, esp_gatt_char_prop_t property, esp_attr_value_t *pCharVal, esp_attr_control_t *pControl);
esp_err_t bat_ble_gatts_add_char_descr(uint16_t serviceHandle, esp_bt_uuid_t *pDescrUuid, esp_gatt_perm_t perm, esp_attr_value_t *pAttrValue, esp_attr_control_t *pControl);
esp_err_t bat_ble_gatts_start_service(uint16_t serviceHandle);
esp_err_t bat_ble_gap_start_advertising(esp_ble_adv_params_t *pAdvParams);
esp_err_t bat_ble_gatts_send_response(esp_gatt_if_t gattsIf, uint16_t connId, uint32_t transId, esp_gatt_status_t status, esp_gatt_rsp_t *pRsp);
esp_err_t bat_ble_gap_stop_advertising(void);
esp_err_t bat_ble_gatts_stop_service(uint16_t serviceHandle);
esp_err_t bat_ble_gatts_app_unregister(esp_gatt_if_t gattsIf);

#ifdef __cplusplus
}
#endif

