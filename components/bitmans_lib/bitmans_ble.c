#include <stdio.h>
#include <string.h> // For memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "bitmans_ble.h"

// See: /docs/ble_intro.md
// Connection Process:
// 1.  Advertiser sends advertising packets.
// 2. Scanner detects advertiser.
// 3. If connectable and desired, scanner (now initiator) sends a Connection Request.
// 4. If advertiser accepts (e.g., not using a Filter Accept List or initiator is on the list), connection is established.
static const char *TAG = "bitmans_lib:ble";

// Handle 'Generic Access Profile' events.
// GAP events are related to device discovery (scanning, advertising), connection management, and security.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-introduction.html
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
    {
        ESP_LOGI(TAG, "Scan parameters set, starting scan...");
        // The duration of the scan, 0 means scan continuously
        uint32_t duration = 30; // Scan for 30 seconds
        esp_ble_gap_start_scanning(duration);
        break;
    }

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Scan start failed, error status = %x", param->scan_start_cmpl.status);
        }
        else
        {
            ESP_LOGI(TAG, "Scan started successfully.");
        }
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt)
        {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            ESP_LOGI(TAG, "Device found: ADDR: %02x:%02x:%02x:%02x:%02x:%02x",
                     scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1],
                     scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3],
                     scan_result->scan_rst.bda[4], scan_result->scan_rst.bda[5]);
            // Here you would check if this is the server you want to connect to
            // For example, by checking the advertised name or service UUID
            // If it is, you would call esp_ble_gap_stop_scanning() and then esp_ble_gattc_open()
            break;

        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan stopped.");
        // If you stopped scanning to connect, initiate connection here
        break;

        // These two events are causing compilation errors, it might need 'menuconfig'
        // Removing them for now.
        //
        // case ESP_GAP_BLE_CONNECT_EVT: // This event is for peripheral role, not central/client
        //     ESP_LOGI(TAG, "ESP_GAP_BLE_CONNECT_EVT");
        //     break;

        // case ESP_GAP_BLE_DISCONNECT_EVT: // This event is for peripheral role
        //     ESP_LOGI(TAG, "ESP_GAP_BLE_DISCONNECT_EVT");
        //     break;

    default:
        ESP_LOGI(TAG, "GAP Event: %d", event);
        break;
    }
}

// Handle `Generic Attribute Profile` events.
// GATT Client operations include connecting to a peripheral, discovering services, reading/writing
// characteristics, and handling notifications/indications.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gattc.html
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    ESP_LOGI(TAG, "GATTC Event: %d, gattc_if %d", event, gattc_if);

    switch (event)
    {

    case ESP_GATTC_REG_EVT:
        if (param->reg.status == ESP_GATT_OK)
        {
            ESP_LOGI(TAG, "GATTC registered successfully, app_id %04x", param->reg.app_id);
        }
        else
        {
            ESP_LOGE(TAG, "GATTC registration failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
        }
        break;

    case ESP_GATTC_CONNECT_EVT:
    {
        // This event indicates the BLE physical link is established.
        // The status of the GATT connection itself will be in ESP_GATTC_OPEN_EVT.
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT: conn_id %d, if %d, remote_bda: %02x:%02x:%02x:%02x:%02x:%02x",
                 param->connect.conn_id,
                 gattc_if,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        // The actual GATT connection status and service discovery initiation should be handled in ESP_GATTC_OPEN_EVT
        break;
    }

    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "gattc open failed, status %d, conn_id %d", param->open.status, param->open.conn_id);
        }
        else
        {
            ESP_LOGI(TAG, "gattc open success, conn_id %d, mtu %d", param->open.conn_id, param->open.mtu);
            ESP_LOGI(TAG, "Connected to remote device: %02x:%02x:%02x:%02x:%02x:%02x",
                     param->open.remote_bda[0], param->open.remote_bda[1], param->open.remote_bda[2],
                     param->open.remote_bda[3], param->open.remote_bda[4], param->open.remote_bda[5]);
            // Discover services after successful GATT open
            esp_err_t err = esp_ble_gattc_search_service(gattc_if, param->open.conn_id, NULL);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ble_gattc_search_service error, status %d", err);
            }
        }
        break;

    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT, conn_id %d, reason %d", param->disconnect.conn_id, param->disconnect.reason);
        // You might want to re-scan or attempt to reconnect here
        break;

    case ESP_GATTC_SEARCH_RES_EVT:
    {
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_RES_EVT: conn_id = %x, is_primary = %d", param->search_res.conn_id, param->search_res.is_primary);
        ESP_LOGI(TAG, "SERVICE UUID: %04x", param->search_res.srvc_id.uuid.uuid.uuid16); // Assuming 16-bit UUID
        // Store or check the service UUID. If it's the one you're looking for,
        // you can then get its characteristics.
        break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (param->search_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "search service failed, error status = %x", param->search_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        // After services are found, you would get characteristics for the desired service
        // esp_ble_gattc_get_char_by_uuid(...);
        break;

    // This events are causing compilation errors, it might need 'menuconfig'
    // Removing them for now.
    // case ESP_GATTC_GET_CHAR_EVT:
    //     if (param->get_char.status != ESP_GATT_OK)
    //     {
    //         ESP_LOGE(TAG, "get char failed, error status = %x", param->get_char.status);
    //         break;
    //     }
    //     ESP_LOGI(TAG, "ESP_GATTC_GET_CHAR_EVT: char uuid: %04x, char handle %d",
    //              param->get_char.char_id.uuid.uuid.uuid16, // Assuming 16-bit UUID
    //              param->get_char.char_handle);
    //     // If this is the characteristic for IP/port, you can read it or register for notifications
    //     // esp_ble_gattc_read_char(...);
    //     // esp_ble_gattc_register_for_notify(...);
    //     break;

    case ESP_GATTC_READ_CHAR_EVT:
        if (param->read.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "read char failed, error status = %x", param->read.status);
            break;
        }
        ESP_LOGI(TAG, "ESP_GATTC_READ_CHAR_EVT: handle %d, value len %d", param->read.handle, param->read.value_len);
        esp_log_buffer_hex(TAG, param->read.value, param->read.value_len);
        // Process the received IP address and port here
        break;

    // Add cases for other GATTC events like ESP_GATTC_WRITE_CHAR_EVT, ESP_GATTC_NOTIFY_EVT etc.
    default:
        break;
    }
}

esp_err_t bitmans_ble_init()
{
    ESP_LOGI(TAG, "Initializing BLE system");

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret)
    {
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return ret;
    }

    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if (ret)
    {
        ESP_LOGE(TAG, "gattc register error, error code = %x", ret);
        return ret;
    }

    // For client functionality, you need to register an application profile
    ret = esp_ble_gattc_app_register(0); // Using a simple app ID 0 for now
    if (ret)
    {
        ESP_LOGE(TAG, "gattc app register error, error code = %x", ret);
        return ret;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret)
    {
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    ESP_LOGI(TAG, "BLE system initialized successfully");
    return ESP_OK;
}

esp_err_t bitmans_ble_term()
{
    ESP_LOGI(TAG, "Terminating BLE system");

    // Stop scanning if it's active
    // Note: You might need more sophisticated logic if connections are active
    // For simplicity, this example doesn't manage active connections during termination
    esp_err_t ret = esp_ble_gap_stop_scanning();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    { 
        // ESP_ERR_INVALID_STATE if not scanning
        ESP_LOGE(TAG, "Failed to stop BLE scanning: %s", esp_err_to_name(ret));
        // Continue deinitialization even if stopping scan fails
    }

    // TODO:
    // Unregister GATTC application
    // Assuming app_id 0 was used in bitmans_ble_init
    // You might need to store the gattc_if from ESP_GATTC_REG_EVT to unregister a specific app if multiple are used.
    // For a single app_id 0, this might not be strictly necessary if bluedroid_deinit handles it,
    // but explicit unregistration is good practice.
    // However, esp_ble_gattc_app_unregister takes gattc_if, not app_id.
    // For now, we'll rely on bluedroid_deinit to clean up app registrations.
    // If specific unregistration is needed, gattc_if from ESP_GATTC_REG_EVT must be stored.

    // It's generally good practice to unregister callbacks, but deinitialization
    // of bluedroid should handle this. If issues arise, explicit unregistration
    // esp_ble_gap_register_callback(NULL);
    // esp_ble_gattc_register_callback(NULL);
    // might be needed, but often causes issues if done out of order with deinit.

    ret = esp_bluedroid_disable();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disable Bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Bluedroid disabled");

    ret = esp_bluedroid_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize Bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Bluedroid deinitialized");

    ret = esp_bt_controller_disable();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disable BT controller: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "BT controller disabled");

    ret = esp_bt_controller_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize BT controller: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "BT controller deinitialized");

    // Release controller memory if it was taken by ESP_BT_MODE_BLE
    // This is often done if you want to reconfigure for Classic BT or completely free resources.
    // ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    // For this example, we assume the initial mem_release for CLASSIC_BT was sufficient.

    ESP_LOGI(TAG, "BLE system terminated successfully");
    return ESP_OK;
}

// You'll need a function to start the scanning process.
// This could be called after bitmans_ble_init() is successful.
void bitmans_ble_start_scan(void)
{
    ESP_LOGI(TAG, "Starting BLE scan...");
    // Configure scan parameters
    esp_ble_scan_params_t ble_scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50, // N * 0.625ms
        .scan_window = 0x30,   // N * 0.625ms
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE};
    esp_err_t ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (ret)
    {
        ESP_LOGE(TAG, "Set scan params error, error code = %x", ret);
    }
    // Note: The actual scan will start in the ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT
    // in the esp_gap_cb callback.
}
