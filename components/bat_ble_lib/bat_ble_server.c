#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "esp_bt_defs.h"
#include "assert.h"

#include "bat_lib.h"
#include "bat_ble_lib.h"

static const char *TAG = "bat_ble_server";

esp_err_t bat_ble_gatts_app_register(uint16_t appId)
{
    esp_err_t ret = esp_ble_gatts_app_register(appId);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register GATTS app with ID %d, error: %s", appId, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "GATTS app registered with ID %d", appId);
    return ESP_OK;
}

esp_err_t bat_ble_gap_set_device_name(const char *pName)
{
    assert(pName != NULL);
    
    esp_err_t ret = esp_ble_gap_set_device_name(pName);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set device name to '%s', error: %s", pName, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Device name set to '%s'", pName);
    return ESP_OK;
}

esp_err_t bat_ble_gap_config_adv_data(esp_ble_adv_data_t *pAdvData)
{
    assert(pAdvData != NULL);
    
    esp_err_t ret = esp_ble_gap_config_adv_data(pAdvData);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure advertising data, error: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Advertising data configured successfully");
    return ESP_OK;
}

esp_err_t bat_ble_gatts_create_service(esp_gatt_if_t gattsIf, esp_gatt_srvc_id_t *pServiceId, uint16_t numHandle)
{
    assert(pServiceId != NULL);
    
    uint16_t serviceHandle = esp_ble_gatts_create_service(gattsIf, pServiceId, numHandle);
    if (serviceHandle == 0)
    {
        ESP_LOGE(TAG, "Failed to create GATTS service");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "GATTS service created with handle %d", serviceHandle);
    return ESP_OK;
}

esp_err_t bat_ble_gatts_add_char(uint16_t serviceHandle, esp_bt_uuid_t *pCharUuid, esp_gatt_perm_t perm, esp_gatt_char_prop_t property, esp_attr_value_t *pCharVal, esp_attr_control_t *pControl)
{
    assert(pCharUuid != NULL);
    
    esp_err_t ret = esp_ble_gatts_add_char(serviceHandle, pCharUuid, perm, property, pCharVal, pControl);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add characteristic to service handle %d, error: %s", serviceHandle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Characteristic added to service handle %d", serviceHandle);
    return ESP_OK;
}

esp_err_t bat_ble_gatts_add_char_descr(uint16_t serviceHandle, esp_bt_uuid_t *pDescrUuid, esp_gatt_perm_t perm, esp_attr_value_t *pAttrValue, esp_attr_control_t *pControl)
{
    assert(pDescrUuid != NULL);
    
    esp_err_t ret = esp_ble_gatts_add_char_descr(serviceHandle, pDescrUuid, perm, pAttrValue, pControl);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add descriptor to service handle %d, error: %s", serviceHandle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Descriptor added to service handle %d", serviceHandle);
    return ESP_OK;
}

esp_err_t bat_gatts_start_service(uint16_t serviceHandle)
{
    esp_err_t ret = esp_ble_gatts_start_service(serviceHandle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start service with handle %d, error: %s", serviceHandle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Service with handle %d started successfully", serviceHandle);
    return ESP_OK;
}

esp_err_t bat_ble_gap_start_advertising(esp_ble_adv_params_t *pAdvParams)
{
    assert(pAdvParams != NULL);
    
    esp_err_t ret = esp_ble_gap_start_advertising(pAdvParams);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start advertising, error: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Advertising started successfully");
    return ESP_OK;
}

esp_err_t bat_ble_gatts_send_response(esp_gatt_if_t gattsIf, uint16_t connId, uint32_t transId, esp_gatt_status_t status, esp_gatt_rsp_t *pRsp)
{
    esp_err_t ret = esp_ble_gatts_send_response(gattsIf, connId, transId, status, pRsp);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send GATTS response, error: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "GATTS response sent successfully");
    return ESP_OK;
}

esp_err_t bat_ble_gap_stop_advertising(void)
{
    esp_err_t ret = esp_ble_gap_stop_advertising();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop advertising, error: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Advertising stopped successfully");
    return ESP_OK;
}

esp_err_t bat_gatts_stop_service(uint16_t serviceHandle)
{
    esp_err_t ret = esp_ble_gatts_stop_service(serviceHandle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop service with handle %d, error: %s", serviceHandle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Service with handle %d stopped successfully", serviceHandle);
    return ESP_OK;
}

esp_err_t bat_ble_gatts_app_unregister(esp_gatt_if_t gattsIf)
{
    esp_err_t ret = esp_ble_gatts_app_unregister(gattsIf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister GATTS app with interface %d, error: %s", gattsIf, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "GATTS app with interface %d unregistered successfully", gattsIf);
    return ESP_OK;
}
