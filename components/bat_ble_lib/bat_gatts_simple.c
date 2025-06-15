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

// Implementing a higher-level abstraction layer that handles the complex BLE workflow while providing a
// simplified API.

// Define common event bits
#define BLE_SERVER_REGISTERED_BIT (1 << 0)
#define BLE_ADV_CONFIG_DONE_BIT (1 << 1)
#define BLE_SCAN_RESPONSE_DONE_BIT (1 << 2)
#define BLE_SERVICE_CREATED_BIT (1 << 3)
#define BLE_SERVICE_STARTED_BIT (1 << 4)
#define BLE_ADVERTISING_STARTED_BIT (1 << 5)
#define BLE_CONNECTED_BIT (1 << 6)
#define BLE_DISCONNECTED_BIT (1 << 7)
#define BLE_ERROR_BIT (1 << 8)

static bat_ble_server_t *gpCurrentServer = NULL;
static EventGroupHandle_t gGattsEventGroup = NULL;
static esp_ble_adv_params_t g_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY};

// Implement the BLE event handlers to manage callbacks:
static void bat_ble_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    if (gpCurrentServer == NULL)
        return;

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        if (pParam->reg.status == ESP_GATT_OK)
        {
            gpCurrentServer->gattsIf = gatts_if;
            xEventGroupSetBits(gGattsEventGroup, BLE_SERVER_REGISTERED_BIT);
            ESP_LOGI(TAG, "GATTS app registered with ID %d", pParam->reg.app_id);
        }
        else
        {
            xEventGroupSetBits(gGattsEventGroup, BLE_ERROR_BIT);
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
        if (pParam->add_char.status == ESP_GATT_OK)
        {
            uint8_t charIdx = gpCurrentServer->numChars - 1;
            gpCurrentServer->charHandles[charIdx] = pParam->add_char.attr_handle;
            ESP_LOGI(TAG, "Characteristic added, handle=%d", pParam->add_char.attr_handle);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to add characteristic with status %d", pParam->add_char.status);
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
        gpCurrentServer->callbacks.onConnect(gpCurrentServer->pContext, pParam);

        xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_CONNECTED_BIT);
        ESP_LOGI(TAG, "GATT client connected");
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        gpCurrentServer->isConnected = false;
        gpCurrentServer->callbacks.onDisconnect(gpCurrentServer->pContext, pParam);

        xEventGroupSetBits(gpCurrentServer->eventGroup, BLE_DISCONNECTED_BIT);
        ESP_LOGI(TAG, "GATT client disconnected, starting advertising again");

        // Auto restart advertising on disconnect
        bat_ble_gap_start_advertising(&g_adv_params);
        break;

    case ESP_GATTS_WRITE_EVT:
        gpCurrentServer->callbacks.onWrite(gpCurrentServer->pContext, pParam);

        // Auto-respond to write if needed
        // if (param->write.need_rsp)
        // {
        //     esp_gatt_rsp_t rsp;
        //     memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        //     rsp.attr_value.len = param->write.len;
        //     rsp.attr_value.handle = param->write.handle;
        //     rsp.attr_value.offset = param->write.offset;
        //     memcpy(rsp.attr_value.value, param->write.value, param->write.len);

        //     bat_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, &rsp);
        // }
        break;

    case ESP_GATTS_READ_EVT:
        gpCurrentServer->callbacks.onRead(gpCurrentServer->pContext, pParam);
        break;

    default:
        break;
    }
}

static void bat_ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *pParam)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        xEventGroupSetBits(gGattsEventGroup, BLE_ADV_CONFIG_DONE_BIT);
        ESP_LOGI(TAG, "Advertising data set complete");
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (pParam->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            xEventGroupSetBits(gGattsEventGroup, BLE_ADVERTISING_STARTED_BIT);
            ESP_LOGI(TAG, "Advertising started");
        }
        else
        {
            xEventGroupSetBits(gGattsEventGroup, BLE_ERROR_BIT);
            ESP_LOGE(TAG, "Advertising start failed: %d", pParam->adv_start_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        if (pParam->scan_rsp_data_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            xEventGroupSetBits(gGattsEventGroup, BLE_SCAN_RESPONSE_DONE_BIT);
            ESP_LOGI(TAG, "Scan response data set complete");
        }
        else
        {
            xEventGroupSetBits(gGattsEventGroup, BLE_ERROR_BIT);
            ESP_LOGE(TAG, "Scan response data set failed: %d", pParam->scan_rsp_data_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (pParam->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "Advertising stopped");
        }
        else
        {
            ESP_LOGE(TAG, "Advertising stop failed: %d", pParam->adv_stop_cmpl.status);
        }
        break;

    default:
        break;
    }
}

// A simple initialization function that handles all the BLE setup.
esp_err_t bat_ble_server_init2(
    bat_ble_server_t *pServer, void *pContext, const char *pDeviceName, uint16_t appId,
    const char *pServiceUuid, int appearance, int timeoutMs)
{
    if (pServer == NULL || pDeviceName == NULL || pServiceUuid == NULL)
        return ESP_ERR_INVALID_ARG;

    memset(pServer, 0, sizeof(bat_ble_server_t));

    esp_err_t ret = bat_uuid_from_string(pServiceUuid, &pServer->serviceId);
    if (ret != ESP_OK)
        return ret;

    if (pDeviceName == NULL)
        pServer->deviceName[0] = '\0';
    else
        strncpy(pServer->deviceName, pDeviceName, sizeof(pServer->deviceName) - 1);

    if (gGattsEventGroup == NULL)
        gGattsEventGroup = xEventGroupCreate();

    pServer->appId = appId;
    pServer->pContext = pContext;
    pServer->appearance = appearance;
    pServer->eventGroup = xEventGroupCreate();

    if (pServer->eventGroup == NULL || gGattsEventGroup == NULL)
        ESP_LOGE(TAG, "Failed to create event groups");
    else
    {
        ret = esp_ble_gatts_register_callback(bat_ble_gatts_event_handler);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "Failed to register GATTS callback: %s", esp_err_to_name(ret));
        else
        {
            ret = esp_ble_gap_register_callback(bat_ble_gap_event_handler);
            if (ret != ESP_OK)
                ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
            else
            {
                ret = bat_ble_gatts_app_register(appId);
                if (ret == ESP_OK)
                {
                    // Wait for registration
                    EventBits_t bits = xEventGroupWaitBits(gGattsEventGroup,
                                                           BLE_SERVER_REGISTERED_BIT | BLE_ERROR_BIT,
                                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

                    if (bits & BLE_ERROR_BIT)
                    {
                        ESP_LOGE(TAG, "Error during BLE server registration");
                        ret = ESP_FAIL;
                    }
                    else if (!(bits & BLE_SERVER_REGISTERED_BIT))
                    {
                        ESP_LOGE(TAG, "BLE server registration timeout");
                        ret = ESP_ERR_TIMEOUT;
                    }
                    else
                    {
                        gpCurrentServer = pServer;
                        ESP_LOGI(TAG, "BLE server initialized successfully with appId: %d", appId);
                        return ESP_OK;
                    }
                }
            }
        }
    }

    bat_ble_server_deinit(pServer);
    return ret;
}

esp_err_t bat_ble_server_deinit(bat_ble_server_t *pServer)
{
    if (pServer->eventGroup != NULL)
        vEventGroupDelete(pServer->eventGroup);

    if (gGattsEventGroup != NULL)
        vEventGroupDelete(gGattsEventGroup);

    gGattsEventGroup = NULL;
    pServer->eventGroup = NULL;

    return ESP_OK;
}

// Create the device and function to add a characteristics.
esp_err_t bat_ble_server_create_service(bat_ble_server_t *pServer, bat_ble_char_config_t *pCharConfigs, uint8_t numChars)
{
    if (pServer == NULL || pCharConfigs == NULL || numChars == 0 || numChars > BAT_MAX_CHARACTERISTICS)
        return ESP_ERR_INVALID_ARG;

    esp_gatt_srvc_id_t service_id = {
        .id = {
            .inst_id = 0,
            .uuid = pServer->serviceId},
        .is_primary = true};

    // Handles = 1 service + 2 per characteristic (char + descriptor)
    uint16_t numHandles = 1 + (numChars * 2);
    esp_err_t ret = esp_ble_gatts_create_service(pServer->gattsIf, &service_id, numHandles);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create service: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for service creation
    EventBits_t bits = xEventGroupWaitBits(pServer->eventGroup,
                                           BLE_SERVICE_CREATED_BIT | BLE_ERROR_BIT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));

    if (bits & BLE_ERROR_BIT)
    {
        ESP_LOGE(TAG, "Error during service creation");
        return ESP_FAIL;
    }

    if (!(bits & BLE_SERVICE_CREATED_BIT))
    {
        ESP_LOGE(TAG, "Service creation timeout");
        return ESP_ERR_TIMEOUT;
    }

    // Add characteristics to service
    pServer->numChars = numChars;
    for (int i = 0; i < numChars; i++)
    {
        esp_bt_uuid_t char_uuid;
        ret = bat_uuid_from_16bit(pCharConfigs[i].uuid, &char_uuid);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create characteristic UUID: %s", esp_err_to_name(ret));
            return ret;
        }

        esp_attr_value_t char_val = {
            .attr_max_len = pCharConfigs[i].maxLen,
            .attr_len = pCharConfigs[i].initValueLen,
            .attr_value = pCharConfigs[i].pInitialValue};

        ret = bat_ble_gatts_add_char(pServer->serviceHandle, &char_uuid,
                                     pCharConfigs[i].permissions, pCharConfigs[i].properties,
                                     &char_val, NULL);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add characteristic: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    return ESP_OK;
}

static esp_err_t bat_update_advert_service_uuid(bat_ble_server_t *pServer, esp_ble_adv_data_t *pAdvData)
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
    }

    return ESP_ERR_INVALID_ARG;
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
esp_err_t bat_ble_server_start(bat_ble_server_t *pServer, int timeoutMs)
{
    esp_ble_adv_data_t adv_data = {
        .manufacturer_len = 0,
        .service_data_len = 0,
        .service_uuid_len = 0,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .set_scan_rsp = false,
        .include_name = false, // Don't include name in adv packet
        .include_txpower = false,
        .p_service_data = NULL,
        .p_service_uuid = NULL,
        .p_manufacturer_data = NULL,
        .appearance = pServer->appearance,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };

    // Set device name
    esp_err_t ret = bat_update_advert_service_uuid(pServer, &adv_data);
    if (ret != ESP_OK)
        return ret;

    // Configure advertising data and wait for it to be configured.
    ret = bat_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK)
        return ret;

    EventBits_t bits = xEventGroupWaitBits(gGattsEventGroup,
                                           BLE_ADV_CONFIG_DONE_BIT | BLE_ERROR_BIT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

    if (bits & BLE_ERROR_BIT)
    {
        ESP_LOGE(TAG, "Error during advertising configuration");
        return ESP_FAIL;
    }

    if (!(bits & BLE_ADV_CONFIG_DONE_BIT))
    {
        ESP_LOGE(TAG, "Advertising configuration timeout");
        return ESP_ERR_TIMEOUT;
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

        bits = xEventGroupWaitBits(gGattsEventGroup,
                                   BLE_SCAN_RESPONSE_DONE_BIT | BLE_ERROR_BIT,
                                   pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

        if (bits & BLE_ERROR_BIT)
        {
            ESP_LOGE(TAG, "Error during scan response");
            return ESP_FAIL;
        }

        if (!(bits & BLE_SCAN_RESPONSE_DONE_BIT))
        {
            ESP_LOGE(TAG, "Scan response configuration timeout");
            return ESP_ERR_TIMEOUT;
        }
    }

    // Start service and wait for it to start.
    ret = bat_ble_gatts_start_service(pServer->serviceHandle);
    if (ret != ESP_OK)
        return ret;

    bits = xEventGroupWaitBits(pServer->eventGroup,
                               BLE_SERVICE_STARTED_BIT | BLE_ERROR_BIT,
                               pdTRUE, pdFALSE, pdMS_TO_TICKS(timeoutMs));

    if (bits & BLE_ERROR_BIT)
    {
        ESP_LOGE(TAG, "Error during service start");
        return ESP_FAIL;
    }

    if (!(bits & BLE_SERVICE_STARTED_BIT))
    {
        ESP_LOGE(TAG, "Service start timeout");
        return ESP_ERR_TIMEOUT;
    }

    return bat_ble_gap_start_advertising(&g_adv_params);
}

esp_err_t bat_ble_server_stop(bat_ble_server_t *pServer)
{
    if (pServer == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = bat_ble_gap_stop_advertising();
    if (ret != ESP_OK)
    {
        return ret;
    }

    return bat_ble_gatts_stop_service(pServer->serviceHandle);
}

// Functions for sending notifications and responding to read/write requests:
esp_err_t bat_ble_server_notify(bat_ble_server_t *pServer, uint16_t charIndex, uint8_t *pData, uint16_t dataLen)
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

static void bat_ble_server_no_op(void *pContext, esp_ble_gatts_cb_param_t *pParam)
{
}

esp_err_t bat_ble_server_set_callbacks(bat_ble_server_t *pServer, bat_ble_server_callbacks_t *pCbs)
{
    if (pServer == NULL)
        return ESP_ERR_INVALID_ARG;

    pServer->callbacks.onRead = pCbs->onRead ? pCbs->onRead : bat_ble_server_no_op;
    pServer->callbacks.onWrite = pCbs->onWrite ? pCbs->onWrite : bat_ble_server_no_op;
    pServer->callbacks.onConnect = pCbs->onConnect ? pCbs->onConnect : bat_ble_server_no_op;
    pServer->callbacks.onDisconnect = pCbs->onDisconnect ? pCbs->onDisconnect : bat_ble_server_no_op;

    return ESP_OK;
}
