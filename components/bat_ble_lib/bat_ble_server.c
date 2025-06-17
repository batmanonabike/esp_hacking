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
#include "bat_uuid.h"
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

esp_err_t bat_ble_gatts_add_char(uint16_t serviceHandle, esp_bt_uuid_t *pCharUuid, esp_gatt_perm_t perm, 
    esp_gatt_char_prop_t property, esp_attr_value_t *pCharVal, esp_attr_control_t *pControl)
{
    assert(pCharUuid != NULL);
    if (serviceHandle == 0) 
    {
        ESP_LOGE(TAG, "Invalid service handle (0)");
        return ESP_ERR_INVALID_ARG;
    }

    char uuid_str[45] = {0}; 
    bat_uuid_to_log_string(pCharUuid, uuid_str, sizeof(uuid_str));
    
    ESP_LOGI(TAG, "Adding characteristic UUID=%s, perm=0x%x, prop=0x%x to service=0x%x", 
             uuid_str, perm, property, serviceHandle);
    
    // Check for initial value validity
    if (pCharVal != NULL) 
    {
        ESP_LOGI(TAG, "  with value: max_len=%d, len=%d, value_ptr=%p", 
                 pCharVal->attr_max_len, pCharVal->attr_len, pCharVal->attr_value);
        
        if (pCharVal->attr_value == NULL && pCharVal->attr_len > 0) 
            ESP_LOGW(TAG, "  Warning: attr_value is NULL but attr_len is %d", pCharVal->attr_len);
        
        if (pCharVal->attr_max_len < pCharVal->attr_len) 
            ESP_LOGW(TAG, "  Warning: max_len (%d) < len (%d)", pCharVal->attr_max_len, pCharVal->attr_len);
        
    } 
    else 
        ESP_LOGI(TAG, "  with no initial value");
    
    esp_err_t ret = esp_ble_gatts_add_char(serviceHandle, pCharUuid, perm, property, pCharVal, pControl);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add characteristic to service handle %d, error: %s (code=%d)", 
                 serviceHandle, esp_err_to_name(ret), ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "Characteristic added to service handle %d", serviceHandle);
    return ESP_OK;
}

esp_err_t bat_ble_gatts_add_char_descr(uint16_t serviceHandle, esp_bt_uuid_t *pDescrUuid, esp_gatt_perm_t perm, esp_attr_value_t *pAttrValue, esp_attr_control_t *pControl)
{
    assert(pDescrUuid != NULL);
    if (serviceHandle == 0) 
    {
        ESP_LOGE(TAG, "Invalid service handle (0) for descriptor");
        return ESP_ERR_INVALID_ARG;
    }
    
    char uuid_str[45] = {0}; 
    bat_uuid_to_log_string(pDescrUuid, uuid_str, sizeof(uuid_str));
    
    ESP_LOGI(TAG, "Adding descriptor UUID=%s, perm=0x%x to service=0x%x", 
             uuid_str, perm, serviceHandle);
    
    // Check for initial value validity
    if (pAttrValue != NULL) 
    {
        ESP_LOGI(TAG, "  with value: max_len=%d, len=%d, value_ptr=%p", 
                 pAttrValue->attr_max_len, pAttrValue->attr_len, pAttrValue->attr_value);
                 
        if (pAttrValue->attr_value == NULL && pAttrValue->attr_len > 0) 
            ESP_LOGW(TAG, "  Warning: attr_value is NULL but attr_len is %d", pAttrValue->attr_len);
        
        if (pAttrValue->attr_max_len < pAttrValue->attr_len) 
            ESP_LOGW(TAG, "  Warning: max_len (%d) < len (%d)", pAttrValue->attr_max_len, pAttrValue->attr_len);
        
    } 
    else 
        ESP_LOGI(TAG, "  with no initial value");
    
    // If this is a CCCD (0x2902), do some extra validation
    if (pDescrUuid->len == ESP_UUID_LEN_16 && pDescrUuid->uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) 
    {
        ESP_LOGI(TAG, "  Adding Client Characteristic Configuration Descriptor (CCCD)");
        
        // CCCDs must be read-write and should have initial value
        if (!(perm & ESP_GATT_PERM_READ) || !(perm & ESP_GATT_PERM_WRITE)) 
        {
            ESP_LOGW(TAG, "  Warning: CCCD should have both READ and WRITE permissions (current=0x%x)", perm);
        }
        
        // Check if we have a proper initial value (should be 0x0000)
        if (pAttrValue != NULL && pAttrValue->attr_value != NULL && pAttrValue->attr_len == 2) 
        {
            ESP_LOGI(TAG, "  CCCD initial value: 0x%02x%02x", 
                     pAttrValue->attr_value[1], pAttrValue->attr_value[0]);
        }
    }
    
    // Add a small delay before adding descriptor to ensure the stack is ready
    vTaskDelay(pdMS_TO_TICKS(10));
    
    esp_err_t ret = esp_ble_gatts_add_char_descr(serviceHandle, pDescrUuid, perm, pAttrValue, pControl);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add descriptor to service handle %d, error: %s (code=%d)", 
                 serviceHandle, esp_err_to_name(ret), ret);
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
