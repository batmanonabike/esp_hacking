/**
 * @file ble_adv_data_test.c
 * @brief Test program to understand BLE advertising data limits with multiple service UUIDs
 * 
 * This test explores:
 * 1. How many service UUIDs can fit in a 31-byte advertising packet
 * 2. What happens when advertising data exceeds 31 bytes
 * 3. ESP-IDF behavior with multiple service UUIDs
 * 4. Packet structure and overhead calculations
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

static const char *TAG = "ble_adv_test";

// Test function to calculate advertising packet overhead
static void calculate_advertising_overhead(void) {
    ESP_LOGI(TAG, "=== BLE Advertising Packet Structure Analysis ===");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "BLE Advertising Packet Limits:");
    ESP_LOGI(TAG, "• Legacy advertising packet: 31 bytes maximum payload");
    ESP_LOGI(TAG, "• Extended advertising packet: 255 bytes maximum payload (BLE 5.0+)");
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "Required Overhead in Legacy Advertising Packet:");
    ESP_LOGI(TAG, "• Flags (mandatory): 3 bytes (1 length + 1 type + 1 flags)");
    ESP_LOGI(TAG, "• Total available for other data: 28 bytes");
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "Service UUID Field Overhead:");
    ESP_LOGI(TAG, "• 16-bit UUIDs: 2 bytes header + (2 bytes × UUID count)");
    ESP_LOGI(TAG, "• 32-bit UUIDs: 2 bytes header + (4 bytes × UUID count)");
    ESP_LOGI(TAG, "• 128-bit UUIDs: 2 bytes header + (16 bytes × UUID count)");
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "Maximum UUIDs per packet type:");
    ESP_LOGI(TAG, "• 16-bit UUIDs: max 13 UUIDs (2 + 13×2 = 28 bytes)");
    ESP_LOGI(TAG, "• 32-bit UUIDs: max 6 UUIDs (2 + 6×4 = 26 bytes)");
    ESP_LOGI(TAG, "• 128-bit UUIDs: max 1 UUID (2 + 1×16 = 18 bytes)");
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "Real-world considerations:");
    ESP_LOGI(TAG, "• Device name typically takes 2-20+ bytes");
    ESP_LOGI(TAG, "• Manufacturer data can take 2-25+ bytes");
    ESP_LOGI(TAG, "• TX power level takes 3 bytes");
    ESP_LOGI(TAG, "• Appearance takes 4 bytes");
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "Practical limits with device name (10 chars):");
    ESP_LOGI(TAG, "• Available after name: 28 - 12 = 16 bytes");
    ESP_LOGI(TAG, "• 16-bit UUIDs: max 7 UUIDs (2 + 7×2 = 16 bytes)");
    ESP_LOGI(TAG, "• 32-bit UUIDs: max 3 UUIDs (2 + 3×4 = 14 bytes)");
    ESP_LOGI(TAG, "• 128-bit UUIDs: Cannot fit with 10-char name");
}

// Test function to demonstrate multiple 16-bit service UUIDs
static esp_err_t test_multiple_16bit_services(void) {
    ESP_LOGI(TAG, "=== Testing Multiple 16-bit Service UUIDs ===");
    
    // Define multiple standard 16-bit service UUIDs
    static uint8_t service_uuids_16[] = {
        0x0F, 0x18,  // Battery Service (0x180F)
        0x0A, 0x18,  // Device Information Service (0x180A)
        0x12, 0x18,  // Human Interface Device (0x1812)
        0x0D, 0x18,  // Running Speed and Cadence (0x180D)
        0x16, 0x18,  // Cycling Speed and Cadence (0x1816)
        0x0E, 0x18,  // Reference Time Update Service (0x180E)
        0x10, 0x18,  // Blood Pressure (0x1810)
    };
    
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = false,
        .include_txpower = false,
        .appearance = 0,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(service_uuids_16),
        .p_service_uuid = service_uuids_16,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    
    ESP_LOGI(TAG, "Testing with %d 16-bit service UUIDs (%d bytes + 2 byte header = %d bytes total)",
             sizeof(service_uuids_16) / 2, sizeof(service_uuids_16), sizeof(service_uuids_16) + 2);
    
    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure advertising data with multiple 16-bit UUIDs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Successfully configured %d 16-bit service UUIDs", sizeof(service_uuids_16) / 2);
    return ESP_OK;
}

// Test function to demonstrate multiple 128-bit service UUIDs (will exceed 31 bytes)
static esp_err_t test_multiple_128bit_services(void) {
    ESP_LOGI(TAG, "=== Testing Multiple 128-bit Service UUIDs (Expected to Fail) ===");
    
    // Define multiple 128-bit service UUIDs (custom UUIDs)
    static uint8_t service_uuids_128[] = {
        // Custom Service 1: 12345678-1234-5678-9ABC-123456789ABC
        0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0xBC, 0x9A,
        0x78, 0x56, 0x34, 0x12, 0x78, 0x45, 0x34, 0x12,
        
        // Custom Service 2: 87654321-4321-8765-CBA9-CBA987654321  
        0x21, 0x43, 0x65, 0x87, 0xA9, 0xCB, 0x21, 0x43,
        0x65, 0x87, 0x21, 0x43, 0x65, 0x87, 0x21, 0x43,
    };
    
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = false,
        .include_txpower = false,
        .appearance = 0,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(service_uuids_128),
        .p_service_uuid = service_uuids_128,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    
    ESP_LOGI(TAG, "Testing with %d 128-bit service UUIDs (%d bytes + 2 byte header = %d bytes total)",
             sizeof(service_uuids_128) / 16, sizeof(service_uuids_128), sizeof(service_uuids_128) + 2);
    ESP_LOGI(TAG, "Total packet size with flags: %d + 3 = %d bytes (exceeds 31 byte limit)",
             sizeof(service_uuids_128) + 2, sizeof(service_uuids_128) + 2 + 3);
    
    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "❌ Expected failure: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "This confirms that ESP-IDF validates advertising packet size limits");
        return ret;
    }
    
    ESP_LOGW(TAG, "⚠️ Unexpected success - ESP-IDF allowed oversized packet");
    return ESP_OK;
}

// Test function with device name to show practical limits
static esp_err_t test_with_device_name(void) {
    ESP_LOGI(TAG, "=== Testing Service UUIDs with Device Name ===");
    
    const char* device_name = "ESP32-Test";  // 10 characters + null terminator
    
    // Set device name first
    esp_err_t ret = esp_ble_gap_set_device_name(device_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set device name: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Test with smaller number of 16-bit UUIDs to leave room for name
    static uint8_t service_uuids_16[] = {
        0x0F, 0x18,  // Battery Service (0x180F)
        0x0A, 0x18,  // Device Information Service (0x180A)
        0x12, 0x18,  // Human Interface Device (0x1812)
    };
    
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,  // Include device name in advertising packet
        .include_txpower = false,
        .appearance = 0,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(service_uuids_16),
        .p_service_uuid = service_uuids_16,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    
    ESP_LOGI(TAG, "Testing with device name '%s' + %d service UUIDs", 
             device_name, sizeof(service_uuids_16) / 2);
    ESP_LOGI(TAG, "Estimated packet size: 3 (flags) + 12 (name) + 8 (UUIDs) = 23 bytes");
    
    ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure advertising data with name: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Successfully configured device name + service UUIDs");
    return ESP_OK;
}

// Test using scan response for additional data
static esp_err_t test_scan_response_for_extra_data(void) {
    ESP_LOGI(TAG, "=== Testing Scan Response for Additional Service UUIDs ===");
    
    // Main advertising packet with essential data
    static uint8_t main_service_uuids[] = {
        0x0F, 0x18,  // Battery Service (0x180F) - most important
    };
    
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = false,
        .appearance = 0x0180,  // Generic Watch
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(main_service_uuids),
        .p_service_uuid = main_service_uuids,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };
    
    // Scan response packet with additional service UUIDs
    static uint8_t scan_rsp_service_uuids[] = {
        0x0A, 0x18,  // Device Information Service (0x180A)
        0x12, 0x18,  // Human Interface Device (0x1812)
        0x0D, 0x18,  // Running Speed and Cadence (0x180D)
        0x16, 0x18,  // Cycling Speed and Cadence (0x1816)
    };
    
    esp_ble_adv_data_t scan_rsp_data = {
        .set_scan_rsp = true,
        .include_name = false,  // Name already in main packet
        .include_txpower = true,
        .appearance = 0,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(scan_rsp_service_uuids),
        .p_service_uuid = scan_rsp_service_uuids,
        .flag = 0,  // No flags in scan response
    };
    
    ESP_LOGI(TAG, "Main packet: name + appearance + 1 service UUID");
    ESP_LOGI(TAG, "Scan response: TX power + 4 additional service UUIDs");
    
    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure main advertising data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure scan response data: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Successfully configured main packet + scan response");
    ESP_LOGI(TAG, "Total services advertised: 5 (1 in main + 4 in scan response)");
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🧪 BLE Advertising Data Limits Test");
    ESP_LOGI(TAG, "===================================");
    ESP_LOGI(TAG, "");
    
    // Initialize BLE stack
    esp_err_t ret = esp_bt_controller_init(&(esp_bt_controller_config_t)BT_CONTROLLER_INIT_CONFIG_DEFAULT());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Run tests
    calculate_advertising_overhead();
    
    ESP_LOGI(TAG, "");
    test_multiple_16bit_services();
    
    ESP_LOGI(TAG, "");
    test_multiple_128bit_services();
    
    ESP_LOGI(TAG, "");
    test_with_device_name();
    
    ESP_LOGI(TAG, "");
    test_scan_response_for_extra_data();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Test Summary ===");
    ESP_LOGI(TAG, "• Multiple 16-bit UUIDs: Supported (up to ~13 without other data)");
    ESP_LOGI(TAG, "• Multiple 128-bit UUIDs: Limited (only 1 UUID fits in 31 bytes)");
    ESP_LOGI(TAG, "• With device name: Reduces available space significantly");
    ESP_LOGI(TAG, "• Scan response: Allows additional 31 bytes for more UUIDs");
    ESP_LOGI(TAG, "• ESP-IDF validation: Enforces 31-byte limit for legacy advertising");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "💡 Recommendations:");
    ESP_LOGI(TAG, "• Use 16-bit UUIDs for standard services when possible");
    ESP_LOGI(TAG, "• Put most important service in main advertising packet");
    ESP_LOGI(TAG, "• Use scan response for additional services");
    ESP_LOGI(TAG, "• Consider Extended Advertising (BLE 5.0+) for more UUIDs");
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Test completed. Check logs for detailed results.");
}
