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

static const char *TAG = "bat_gatts_simple";

// Simplifying BLE ESP for Client Code
// To make BLE ESP as simple as possible for client code by implementing a higher-level abstraction
// layer that handles the complex BLE workflow while providing a simplified API. 

// 1. Create a Server Context Structure
// First, create a context structure to maintain server state and configuration:
#define BAT_MAX_CHARACTERISTICS 10
typedef void (*bat_ble_server_cb_t)(void *pContext, esp_ble_gatts_cb_param_t *pParam);

typedef struct
{
  // Server identification
  uint16_t appId;
  char deviceName[32];

  // Service information
  uint16_t serviceHandle;
  uint16_t charHandles[BAT_MAX_CHARACTERISTICS];
  uint8_t numChars;

  // Connection state
  bool isConnected;
  uint16_t connId;
  esp_gatt_if_t gattsIf;

  // Event handling
  EventGroupHandle_t eventGroup;

  // Callback functions
  struct
  {
    void *pContext;
    void (*onConnect)(void *pContext, esp_ble_gatts_cb_param_t *param);
    void (*onDisconnect)(void *pContext, esp_ble_gatts_cb_param_t *param);
    void (*onWrite)(void *pContext, esp_ble_gatts_cb_param_t *param);
    void (*onRead)(void *pContext, esp_ble_gatts_cb_param_t *param);
  } callbacks;
} bat_ble_server_t;

// 2. Create an Easy Initialization Function
// Create a simple initialization function that handles all the BLE setup:

// Define common event bits
#define BLE_SERVER_REGISTERED_BIT      (1 << 0)
#define BLE_ADV_CONFIG_DONE_BIT        (1 << 1)
#define BLE_SERVICE_CREATED_BIT        (1 << 2)
#define BLE_SERVICE_STARTED_BIT        (1 << 3)
#define BLE_ADVERTISING_STARTED_BIT    (1 << 4)
#define BLE_CONNECTED_BIT              (1 << 5)
#define BLE_DISCONNECTED_BIT           (1 << 6)
#define BLE_ERROR_BIT                  (1 << 7)

static esp_ble_adv_data_t default_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, // 7.5ms
    .max_interval = 0x000C, // 15ms
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)
};

static esp_ble_adv_params_t default_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
};

static bat_ble_server_t *gpCurrentServer = NULL;
static EventGroupHandle_t gGattsEventGroup = NULL;

static void bat_ble_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void bat_ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

esp_err_t bat_ble_server_init2(bat_ble_server_t *pServer, const char* deviceName, uint16_t appId, uint16_t serviceUuid)
{
    if (pServer == NULL || deviceName == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize the server structure
    memset(pServer, 0, sizeof(bat_ble_server_t));
    pServer->appId = appId;
    strncpy(pServer->deviceName, deviceName, sizeof(pServer->deviceName) - 1);
    pServer->eventGroup = xEventGroupCreate();
    gpCurrentServer = pServer;
    
    if (gGattsEventGroup == NULL) {
        gGattsEventGroup = xEventGroupCreate();
    }
    
    // Initialize BLE
    // esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to release BT Classic memory: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    
    // esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    // ret = esp_bt_controller_init(&bt_cfg);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to initialize BT controller: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    
    // ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to enable BT controller: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    
    // ret = esp_bluedroid_init();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to initialize Bluedroid: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    
    // ret = esp_bluedroid_enable();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to enable Bluedroid: %s", esp_err_to_name(ret));
    //     return ret;
    // }
    
    // Register callbacks
    esp_err_t ret = esp_ble_gatts_register_callback(bat_ble_gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATTS callback: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_ble_gap_register_callback(bat_ble_gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register the app
    ret = bat_ble_gatts_app_register(pServer->appId);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Wait for registration
    EventBits_t bits = xEventGroupWaitBits(gGattsEventGroup, 
                                          BLE_SERVER_REGISTERED_BIT | BLE_ERROR_BIT, 
                                          pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
                                          
    if (bits & BLE_ERROR_BIT) {
        ESP_LOGE(TAG, "Error during BLE server registration");
        return ESP_FAIL;
    }
    
    if (!(bits & BLE_SERVER_REGISTERED_BIT)) {
        ESP_LOGE(TAG, "BLE server registration timeout");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

// 3. Simplified Service and Characteristic Creation
// Create a simple function to add a characteristic to a service:
typedef struct {
    uint16_t uuid;
    esp_gatt_perm_t permissions;
    esp_gatt_char_prop_t properties;
    uint16_t maxLen;
    uint8_t *pInitialValue;
    uint16_t initValueLen;
} bat_ble_char_config_t;

esp_err_t bat_ble_server_create_service(bat_ble_server_t *pServer, uint16_t serviceUuid, 
                                       bat_ble_char_config_t *pCharConfigs, uint8_t numChars)
{
    if (pServer == NULL || pCharConfigs == NULL || numChars == 0 || numChars > BAT_MAX_CHARACTERISTICS) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create service
    esp_gatt_srvc_id_t service_id;
    service_id.id.inst_id = 0;
    service_id.id.uuid.len = ESP_UUID_LEN_16;
    service_id.id.uuid.uuid.uuid16 = serviceUuid;
    service_id.is_primary = true;
    
    // Handles = 1 service + 2 per characteristic (char + descriptor)
    uint16_t numHandles = 1 + (numChars * 2);
    
    esp_err_t ret = esp_ble_gatts_create_service(pServer->gattsIf, &service_id, numHandles);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for service creation
    EventBits_t bits = xEventGroupWaitBits(pServer->eventGroup,
                                          BLE_SERVICE_CREATED_BIT | BLE_ERROR_BIT,
                                          pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
                                          
    if (bits & BLE_ERROR_BIT) {
        ESP_LOGE(TAG, "Error during service creation");
        return ESP_FAIL;
    }
    
    if (!(bits & BLE_SERVICE_CREATED_BIT)) {
        ESP_LOGE(TAG, "Service creation timeout");
        return ESP_ERR_TIMEOUT;
    }
    
    // Add characteristics to service
    pServer->numChars = numChars;
    for (int i = 0; i < numChars; i++) {
        esp_bt_uuid_t char_uuid;
        char_uuid.len = ESP_UUID_LEN_16;
        char_uuid.uuid.uuid16 = pCharConfigs[i].uuid;
        
        esp_attr_value_t char_val;
        char_val.attr_max_len = pCharConfigs[i].maxLen;
        char_val.attr_len = pCharConfigs[i].initValueLen;
        char_val.attr_value = pCharConfigs[i].pInitialValue;
        
        ret = bat_ble_gatts_add_char(pServer->serviceHandle, &char_uuid, 
                                    pCharConfigs[i].permissions, pCharConfigs[i].properties, 
                                    &char_val, NULL);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    
    return ESP_OK;
}

// 4. Simple Start/Stop Functions
esp_err_t bat_ble_server_start(bat_ble_server_t *pServer)
{
    if (pServer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Set device name
    esp_err_t ret = bat_ble_gap_set_device_name(pServer->deviceName);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Configure advertising data
    ret = bat_ble_gap_config_adv_data(&default_adv_data);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Wait for advertising data configuration
    EventBits_t bits = xEventGroupWaitBits(gGattsEventGroup,
                                          BLE_ADV_CONFIG_DONE_BIT | BLE_ERROR_BIT,
                                          pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
                                          
    if (bits & BLE_ERROR_BIT) {
        ESP_LOGE(TAG, "Error during advertising configuration");
        return ESP_FAIL;
    }
    
    if (!(bits & BLE_ADV_CONFIG_DONE_BIT)) {
        ESP_LOGE(TAG, "Advertising configuration timeout");
        return ESP_ERR_TIMEOUT;
    }
    
    // Start service
    ret = bat_ble_gatts_start_service(pServer->serviceHandle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Wait for service to start
    bits = xEventGroupWaitBits(pServer->eventGroup,
                              BLE_SERVICE_STARTED_BIT | BLE_ERROR_BIT,
                              pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
                              
    if (bits & BLE_ERROR_BIT) {
        ESP_LOGE(TAG, "Error during service start");
        return ESP_FAIL;
    }
    
    if (!(bits & BLE_SERVICE_STARTED_BIT)) {
        ESP_LOGE(TAG, "Service start timeout");
        return ESP_ERR_TIMEOUT;
    }
    
    // Start advertising
    ret = bat_ble_gap_start_advertising(&default_adv_params);
    if (ret != ESP_OK) {
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t bat_ble_server_stop(bat_ble_server_t *pServer)
{
    if (pServer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = bat_ble_gap_stop_advertising();
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = bat_ble_gatts_stop_service(pServer->serviceHandle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    return ESP_OK;
}

// 5. Simple Data Operations
// Add functions for sending notifications and responding to read/write requests:
esp_err_t bat_ble_server_notify(bat_ble_server_t *pServer, uint16_t charIndex, uint8_t *pData, uint16_t dataLen)
{
    if (pServer == NULL || charIndex >= pServer->numChars || pData == NULL || dataLen == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!pServer->isConnected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(pServer->gattsIf, pServer->connId, 
                                              pServer->charHandles[charIndex],
                                              dataLen, pData, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send notification: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t bat_ble_server_set_callback(bat_ble_server_t *pServer, void *pContext,
                                     void (*onConnect)(void*, esp_ble_gatts_cb_param_t*),
                                     void (*onDisconnect)(void*, esp_ble_gatts_cb_param_t*),
                                     void (*onWrite)(void*, esp_ble_gatts_cb_param_t*),
                                     void (*onRead)(void*, esp_ble_gatts_cb_param_t*))
{
    if (pServer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    pServer->callbacks.pContext = pContext;
    pServer->callbacks.onConnect = onConnect;
    pServer->callbacks.onDisconnect = onDisconnect;
    pServer->callbacks.onWrite = onWrite;
    pServer->callbacks.onRead = onRead;
    
    return ESP_OK;
}

// 6. Complete the Event Handlers
// Implement the BLE event handlers to manage callbacks:
static void bat_ble_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (gpCurrentServer == NULL) {
        return;
    }
    
    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) {
                gpCurrentServer->gattsIf = gatts_if;
                xEventGroupSetBits(gGattsEventGroup, BLE_SERVER_REGISTERED_BIT);
            } else {
                xEventGroupSetBits(gGattsEventGroup, BLE_ERROR_BIT);
                ESP_LOGE(TAG, "GATTS app registration failed with status %d", param->reg.status);
            }
            break;
            
        case ESP_GATTS_CREATE_EVT:
            if (param->create.status == ESP_GATT_OK) {
                gpCurrentServer->serviceHandle = param->create.service_handle;
                xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_SERVICE_CREATED_BIT);
                ESP_LOGI(TAG, "Service created with handle %d", param->create.service_handle);
            } else {
                xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
                ESP_LOGE(TAG, "Service creation failed with status %d", param->create.status);
            }
            break;
            
        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.status == ESP_GATT_OK) {
                uint8_t charIdx = gpCurrentServer->numChars - 1;
                gpCurrentServer->charHandles[charIdx] = param->add_char.attr_handle;
                ESP_LOGI(TAG, "Characteristic added, handle=%d", param->add_char.attr_handle);
            } else {
                ESP_LOGE(TAG, "Failed to add characteristic with status %d", param->add_char.status);
            }
            break;
            
        case ESP_GATTS_START_EVT:
            if (param->start.status == ESP_GATT_OK) {
                xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_SERVICE_STARTED_BIT);
                ESP_LOGI(TAG, "Service started");
            } else {
                xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_ERROR_BIT);
                ESP_LOGE(TAG, "Service start failed with status %d", param->start.status);
            }
            break;
            
        case ESP_GATTS_CONNECT_EVT:
            gpCurrentServer->isConnected = true;
            gpCurrentServer->connId = param->connect.conn_id;
            
            if (gpCurrentServer->callbacks.onConnect) {
                gpCurrentServer->callbacks.onConnect(gpCurrentServer->callbacks.pContext, param);
            }
            
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_CONNECTED_BIT);
            ESP_LOGI(TAG, "GATT client connected");
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            gpCurrentServer->isConnected = false;
            
            if (gpCurrentServer->callbacks.onDisconnect) {
                gpCurrentServer->callbacks.onDisconnect(gpCurrentServer->callbacks.pContext, param);
            }
            
            xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_DISCONNECTED_BIT);
            ESP_LOGI(TAG, "GATT client disconnected, starting advertising again");
            
            // Auto restart advertising on disconnect
            bat_ble_gap_start_advertising(&default_adv_params);
            break;
            
        case ESP_GATTS_WRITE_EVT:
            if (gpCurrentServer->callbacks.onWrite) {
                gpCurrentServer->callbacks.onWrite(gpCurrentServer->callbacks.pContext, param);
            }
            
            // Auto-respond to write if needed
            if (param->write.need_rsp) {
                esp_gatt_rsp_t rsp;
                memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
                rsp.attr_value.len = param->write.len;
                rsp.attr_value.handle = param->write.handle;
                rsp.attr_value.offset = param->write.offset;
                memcpy(rsp.attr_value.value, param->write.value, param->write.len);
                
                bat_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, &rsp);
            }
            break;
            
        case ESP_GATTS_READ_EVT:
            if (gpCurrentServer->callbacks.onRead) {
                gpCurrentServer->callbacks.onRead(gpCurrentServer->callbacks.pContext, param);
            }
            break;
            
        default:
            break;
    }
}

static void bat_ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            xEventGroupSetBits(gGattsEventGroup, BLE_ADV_CONFIG_DONE_BIT);
            ESP_LOGI(TAG, "Advertising data set complete");
            break;
            
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                xEventGroupSetBits(gGattsEventGroup, BLE_ADVERTISING_STARTED_BIT);
                ESP_LOGI(TAG, "Advertising started");
            } else {
                xEventGroupSetBits(gGattsEventGroup, BLE_ERROR_BIT);
                ESP_LOGE(TAG, "Advertising start failed: %d", param->adv_start_cmpl.status);
            }
            break;
            
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Advertising stopped");
            } else {
                ESP_LOGE(TAG, "Advertising stop failed: %d", param->adv_stop_cmpl.status);
            }
            break;
            
        default:
            break;
    }
}

// TODO: Implement...
#if 0
// Client code example

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bat_ble_server.h"

static const char *TAG = "ble_app";
static bat_ble_server_t bleServer;

// Callback for write events
static void on_write(void *pContext, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(TAG, "Received write: %.*s", param->write.len, param->write.value);
    
    // Echo back the received data as a notification
    bat_ble_server_notify(&bleServer, 0, param->write.value, param->write.len);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BLE application");
    
    // Define our service characteristics
    uint8_t initialValue[] = "Hello BLE";
    bat_ble_char_config_t charConfigs[] = {
        {
            .uuid = 0xFF01,
            .permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
            .maxLen = 100,
            .pInitialValue = initialValue,
            .initValueLen = sizeof(initialValue) - 1
        }
    };
    
    // Initialize the BLE server
    esp_err_t ret = bat_ble_server_init(&bleServer, "ESP32-BLE", 0x55, 0xFF00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE server: %s", esp_err_to_name(ret));
        return;
    }
    
    // Create the service with characteristics
    ret = bat_ble_server_create_service(&bleServer, 0xFF00, charConfigs, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create BLE service: %s", esp_err_to_name(ret));
        return;
    }
    
    // Set callbacks
    bat_ble_server_set_callback(&bleServer, NULL, NULL, NULL, on_write, NULL);
    
    // Start the BLE server
    ret = bat_ble_server_start(&bleServer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE server: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "BLE server started successfully");
}
#endif

