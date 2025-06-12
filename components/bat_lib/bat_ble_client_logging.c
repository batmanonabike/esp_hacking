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

#include "bat_ble_client_logging.h"

static const char *TAG = "bat_lib:ble_client_logging";

void bat_log_ble_scan(bat_scan_result_t *pScanResult, bool ignoreNoAdvertisedName) // Changed type to esp_ble_scan_result_evt_param_t
{
    assert(pScanResult != NULL);

    bat_advertised_name_t advertised_name;
    if ((bat_ble_client_get_advertised_name(pScanResult, &advertised_name) != ESP_OK) && ignoreNoAdvertisedName)
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

void bat_log_verbose_ble_scan(bat_scan_result_t *pScanResult, bool ignoreNoAdvertisedName)
{
    assert(pScanResult != NULL);

    bat_advertised_name_t advertised_name;
    if ((bat_ble_client_get_advertised_name(pScanResult, &advertised_name) != ESP_OK) && ignoreNoAdvertisedName)
        return;

    ESP_LOGI(TAG, "=== COMPREHENSIVE BLE DEVICE SCAN RESULT ===");
    ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
             pScanResult->bda[0], pScanResult->bda[1],
             pScanResult->bda[2], pScanResult->bda[3],
             pScanResult->bda[4], pScanResult->bda[5]);

    ESP_LOGI(TAG, "RSSI: %d dBm", pScanResult->rssi);
    ESP_LOGI(TAG, "Address Type: %s", pScanResult->ble_addr_type == BLE_ADDR_TYPE_PUBLIC ? "Public" : "Random");
    ESP_LOGI(TAG, "Device Type: %s",
             pScanResult->dev_type == ESP_BT_DEVICE_TYPE_BLE ? "BLE" : (pScanResult->dev_type == ESP_BT_DEVICE_TYPE_DUMO ? "Dual-Mode" : "Classic"));

    ESP_LOGI(TAG, "Advertising Data Length: %d bytes", pScanResult->adv_data_len);
    ESP_LOGI(TAG, "Scan Response Length: %d bytes", pScanResult->scan_rsp_len);

    // Debug raw advertising data
    if (pScanResult->adv_data_len > 0)
    {
        ESP_LOGI(TAG, "Raw Advertising Data:");
        esp_log_buffer_hex(TAG, pScanResult->ble_adv, pScanResult->adv_data_len);
    }

    // Log advertised name
    ESP_LOGI(TAG, "Advertised Name: %s", advertised_name.name);

    // Helper function to log advertising data with debug info
    uint8_t *data_ptr = NULL;
    uint8_t data_len = 0;

    // === FLAGS ===
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_FLAG, &data_len);
    if (data_ptr != NULL && data_len > 0)
    {
        ESP_LOGI(TAG, "Flags (len %d): 0x%02x", data_len, data_ptr[0]);
        if (data_ptr[0] & 0x01)
            ESP_LOGI(TAG, "  - LE Limited Discoverable Mode");
        if (data_ptr[0] & 0x02)
            ESP_LOGI(TAG, "  - LE General Discoverable Mode");
        if (data_ptr[0] & 0x04)
            ESP_LOGI(TAG, "  - BR/EDR Not Supported");
        if (data_ptr[0] & 0x08)
            ESP_LOGI(TAG, "  - Simultaneous LE and BR/EDR Controller");
        if (data_ptr[0] & 0x10)
            ESP_LOGI(TAG, "  - Simultaneous LE and BR/EDR Host");
    }
    else
    {
        ESP_LOGD(TAG, "DEBUG: No Flags found (ptr=%p, len=%d)", data_ptr, data_len);
    }

    // === TX POWER LEVEL ===
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_TX_PWR, &data_len);
    if (data_ptr != NULL && data_len > 0)
    {
        int8_t tx_power = (int8_t)data_ptr[0];
        ESP_LOGI(TAG, "TX Power Level: %d dBm", tx_power);
    }
    else
    {
        ESP_LOGD(TAG, "DEBUG: No TX Power Level found (ptr=%p, len=%d)", data_ptr, data_len);
    }

    // === APPEARANCE ===
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_APPEARANCE, &data_len);
    if (data_ptr != NULL && data_len >= 2)
    {
        uint16_t appearance = (data_ptr[1] << 8) | data_ptr[0]; // Little endian
        ESP_LOGI(TAG, "Appearance: 0x%04x", appearance);
    }
    else
    {
        ESP_LOGD(TAG, "DEBUG: No Appearance found (ptr=%p, len=%d)", data_ptr, data_len);
    }

    // === MANUFACTURER DATA ===
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, &data_len);
    if (data_ptr != NULL && data_len >= 2)
    {
        uint16_t company_id = (data_ptr[1] << 8) | data_ptr[0]; // Little endian
        ESP_LOGI(TAG, "Manufacturer Data (len %d):", data_len);
        ESP_LOGI(TAG, "  Company ID: 0x%04x", company_id);
        if (data_len > 2)
        {
            ESP_LOGI(TAG, "  Data:");
            esp_log_buffer_hex(TAG, data_ptr + 2, data_len - 2);
        }
    }
    else
    {
        ESP_LOGD(TAG, "DEBUG: No Manufacturer Data found (ptr=%p, len=%d)", data_ptr, data_len);
    }

    // === SERVICE DATA ===
    // 16-bit Service Data
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_SERVICE_DATA, &data_len);
    if (data_ptr != NULL && data_len >= 2)
    {
        uint16_t service_uuid = (data_ptr[1] << 8) | data_ptr[0]; // Little endian
        ESP_LOGI(TAG, "16-bit Service Data (len %d):", data_len);
        ESP_LOGI(TAG, "  Service UUID: 0x%04x", service_uuid);
        if (data_len > 2)
        {
            ESP_LOGI(TAG, "  Data:");
            esp_log_buffer_hex(TAG, data_ptr + 2, data_len - 2);
        }
    }
    else
    {
        ESP_LOGD(TAG, "DEBUG: No 16-bit Service Data found (ptr=%p, len=%d)", data_ptr, data_len);
    }

    // 32-bit Service Data
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_32SERVICE_DATA, &data_len);
    if (data_ptr != NULL && data_len >= 4)
    {
        uint32_t service_uuid = ((uint32_t)data_ptr[3] << 24) | ((uint32_t)data_ptr[2] << 16) |
                                ((uint32_t)data_ptr[1] << 8) | (uint32_t)data_ptr[0]; // Little endian
        ESP_LOGI(TAG, "32-bit Service Data (len %d):", data_len);
        ESP_LOGI(TAG, "  Service UUID: 0x%08lx", (unsigned long)service_uuid);
        if (data_len > 4)
        {
            ESP_LOGI(TAG, "  Data:");
            esp_log_buffer_hex(TAG, data_ptr + 4, data_len - 4);
        }
    }
    else
    {
        ESP_LOGD(TAG, "DEBUG: No 32-bit Service Data found (ptr=%p, len=%d)", data_ptr, data_len);
    }

    // 128-bit Service Data
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_128SERVICE_DATA, &data_len);
    if (data_ptr != NULL && data_len >= 16)
    {
        ESP_LOGI(TAG, "128-bit Service Data (len %d):", data_len);
        ESP_LOGI(TAG, "  Service UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 data_ptr[15], data_ptr[14], data_ptr[13], data_ptr[12],
                 data_ptr[11], data_ptr[10], data_ptr[9], data_ptr[8],
                 data_ptr[7], data_ptr[6], data_ptr[5], data_ptr[4],
                 data_ptr[3], data_ptr[2], data_ptr[1], data_ptr[0]);
        if (data_len > 16)
        {
            ESP_LOGI(TAG, "  Data:");
            esp_log_buffer_hex(TAG, data_ptr + 16, data_len - 16);
        }
    }
    else
    {
        ESP_LOGD(TAG, "DEBUG: No 128-bit Service Data found (ptr=%p, len=%d)", data_ptr, data_len);
    }

    // === SERVICE UUIDs (using existing logic but with enhanced debugging) ===
    ESP_LOGI(TAG, "=== SERVICE UUIDs ===");

    // --- 16-bit Service UUIDs ---
    // Complete list
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_16SRV_CMPL, &data_len);
    ESP_LOGD(TAG, "DEBUG: 16-bit Complete UUIDs - ptr=%p, len=%d", data_ptr, data_len);
    if (data_ptr != NULL && data_len > 0 && (data_len % ESP_UUID_LEN_16 == 0))
    {
        ESP_LOGI(TAG, "Complete 16-bit Service UUIDs (count %d):", data_len / ESP_UUID_LEN_16);
        for (int i = 0; i < data_len; i += ESP_UUID_LEN_16)
        {
            uint16_t service_uuid = (data_ptr[i + 1] << 8) | data_ptr[i]; // Little endian
            ESP_LOGI(TAG, "  - 0x%04x", service_uuid);
        }
    }
    else if (data_ptr != NULL)
    {
        ESP_LOGW(TAG, "WARNING: 16-bit Complete UUIDs data length mismatch (len=%d, expected multiple of %d)",
                 data_len, ESP_UUID_LEN_16);
    }

    // Partial list
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_16SRV_PART, &data_len);
    ESP_LOGD(TAG, "DEBUG: 16-bit Incomplete UUIDs - ptr=%p, len=%d", data_ptr, data_len);
    if (data_ptr != NULL && data_len > 0 && (data_len % ESP_UUID_LEN_16 == 0))
    {
        ESP_LOGI(TAG, "Incomplete 16-bit Service UUIDs (count %d):", data_len / ESP_UUID_LEN_16);
        for (int i = 0; i < data_len; i += ESP_UUID_LEN_16)
        {
            uint16_t service_uuid = (data_ptr[i + 1] << 8) | data_ptr[i]; // Little endian
            ESP_LOGI(TAG, "  - 0x%04x", service_uuid);
        }
    }
    else if (data_ptr != NULL)
    {
        ESP_LOGW(TAG, "WARNING: 16-bit Incomplete UUIDs data length mismatch (len=%d, expected multiple of %d)",
                 data_len, ESP_UUID_LEN_16);
    }

    // --- 32-bit Service UUIDs ---
    // Complete list
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_32SRV_CMPL, &data_len);
    ESP_LOGD(TAG, "DEBUG: 32-bit Complete UUIDs - ptr=%p, len=%d", data_ptr, data_len);
    if (data_ptr != NULL && data_len > 0 && (data_len % ESP_UUID_LEN_32 == 0))
    {
        ESP_LOGI(TAG, "Complete 32-bit Service UUIDs (count %d):", data_len / ESP_UUID_LEN_32);
        for (int i = 0; i < data_len; i += ESP_UUID_LEN_32)
        {
            uint32_t service_uuid = ((uint32_t)data_ptr[i + 3] << 24) | ((uint32_t)data_ptr[i + 2] << 16) |
                                    ((uint32_t)data_ptr[i + 1] << 8) | (uint32_t)data_ptr[i]; // Little endian
            ESP_LOGI(TAG, "  - 0x%08lx", (unsigned long)service_uuid);
        }
    }
    else if (data_ptr != NULL)
    {
        ESP_LOGW(TAG, "WARNING: 32-bit Complete UUIDs data length mismatch (len=%d, expected multiple of %d)",
                 data_len, ESP_UUID_LEN_32);
    }

    // Partial list
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_32SRV_PART, &data_len);
    ESP_LOGD(TAG, "DEBUG: 32-bit Incomplete UUIDs - ptr=%p, len=%d", data_ptr, data_len);
    if (data_ptr != NULL && data_len > 0 && (data_len % ESP_UUID_LEN_32 == 0))
    {
        ESP_LOGI(TAG, "Incomplete 32-bit Service UUIDs (count %d):", data_len / ESP_UUID_LEN_32);
        for (int i = 0; i < data_len; i += ESP_UUID_LEN_32)
        {
            uint32_t service_uuid = ((uint32_t)data_ptr[i + 3] << 24) | ((uint32_t)data_ptr[i + 2] << 16) |
                                    ((uint32_t)data_ptr[i + 1] << 8) | (uint32_t)data_ptr[i]; // Little endian
            ESP_LOGI(TAG, "  - 0x%08lx", (unsigned long)service_uuid);
        }
    }
    else if (data_ptr != NULL)
    {
        ESP_LOGW(TAG, "WARNING: 32-bit Incomplete UUIDs data length mismatch (len=%d, expected multiple of %d)",
                 data_len, ESP_UUID_LEN_32);
    }

    // --- 128-bit Service UUIDs ---
    // Complete list
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_128SRV_CMPL, &data_len);
    ESP_LOGD(TAG, "DEBUG: 128-bit Complete UUIDs - ptr=%p, len=%d", data_ptr, data_len);
    if (data_ptr != NULL && data_len > 0 && (data_len % ESP_UUID_LEN_128 == 0))
    {
        ESP_LOGI(TAG, "Complete 128-bit Service UUIDs (count %d):", data_len / ESP_UUID_LEN_128);
        for (int i = 0; i < data_len; i += ESP_UUID_LEN_128)
        {
            ESP_LOGI(TAG, "  - %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     data_ptr[i + 15], data_ptr[i + 14], data_ptr[i + 13], data_ptr[i + 12],
                     data_ptr[i + 11], data_ptr[i + 10], data_ptr[i + 9], data_ptr[i + 8],
                     data_ptr[i + 7], data_ptr[i + 6], data_ptr[i + 5], data_ptr[i + 4],
                     data_ptr[i + 3], data_ptr[i + 2], data_ptr[i + 1], data_ptr[i + 0]);
        }
    }
    else if (data_ptr != NULL)
    {
        ESP_LOGW(TAG, "WARNING: 128-bit Complete UUIDs data length mismatch (len=%d, expected multiple of %d)",
                 data_len, ESP_UUID_LEN_128);
    }

    // Partial list
    data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, ESP_BLE_AD_TYPE_128SRV_PART, &data_len);
    ESP_LOGD(TAG, "DEBUG: 128-bit Incomplete UUIDs - ptr=%p, len=%d", data_ptr, data_len);
    if (data_ptr != NULL && data_len > 0 && (data_len % ESP_UUID_LEN_128 == 0))
    {
        ESP_LOGI(TAG, "Incomplete 128-bit Service UUIDs (count %d):", data_len / ESP_UUID_LEN_128);
        for (int i = 0; i < data_len; i += ESP_UUID_LEN_128)
        {
            ESP_LOGI(TAG, "  - %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     data_ptr[i + 15], data_ptr[i + 14], data_ptr[i + 13], data_ptr[i + 12],
                     data_ptr[i + 11], data_ptr[i + 10], data_ptr[i + 9], data_ptr[i + 8],
                     data_ptr[i + 7], data_ptr[i + 6], data_ptr[i + 5], data_ptr[i + 4],
                     data_ptr[i + 3], data_ptr[i + 2], data_ptr[i + 1], data_ptr[i + 0]);
        }
    }
    else if (data_ptr != NULL)
    {
        ESP_LOGW(TAG, "WARNING: 128-bit Incomplete UUIDs data length mismatch (len=%d, expected multiple of %d)",
                 data_len, ESP_UUID_LEN_128);
    }

    // === SCAN RESPONSE DATA ===
    if (pScanResult->scan_rsp_len > 0)
    {
        ESP_LOGI(TAG, "=== SCAN RESPONSE DATA ===");
        ESP_LOGI(TAG, "Scan Response Data (len %d):", pScanResult->scan_rsp_len);
        esp_log_buffer_hex(TAG, pScanResult->ble_adv + pScanResult->adv_data_len, pScanResult->scan_rsp_len);
    }

    ESP_LOGI(TAG, "============================================");
}

void bat_debug_esp_ble_resolve_adv_data(bat_scan_result_t *pScanResult)
{
    ESP_LOGI(TAG, "=== DEBUG esp_ble_resolve_adv_data FUNCTION ===");
    ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
             pScanResult->bda[0], pScanResult->bda[1],
             pScanResult->bda[2], pScanResult->bda[3],
             pScanResult->bda[4], pScanResult->bda[5]);

    ESP_LOGI(TAG, "Advertising Data Length: %d", pScanResult->adv_data_len);
    ESP_LOGI(TAG, "Scan Response Length: %d", pScanResult->scan_rsp_len);

    if (pScanResult->adv_data_len == 0)
    {
        ESP_LOGW(TAG, "WARNING: No advertising data to parse");
        return;
    }

    ESP_LOGI(TAG, "Raw advertising data:");
    esp_log_buffer_hex(TAG, pScanResult->ble_adv, pScanResult->adv_data_len);    // Test all common advertising data types
    const uint8_t test_types[] = {
        ESP_BLE_AD_TYPE_FLAG,
        ESP_BLE_AD_TYPE_16SRV_PART,
        ESP_BLE_AD_TYPE_16SRV_CMPL,
        ESP_BLE_AD_TYPE_32SRV_PART,
        ESP_BLE_AD_TYPE_32SRV_CMPL,
        ESP_BLE_AD_TYPE_128SRV_PART,
        ESP_BLE_AD_TYPE_128SRV_CMPL,
        ESP_BLE_AD_TYPE_NAME_SHORT,
        ESP_BLE_AD_TYPE_NAME_CMPL,
        ESP_BLE_AD_TYPE_TX_PWR,
        ESP_BLE_AD_TYPE_DEV_CLASS,
        ESP_BLE_AD_TYPE_SERVICE_DATA,
        ESP_BLE_AD_TYPE_APPEARANCE,
        ESP_BLE_AD_TYPE_ADV_INT,
        ESP_BLE_AD_TYPE_32SERVICE_DATA,
        ESP_BLE_AD_TYPE_128SERVICE_DATA,
        ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE};

    const char *type_names[] = {
        "FLAGS",
        "16SRV_PART",
        "16SRV_CMPL",
        "32SRV_PART",
        "32SRV_CMPL",
        "128SRV_PART",
        "128SRV_CMPL",
        "NAME_SHORT",
        "NAME_CMPL",
        "TX_PWR",
        "DEV_CLASS",
        "SERVICE_DATA",
        "APPEARANCE",
        "ADV_INT",
        "32SERVICE_DATA",
        "128SERVICE_DATA",
        "MANUFACTURER_SPECIFIC"};

    for (int i = 0; i < sizeof(test_types) / sizeof(test_types[0]); i++)
    {
        uint8_t *data_ptr = NULL;
        uint8_t data_len = 0;

        data_ptr = esp_ble_resolve_adv_data(pScanResult->ble_adv, test_types[i], &data_len);

        ESP_LOGI(TAG, "Type 0x%02x (%s): ptr=%p, len=%d",
                 test_types[i], type_names[i], data_ptr, data_len);

        if (data_ptr != NULL && data_len > 0)
        {
            ESP_LOGI(TAG, "  Data: ");
            esp_log_buffer_hex(TAG, data_ptr, data_len);
        }
        else if (data_ptr != NULL && data_len == 0)
        {
            ESP_LOGW(TAG, "  WARNING: Non-null pointer but zero length!");
        }
    }

    ESP_LOGI(TAG, "===============================================");
}
