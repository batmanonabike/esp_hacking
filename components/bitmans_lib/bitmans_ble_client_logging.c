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

#include "bitmans_ble_client_logging.h"

static const char *TAG = "bitmans_lib:ble_client_logging";

void bitmans_log_ble_scan(bitmans_scan_result_t *pScanResult, bool ignoreNoAdvertisedName) // Changed type to esp_ble_scan_result_evt_param_t
{
    assert(pScanResult != NULL);

    bitmans_advertised_name_t advertised_name;
    if ((bitmans_ble_client_get_advertised_name(pScanResult, &advertised_name) != ESP_OK) && ignoreNoAdvertisedName)
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
