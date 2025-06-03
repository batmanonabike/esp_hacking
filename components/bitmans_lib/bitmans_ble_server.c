#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"

#include "bitmans_ble_server.h"

// See: /docs/ble_intro.md
// GATT Server implementation for Bitman's BLE server
static const char *TAG = "bitmans_lib:ble_server";

#define BITMANS_SERVICE_UUID 0x1234
#define BITMANS_CHAR_UUID 0x5678
#define BITMANS_APP_ID 0x55

static uint16_t bitmans_service_handle = 0;
static esp_gatt_if_t bitmans_gatts_if = 0;
static uint16_t bitmans_conn_id = 0;

static const uint8_t bitmans_service_uuid128[16] = {
    0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
    0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x56, 0x12};
static const uint8_t bitmans_char_uuid128[16] = {
    0xf0, 0xde, 0xbc, 0x9a, 0x34, 0x12, 0x78, 0x56,
    0x34, 0x12, 0x78, 0x56, 0x01, 0xef, 0xcd, 0xab};

static const char *BITMANS_ADV_NAME = "BitmansBLE";

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(bitmans_service_uuid128),
    .p_service_uuid = bitmans_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// GAP (Generic Access Profile) events to about BLE advertising, scanning, connection, and security events.
// Common events include:
// - ESP_GAP_BLE_ADV_START_COMPLETE_EVT: Advertising has started.
// - ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: Advertising has stopped.
// - ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: Scanning has started.
// - ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: Scanning has stopped.
// - ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: Connection parameters updated.
// - ESP_GAP_BLE_SEC_REQ_EVT: Security request from peer device.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html#_CPPv426esp_gap_ble_cb_event_t
static void bitmans_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertising data set, starting advertising");
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "Advertising started successfully");
        }
        else
        {
            ESP_LOGE(TAG, "Advertising start failed, status=0x%x", param->adv_start_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "Advertising stopped successfully");
        }
        else
        {
            ESP_LOGE(TAG, "Advertising stop failed, status=0x%x", param->adv_stop_cmpl.status);
        }
        break;

    default:
        ESP_LOGD(TAG, "Unhandled GAP event: %d", event);
        break;
    }
}

static bitmans_gatts_callbacks_t *gatts_cb_table[GATTS_APPCOUNT] = {0};

const bitmans_gatts_callbacks_t bitmans_gatts_callbacks_empty = {
    .on_reg = NULL,
    .on_create = NULL,
    .on_connect = NULL,
    .on_read = NULL,
    .on_write = NULL,
    // Add more fields as needed
};

// GATTS (GATT Server) events notify about BLE server events.
// These include:
// - ESP_GATTS_REG_EVT: GATT server profile registered, usually where you create your service.
// - ESP_GATTS_CREATE_EVT: Service created, add characteristics here.
// - ESP_GATTS_ADD_CHAR_EVT: Characteristic added, start the service.
// - ESP_GATTS_START_EVT: Service started, begin advertising.
// - ESP_GATTS_CONNECT_EVT: A client has connected.
// - ESP_GATTS_DISCONNECT_EVT: A client has disconnected, often restart advertising.
// - ESP_GATTS_READ_EVT: A client is reading a characteristic value.
// - ESP_GATTS_WRITE_EVT: A client is writing to a characteristic value.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gatts.html#_CPPv426esp_gatts_cb_event_t
static void bitmans_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "GATTS_REG_EVT, creating service");
        bitmans_gatts_if = gatts_if;
        esp_ble_gap_set_device_name(BITMANS_ADV_NAME);
        esp_ble_gap_config_adv_data(&adv_data);
        {
            esp_gatt_id_t service_id = {
                .inst_id = 0,
                .uuid = {
                    .len = ESP_UUID_LEN_128,
                    .uuid = {.uuid128 = {0}}}};
            memcpy(service_id.uuid.uuid.uuid128, bitmans_service_uuid128, 16);
            esp_ble_gatts_create_service(gatts_if, (void *)&service_id, 8);
        }
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "Service created, adding characteristic");
        bitmans_service_handle = param->create.service_handle;
        {
            esp_gatt_id_t char_id = {
                .inst_id = 0,
                .uuid = {
                    .len = ESP_UUID_LEN_128,
                    .uuid = {.uuid128 = {0}}}};
            memcpy(char_id.uuid.uuid.uuid128, bitmans_char_uuid128, 16);
            esp_gatt_char_prop_t prop = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
            esp_ble_gatts_add_char(bitmans_service_handle, (void *)&char_id.uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, prop, NULL, NULL);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "Characteristic added, starting service");
        esp_ble_gatts_start_service(bitmans_service_handle);
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "Service started");
        // Advertising is now started in ADV_DATA_SET_COMPLETE event
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Client connected");
        bitmans_conn_id = param->connect.conn_id;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Client disconnected, restarting advertising");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "Read event");
        // Respond with dummy data
        {
            uint8_t value[4] = {0x42, 0x43, 0x44, 0x45};
            esp_gatt_rsp_t rsp = {0};
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 4;
            memcpy(rsp.attr_value.value, value, 4);
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "Write event, len=%d", param->write.len);
        // Optionally process param->write.value
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        break;

    default:
        break;
    }
}

#define MAX_GATTS_IF 16
//#define INVALID_GATT

// Safe assumptions *on ESP32*.
static gatts_app_id_t gatts_if_to_app_id[MAX_GATTS_IF] = {0};

// static gatts_app_id_t bitmans_gatts_app_id_from_if(esp_gatt_if_t gatts_if)
// {


//     if (gatts_if >= MAX_GATTS_IF && gatts_if_to_app_id[gatts_if] != 0) {
//         *app_id = gatts_if_to_app_id[gatts_if];
//     } else {
//         *app_id = GATTS_APP0; // Default to first app if not found
//     }
// }

// static void bitmans_gatts_event_handler_todo(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
// {
//     uint8_t app_id = 0xFF;
//     switch (event) {
//         case ESP_GATTS_REG_EVT:
//             app_id = param->reg.app_id;
//             break;
//         case ESP_GATTS_CREATE_EVT:
//             app_id = param->create.app_id;
//             break;
//         case ESP_GATTS_CONNECT_EVT:
//             app_id = param->connect.app_id;
//             break;
//         case ESP_GATTS_READ_EVT:
//             app_id = param->read.app_id;
//             break;
//         case ESP_GATTS_WRITE_EVT:
//             app_id = param->write.app_id;
//             break;
//         // ...handle other events as needed
//         default:
//             break;
//     }
//     if (app_id < GATTS_APPCOUNT && gatts_cb_table[app_id]) {
//         switch (event) {
//             case ESP_GATTS_REG_EVT:
//                 if (gatts_cb_table[app_id]->on_reg)
//                     gatts_cb_table[app_id]->on_reg(gatts_if, param);
//                 break;
//             case ESP_GATTS_CREATE_EVT:
//                 if (gatts_cb_table[app_id]->on_create)
//                     gatts_cb_table[app_id]->on_create(gatts_if, param);
//                 break;
//             case ESP_GATTS_CONNECT_EVT:
//                 if (gatts_cb_table[app_id]->on_connect)
//                     gatts_cb_table[app_id]->on_connect(gatts_if, param);
//                 break;
//             case ESP_GATTS_READ_EVT:
//                 if (gatts_cb_table[app_id]->on_read)
//                     gatts_cb_table[app_id]->on_read(gatts_if, param);
//                 break;
//             case ESP_GATTS_WRITE_EVT:
//                 if (gatts_cb_table[app_id]->on_write)
//                     gatts_cb_table[app_id]->on_write(gatts_if, param);
//                 break;
//             // ...other events
//             default:
//                 break;
//         }
//     }
// }

esp_err_t bitmans_ble_server_init()
{
    //ESP_APP_ID_MAX
    ESP_LOGI(TAG, "Initializing BLE GATT server");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
        return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
        return ret;

    ret = esp_bluedroid_init();
    if (ret)
        return ret;

    return esp_bluedroid_enable();
}

esp_err_t bitmans_ble_server_term()
{
    ESP_LOGI(TAG, "Terminating BLE GATT server");

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    return ESP_OK;
}

esp_err_t bitmans_ble_server_register_gatts(gatts_app_id_t app_id, bitmans_gatts_callbacks_t *callbacks)
{
    ESP_LOGI(TAG, "Registering GATT server");

    if (callbacks == NULL)
        callbacks = &bitmans_gatts_callbacks_empty;

    gatts_cb_table[app_id] = callbacks;
    esp_ble_gatts_register_callback(bitmans_gatts_event_handler);
    esp_ble_gap_register_callback(bitmans_gap_event_handler);
    esp_ble_gatts_app_register(BITMANS_APP_ID);
    return ESP_OK;
}

esp_err_t bitmans_ble_server_unregister_gatts()
{
    ESP_LOGI(TAG, "Unregistering GATT server");

    // No explicit unregister in ESP-IDF, but could clean up here if needed
    return ESP_OK;
}
