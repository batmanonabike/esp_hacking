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

#include "bitmans_ble.h"
#include "bitmans_hash_table.h"
#include "bitmans_ble_server.h"

// See: /docs/ble_intro.md
// GATT Server implementation for Bitman's BLE server
static const char *TAG = "bitmans_lib:ble_server";

static uint16_t bitmans_conn_id = 0;

// The GATTS interface is a unique identifier for each GATT server instance, we get that during registration.
static bitmans_hash_table_t app_cb_table;   // Hash table to map app IDs to GATTS callbacks.
static bitmans_hash_table_t gatts_cb_table; // Hash table to map GATTS interfaces to callbacks.

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void bitman_gatts_no_op(bitmans_gatts_callbacks_t * pCb, esp_ble_gatts_cb_param_t * pParam)
{
}

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

static bitmans_gatts_callbacks_t *bitmans_gatts_callbacks_create_mapping(
    esp_gatt_if_t gatts_if, bitmans_gatts_app_id app_id)
{
    bitmans_gatts_callbacks_t *pCallbacks = NULL;
    esp_err_t err = bitmans_hash_table_get(&app_cb_table, app_id, (void **)&pCallbacks);

    if (err == ESP_OK && pCallbacks != NULL)
    {
        bitmans_hash_table_remove(&app_cb_table, app_id);
        err = bitmans_hash_table_set(&gatts_cb_table, gatts_if, pCallbacks);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Callback registered for appId: %d, gatts_if: %d", app_id, gatts_if);
            pCallbacks->service_handle = 0; 
            pCallbacks->gatts_if = gatts_if;
            return pCallbacks;
        }
    }

    ESP_LOGW(TAG, "No callback registered for appId: %d, gatts_if: %d", app_id, gatts_if);
    return NULL;
}

static bitmans_gatts_callbacks_t *bitmans_gatts_callbacks_lookup(esp_gatt_if_t gatts_if)
{
    bitmans_gatts_callbacks_t *pCallbacks = NULL;
    esp_err_t err = bitmans_hash_table_get(&gatts_cb_table, gatts_if, (void **)&pCallbacks);

    if (err == ESP_OK && pCallbacks != NULL)
    {
        ESP_LOGV(TAG, "Got callback for gatts_if: %d", gatts_if);
        return pCallbacks;
    }

    ESP_LOGW(TAG, "No callback registered for gatts_if: %d", gatts_if);
    return NULL;
}

esp_err_t bitmans_gatts_advertise(const char *pszAdvertisedName, bitmans_ble_uuid_t *pServiceUUID)
{
    esp_err_t err = ESP_OK;
    if (pszAdvertisedName != NULL)
    {
        err = esp_ble_gap_set_device_name(pszAdvertisedName);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set device name for: %s, %s", pszAdvertisedName, esp_err_to_name(err));
            return err;
        }
    }

    esp_ble_adv_data_t adv_data = {
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
        .service_uuid_len = sizeof(pServiceUUID->uuid),
        .p_service_uuid = pServiceUUID->uuid,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    err = esp_ble_gap_config_adv_data(&adv_data);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure advertising data: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGV(TAG, "Advertising succeeded");
    return ESP_OK;
}

esp_err_t bitmans_gatts_create_service(esp_gatt_if_t gatts_if, bitmans_ble_uuid_t *pServiceUUID)
{
    esp_gatt_id_t service_id = {
        .inst_id = 0,
        .uuid = {
            .len = ESP_UUID_LEN_128,
            .uuid = {.uuid128 = {0}}}};

    memcpy(service_id.uuid.uuid.uuid128, pServiceUUID->uuid, sizeof(pServiceUUID->uuid));
    esp_err_t err = esp_ble_gatts_create_service(gatts_if, (void *)&service_id, 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create GATTS service: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t bitmans_gatts_create_characteristic(
    esp_gatt_if_t gatts_if, uint16_t service_handle,
    bitmans_ble_uuid_t *pCharUUID, esp_gatt_char_prop_t properties)
{
    esp_gatt_id_t char_id = {
        .inst_id = 0,
        .uuid = {
            .len = ESP_UUID_LEN_128,
            .uuid = {.uuid128 = {0}}}};

    memcpy(char_id.uuid.uuid.uuid128, pCharUUID->uuid, sizeof(pCharUUID->uuid));

    esp_err_t err = esp_ble_gatts_add_char(
        service_handle,
        (void *)&char_id.uuid,
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, // Consider making permissions a parameter as well
        properties,
        NULL,
        NULL);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add characteristic: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

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

// Order of Operations: The correct order of operations is:
// ESP_GATTS_REG_EVT: Register the GATT application and create the service.
// ESP_GATTS_CREATE_EVT: The service is created. Now add the characteristic.
// ESP_GATTS_ADD_CHAR_EVT: The characteristic is added. Now start the service.
// ESP_GATTS_START_EVT: The service is started. Now start advertising.
static void bitmans_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_callbacks_t *pCallbacks = NULL;

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
    {
        ESP_LOGI(TAG, "ESP_GATTS_REG_EVT, Service registered, gatts_if=%d, app_id=%d", gatts_if, pParam->reg.app_id);
        pCallbacks = bitmans_gatts_callbacks_create_mapping(gatts_if, pParam->reg.app_id);
        if (pCallbacks != NULL)
            pCallbacks->on_reg(pCallbacks, pParam);
    }
    break;

    case ESP_GATTS_CREATE_EVT:
    {
        ESP_LOGI(TAG, "ESP_GATTS_CREATE_EVT, Service created, service_handle=%d", pParam->create.service_handle);
        pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);
        if (pCallbacks != NULL)
        {
            pCallbacks->service_handle = pParam->create.service_handle;
            pCallbacks->on_create(pCallbacks, pParam);            
        }
        break;
    }

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_EVT, Characteristic added, char_handle=%d", pParam->add_char.attr_handle);
        pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);
        if (pCallbacks != NULL)
            pCallbacks->on_add_char(pCallbacks, pParam);            
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_START_EVT, Service started, gatts_if=%d", gatts_if);
        pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);
        if (pCallbacks != NULL)
            pCallbacks->on_start(pCallbacks, pParam);    
        break;

    //////
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, Client connected, gatts_if=%d, conn_id=%d", gatts_if, pParam->connect.conn_id);
        pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);

        bitmans_conn_id = pParam->connect.conn_id;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, Client disconnected, restarting advertising");
        pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);

        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_READ_EVT, Read event");
        pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);

        // Respond with dummy data
        {
            uint8_t value[4] = {0x42, 0x43, 0x44, 0x45};
            esp_gatt_rsp_t rsp = {0};
            rsp.attr_value.handle = pParam->read.handle;
            rsp.attr_value.len = 4;
            memcpy(rsp.attr_value.value, value, 4);
            esp_ble_gatts_send_response(gatts_if, pParam->read.conn_id, pParam->read.trans_id, ESP_GATT_OK, &rsp);
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT, Write event, len=%d", pParam->write.len);
        pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);

        // Optionally process param->write.value
        esp_ble_gatts_send_response(gatts_if, pParam->write.conn_id, pParam->write.trans_id, ESP_GATT_OK, NULL);
        break;
    
    case ESP_GATTS_UNREG_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_UNREG_EVT, unregistering app");
        pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);
        bitmans_hash_table_remove(&gatts_cb_table, gatts_if);
        break;
    
    default:
        break;
    }
}

esp_err_t bitmans_ble_server_init()
{
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

    ret = esp_bluedroid_enable();
    if (ret)
        return ret;

    // Initialize hash tables for GATT callbacks
    ret = bitmans_hash_table_init(&gatts_cb_table, 16, NULL, NULL);
    if (ret)
        return ret;

    return bitmans_hash_table_init(&app_cb_table, 4, NULL, NULL);
}

esp_err_t bitmans_ble_server_term()
{
    ESP_LOGI(TAG, "Terminating BLE GATT server");

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    bitmans_hash_table_cleanup(&app_cb_table);
    bitmans_hash_table_cleanup(&gatts_cb_table);
    return ESP_OK;
}

esp_err_t bitmans_ble_gatts_register(bitmans_gatts_app_id app_id, bitmans_gatts_callbacks_t *pCallbacks)
{
    ESP_LOGI(TAG, "Registering GATT server");

    if (pCallbacks == NULL)
        return ESP_ERR_INVALID_ARG;

    esp_err_t ret = bitmans_hash_table_set(&app_cb_table, app_id, pCallbacks);
    if (ret)
        return ret;

    ret = esp_ble_gatts_register_callback(bitmans_gatts_event_handler);
    if (ret)
        return ret;

    ret = esp_ble_gap_register_callback(bitmans_gap_event_handler);
    if (ret)
        return ret;

    ret = esp_ble_gatts_app_register(app_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register GATT application: %s", esp_err_to_name(ret)); // Add this error check
        return ret;
    }

    return ESP_OK;
}

esp_err_t bitmans_ble_gatts_unregister(bitmans_gatts_app_id app_id)
{
    ESP_LOGI(TAG, "Unregistering GATT server");
    ESP_ERROR_CHECK(bitmans_hash_table_remove(&gatts_cb_table, app_id));
    return ESP_OK;
}
