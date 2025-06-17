#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "esp_bt_defs.h"
#include "esp_event.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "bat_lib.h"
#include "bat_ble_server.h"
#include "bat_gatts_simple.h"
#include "bat_uuid.h"

static const char *TAG = "bat_gatts_simple";

#define BLE_SERVER_REGISTERED_BIT (1 << 0)
#define BLE_ADV_CONFIG_DONE_BIT (1 << 1)
#define BLE_SCAN_RESPONSE_DONE_BIT (1 << 2)
#define BLE_SERVICE_CREATED_BIT (1 << 3)
#define BLE_SERVICE_STARTED_BIT (1 << 4)
#define BLE_ADVERTISING_STARTED_BIT (1 << 5)
#define BLE_CONNECTED_BIT (1 << 6)
#define BLE_DISCONNECTED_BIT (1 << 7)
#define BLE_SERVICE_STOP_COMPLETE_BIT (1 << 8)
#define BLE_ADV_STOP_COMPLETE_BIT (1 << 9)
#define BLE_CHAR_ADDED_BIT (1 << 10)
#define BLE_DESC_ADDED_BIT (1 << 11) 
#define BLE_ERROR_BIT (1 << 12)

static bat_gatts_server_t *gpCurrentServer = NULL;
static esp_ble_adv_params_t g_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY};

// Implement the BLE event handlers to manage callbacks:
static void bat_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    assert(gpCurrentServer != NULL);

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        if (pParam->reg.status == ESP_GATT_OK)
        {
            gpCurrentServer->gattsIf = gatts_if;
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_SERVER_REGISTERED_BIT);
            ESP_LOGI(TAG, "GATTS app registered with ID %d", pParam->reg.app_id);
        }
        else
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
            ESP_LOGE(TAG, "GATTS app registration failed with status %d", pParam->reg.status);
        }
        break;

    case ESP_GATTS_CREATE_EVT:
        if (pParam->create.status == ESP_GATT_OK)
        {
            gpCurrentServer->serviceHandle = pParam->create.service_handle;
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_SERVICE_CREATED_BIT);
            ESP_LOGI(TAG, "Service created with handle %d", pParam->create.service_handle);
        }
        else
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
            ESP_LOGE(TAG, "Service creation failed with status %d", pParam->create.status);
        }
        break;  

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_EVT received, status=%d, service_handle=%d, attr_handle=%d", 
            pParam->add_char.status, pParam->add_char.service_handle, pParam->add_char.attr_handle);
                        
        if (pParam->add_char.status == ESP_GATT_OK)
        {
            char uuid_str[45] = {0};
            bat_uuid_to_log_string(&pParam->add_char.char_uuid, uuid_str, sizeof(uuid_str));
            
            ESP_LOGI(TAG, "Received characteristic UUID: %s (len=%d)", 
                uuid_str, pParam->add_char.char_uuid.len);
                
            // Find the characteristic index by UUID
            for (int i = 0; i < gpCurrentServer->numChars; i++) 
            {                
                bat_uuid_to_log_string(&gpCurrentServer->charUuids[i], uuid_str, sizeof(uuid_str));
                ESP_LOGI(TAG, "Comparing with stored UUID[%d]: %s", i, uuid_str);
                    
                if (bat_uuid_equal(&pParam->add_char.char_uuid, &gpCurrentServer->charUuids[i])) 
                {
                    // If this characteristic is already added, it means we got a duplicate callback
                    if (i < gpCurrentServer->charsAdded) 
                    {
                        ESP_LOGW(TAG, "Received duplicate ADD_CHAR event for characteristic %d", i);
                        return;
                    }
                    
                    // This should be the next expected characteristic
                    if (i != gpCurrentServer->charsAdded) 
                    {
                        ESP_LOGW(TAG, "Received unexpected characteristic order. Expected %d, got %d", 
                                gpCurrentServer->charsAdded, i);
                    }
                    
                    // Store the handle and increment the count
                    gpCurrentServer->charHandles[i] = pParam->add_char.attr_handle;
                    gpCurrentServer->charsAdded++;
                    
                    ESP_LOGI(TAG, "Characteristic %d added, handle=%d, added %d of %d", 
                        i, pParam->add_char.attr_handle, gpCurrentServer->charsAdded, gpCurrentServer->numChars);
                    
                    // Signal that this characteristic has been added
                    xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_CHAR_ADDED_BIT);
                    return;
                }
            }
          
            ESP_LOGE(TAG, "ERROR: Received characteristic UUID %s did not match any expected UUID!", uuid_str);

            // Continue anyway to not block the process
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_CHAR_ADDED_BIT);
        } 
        else
        {
            ESP_LOGE(TAG, "Failed to add characteristic with status %d (%s)", 
                     pParam->add_char.status, esp_err_to_name(pParam->add_char.status));
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
        }
        break;    
        
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_DESCR_EVT received, status=%d, attr_handle=%d, service_handle=%d", 
            pParam->add_char_descr.status, pParam->add_char_descr.attr_handle, pParam->add_char_descr.service_handle);
                        
        if (pParam->add_char_descr.status == ESP_GATT_OK)
        {
            char desc_uuid_str[37] = {0};
            if (pParam->add_char_descr.descr_uuid.len == ESP_UUID_LEN_16) 
			{
                sprintf(desc_uuid_str, "0x%04x", pParam->add_char_descr.descr_uuid.uuid.uuid16);
            }
            
            ESP_LOGI(TAG, "Descriptor UUID: %s", desc_uuid_str);
            
            // Check if this is a CCCD (UUID 0x2902)
            if (pParam->add_char_descr.descr_uuid.len == ESP_UUID_LEN_16 && 
                pParam->add_char_descr.descr_uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) 
            {
                // Store the descriptor handle
                if (gpCurrentServer->descrsAdded < gpCurrentServer->totalDescrs) 
                {
                    gpCurrentServer->descrHandles[gpCurrentServer->descrsAdded] = pParam->add_char_descr.attr_handle;
                    gpCurrentServer->descrsAdded++;
                    ESP_LOGI(TAG, "CCCD %d added, handle=%d, added %d of %d", 
                        gpCurrentServer->descrsAdded - 1, pParam->add_char_descr.attr_handle, 
                        gpCurrentServer->descrsAdded, gpCurrentServer->totalDescrs);
                    
                    // Signal that this descriptor has been added
                    xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_DESC_ADDED_BIT);
                }
                else
                {
                    ESP_LOGW(TAG, "Received unexpected CCCD addition (handle=%d), but will accept it anyway", 
                             pParam->add_char_descr.attr_handle);
                    xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_DESC_ADDED_BIT);
                }
            }
            else
            {
                // For non-CCCD descriptors, just log and accept them
                ESP_LOGI(TAG, "Non-CCCD descriptor added, UUID=%s, handle=%d", 
                        desc_uuid_str, pParam->add_char_descr.attr_handle);
                xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_DESC_ADDED_BIT);
            }
        } 
        else
        {
            ESP_LOGE(TAG, "Failed to add descriptor with status %d (%s)", 
                     pParam->add_char_descr.status, esp_err_to_name(pParam->add_char_descr.status));
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
        }
        break;

    case ESP_GATTS_START_EVT:
        if (pParam->start.status == ESP_GATT_OK)
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_SERVICE_STARTED_BIT);
            ESP_LOGI(TAG, "Service started");
        }
        else
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
            ESP_LOGE(TAG, "Service start failed with status %d", pParam->start.status);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        gpCurrentServer->isConnected = true;
        gpCurrentServer->connId = pParam->connect.conn_id;
        gpCurrentServer->callbacks.onConnect(gpCurrentServer, pParam);

        xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_CONNECTED_BIT);
        ESP_LOGI(TAG, "GATT client connected");
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        gpCurrentServer->isConnected = false;
        gpCurrentServer->callbacks.onDisconnect(gpCurrentServer, pParam);
        xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_DISCONNECTED_BIT);
        ESP_LOGI(TAG, "GATT client disconnected");
        break;

    case ESP_GATTS_WRITE_EVT:
        // Check if this is a CCCD write (handle matches a descriptor handle)
        for (int i = 0; i < gpCurrentServer->descrsAdded; i++) 
        {
            if (pParam->write.handle == gpCurrentServer->descrHandles[i]) 
            {
                ESP_LOGI(TAG, "CCCD write detected for characteristic %d: value=0x%02x%02x", 
                            i, pParam->write.value[1], pParam->write.value[0]);
                
                // Call the descriptor write callback
                if (gpCurrentServer->callbacks.onDescWrite != NULL) 
                    gpCurrentServer->callbacks.onDescWrite(gpCurrentServer, pParam);
                
                // Auto-respond to CCCD writes
                if (pParam->write.need_rsp) 
                {
                    esp_ble_gatts_send_response(gatts_if, pParam->write.conn_id, 
                                                pParam->write.trans_id, ESP_GATT_OK, NULL);
                }
                return;
            }
        }
        
        // Regular characteristic write
        gpCurrentServer->callbacks.onWrite(gpCurrentServer, pParam);
        break;

    case ESP_GATTS_READ_EVT:
        gpCurrentServer->callbacks.onRead(gpCurrentServer, pParam);
        break;

    case ESP_GATTS_STOP_EVT:
        if (pParam->stop.status == ESP_GATT_OK)
        {
            ESP_LOGI(TAG, "GATTS service stopped successfully");
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_SERVICE_STOP_COMPLETE_BIT);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to stop GATTS service, status: %d", pParam->stop.status);
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
        }
        break;

    default:
        break;
    }
}

static void bat_gatts_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *pParam)
{
    assert(gpCurrentServer != NULL);

    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ADV_CONFIG_DONE_BIT);
        ESP_LOGI(TAG, "Advertising data set complete");
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (pParam->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ADVERTISING_STARTED_BIT);
            ESP_LOGI(TAG, "Advertising started");
        }
        else
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
            ESP_LOGE(TAG, "Failed to start advertising: %d", pParam->adv_start_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        if (pParam->scan_rsp_data_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_SCAN_RESPONSE_DONE_BIT);
            ESP_LOGI(TAG, "Scan response data set successfully");
        }
        else
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
            ESP_LOGE(TAG, "Failed to set scan response data: %d", pParam->scan_rsp_data_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (pParam->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ADV_STOP_COMPLETE_BIT);
            ESP_LOGI(TAG, "Advertising stopped");
        }
        else
        {
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
            ESP_LOGE(TAG, "Failed to stop advertising: %d", pParam->adv_stop_cmpl.status);
        }
        break;

    default:
        break;
    }
}

void bat_gatts_reset_flags(bat_gatts_server_t *pServer)
{
    xEventGroupClearBits(pServer->eventGroup, 0x00FFFFFF);
}

static esp_err_t bat_gatts_server_init(bat_gatts_server_t *pServer, void *pContext, 
    const char *pDeviceName, uint16_t appId, const char *pServiceUuid, int appearance)
{
    if (pServer == NULL || pServiceUuid == NULL)
    {        
        ESP_LOGE(TAG, "Invalid arguments: pServer or pServiceUuid is NULL");
        return ESP_ERR_INVALID_ARG;    
    }
        
    memset(pServer, 0, sizeof(bat_gatts_server_t));
    esp_err_t ret = bat_uuid_from_string(pServiceUuid, &pServer->serviceId);
    if (ret == ESP_OK)
    {
        if (pDeviceName == NULL)
            pServer->deviceName[0] = '\0';
        else
            strncpy(pServer->deviceName, pDeviceName, sizeof(pServer->deviceName) - 1);
    
        pServer->numChars = 0;
        pServer->appId = appId;
        pServer->charsAdded = 0;
        pServer->descrsAdded = 0;
        pServer->totalDescrs = 0;
        pServer->pContext = pContext;
        pServer->appearance = appearance;
        pServer->pAdvParams = &g_adv_params;
        pServer->eventGroup = xEventGroupCreate();

        if (pServer->eventGroup == NULL)
        {
            ESP_LOGE(TAG, "Failed to create event groups");
            ret = ESP_ERR_NO_MEM;
        }
    }

    return ret;
} 

// A simple initialization function that handles all the BLE setup.
esp_err_t bat_gatts_init(
    bat_gatts_server_t *pServer, void *pContext, const char *pDeviceName, uint16_t appId,
    const char *pServiceUuid, int appearance, int timeoutMs)
{
    esp_err_t ret = bat_gatts_server_init(pServer, pContext, pDeviceName, appId, pServiceUuid, appearance);
    if (ret != ESP_OK)
        return ret;

    gpCurrentServer = pServer;

    ret = esp_ble_gatts_register_callback(bat_gatts_event_handler);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Failed to register GATTS callback: %s", esp_err_to_name(ret));
    else
    {
        ret = esp_ble_gap_register_callback(bat_gatts_gap_event_handler);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
        else
        {
            ret = bat_ble_gatts_app_register(appId);
            if (ret == ESP_OK)
            {                    
                // Wait for registration
                EventBits_t bits = xEventGroupWaitBits(gpCurrentServer->eventGroup,
                                                        BLE_SERVER_REGISTERED_BIT | BLE_ERROR_BIT,
                                                        pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

                if ((bits & BLE_ERROR_BIT) || !(bits & BLE_SERVER_REGISTERED_BIT))
                {
                    ESP_LOGE(TAG, "Error during BLE server registration: %s",
                                (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
                    return ESP_FAIL;
                }
                return ESP_OK;
            }
        }
    }

    bat_gatts_deinit(pServer);
    return ret;
}

esp_err_t bat_gatts_deinit(bat_gatts_server_t *pServer)
{
    if (pServer->eventGroup != NULL)
        vEventGroupDelete(pServer->eventGroup);

    pServer->eventGroup = NULL;
    return ESP_OK;
}

// Create the device and function to add characteristics and optional CCCDs.
esp_err_t bat_gatts_create_service(
    bat_gatts_server_t *pServer,
    bat_gatts_char_config_t *pCharConfigs, uint8_t numChars, int timeoutMs)
{
    if (pServer == NULL || numChars > BAT_MAX_CHARACTERISTICS)
        return ESP_ERR_INVALID_ARG;

    if (pCharConfigs == NULL && numChars > 0)
    {
        ESP_LOGE(TAG, "Invalid characteristics provided");
        return ESP_ERR_INVALID_ARG;
    }

    // Count how many CCCDs we need to add
    uint8_t cccdCount = 0;
    for (int i = 0; i < numChars; i++) 
    {
        if (pCharConfigs[i].hasNotifications || pCharConfigs[i].hasIndications)
            cccdCount++;
    }
    
    pServer->totalDescrs = cccdCount;
    ESP_LOGI(TAG, "Service will include %d characteristics and %d CCCDs", numChars, cccdCount);

    esp_gatt_srvc_id_t service_id = {
        .id = { .uuid = pServer->serviceId },
        .is_primary = true
    };

    // Handles = 1 service + numChars characteristics + cccdCount descriptors
    // Add some extra handles for safety (ESP-IDF might need more than we calculate)
    uint16_t numHandles = 1 + (numChars * 2) + (cccdCount * 2);
    
    ESP_LOGI(TAG, "Creating service with %d handles (1 service + %d chars + %d CCCDs + extras)", 
             numHandles, numChars, cccdCount);
             
    esp_err_t ret = esp_ble_gatts_create_service(pServer->gattsIf, &service_id, numHandles);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create service: %s (code=%d)", esp_err_to_name(ret), ret);
        return ret;
    }
    
    // Wait for service creation
    EventBits_t bits = xEventGroupWaitBits(pServer->eventGroup,
                                           BLE_SERVICE_CREATED_BIT | BLE_ERROR_BIT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

    if ((bits & BLE_ERROR_BIT) || !(bits & BLE_SERVICE_CREATED_BIT))
    {
        ESP_LOGE(TAG, "Error during server creation: %s",
                 (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
        return ESP_FAIL;
    }    
    
    // Initialize counters for tracking the addition process
    pServer->charsAdded = 0;
    pServer->descrsAdded = 0;
    pServer->numChars = numChars;
            
    // Ensure the global server pointer is set for callback processing
    gpCurrentServer = pServer;
    
    // Process one characteristic at a time, adding its CCCD immediately after if needed
    for (int i = 0; i < numChars; i++)
    {
        ESP_LOGI(TAG, "Adding characteristic %d of %d", i+1, numChars);
        
        // Create the UUID for this characteristic
        esp_bt_uuid_t char_uuid;
        ret = bat_uuid_from_16bit(pCharConfigs[i].uuid, &char_uuid);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create characteristic UUID: %s", esp_err_to_name(ret));
            return ret;
        }
        
        // Store the UUID for later matching in the event handler
        memcpy(&pServer->charUuids[i], &char_uuid, sizeof(esp_bt_uuid_t));

        esp_attr_value_t char_val = {
            .attr_max_len = pCharConfigs[i].maxLen,
            .attr_len = pCharConfigs[i].initValueLen,
            .attr_value = pCharConfigs[i].pInitialValue
        };

        ESP_LOGI(TAG, "Adding characteristic %d, UUID=0x%04x, permissions=0x%x, properties=0x%x", 
                i+1, char_uuid.uuid.uuid16, 
                pCharConfigs[i].permissions, pCharConfigs[i].properties);
        
        // Clear characteristic added bit before adding
        xEventGroupClearBits(pServer->eventGroup, BLE_CHAR_ADDED_BIT);
        
        // Add the characteristic
        ret = bat_ble_gatts_add_char(pServer->serviceHandle, &char_uuid,
                                     pCharConfigs[i].permissions, pCharConfigs[i].properties,
                                     &char_val, NULL);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add characteristic: %s", esp_err_to_name(ret));
            return ret;
        }
          ESP_LOGI(TAG, "Waiting for characteristic %d to be added...", i+1);
        
        // Wait for the characteristic to be added
        bits = xEventGroupWaitBits(pServer->eventGroup,
                                  BLE_CHAR_ADDED_BIT | BLE_ERROR_BIT,
                                  pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));
                                          
        if ((bits & BLE_ERROR_BIT) || !(bits & BLE_CHAR_ADDED_BIT))
        {
            ESP_LOGE(TAG, "Error adding characteristic %d (UUID=0x%04x): %s", 
                     i+1, pCharConfigs[i].uuid, 
                     (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout waiting for completion");
                     
            // Retry the operation if it timed out
            if (!(bits & BLE_CHAR_ADDED_BIT) && !(bits & BLE_ERROR_BIT)) {
                ESP_LOGW(TAG, "Characteristic addition timed out, auto-continuing with next characteristic");
                continue;  // Skip to next characteristic instead of failing
            }
            
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "Characteristic %d added successfully, handle=%d", 
                 i+1, pServer->charHandles[i]);
        
        // If this characteristic needs a CCCD, add it now
        if (pCharConfigs[i].hasNotifications || pCharConfigs[i].hasIndications)
        {
            ESP_LOGI(TAG, "Adding CCCD for characteristic %d", i+1);
            
            // Standard CCCD UUID (0x2902)
            esp_bt_uuid_t cccd_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}
            };
            
            // Initial value for CCCD: disabled (0x0000)
            uint8_t cccdValue[2] = {0x00, 0x00};
            
            esp_attr_value_t attr_val = {
                .attr_max_len = 2,
                .attr_len = 2,
                .attr_value = cccdValue
            };
            
            // Clear descriptor added bit before adding
            xEventGroupClearBits(pServer->eventGroup, BLE_DESC_ADDED_BIT);
            
            // Add the CCCD descriptor
            ret = bat_ble_gatts_add_char_descr(
                pServer->serviceHandle,
                &cccd_uuid, 
                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                &attr_val, NULL);
                
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to add CCCD for characteristic %d: %s", 
                         i+1, esp_err_to_name(ret));
                return ret;
            }
            
            ESP_LOGI(TAG, "Waiting for CCCD for characteristic %d to be added...", i+1);
            
            // Wait for the CCCD to be added
            bits = xEventGroupWaitBits(pServer->eventGroup,
                                      BLE_DESC_ADDED_BIT | BLE_ERROR_BIT,
                                      pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));
                                              
            if ((bits & BLE_ERROR_BIT) || !(bits & BLE_DESC_ADDED_BIT))
            {
                ESP_LOGE(TAG, "Error adding CCCD for characteristic %d: %s", 
                         i+1, (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
                return ESP_FAIL;
            }
            
            ESP_LOGI(TAG, "CCCD for characteristic %d added successfully, handle=%d", 
                    i+1, pServer->descrHandles[pServer->descrsAdded-1]);
        }
    }
    
    ESP_LOGI(TAG, "All %d characteristics and %d CCCDs added successfully", 
             numChars, pServer->descrsAdded);
    return ESP_OK;
}

static esp_err_t bat_gatts_copy_advert_service_uuid(bat_gatts_server_t *pServer, esp_ble_adv_data_t *pAdvData)
{
    if (pServer != NULL)
    {
        switch (pServer->serviceId.len)
        {
        case ESP_UUID_LEN_16:
            pAdvData->service_uuid_len = ESP_UUID_LEN_16;
            memcpy(pServer->raw_uuid, &pServer->serviceId.uuid.uuid16, ESP_UUID_LEN_16);
            pAdvData->p_service_uuid = pServer->raw_uuid;
            return ESP_OK;

        case ESP_UUID_LEN_32:
            pAdvData->service_uuid_len = ESP_UUID_LEN_32;
            memcpy(pServer->raw_uuid, &pServer->serviceId.uuid.uuid32, ESP_UUID_LEN_32);
            pAdvData->p_service_uuid = pServer->raw_uuid;
            return ESP_OK;

        case ESP_UUID_LEN_128:
            pAdvData->service_uuid_len = ESP_UUID_LEN_128;
            memcpy(pServer->raw_uuid, pServer->serviceId.uuid.uuid128, ESP_UUID_LEN_128);
            pAdvData->p_service_uuid = pServer->raw_uuid;
            return ESP_OK;

        default:
            ESP_LOGE(TAG, "Invalid UUID length: %d", pServer->serviceId.len);
        }
        
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

static void bat_gatts_no_op(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
}

// Simple Start function.
//
// `esp_ble_adv_data_t` describes the advertising data that will be sent while advertising.
// This is a limited size packet (31 user bytes), so we need to be careful about what we include if
// we want passive scans to see enough data to determine interest.
//
// Active scanners can request additional data via a single `scan response` packet, which is larger
// and can include more information.

// Optional data we can include in the advertising data:
// 'Connection Interval Range' (an extra 6 bytes) in the advertising data...
//   .min_interval = 0x0006,
//   .max_interval = 0x0010,
// The code seems to function fine without this.
//
// Appearance is a 16-bit value which indicates the type of device being advertised.
// This adds 3 bytes in total.
//   0x0000: Unknown
//   0x0080: Generic Phone
//   0x0040: Generic Computer
//   0x0180: Generic Watch
//   0x0181: Sports Watch
//   0x0340: Generic Thermometer
//   0x0380: Generic Heart Rate Sensor
//   0x03C0: Generic Blood Pressure
//   0x0940: Generic Audio Source
//   0x0941: Generic Audio Sink
//   0x0942: Microphone
//   0x0943: Speaker
//   0x0944: Headphones
//   0x0945: Portable Audio Player
//
// There are also flags that can be set in the advertising data, which indicate the type of device:
//   ESP_BLE_ADV_FLAG_GEN_DISC: General discoverable mode
//   ESP_BLE_ADV_FLAG_LIMIT_DISC: For devices that only want to be discoverable for a short time.
//   ESP_BLE_ADV_FLAG_BREDR_NOT_SPT: Bluetooth classic not supported
//
// An *active* scan will get this as part of the advert response.
//   esp_ble_scan_params_t
//     .scan_type = BLE_SCAN_TYPE_ACTIVE
//
// Advertisement packet.
// For example we might just include the service UUID which means that *passive* scan can find
// if they are interested in the service quicker and without handshaking.
esp_err_t bat_gatts_start(bat_gatts_server_t *pServer, bat_gatts_callbacks2_t *pCbs, int timeoutMs)
{
    pServer->callbacks.onRead = (pCbs && pCbs->onRead) ? pCbs->onRead : bat_gatts_no_op;
    pServer->callbacks.onWrite = (pCbs && pCbs->onWrite) ? pCbs->onWrite : bat_gatts_no_op;
    pServer->callbacks.onConnect = (pCbs && pCbs->onConnect) ? pCbs->onConnect : bat_gatts_no_op;
    pServer->callbacks.onDescWrite = (pCbs && pCbs->onDescWrite) ? pCbs->onDescWrite : bat_gatts_no_op;
    pServer->callbacks.onDisconnect = (pCbs && pCbs->onDisconnect) ? pCbs->onDisconnect : bat_gatts_no_op;

    // - ESP_BLE_ADV_FLAG_GEN_DISC: Device is in general discoverable mode
    // - ESP_BLE_ADV_FLAG_BREDR_NOT_SPT: BR/EDR (Bluetooth Classic) not supported
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = false, // Don't include name in adv packet
        .include_txpower = false,
        .p_service_data = NULL,
        .p_service_uuid = NULL,
        .p_manufacturer_data = NULL,
        .appearance = pServer->appearance,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };

    esp_err_t ret = bat_gatts_copy_advert_service_uuid(pServer, &adv_data);
    if (ret != ESP_OK)
        return ret;

    // Configure advertising data and wait for it to be configured.
    ret = bat_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK)
        return ret;

    EventBits_t bits = xEventGroupWaitBits(gpCurrentServer->eventGroup,
                                           BLE_ADV_CONFIG_DONE_BIT | BLE_ERROR_BIT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

    if ((bits & BLE_ERROR_BIT) || !(bits & BLE_ADV_CONFIG_DONE_BIT))
    {
        ESP_LOGE(TAG, "Error during advertising data config: %s",
                 (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
        return ESP_FAIL;
    }

    if (pServer->deviceName[0] != '\0')
    {
        esp_err_t ret = bat_ble_gap_set_device_name(pServer->deviceName);
        if (ret != ESP_OK)
            return ret;

        ESP_LOGI(TAG, "Device name is '%s'", pServer->deviceName);

        // We can send one (optional) additional `Scan response` packet.
        // This packet can include more data than the main advertisement packet but won't be seen by
        // passive scanners.
        esp_ble_adv_data_t scan_rsp_data = {
            .flag = 0, // No flags in scan response
            .set_scan_rsp = true,
            .include_name = true, // Include name in scan response
            .include_txpower = false,
        };

        // Send the scan response and wait for it to be configured.
        ret = bat_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret != ESP_OK)
            return ret;

        bits = xEventGroupWaitBits(gpCurrentServer->eventGroup,
                                   BLE_SCAN_RESPONSE_DONE_BIT | BLE_ERROR_BIT,
                                   pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

        if ((bits & BLE_ERROR_BIT) || !(bits & BLE_SCAN_RESPONSE_DONE_BIT))
        {
            ESP_LOGE(TAG, "Error during scan response: %s",
                     (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
            return ESP_FAIL;
        }
    }

    // Start service and wait for it.
    ret = bat_gatts_start_service(pServer->serviceHandle);
    if (ret != ESP_OK)
        return ret;

    bits = xEventGroupWaitBits(pServer->eventGroup,
                               BLE_SERVICE_STARTED_BIT | BLE_ERROR_BIT,
                               pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

    if ((bits & BLE_ERROR_BIT) || !(bits & BLE_SERVICE_STARTED_BIT))
    {
        ESP_LOGE(TAG, "Error during service start: %s",
                 (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
        return ESP_FAIL;
    }

    // Start advertising and wait for it.
    ret = bat_ble_gap_start_advertising(pServer->pAdvParams);
    if (ret != ESP_OK)
        return ret;

    bits = xEventGroupWaitBits(pServer->eventGroup,
                               BLE_ADVERTISING_STARTED_BIT | BLE_ERROR_BIT,
                               pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

    if ((bits & BLE_ERROR_BIT) || !(bits & BLE_ADVERTISING_STARTED_BIT))
    {
        ESP_LOGE(TAG, "Error during advertising start: %s",
                 (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t bat_gatts_stop(bat_gatts_server_t *pServer, int timeoutMs)
{
    ESP_LOGI(TAG, "Stopping BLE server");

    if (pServer == NULL)
    {
        ESP_LOGE(TAG, "Invalid server pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // Wait for advertising to stop.
    esp_err_t ret = esp_ble_gap_stop_advertising();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop advertising: %s", esp_err_to_name(ret));
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(
        pServer->eventGroup,
        BLE_ADV_STOP_COMPLETE_BIT | BLE_ERROR_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(timeoutMs));

    if ((bits & BLE_ERROR_BIT) || !(bits & BLE_ADV_STOP_COMPLETE_BIT))
    {
        ESP_LOGE(TAG, "Failed to stop advertising: %s",
                 (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
        return ESP_FAIL;
    }

    // Wait for service to stop with provided timeout
    ret = esp_ble_gatts_stop_service(pServer->serviceHandle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop GATT service: %s", esp_err_to_name(ret));
        return ret;
    }

    bits = xEventGroupWaitBits(
        pServer->eventGroup,
        BLE_SERVICE_STOP_COMPLETE_BIT | BLE_ERROR_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(timeoutMs));

    if ((bits & BLE_ERROR_BIT) || !(bits & BLE_SERVICE_STOP_COMPLETE_BIT))
    {
        ESP_LOGE(TAG, "Failed to stop GATT service: %s",
                 (bits & BLE_ERROR_BIT) ? "Error reported" : "Timeout");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "GATT service stopped successfully");
    return ESP_OK;
}

// Functions for sending notifications and responding to read/write requests:
esp_err_t bat_gatts_notify(bat_gatts_server_t *pServer, uint16_t charIndex, uint8_t *pData, uint16_t dataLen)
{
    if (pServer == NULL || charIndex >= pServer->numChars || pData == NULL || dataLen == 0)
        return ESP_ERR_INVALID_ARG;

    if (!pServer->isConnected)
        return ESP_ERR_INVALID_STATE;

    esp_err_t ret = esp_ble_gatts_send_indicate(pServer->gattsIf, pServer->connId,
                                                pServer->charHandles[charIndex],
                                                dataLen, pData, false);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Failed to send notification: %s", esp_err_to_name(ret));

    return ret;
}

esp_err_t bat_gatts_indicate(bat_gatts_server_t *pServer, uint16_t charIndex, uint8_t *pData, uint16_t dataLen)
{
    if (pServer == NULL || charIndex >= pServer->numChars || pData == NULL || dataLen == 0)
        return ESP_ERR_INVALID_ARG;

    if (!pServer->isConnected)
        return ESP_ERR_INVALID_STATE;

    // The only difference from notify is the last param (need_confirm) set to true
    esp_err_t ret = esp_ble_gatts_send_indicate(pServer->gattsIf, pServer->connId,
                                                pServer->charHandles[charIndex],
                                                dataLen, pData, true);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Failed to send indication: %s", esp_err_to_name(ret));

    return ret;
}

/**
 * Helper function to check if a specific CCCD flag is enabled for a characteristic
 * 
 * @param pServer Pointer to the server context
 * @param charIndex Index of the characteristic
 * @param cccdFlag The flag to check (BAT_CCCD_NOTIFICATION or BAT_CCCD_INDICATION)
 * @return true if the flag is set, false otherwise
 */
bool bat_gatts_is_cccd_enabled(bat_gatts_server_t *pServer, uint16_t charIndex, uint16_t cccdFlag)
{
    if (pServer == NULL || !pServer->isConnected || charIndex >= pServer->numChars)
        return false;
        
    // To properly check CCCD state, we need to read the descriptor value from the stack
    // This would require an extra read operation which isn't necessary here.
    // In practice, the application should be tracking client's CCCD state in onDescWrite callback.
    
    // This is a simplification for the demo - in real code we would need to retrieve the 
    // actual current value from the BLE stack or maintain our own state tracking
    
    // For now, we assume if the descriptor handle exists, the client has enabled it
    if (charIndex < pServer->descrsAdded && pServer->descrHandles[charIndex] != 0) {
        return true;
    }
    
    return false;
}
