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
#include "esp_bt_defs.h"
#include "esp_gatt_defs.h" // Added for ESP_UUID_LEN_XX and potentially esp_bt_uuid_t resolution

#include "bitmans_ble.h"

// See: /docs/ble_intro.md
// Connection Process:
// 1.  Advertiser sends advertising packets.
// 2. Scanner detects advertiser.
// 3. If connectable and desired, scanner (now initiator) sends a Connection Request.
// 4. If advertiser accepts (e.g., not using a Filter Accept List or initiator is on the list), connection is established.
static const char *TAG = "bitmans_lib:ble";

static uint32_t g_scan_duration_secs = 0;
static esp_gatt_if_t g_gattc_handles[GATTC_APPLAST + 1];

// TODO: Use calls to set this outside of this library, don't use globals.
// Define your custom service UUID here
// DA782B1A-F2F0-4C88-B47A-E93F3A55A9C5
// Convert to little-endian byte order for comparison with advertising data
static const uint8_t custom_service_uuid[ESP_UUID_LEN_128] = {
    0xC5, 0xA9, 0x55, 0x3A, 0x3F, 0xE9, 0x7A, 0xB4,
    0x88, 0x4C, 0xF0, 0xF2, 0x1A, 0x2B, 0x78, 0xDA};

static void log_ble_scan(const ble_scan_result_t *pScanResult, bool ignoreNoAdvertisedName) // Changed type to esp_ble_scan_result_evt_param_t
{
    assert(pScanResult != NULL);

    advertised_name_t advertised_name;
    if ((bitmans_ble_get_advertised_name(pScanResult, &advertised_name) != ESP_OK) && ignoreNoAdvertisedName)
        return;

    ESP_LOGI(TAG, "Device found (ptr): ADDR: %02x:%02x:%02x:%02x:%02x:%02x",
             pScanResult->bda[0], pScanResult->bda[1],
             pScanResult->bda[2], pScanResult->bda[3],
             pScanResult->bda[4], pScanResult->bda[5]);

    ESP_LOGI(TAG, "  RSSI: %d dBm", pScanResult->rssi);
    ESP_LOGI(TAG, "  Address Type: %s", pScanResult->ble_addr_type == BLE_ADDR_TYPE_PUBLIC ? "Public" : "Random");
    ESP_LOGI(TAG, "  Device Type: %s",
             pScanResult->dev_type == ESP_BT_DEVICE_TYPE_BLE ? "BLE" : (pScanResult->dev_type == ESP_BT_DEVICE_TYPE_DUMO ? "Dual-Mode" : "Classic"));

    // Log advertising data
    ESP_LOGI(TAG, "  Advertising Data (len %d):", pScanResult->adv_data_len);
    ESP_LOGI(TAG, "  Advertised Name: %s", advertised_name.name);

    // Log advertised service UUIDs
    uint8_t *uuid_data_ptr = NULL;
    uint8_t uuid_data_len = 0;

    // --- 16-bit Service UUIDs ---
    // Complete list
    uuid_data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_16SRV_CMPL, &uuid_data_len);
    if (uuid_data_ptr != NULL && uuid_data_len > 0 && (uuid_data_len % ESP_UUID_LEN_16 == 0))
    {
        ESP_LOGI(TAG, "  Complete 16-bit Service UUIDs (count %d):", uuid_data_len / ESP_UUID_LEN_16);
        for (int i = 0; i < uuid_data_len; i += ESP_UUID_LEN_16)
        {
            uint16_t service_uuid = (uuid_data_ptr[i + 1] << 8) | uuid_data_ptr[i]; // BLE UUIDs are Little Endian
            ESP_LOGI(TAG, "    - 0x%04x", service_uuid);
        }
    }
    // Partial list
    uuid_data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_16SRV_PART, &uuid_data_len);
    if (uuid_data_ptr != NULL && uuid_data_len > 0 && (uuid_data_len % ESP_UUID_LEN_16 == 0))
    {
        ESP_LOGI(TAG, "  Incomplete 16-bit Service UUIDs (count %d):", uuid_data_len / ESP_UUID_LEN_16);
        for (int i = 0; i < uuid_data_len; i += ESP_UUID_LEN_16)
        {
            uint16_t service_uuid = (uuid_data_ptr[i + 1] << 8) | uuid_data_ptr[i]; // BLE UUIDs are Little Endian
            ESP_LOGI(TAG, "    - 0x%04x", service_uuid);
        }
    }

    // --- 32-bit Service UUIDs ---
    // Complete list
    uuid_data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_32SRV_CMPL, &uuid_data_len);
    if (uuid_data_ptr != NULL && uuid_data_len > 0 && (uuid_data_len % ESP_UUID_LEN_32 == 0))
    {
        ESP_LOGI(TAG, "  Complete 32-bit Service UUIDs (count %d):", uuid_data_len / ESP_UUID_LEN_32);
        for (int i = 0; i < uuid_data_len; i += ESP_UUID_LEN_32)
        {
            uint32_t service_uuid = ((uint32_t)uuid_data_ptr[i + 3] << 24) | ((uint32_t)uuid_data_ptr[i + 2] << 16) | ((uint32_t)uuid_data_ptr[i + 1] << 8) | (uint32_t)uuid_data_ptr[i]; // BLE UUIDs are Little Endian
            ESP_LOGI(TAG, "    - 0x%08lx", (unsigned long)service_uuid);
        }
    }
    // Partial list
    uuid_data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_32SRV_PART, &uuid_data_len);
    if (uuid_data_ptr != NULL && uuid_data_len > 0 && (uuid_data_len % ESP_UUID_LEN_32 == 0))
    {
        ESP_LOGI(TAG, "  Incomplete 32-bit Service UUIDs (count %d):", uuid_data_len / ESP_UUID_LEN_32);
        for (int i = 0; i < uuid_data_len; i += ESP_UUID_LEN_32)
        {
            uint32_t service_uuid = ((uint32_t)uuid_data_ptr[i + 3] << 24) | ((uint32_t)uuid_data_ptr[i + 2] << 16) | ((uint32_t)uuid_data_ptr[i + 1] << 8) | (uint32_t)uuid_data_ptr[i]; // BLE UUIDs are Little Endian
            ESP_LOGI(TAG, "    - 0x%08lx", (unsigned long)service_uuid);
        }
    }

    // --- 128-bit Service UUIDs ---
    // Complete list
    uuid_data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_128SRV_CMPL, &uuid_data_len);
    if (uuid_data_ptr != NULL && uuid_data_len > 0 && (uuid_data_len % ESP_UUID_LEN_128 == 0))
    {
        ESP_LOGI(TAG, "  Complete 128-bit Service UUIDs (count %d):", uuid_data_len / ESP_UUID_LEN_128);
        for (int i = 0; i < uuid_data_len; i += ESP_UUID_LEN_128)
        {
            ESP_LOGI(TAG, "    - %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     uuid_data_ptr[i + 15], uuid_data_ptr[i + 14], uuid_data_ptr[i + 13], uuid_data_ptr[i + 12],
                     uuid_data_ptr[i + 11], uuid_data_ptr[i + 10], uuid_data_ptr[i + 9], uuid_data_ptr[i + 8],
                     uuid_data_ptr[i + 7], uuid_data_ptr[i + 6], uuid_data_ptr[i + 5], uuid_data_ptr[i + 4],
                     uuid_data_ptr[i + 3], uuid_data_ptr[i + 2], uuid_data_ptr[i + 1], uuid_data_ptr[i + 0]);
        }
    }
    // Partial list
    uuid_data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_128SRV_PART, &uuid_data_len);
    if (uuid_data_ptr != NULL && uuid_data_len > 0 && (uuid_data_len % ESP_UUID_LEN_128 == 0))
    {
        ESP_LOGI(TAG, "  Incomplete 128-bit Service UUIDs (count %d):", uuid_data_len / ESP_UUID_LEN_128);
        for (int i = 0; i < uuid_data_len; i += ESP_UUID_LEN_128)
        {
            ESP_LOGI(TAG, "    - %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     uuid_data_ptr[i + 15], uuid_data_ptr[i + 14], uuid_data_ptr[i + 13], uuid_data_ptr[i + 12],
                     uuid_data_ptr[i + 11], uuid_data_ptr[i + 10], uuid_data_ptr[i + 9], uuid_data_ptr[i + 8],
                     uuid_data_ptr[i + 7], uuid_data_ptr[i + 6], uuid_data_ptr[i + 5], uuid_data_ptr[i + 4],
                     uuid_data_ptr[i + 3], uuid_data_ptr[i + 2], uuid_data_ptr[i + 1], uuid_data_ptr[i + 0]);
        }
    }

    // Log scan response data (if present, usually for active scans)
    if (pScanResult->scan_rsp_len > 0)
    {
        ESP_LOGI(TAG, "  Scan Response Data (len %d):", pScanResult->scan_rsp_len);
        // The scan response data starts immediately after the advertising data in the ble_adv buffer
        // esp_log_buffer_hex(TAG, pScanResult->ble_adv + pScanResult->adv_data_len, pScanResult->scan_rsp_len); // Temporarily commented out
    }
}

static bool find_custom_service_uuid(const ble_scan_result_t *scan_rst)
{
    uint8_t *adv_data = NULL;
    uint8_t adv_data_len = 0;

    // Check complete list of 128-bit service UUIDs
    adv_data = esp_ble_resolve_adv_data(scan_rst->ble_adv,
                                        ESP_BLE_AD_TYPE_128SRV_CMPL, &adv_data_len);
    if (adv_data != NULL && adv_data_len > 0)
    {
        for (int i = 0; i < adv_data_len; i += ESP_UUID_LEN_128)
        {
            if (memcmp(&adv_data[i], custom_service_uuid, ESP_UUID_LEN_128) == 0)
            {
                ESP_LOGI(TAG, "Found custom service UUID (complete list)!");
                return true;
            }
        }
    }

    // Check partial list of 128-bit service UUIDs
    adv_data = esp_ble_resolve_adv_data(scan_rst->ble_adv,
                                        ESP_BLE_AD_TYPE_128SRV_PART, &adv_data_len);
    if (adv_data != NULL && adv_data_len > 0)
    {
        for (int i = 0; i < adv_data_len; i += ESP_UUID_LEN_128)
        {
            if (memcmp(&adv_data[i], custom_service_uuid, ESP_UUID_LEN_128) == 0)
            {
                ESP_LOGI(TAG, "Found custom service UUID (partial list)!");
                return true;
            }
        }
    }
    return false;
}

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
        esp_err_t ret = esp_ble_gap_start_scanning(g_scan_duration_secs);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Scanning started for %lu seconds.", g_scan_duration_secs);
        }
        else
        {
            ESP_LOGE(TAG, "esp_ble_gap_start_scanning failed, error code = %x", ret);
        }
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
            log_ble_scan(&scan_result->scan_rst, true);

            // Here you would check if this is the server you want to connect to
            // For example, by checking the advertised name or service UUID
            // If it is, you would call esp_ble_gap_stop_scanning() and then esp_ble_gattc_open()
            if (find_custom_service_uuid(&scan_result->scan_rst))
            {
                ESP_LOGI(TAG, "Device with custom service UUID found. BDA: %02x:%02x:%02x:%02x:%02x:%02x",
                         scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1],
                         scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3],
                         scan_result->scan_rst.bda[4], scan_result->scan_rst.bda[5]);

                // Store BDA for connection if needed:
                // memcpy(g_target_bda, scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
                // g_target_addr_type = scan_result->scan_rst.ble_addr_type;

                esp_err_t stop_err = esp_ble_gap_stop_scanning();
                if (stop_err == ESP_OK)
                {
                    ESP_LOGI(TAG, "Stopping scan to connect to the desired device.");
                }
                else
                {
                    ESP_LOGE(TAG, "esp_ble_gap_stop_scanning failed, error code = %x", stop_err);
                }
                // The connection attempt should ideally happen in ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT
            }
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
esp_err_t bitmans_ble_init()
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

    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret)
    {
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return ret;
    }

    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
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

esp_err_t bitmans_ble_register_gattc(gattc_app_id_t app_id)
{
    esp_err_t ret = esp_ble_gattc_app_register(app_id);
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "GATTC register ok for app_id %d.", app_id);
    else
        ESP_LOGE(TAG, "GATTC register error, app_id %d, error code = %x", app_id, ret);
    return ret;
}

esp_err_t bitmans_ble_unregister_gattc(gattc_app_id_t app_id)
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

    // Unregister GATTC applications
    for (int n = GATTC_APPFIRST; n <= GATTC_APPLAST; n++)
        bitmans_ble_unregister_gattc(n);

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
esp_err_t bitmans_ble_start_scan(uint32_t scan_duration_secs)
{
    ESP_LOGI(TAG, "Starting BLE scan for %lu seconds...", scan_duration_secs);

    g_scan_duration_secs = scan_duration_secs;

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

esp_err_t bitmans_ble_stop_scan()
{
    ESP_LOGI(TAG, "Stopping BLE scan...");
    esp_err_t ret = esp_ble_gap_stop_scanning();
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Scan stop command sent successfully.");
        // The actual confirmation that the scan has stopped will come via the
        // ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT in the esp_gap_cb callback.
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

esp_err_t bitmans_ble_get_advertised_name(const ble_scan_result_t *pScanResult, advertised_name_t *pAdvertisedName) 
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

/**
 * @brief Converts a 128-bit UUID string to a 16-byte array (little-endian)
 *        and stores it in the service_uuid_t struct.
 *
 * The UUID string must be in the format "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" (36 characters).
 * The output byte array (in out_struct->service_uuid) will have the byte order reversed
 * compared to the string representation, which is common for 128-bit service UUIDs
 * in BLE advertising data.
 *
 * @param uuid_str The null-terminated UUID string.
 * @param out_struct Pointer to a service_uuid_t struct where the converted UUID will be stored.
 * @return true if conversion was successful, false otherwise (e.g., invalid format, null pointers).
 */
esp_err_t bitmans_ble_uuid_to_service_uuid_t(const char *pszUUID, service_uuid_t *pServiceUUID)
{
    if (pszUUID == NULL || pServiceUUID == NULL)
    {
        ESP_LOGE(TAG, "bitmans_uuid_to_service_uuid_t: Null pointer provided.");
        return ESP_ERR_INVALID_STATE;
    }

    // Validate length and hyphen positions
    if (strlen(pszUUID) != 36 ||
        pszUUID[8] != '-' || pszUUID[13] != '-' ||
        pszUUID[18] != '-' || pszUUID[23] != '-')
    {
        ESP_LOGE(TAG, "bitmans_uuid_to_service_uuid_t: Invalid UUID string format or length: %s", pszUUID);
        return ESP_ERR_INVALID_STATE;
    }

    int parts[ESP_UUID_LEN_128]; 
    uint8_t temp_uuid_big_endian[ESP_UUID_LEN_128];

    int ret = sscanf(pszUUID,
                     "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
                     &parts[0], &parts[1], &parts[2], &parts[3],
                     &parts[4], &parts[5], &parts[6], &parts[7],
                     &parts[8], &parts[9], &parts[10], &parts[11],
                     &parts[12], &parts[13], &parts[14], &parts[15]);

    if (ret != ESP_UUID_LEN_128)
    {
        ESP_LOGE(TAG, "bitmans_uuid_to_service_uuid_t: Failed to parse UUID string '%s', sscanf returned %d.", pszUUID, ret);
        return ESP_ERR_INVALID_STATE; // Failed to parse all 16 parts
    }

    // Convert parsed integers to uint8_t and store in big-endian temporary array
    for (int i = 0; i < ESP_UUID_LEN_128; ++i)
    {
        if (parts[i] < 0 || parts[i] > 255)
        {
            ESP_LOGE(TAG, "bitmans_uuid_to_service_uuid_t: Parsed UUID component out of byte range: %d at index %d for UUID %s", parts[i], i, pszUUID);
            return ESP_ERR_INVALID_STATE;
        }
        temp_uuid_big_endian[i] = (uint8_t)parts[i];
    }

    // Reverse the byte order to get the little-endian representation
    // and store it in the char array of the output struct.
    for (int i = 0; i < ESP_UUID_LEN_128; ++i)
        pServiceUUID->service_uuid[i] = temp_uuid_big_endian[ESP_UUID_LEN_128 - 1 - i];

    return ESP_OK;
}
