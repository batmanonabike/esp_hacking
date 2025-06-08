#include <stdio.h>
#include <stdint.h>
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
#include "esp_bt_defs.h"
#include "esp_gatt_defs.h" // Added for ESP_UUID_LEN_XX and potentially esp_bt_uuid_t resolution

#include "bitmans_ble.h"
#include "bitmans_hash_table.h"
#include "bitmans_ble_client.h"
#include "bitmans_ble_client_logging.h"

// See: /docs/ble_intro.md
// Connection Process:
// 1. Advertiser sends advertising packets.
// 2. Scanner detects advertiser.
// 3. If connectable and desired, scanner (now initiator) sends a Connection Request.
// 4. If advertiser accepts (e.g., not using a Filter Accept List or initiator is on the list), connection is established.
static const char *TAG = "bitmans_lib:ble_client";

typedef struct
{
    void *pContext;
    uint8_t bda[ESP_BD_ADDR_LEN]; // Bluetooth Device Address (BDA)
} gap_bda_callback_entry_t;

#define GAP_CB_TABLE_SIZE 16
static gap_bda_callback_entry_t gap_cb_table[GAP_CB_TABLE_SIZE] = {0}; // Array for per-BDA callbacks
static esp_gatt_if_t g_gattc_handles[GATTC_APPLAST + 1]; // AppIds to GATT Client handles mapping

static bitmans_gap_callbacks_t *g_pGapCallbacks = NULL; 

// GAP (Generic Access Profile) events notify about BLE advertising, scanning, connection management, and security events.
// Common events include:
// - ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: Scan parameters set, ready to start scanning.
// - ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: Scanning has started.
// - ESP_GAP_BLE_SCAN_RESULT_EVT: Scan result received (device found).
// - ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: Scanning has stopped.
// - ESP_GAP_BLE_ADV_START_COMPLETE_EVT: Advertising has started (for peripheral/server roles).
// - ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: Advertising has stopped.
// - ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: Connection parameters updated.
// - ESP_GAP_BLE_SEC_REQ_EVT: Security request from peer device.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html#_CPPv426esp_gap_ble_cb_event_t
static void bitmans_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *pParam)
{
    assert(g_pGapCallbacks != NULL); // call bitmans_ble_gap_callbacks_init!

    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan parameters set, starting scan...");
        g_pGapCallbacks->on_scan_param_set_complete(g_pGapCallbacks, pParam);
        break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT");
        g_pGapCallbacks->on_scan_start_complete(g_pGapCallbacks, pParam);
        break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT");
        g_pGapCallbacks->on_update_conn_params(g_pGapCallbacks, pParam);
        break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_SEC_REQ_EVT");
        g_pGapCallbacks->on_sec_req(g_pGapCallbacks, pParam);
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
        g_pGapCallbacks->on_scan_result(g_pGapCallbacks, pParam);
        break;

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");
        g_pGapCallbacks->on_scan_stop_complete(g_pGapCallbacks, pParam);
        break;

    default:
        ESP_LOGI(TAG, "GAP Event: %d", event);
        break;
    }
}

// GATTC (GATT Client) events notify about important BLE client events.
// These include:
// - ESP_GATTC_REG_EVT: GATT client profile registered, usually where you start scanning or initiate connection.
// - ESP_GATTC_CONNECT_EVT: BLE physical link established (not GATT yet).
// - ESP_GATTC_OPEN_EVT: GATT connection established, ready for service discovery.
// - ESP_GATTC_DISCONNECT_EVT: GATT connection closed.
// - ESP_GATTC_SEARCH_RES_EVT: Service discovered on the server.
// - ESP_GATTC_SEARCH_CMPL_EVT: Service discovery complete.
// - ESP_GATTC_READ_CHAR_EVT: Characteristic value read from the server.
// - ESP_GATTC_WRITE_CHAR_EVT: Write operation to a characteristic completed.
// - ESP_GATTC_NOTIFY_EVT: Notification received from the server.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gattc.html#_CPPv426esp_gattc_cb_event_t
static void bitmans_gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    ESP_LOGI(TAG, "GATTC Event: %d, gattc_if %d", event, gattc_if);

    switch (event)
    {

    case ESP_GATTC_REG_EVT:
        if (param->reg.status == ESP_GATT_OK)
        {
            g_gattc_handles[param->reg.app_id] = gattc_if;
            ESP_LOGI(TAG, "GATTC app_id %d registered successfully, gattc_if %d stored", param->reg.app_id, gattc_if);
        }
        else
        {
            ESP_LOGE(TAG, "GATTC registration failed for app_id %04x, status %d", param->reg.app_id, param->reg.status);
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
            ESP_LOGE(TAG, "GATTC open failed, status %d, conn_id %d", param->open.status, param->open.conn_id);
        }
        else
        {
            ESP_LOGI(TAG, "GATTC open success, conn_id %d, mtu %d", param->open.conn_id, param->open.mtu);
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
        esp_bt_uuid_t *srvc_uuid = &param->search_res.srvc_id.uuid;
        if (srvc_uuid->len == ESP_UUID_LEN_16)
        {
            ESP_LOGI(TAG, "SERVICE UUID (16-bit): 0x%04x", srvc_uuid->uuid.uuid16);
        }
        else if (srvc_uuid->len == ESP_UUID_LEN_32)
        {
            ESP_LOGI(TAG, "SERVICE UUID (32-bit): 0x%08lx", (unsigned long)srvc_uuid->uuid.uuid32); // Use lx for uint32_t
        }
        else if (srvc_uuid->len == ESP_UUID_LEN_128)
        {
            ESP_LOGI(TAG, "SERVICE UUID (128-bit): %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     srvc_uuid->uuid.uuid128[0], srvc_uuid->uuid.uuid128[1], srvc_uuid->uuid.uuid128[2], srvc_uuid->uuid.uuid128[3],
                     srvc_uuid->uuid.uuid128[4], srvc_uuid->uuid.uuid128[5], srvc_uuid->uuid.uuid128[6], srvc_uuid->uuid.uuid128[7],
                     srvc_uuid->uuid.uuid128[8], srvc_uuid->uuid.uuid128[9], srvc_uuid->uuid.uuid128[10], srvc_uuid->uuid.uuid128[11],
                     srvc_uuid->uuid.uuid128[12], srvc_uuid->uuid.uuid128[13], srvc_uuid->uuid.uuid128[14], srvc_uuid->uuid.uuid128[15]);
        }
        else
        {
            ESP_LOGW(TAG, "SERVICE UUID: Unknown length %d", srvc_uuid->len);
        }
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

// See: /docs/ble_client_on_esp.md
esp_err_t bitmans_ble_client_init()
{
    ESP_LOGI(TAG, "Initializing BLE system");
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    for (int n = GATTC_APPFIRST; n <= GATTC_APPLAST; n++)
        g_gattc_handles[n] = ESP_GATT_IF_NONE;

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

    ret = esp_ble_gap_register_callback(bitmans_gap_event_handler);
    if (ret)
    {
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return ret;
    }

    ret = esp_ble_gattc_register_callback(bitmans_gattc_event_handler);
    if (ret)
    {
        ESP_LOGE(TAG, "GATTC register error, error code = %x", ret);
        return ret;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret)
    {
        ESP_LOGE(TAG, "set local MTU failed, error code = %x", local_mtu_ret);
    }

    ESP_LOGI(TAG, "BLE system initialized successfully");
    return ESP_OK;
}

static bool bitmans_ble_find_service_uuid_by_type(bitmans_scan_result_t *pScanResult, bitmans_ble_uuid128_t *pId, uint8_t type)
{
    uint8_t adv_data_len = 0;
    uint8_t *adv_data = esp_ble_resolve_adv_data(pScanResult->ble_adv, type, &adv_data_len);

    if (adv_data != NULL && adv_data_len > 0)
    {
        for (int i = 0; i < adv_data_len; i += ESP_UUID_LEN_128)
        {
            if (memcmp(&adv_data[i], pId, ESP_UUID_LEN_128) == 0)
            {
                ESP_LOGI(TAG, "Found custom service UUID (complete list)!");
                return true;
            }
        }
    }

    return false;
}

bool bitmans_ble_find_service_uuid(bitmans_scan_result_t *pScanResult, bitmans_ble_uuid128_t *pId)
{
    if (bitmans_ble_find_service_uuid_by_type(pScanResult, pId, ESP_BLE_AD_TYPE_128SRV_CMPL))
    {
        ESP_LOGI(TAG, "Found custom service UUID (complete list)!");
        return true;
    }

    if (bitmans_ble_find_service_uuid_by_type(pScanResult, pId, ESP_BLE_AD_TYPE_128SRV_PART))
    {
        ESP_LOGI(TAG, "Found custom service UUID (partial list)!");
        return true;
    }

    return false;
}

esp_err_t bitmans_ble_client_register_gattc(bitmans_gattc_app_id_t app_id)
{
    esp_err_t ret = esp_ble_gattc_app_register(app_id);
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "GATTC register ok for app_id %d.", app_id);
    else
        ESP_LOGE(TAG, "GATTC register error, app_id %d, error code = %x", app_id, ret);
    return ret;
}

esp_err_t bitmans_ble_client_unregister_gattc(bitmans_gattc_app_id_t app_id)
{
    if (g_gattc_handles[app_id] == ESP_GATT_IF_NONE)
        return ESP_OK;

    esp_err_t ret = esp_ble_gattc_app_unregister(g_gattc_handles[app_id]);
    if (ret == ESP_OK)
    {
        g_gattc_handles[app_id] = ESP_GATT_IF_NONE;
        ESP_LOGI(TAG, "GATTC unregister ok for app_id %d.", app_id);
    }
    else
    {
        ESP_LOGE(TAG, "GATTC unregister error, app_id %d, gattc_if %d, error code = %x",
                 app_id, g_gattc_handles[app_id], ret);
    }

    return ret;
}

esp_err_t bitmans_ble_client_term()
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

    // Unregister GATTC applications
    for (int n = GATTC_APPFIRST; n <= GATTC_APPLAST; n++)
        bitmans_ble_client_unregister_gattc(n);

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
esp_err_t bitmans_ble_set_scan_params()
{
    ESP_LOGI(TAG, "Starting BLE scan soon...");

    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html#_CPPv421esp_ble_scan_params_t
    esp_ble_scan_params_t ble_scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50, // N * 0.625ms
        .scan_window = 0x30,   // N * 0.625ms
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE};
    esp_err_t ret = esp_ble_gap_set_scan_params(&ble_scan_params);

    if (ret == ESP_OK)
    {
        // The actual scan will start with: ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT
        ESP_LOGI(TAG, "Set scan params Ok");
    }
    else
    {
        ESP_LOGE(TAG, "Set scan params error, error code = %x", ret);
    }

    return ret;
}

esp_err_t bitmans_ble_stop_scanning()
{
    ESP_LOGI(TAG, "Stopping BLE scan...");
    esp_err_t ret = esp_ble_gap_stop_scanning();
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Scan stop command sent successfully.");
        // The actual confirmation that the scan has stopped will come via the
        // ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT in the bitmans_gap_event_handler callback.
    }
    else if (ret == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "Scan stop command failed: No scan in progress or already stopping. Status: %s", esp_err_to_name(ret));
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to send scan stop command, error code = %x (%s)", ret, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t bitmans_ble_client_get_advertised_name(bitmans_scan_result_t *pScanResult, bitmans_advertised_name_t *pAdvertisedName)
{
    assert(pScanResult != NULL);
    assert(pAdvertisedName != NULL);

    pAdvertisedName->name[0] = '\0';

    if (pScanResult->adv_data_len > 0)
    {
        // Try to get the complete name or short name.
        uint8_t adv_name_len_val = 0;
        uint8_t *adv_name_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len_val);

        // If complete name not found or is empty, try short name
        if (adv_name_ptr == NULL || adv_name_len_val == 0)
            adv_name_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &adv_name_len_val);

        if (adv_name_ptr != NULL && adv_name_len_val > 0)
        {
            uint8_t copy_len = adv_name_len_val < (ADVERTISED_NAME_BUFFER_LEN - 1) ? adv_name_len_val : (ADVERTISED_NAME_BUFFER_LEN - 1);
            memcpy(pAdvertisedName->name, adv_name_ptr, copy_len);
            pAdvertisedName->name[copy_len] = '\0';
        }
    }

    return (pAdvertisedName->name[0] == '\0') ? ESP_ERR_NOT_FOUND : ESP_OK;
}

void *bitmans_bda_context_lookup(const esp_bd_addr_t *pbda)
{
    for (int i = 0; i < GAP_CB_TABLE_SIZE; ++i)
    {
        if (gap_cb_table[i].pContext != NULL && memcmp(gap_cb_table[i].bda, pbda, ESP_BD_ADDR_LEN) == 0)
            return gap_cb_table[i].pContext;
    }
    return NULL;
}

esp_err_t bitmans_ble_start_scanning(uint32_t scan_duration_secs)
{
    esp_err_t ret = esp_ble_gap_start_scanning(scan_duration_secs);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Scanning started for %lu seconds.", scan_duration_secs);
    }
    else
    {
        ESP_LOGE(TAG, "esp_ble_gap_start_scanning failed, error code = %x", ret);
    }

    return ret;
}

esp_err_t bitmans_bda_context_set(const esp_bd_addr_t *pbda, void *pContext)
{
    for (int i = 0; i < GAP_CB_TABLE_SIZE; ++i)
    {
        if (gap_cb_table[i].pContext == NULL)
        {
            memcpy(gap_cb_table[i].bda, pbda, ESP_BD_ADDR_LEN);
            gap_cb_table[i].pContext = pContext;
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "No space in GAP callback table for BDA: %02x:%02x:%02x:%02x:%02x:%02x",
             (unsigned int)pbda[0], (unsigned int)pbda[1], (unsigned int)pbda[2],
             (unsigned int)pbda[3], (unsigned int)pbda[4], (unsigned int)pbda[5]);

    return ESP_ERR_INVALID_STATE;
}

void bitmans_bda_context_reset(const esp_bd_addr_t *pbda)
{
    for (int i = 0; i < GAP_CB_TABLE_SIZE; ++i)
    {
        if (memcmp(gap_cb_table[i].bda, pbda, ESP_BD_ADDR_LEN) == 0)
        {
            memset(gap_cb_table[i].bda, 0, ESP_BD_ADDR_LEN);
            gap_cb_table[i].pContext = NULL;
            break;
        }
    }
}

void bitmans_gap_no_op(struct bitmans_gap_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
}

void bitmans_ble_gap_callbacks_init(bitmans_gap_callbacks_t *pCb, void *pContext)
{
    assert(pCb != NULL);

    g_pGapCallbacks = pCb;
    g_pGapCallbacks->pContext = pContext;
    if (g_pGapCallbacks->on_sec_req == NULL)
        g_pGapCallbacks->on_sec_req = bitmans_gap_no_op;
    if (g_pGapCallbacks->on_scan_result == NULL)
        g_pGapCallbacks->on_scan_result = bitmans_gap_no_op;
    if (g_pGapCallbacks->on_scan_stop_complete == NULL)
        g_pGapCallbacks->on_scan_stop_complete = bitmans_gap_no_op;
    if (g_pGapCallbacks->on_update_conn_params == NULL)
        g_pGapCallbacks->on_update_conn_params = bitmans_gap_no_op;
    if (g_pGapCallbacks->on_scan_start_complete == NULL)
        g_pGapCallbacks->on_scan_start_complete = bitmans_gap_no_op;
    if (g_pGapCallbacks->on_scan_param_set_complete == NULL)
        g_pGapCallbacks->on_scan_param_set_complete = bitmans_gap_no_op;
}
