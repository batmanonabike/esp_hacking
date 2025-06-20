#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt_defs.h"
#include "esp_gattc_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"

#include "bat_ble_lib.h"
#include "bat_gattc_simple.h"
#include "bat_uuid.h"

static const char *TAG = "bat_gattc_simple";

// Global client pointer for callback handling
static bat_gattc_client_t *gpCurrentClient = NULL;

// Default scan parameters
static esp_ble_scan_params_t g_default_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50, // 50ms
    .scan_window            = 0x30, // 30ms
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

// GATT client event handler prototype
static void bat_gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
// GAP event handler prototype
static void bat_gattc_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

// Helper function to extract device name from adv data
static bool extract_device_name(uint8_t *adv_data, uint8_t adv_data_len, char *name, size_t name_size)
{
    if (adv_data == NULL || name == NULL || name_size == 0) 
    {
        return false;
    }
    
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    
    adv_name = esp_ble_resolve_adv_data(adv_data, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
    if (adv_name == NULL) {
        adv_name = esp_ble_resolve_adv_data(adv_data, ESP_BLE_AD_TYPE_NAME_SHORT, &adv_name_len);
    }
    
    if (adv_name != NULL && adv_name_len > 0) {
        size_t copy_len = adv_name_len < name_size - 1 ? adv_name_len : name_size - 1;
        memcpy(name, adv_name, copy_len);
        name[copy_len] = '\0';
        return true;
    }
    
    return false;
}

// Helper function to check if an UUID exists in adv data
static bool has_service_uuid(uint8_t *adv_data, uint8_t adv_data_len, esp_bt_uuid_t *target_uuid)
{
    if (adv_data == NULL || target_uuid == NULL) {
        return false;
    }
    
    uint8_t *uuid_data = NULL;
    uint8_t uuid_data_len = 0;
      // Check for 16-bit UUIDs
    uuid_data = esp_ble_resolve_adv_data(adv_data, ESP_BLE_AD_TYPE_16SRV_CMPL, &uuid_data_len);
    if (uuid_data != NULL && target_uuid->len == ESP_UUID_LEN_16) 
    {
        for (int i = 0; i < uuid_data_len; i += 2) 
        {
            uint16_t uuid16 = uuid_data[i] | (uuid_data[i+1] << 8);
            if (uuid16 == target_uuid->uuid.uuid16) 
            {
                return true;
            }
        }
    }
      // Check for 32-bit UUIDs
    uuid_data = esp_ble_resolve_adv_data(adv_data, ESP_BLE_AD_TYPE_32SRV_CMPL, &uuid_data_len);
    if (uuid_data != NULL && target_uuid->len == ESP_UUID_LEN_32) 
    {
        for (int i = 0; i < uuid_data_len; i += 4) 
        {
            uint32_t uuid32 = uuid_data[i] | (uuid_data[i+1] << 8) | 
                             (uuid_data[i+2] << 16) | (uuid_data[i+3] << 24);
            if (uuid32 == target_uuid->uuid.uuid32) 
            {
                return true;
            }
        }
    }
      // Check for 128-bit UUIDs
    uuid_data = esp_ble_resolve_adv_data(adv_data, ESP_BLE_AD_TYPE_128SRV_CMPL, &uuid_data_len);
    if (uuid_data != NULL && target_uuid->len == ESP_UUID_LEN_128) 
    {
        for (int i = 0; i < uuid_data_len; i += 16) 
        {
            if (memcmp(uuid_data + i, target_uuid->uuid.uuid128, ESP_UUID_LEN_128) == 0) 
            {
                return true;
            }
        }
    }
    
    return false;
}

// Handler for GATTC events
static void bat_gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    // Check if we have a valid client
    if (gpCurrentClient == NULL) 
    {
        ESP_LOGE(TAG, "GATTC event without valid client");
        return;
    }
      
    // Handle events
    switch (event) 
    {
        case ESP_GATTC_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) 
            {
                ESP_LOGI(TAG, "GATTC app registered with ID %d", param->reg.app_id);
                gpCurrentClient->gattcIf = gattc_if;
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_REGISTERED_BIT);
            } 
            else 
            {
                ESP_LOGE(TAG, "GATTC app registration failed, status %d", param->reg.status);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
              case ESP_GATTC_OPEN_EVT:
            if (param->open.status == ESP_GATT_OK) 
            {
                ESP_LOGI(TAG, "Connection established with server");
                gpCurrentClient->isConnected = true;
                gpCurrentClient->connId = param->open.conn_id;
                memcpy(gpCurrentClient->remoteBda, param->open.remote_bda, sizeof(esp_bd_addr_t));
                
                if (gpCurrentClient->callbacks.onConnect != NULL) 
                {
                    gpCurrentClient->callbacks.onConnect(gpCurrentClient, param);
                }
                
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_CONNECTED_BIT);
            } 
            else 
            {
                ESP_LOGE(TAG, "Failed to connect to server, status %d", param->open.status);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
              case ESP_GATTC_CLOSE_EVT:
            ESP_LOGI(TAG, "Connection closed, reason %d", param->close.reason);
            gpCurrentClient->isConnected = false;
            
            if (gpCurrentClient->callbacks.onDisconnect != NULL) 
            {
                gpCurrentClient->callbacks.onDisconnect(gpCurrentClient, param);
            }
            
            xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_DISCONNECTED_BIT);
            break;
              case ESP_GATTC_SEARCH_RES_EVT:
            ESP_LOGI(TAG, "GATT service found");
            
            // Check if this is the service we're looking for
            if (bat_uuid_equal(&param->search_res.srvc_id.uuid, &gpCurrentClient->targetServiceUuid)) 
            {
                ESP_LOGI(TAG, "Found target service");
                gpCurrentClient->serviceStartHandle = param->search_res.start_handle;
                gpCurrentClient->serviceEndHandle = param->search_res.end_handle;
                
                if (gpCurrentClient->callbacks.onServiceFound != NULL) 
                {
                    gpCurrentClient->callbacks.onServiceFound(gpCurrentClient, param);
                }
            }
            break;
              case ESP_GATTC_SEARCH_CMPL_EVT:
            ESP_LOGI(TAG, "Service search completed, status %d", param->search_cmpl.status);
            
            if (param->search_cmpl.status == ESP_GATT_OK && 
                gpCurrentClient->serviceStartHandle != 0 && 
                gpCurrentClient->serviceEndHandle != 0) 
            {
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_SERVICE_FOUND_BIT);
            } 
            else if (param->search_cmpl.status != ESP_GATT_OK) 
            {
                ESP_LOGE(TAG, "Service search failed");
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            } 
            else 
            {
                ESP_LOGW(TAG, "Target service not found");
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
              case ESP_GATTC_READ_CHAR_EVT:
            if (param->read.status == ESP_GATT_OK) 
            {
                ESP_LOGI(TAG, "Read characteristic success, handle 0x%04X, value len %d", 
                         param->read.handle, param->read.value_len);
                
                if (gpCurrentClient->callbacks.onRead != NULL) 
                {
                    gpCurrentClient->callbacks.onRead(gpCurrentClient, param);
                }
                
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_READ_DONE_BIT);
            } 
            else 
            {
                ESP_LOGE(TAG, "Read characteristic failed, status %d", param->read.status);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
              case ESP_GATTC_WRITE_CHAR_EVT:
            if (param->write.status == ESP_GATT_OK) 
            {
                ESP_LOGI(TAG, "Write characteristic success, handle 0x%04X", param->write.handle);
                
                if (gpCurrentClient->callbacks.onWrite != NULL) 
                {
                    gpCurrentClient->callbacks.onWrite(gpCurrentClient, param);
                }
                
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_WRITE_DONE_BIT);
            } 
            else 
            {
                ESP_LOGE(TAG, "Write characteristic failed, status %d", param->write.status);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
              case ESP_GATTC_WRITE_DESCR_EVT:
            if (param->write.status == ESP_GATT_OK) 
            {
                ESP_LOGI(TAG, "Write descriptor success, handle 0x%04X", param->write.handle);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_DESC_WRITE_DONE_BIT);
            } 
            else 
            {
                ESP_LOGE(TAG, "Write descriptor failed, status %d", param->write.status);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
              case ESP_GATTC_NOTIFY_EVT:
            ESP_LOGI(TAG, "Notification/indication received, handle 0x%04X, value len %d, is_notify %d", 
                     param->notify.handle, param->notify.value_len, param->notify.is_notify);
            
            if (gpCurrentClient->callbacks.onNotify != NULL) 
            {
                gpCurrentClient->callbacks.onNotify(gpCurrentClient, param);
            }
            
            xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_NOTIFY_RECV_BIT);
            break;
              case ESP_GATTC_REG_FOR_NOTIFY_EVT:
            if (param->reg_for_notify.status == ESP_GATT_OK) 
            {
                ESP_LOGI(TAG, "Registered for notifications, handle 0x%04X", param->reg_for_notify.handle);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_NOTIFY_REG_BIT);
            } 
            else 
            {
                ESP_LOGE(TAG, "Failed to register for notifications, status %d", param->reg_for_notify.status);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "Unhandled GATTC event %d", event);
            break;
    }
}

// Handler for GAP events
static void bat_gattc_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    // Check if we have a valid client
    if (gpCurrentClient == NULL) 
    {
        ESP_LOGE(TAG, "GAP event without valid client");
        return;
    }
    
    // Handle events
    switch (event) 
    {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "Scan parameters set, status %d", param->scan_param_cmpl.status);
            break;
            
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) 
            {
                ESP_LOGI(TAG, "Scan started successfully");
            } 
            else 
            {
                ESP_LOGE(TAG, "Scan start failed, status %d", param->scan_start_cmpl.status);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
              case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            if (param->scan_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) 
            {
                ESP_LOGI(TAG, "Scan stopped successfully");
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_SCAN_DONE_BIT);
            } 
            else 
            {
                ESP_LOGE(TAG, "Scan stop failed, status %d", param->scan_stop_cmpl.status);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_ERROR_BIT);
            }
            break;
              case ESP_GAP_BLE_SCAN_RESULT_EVT:
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) 
            {
                // Process scan result
                ESP_LOGD(TAG, "Device found, addr type %d", param->scan_rst.ble_addr_type);
                
                // Check if we've reached max devices
                if (gpCurrentClient->scanResultCount >= BAT_MAX_SCAN_DEVICES) 
                {
                    ESP_LOGW(TAG, "Scan result buffer full, ignoring device");
                    return;
                }
                  // Check if we're filtering by service UUID
                if (gpCurrentClient->targetServiceUuid.len != 0) 
                {
                    if (!has_service_uuid(param->scan_rst.ble_adv, param->scan_rst.adv_data_len, 
                                        &gpCurrentClient->targetServiceUuid)) 
                    {
                        // Skip devices that don't advertise our target service
                        return;
                    }
                }
                
                // Store device information
                bat_gattc_scan_result_t *result = &gpCurrentClient->scanResults[gpCurrentClient->scanResultCount];
                
                memcpy(result->addr, param->scan_rst.bda, sizeof(esp_bd_addr_t));
                result->addr_type = param->scan_rst.ble_addr_type;
                result->rssi = param->scan_rst.rssi;
                  // Extract name
                if (!extract_device_name(param->scan_rst.ble_adv, param->scan_rst.adv_data_len, 
                                      result->name, sizeof(result->name))) 
                {
                    sprintf(result->name, "Unknown-%02X%02X%02X", 
                            param->scan_rst.bda[3], param->scan_rst.bda[4], param->scan_rst.bda[5]);
                }
                
                // Extract service UUID if present
                result->has_service_uuid = false;
                uint8_t *uuid_data = NULL;
                uint8_t uuid_len = 0;
                  // Try to find any advertised service UUID
                uuid_data = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, 
                                                  ESP_BLE_AD_TYPE_128SRV_CMPL, &uuid_len);
                if (uuid_data != NULL && uuid_len == ESP_UUID_LEN_128) 
                {
                    result->service_uuid.len = ESP_UUID_LEN_128;
                    memcpy(result->service_uuid.uuid.uuid128, uuid_data, ESP_UUID_LEN_128);
                    result->has_service_uuid = true;
                } 
                else 
                {
                    uuid_data = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, 
                                                      ESP_BLE_AD_TYPE_32SRV_CMPL, &uuid_len);
                    if (uuid_data != NULL && uuid_len == 4) 
                    {
                        result->service_uuid.len = ESP_UUID_LEN_32;
                        result->service_uuid.uuid.uuid32 = uuid_data[0] | (uuid_data[1] << 8) | 
                                                         (uuid_data[2] << 16) | (uuid_data[3] << 24);
                        result->has_service_uuid = true;
                    } 
                    else 
                    {
                        uuid_data = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, 
                                                          ESP_BLE_AD_TYPE_16SRV_CMPL, &uuid_len);
                        if (uuid_data != NULL && uuid_len == 2) 
                        {
                            result->service_uuid.len = ESP_UUID_LEN_16;
                            result->service_uuid.uuid.uuid16 = uuid_data[0] | (uuid_data[1] << 8);
                            result->has_service_uuid = true;
                        }
                    }
                }
                
                ESP_LOGI(TAG, "Device [%d] %s, RSSI %d", gpCurrentClient->scanResultCount, 
                         result->name, result->rssi);
                  // Call scan result callback if registered
                if (gpCurrentClient->callbacks.onScanResult != NULL) 
                {
                    gpCurrentClient->callbacks.onScanResult(gpCurrentClient, param);
                }
                
                // Increment scan result count
                gpCurrentClient->scanResultCount++;
                  } 
            else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) 
            {
                ESP_LOGI(TAG, "Scan complete, found %d devices", gpCurrentClient->scanResultCount);
                xEventGroupSetBits(gpCurrentClient->eventGroup, BLE_CLIENT_SCAN_DONE_BIT);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "Unhandled GAP event %d", event);
            break;
    }
}

/************************************
 * Public API implementations
 ************************************/

esp_err_t bat_gattc_init(bat_gattc_client_t *pClient, void *pContext, uint16_t appId, int timeoutMs)
{
    if (pClient == NULL) 
    {
        ESP_LOGE(TAG, "Invalid client pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize client structure
    memset(pClient, 0, sizeof(bat_gattc_client_t));
    pClient->pContext = pContext;
    pClient->appId = appId;
    pClient->isConnected = false;
      // Create event group for synchronization
    pClient->eventGroup = xEventGroupCreate();
    if (pClient->eventGroup == NULL) 
    {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Set default scan parameters
    memcpy(&pClient->scanParams, &g_default_scan_params, sizeof(esp_ble_scan_params_t));
    
    // Set as current client for callbacks
    gpCurrentClient = pClient;
      // Register callbacks
    esp_err_t ret = esp_ble_gattc_register_callback(bat_gattc_event_handler);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to register GATTC callback: %s", esp_err_to_name(ret));
        vEventGroupDelete(pClient->eventGroup);
        pClient->eventGroup = NULL;
        return ret;
    }
    
    ret = esp_ble_gap_register_callback(bat_gattc_gap_event_handler);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
        vEventGroupDelete(pClient->eventGroup);
        pClient->eventGroup = NULL;
        return ret;
    }
    
    // Register app with GATT
    ret = esp_ble_gattc_app_register(appId);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to register GATTC app: %s", esp_err_to_name(ret));
        vEventGroupDelete(pClient->eventGroup);
        pClient->eventGroup = NULL;
        return ret;
    }
    
    // Wait for registration to complete
    EventBits_t bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_REGISTERED_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(timeoutMs));
      if ((bits & BLE_CLIENT_ERROR_BIT) || !(bits & BLE_CLIENT_REGISTERED_BIT)) 
    {
        ESP_LOGE(TAG, "Failed to register GATTC app (timeout or error)");
        
        // Clean up
        vEventGroupDelete(pClient->eventGroup);
        pClient->eventGroup = NULL;
        
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "GATTC client initialized successfully");
    return ESP_OK;
}

esp_err_t bat_gattc_set_scan_params(bat_gattc_client_t *pClient, uint16_t scanInterval, uint16_t scanWindow)
{
    if (pClient == NULL) 
    {
        ESP_LOGE(TAG, "Invalid client pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    pClient->scanParams.scan_interval = scanInterval;
    pClient->scanParams.scan_window = scanWindow;
    
    esp_err_t ret = esp_ble_gap_set_scan_params(&pClient->scanParams);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set scan parameters: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t bat_gattc_start_scan(bat_gattc_client_t *pClient, uint32_t durationSec, 
                              bat_gattc_callbacks_t *pCallbacks, esp_bt_uuid_t *pTargetServiceUuid, 
                              int timeoutMs)
{
    if (pClient == NULL) 
    {
        ESP_LOGE(TAG, "Invalid client pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Reset scan results
    pClient->scanResultCount = 0;
    memset(pClient->scanResults, 0, sizeof(pClient->scanResults));
      // Store callbacks if provided
    if (pCallbacks != NULL) 
    {
        memcpy(&pClient->callbacks, pCallbacks, sizeof(bat_gattc_callbacks_t));
    } 
    else 
    {
        memset(&pClient->callbacks, 0, sizeof(bat_gattc_callbacks_t));
    }
    
    // Store target service UUID if provided
    if (pTargetServiceUuid != NULL) 
    {
        memcpy(&pClient->targetServiceUuid, pTargetServiceUuid, sizeof(esp_bt_uuid_t));
    } 
    else 
    {
        memset(&pClient->targetServiceUuid, 0, sizeof(esp_bt_uuid_t));
    }
    
    // Clear event bits
    bat_gattc_reset_flags(pClient);
    
    // Set scan parameters
    esp_err_t ret = esp_ble_gap_set_scan_params(&pClient->scanParams);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set scan parameters: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Start scan
    ret = esp_ble_gap_start_scanning(durationSec);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t bat_gattc_stop_scan(bat_gattc_client_t *pClient)
{
    if (pClient == NULL) {
        ESP_LOGE(TAG, "Invalid client pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_ble_gap_stop_scanning();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop scan: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for scan stop to complete
    EventBits_t bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_SCAN_DONE_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(1000));
    
    if (bits & BLE_CLIENT_ERROR_BIT) {
        ESP_LOGE(TAG, "Error stopping scan");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t bat_gattc_connect(bat_gattc_client_t *pClient, uint8_t deviceIndex, int timeoutMs)
{
    if (pClient == NULL || deviceIndex >= pClient->scanResultCount) {
        ESP_LOGE(TAG, "Invalid client pointer or device index");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clear event bits
    bat_gattc_reset_flags(pClient);
    
    // Get device info from scan results
    bat_gattc_scan_result_t *device = &pClient->scanResults[deviceIndex];
    ESP_LOGI(TAG, "Connecting to device %s", device->name);
    
    // Open connection
    esp_err_t ret = esp_ble_gattc_open(        pClient->gattcIf,
        device->addr,
        device->addr_type,
        true);  // direct connection
        
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for connection to complete
    EventBits_t bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_CONNECTED_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(timeoutMs));
    
    if ((bits & BLE_CLIENT_ERROR_BIT) || !(bits & BLE_CLIENT_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "Failed to connect (timeout or error)");
        return ESP_FAIL;
    }
    
    // Store remote address
    memcpy(pClient->remoteBda, device->addr, sizeof(esp_bd_addr_t));
    pClient->remoteAddrType = device->addr_type;
    
    return ESP_OK;
}

esp_err_t bat_gattc_connect_by_addr(bat_gattc_client_t *pClient, esp_bd_addr_t addr, 
                                   esp_ble_addr_type_t addrType, int timeoutMs)
{
    if (pClient == NULL || addr == NULL) 
    {
        ESP_LOGE(TAG, "Invalid client pointer or address");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clear event bits
    bat_gattc_reset_flags(pClient);
    
    char addr_str[18];
    sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", 
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            
    ESP_LOGI(TAG, "Connecting to device with address %s", addr_str);
    
    // Open connection
    esp_err_t ret = esp_ble_gattc_open(
        pClient->gattcIf,
        addr,
        addrType,
        true);  // direct connection
          if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for connection to complete
    EventBits_t bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_CONNECTED_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(timeoutMs));
    
    if ((bits & BLE_CLIENT_ERROR_BIT) || !(bits & BLE_CLIENT_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "Failed to connect (timeout or error)");
        return ESP_FAIL;
    }
    
    // Store remote address
    memcpy(pClient->remoteBda, addr, sizeof(esp_bd_addr_t));
    pClient->remoteAddrType = addrType;
    
    return ESP_OK;
}

esp_err_t bat_gattc_disconnect(bat_gattc_client_t *pClient)
{
    if (pClient == NULL || !pClient->isConnected) 
    {
        ESP_LOGE(TAG, "Invalid client pointer or not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear event bits
    bat_gattc_reset_flags(pClient);
      esp_err_t ret = esp_ble_gattc_close(pClient->gattcIf, pClient->connId);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for disconnection to complete
    EventBits_t bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_DISCONNECTED_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(1000));    
    if (bits & BLE_CLIENT_ERROR_BIT) 
    {
        ESP_LOGE(TAG, "Error disconnecting");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t bat_gattc_search_service(bat_gattc_client_t *pClient, esp_bt_uuid_t serviceUuid, int timeoutMs)
{
    if (pClient == NULL || !pClient->isConnected) 
    {
        ESP_LOGE(TAG, "Invalid client pointer or not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Store the service UUID we're looking for
    memcpy(&pClient->targetServiceUuid, &serviceUuid, sizeof(esp_bt_uuid_t));
    
    // Reset service handles
    pClient->serviceStartHandle = 0;
    pClient->serviceEndHandle = 0;
    
    // Clear event bits
    bat_gattc_reset_flags(pClient);
      esp_err_t ret = esp_ble_gattc_search_service(pClient->gattcIf, pClient->connId, &serviceUuid);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to start service search: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for service discovery to complete
    EventBits_t bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_SERVICE_FOUND_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(timeoutMs));
      if ((bits & BLE_CLIENT_ERROR_BIT) || !(bits & BLE_CLIENT_SERVICE_FOUND_BIT)) 
    {
        ESP_LOGE(TAG, "Service discovery failed or target service not found");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Service found with handles 0x%04X - 0x%04X", 
             pClient->serviceStartHandle, pClient->serviceEndHandle);
    
    return ESP_OK;
}

esp_err_t bat_gattc_get_characteristics(bat_gattc_client_t *pClient, esp_bt_uuid_t *pCharUuids, 
                                       uint8_t numChars, int timeoutMs)
{
    if (pClient == NULL || !pClient->isConnected || numChars > BAT_MAX_CHARACTERISTICS || 
        pClient->serviceStartHandle == 0 || pClient->serviceEndHandle == 0) 
    {
        ESP_LOGE(TAG, "Invalid client state for characteristic discovery");
        return ESP_ERR_INVALID_STATE;
    }

    if (pCharUuids == NULL && numChars > 0) 
    {
        ESP_LOGE(TAG, "Invalid characteristic UUIDs array");
        return ESP_ERR_INVALID_ARG;
    }
      // Store the characteristic UUIDs we're looking for
    for (int i = 0; i < numChars; i++) 
    {
        memcpy(&pClient->targetCharUuids[i], &pCharUuids[i], sizeof(esp_bt_uuid_t));
    }
    
    // Clear the characteristics array
    memset(pClient->chars, 0, sizeof(pClient->chars));
    pClient->charCount = 0;
      // For each target characteristic UUID, find all matching characteristics
    for (int i = 0; i < numChars; i++) 
    {
        esp_bt_uuid_t *charUuid = &pClient->targetCharUuids[i];
        
        char uuid_str[37];
        bat_uuid_to_string(charUuid, uuid_str, sizeof(uuid_str));
        ESP_LOGI(TAG, "Looking for characteristic with UUID %s", uuid_str);
        
        uint16_t char_count = 1; // We want to find just one instance per UUID
        esp_gattc_char_elem_t char_elem;
        
        esp_gatt_status_t status = esp_ble_gattc_get_all_char(
            pClient->gattcIf,
            pClient->connId,
            pClient->serviceStartHandle,
            pClient->serviceEndHandle,
            &char_elem,
            &char_count,
            0); // offset
              if (status == ESP_GATT_OK && char_count > 0) 
        {
            // Now check each characteristic to find our target
            for (int j = 0; j < char_count; j++) 
            {
                if (bat_uuid_equal(charUuid, &char_elem.uuid)) 
                {
                    // Found a match
                    ESP_LOGI(TAG, "Found characteristic with handle 0x%04X", char_elem.char_handle);
                    
                    // Store the characteristic
                    if (pClient->charCount < BAT_MAX_CHARACTERISTICS) 
                    {
                        memcpy(&pClient->chars[pClient->charCount], &char_elem, sizeof(esp_gattc_char_elem_t));
                          // Call callback if registered
                        if (pClient->callbacks.onCharFound != NULL) 
                        {
                            // Create a custom parameter structure since we can't easily populate the get_char field
                            // Just pass the char_elem directly to the callback
                            esp_ble_gattc_cb_param_t param = {0};
                            
                            pClient->callbacks.onCharFound(pClient, &param, &char_elem);
                        }
                          pClient->charCount++;
                    } 
                    else 
                    {
                        ESP_LOGW(TAG, "Characteristic buffer full, ignoring additional characteristics");
                    }
                }
            }        } 
        else 
        {
            ESP_LOGW(TAG, "No characteristics found for UUID %s, status %d", uuid_str, status);
        }
    }
      if (pClient->charCount > 0) 
    {
        // Set event bit to indicate success
        xEventGroupSetBits(pClient->eventGroup, BLE_CLIENT_CHAR_FOUND_BIT);
        return ESP_OK;
    } 
    else 
    {
        ESP_LOGE(TAG, "No target characteristics found");
        return ESP_FAIL;
    }
}

esp_err_t bat_gattc_get_descriptor(bat_gattc_client_t *pClient, uint8_t charIndex, int timeoutMs)
{
    if (pClient == NULL || !pClient->isConnected || charIndex >= pClient->charCount) 
    {
        ESP_LOGE(TAG, "Invalid client state or characteristic index");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_gattc_char_elem_t *pChar = &pClient->chars[charIndex];
    
    // Look specifically for the CCCD (0x2902)
    esp_bt_uuid_t descr_uuid;
    descr_uuid.len = ESP_UUID_LEN_16;
    descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
    
    ESP_LOGI(TAG, "Looking for CCCD for characteristic handle 0x%04X", pChar->char_handle);
    
    uint16_t descr_count = 1; // We want to find just the CCCD
    esp_gattc_descr_elem_t descr_elem;
    
    esp_gatt_status_t status = esp_ble_gattc_get_all_descr(
        pClient->gattcIf,
        pClient->connId,
        pChar->char_handle,
        &descr_elem,
        &descr_count,
        0); // offset
          if (status == ESP_GATT_OK && descr_count > 0) 
    {
        ESP_LOGI(TAG, "Found descriptor with handle 0x%04X", descr_elem.handle);
        
        // Store the descriptor handle
        pClient->cccdHandles[charIndex] = descr_elem.handle;
          // Call callback if registered
        if (pClient->callbacks.onDescrFound != NULL) 
        {
            // Create a custom parameter structure - we'll pass descriptor info directly
            esp_ble_gattc_cb_param_t param = {0};
            
            pClient->callbacks.onDescrFound(pClient, &param, &descr_elem);
        }
        
        // Set event bit to indicate success
        xEventGroupSetBits(pClient->eventGroup, BLE_CLIENT_DESC_FOUND_BIT);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "CCCD not found, status %d", status);
        return ESP_FAIL;
    }
}

esp_err_t bat_gattc_read_char(bat_gattc_client_t *pClient, uint8_t charIndex, int timeoutMs)
{
    if (pClient == NULL || !pClient->isConnected || charIndex >= pClient->charCount) {
        ESP_LOGE(TAG, "Invalid client state or characteristic index");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_gattc_char_elem_t *pChar = &pClient->chars[charIndex];
    
    // Clear event bits
    bat_gattc_reset_flags(pClient);
    
    ESP_LOGI(TAG, "Reading characteristic with handle 0x%04X", pChar->char_handle);
    
    esp_err_t ret = esp_ble_gattc_read_char(
        pClient->gattcIf,
        pClient->connId,
        pChar->char_handle,
        ESP_GATT_AUTH_REQ_NONE);
        
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read characteristic: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for read to complete
    EventBits_t bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_READ_DONE_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(timeoutMs));
    
    if ((bits & BLE_CLIENT_ERROR_BIT) || !(bits & BLE_CLIENT_READ_DONE_BIT)) {
        ESP_LOGE(TAG, "Failed to read characteristic (timeout or error)");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t bat_gattc_write_char(bat_gattc_client_t *pClient, uint8_t charIndex, 
                              uint8_t *pValue, uint16_t length, 
                              esp_gatt_write_type_t writeType, int timeoutMs)
{
    if (pClient == NULL || !pClient->isConnected || charIndex >= pClient->charCount) {
        ESP_LOGE(TAG, "Invalid client state or characteristic index");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (pValue == NULL && length > 0) {
        ESP_LOGE(TAG, "Invalid value pointer for non-zero length");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_gattc_char_elem_t *pChar = &pClient->chars[charIndex];
    
    // Clear event bits
    bat_gattc_reset_flags(pClient);
    
    ESP_LOGI(TAG, "Writing to characteristic with handle 0x%04X, length %d", 
             pChar->char_handle, length);
    
    esp_err_t ret = esp_ble_gattc_write_char(
        pClient->gattcIf,
        pClient->connId,
        pChar->char_handle,
        length,
        pValue,
        writeType,
        ESP_GATT_AUTH_REQ_NONE);
        
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write characteristic: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // If using write with response, wait for it
    if (writeType == ESP_GATT_WRITE_TYPE_RSP || writeType == ESP_GATT_WRITE_TYPE_NO_RSP) {
        EventBits_t bits = xEventGroupWaitBits(
            pClient->eventGroup,
            BLE_CLIENT_WRITE_DONE_BIT | BLE_CLIENT_ERROR_BIT,
            pdTRUE,  // Clear bits on exit
            pdFALSE, // Wait for any bit
            pdMS_TO_TICKS(timeoutMs));
        
        if ((bits & BLE_CLIENT_ERROR_BIT) || !(bits & BLE_CLIENT_WRITE_DONE_BIT)) {
            ESP_LOGE(TAG, "Failed to write characteristic (timeout or error)");
            return ESP_FAIL;
        }
    }
    
    return ESP_OK;
}

esp_err_t bat_gattc_register_for_notify(bat_gattc_client_t *pClient, uint8_t charIndex,
                                       bool enableNotifications, bool enableIndications,
                                       int timeoutMs)
{
    if (pClient == NULL || !pClient->isConnected || charIndex >= pClient->charCount) {
        ESP_LOGE(TAG, "Invalid client state or characteristic index");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_gattc_char_elem_t *pChar = &pClient->chars[charIndex];
    uint16_t cccdHandle = pClient->cccdHandles[charIndex];
    
    // If we don't have the CCCD handle, try to find it
    if (cccdHandle == 0) {
        ESP_LOGI(TAG, "CCCD handle not found, attempting to discover it");
        esp_err_t ret = bat_gattc_get_descriptor(pClient, charIndex, timeoutMs);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to find CCCD handle");
            return ret;
        }
        cccdHandle = pClient->cccdHandles[charIndex];
    }
    
    // Clear event bits
    bat_gattc_reset_flags(pClient);
    
    // First, register with the stack
    esp_err_t ret = esp_ble_gattc_register_for_notify(
        pClient->gattcIf,
        pClient->remoteBda,
        pChar->char_handle);
        
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register for notifications: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for registration to complete
    EventBits_t bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_NOTIFY_REG_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(timeoutMs));
    
    if ((bits & BLE_CLIENT_ERROR_BIT) || !(bits & BLE_CLIENT_NOTIFY_REG_BIT)) {
        ESP_LOGE(TAG, "Failed to register for notifications (timeout or error)");
        return ESP_FAIL;
    }
    
    // Now write to the CCCD to enable notifications/indications
    uint16_t cccdValue = 0;
    if (enableNotifications) {
        cccdValue |= 0x0001;  // Enable notifications
    }
    if (enableIndications) {
        cccdValue |= 0x0002;  // Enable indications
    }
    
    ESP_LOGI(TAG, "Writing CCCD value 0x%04X to handle 0x%04X", cccdValue, cccdHandle);
    
    // Use a buffer to handle the 16-bit value in little-endian format
    uint8_t cccdBuffer[2];
    cccdBuffer[0] = cccdValue & 0xFF;
    cccdBuffer[1] = (cccdValue >> 8) & 0xFF;
    
    // Write to the CCCD
    ret = esp_ble_gattc_write_char_descr(
        pClient->gattcIf,
        pClient->connId,
        cccdHandle,
        sizeof(cccdBuffer),
        cccdBuffer,
        ESP_GATT_WRITE_TYPE_RSP,
        ESP_GATT_AUTH_REQ_NONE);
        
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to CCCD: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for write to complete
    bits = xEventGroupWaitBits(
        pClient->eventGroup,
        BLE_CLIENT_DESC_WRITE_DONE_BIT | BLE_CLIENT_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(timeoutMs));
    
    if ((bits & BLE_CLIENT_ERROR_BIT) || !(bits & BLE_CLIENT_DESC_WRITE_DONE_BIT)) {
        ESP_LOGE(TAG, "Failed to write to CCCD (timeout or error)");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Successfully %s notifications/indications",
             (cccdValue > 0) ? "enabled" : "disabled");
    
    return ESP_OK;
}

esp_err_t bat_gattc_deinit(bat_gattc_client_t *pClient)
{
    if (pClient == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // If connected, disconnect first
    if (pClient->isConnected) {
        bat_gattc_disconnect(pClient);
    }
    
    // Unregister app
    if (pClient->gattcIf != ESP_GATT_IF_NONE) {
        esp_err_t ret = esp_ble_gattc_app_unregister(pClient->gattcIf);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to unregister GATTC app: %s", esp_err_to_name(ret));
            // Continue with cleanup anyway
        }
    }
    
    // Clean up event group
    if (pClient->eventGroup != NULL) {
        vEventGroupDelete(pClient->eventGroup);
        pClient->eventGroup = NULL;
    }
    
    // Clear global pointer if it's our client
    if (gpCurrentClient == pClient) {
        gpCurrentClient = NULL;
    }
    
    return ESP_OK;
}

void bat_gattc_reset_flags(bat_gattc_client_t *pClient)
{
    if (pClient != NULL && pClient->eventGroup != NULL) {
        xEventGroupClearBits(pClient->eventGroup, 0x00FFFFFF);
    }
}

esp_err_t bat_gattc_get_device_name(bat_gattc_client_t *pClient, uint8_t deviceIndex, 
                                   char *nameBuffer, size_t bufferSize)
{
    if (pClient == NULL || deviceIndex >= pClient->scanResultCount || 
        nameBuffer == NULL || bufferSize == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy device name from scan results
    strncpy(nameBuffer, pClient->scanResults[deviceIndex].name, bufferSize - 1);
    nameBuffer[bufferSize - 1] = '\0';
    
    return ESP_OK;
}
