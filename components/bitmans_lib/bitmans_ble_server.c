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

esp_err_t bitmans_ble_server_init()
{
    ESP_LOGI(TAG, "Initializing BLE GATT server");
    // TODO: Add initialization logic
    return ESP_OK;
}

esp_err_t bitmans_ble_server_term()
{
    ESP_LOGI(TAG, "Terminating BLE GATT server");
    // TODO: Add termination logic
    return ESP_OK;
}

esp_err_t bitmans_ble_server_register_gatts()
{
    ESP_LOGI(TAG, "Registering GATT server");
    // TODO: Add registration logic
    return ESP_OK;
}

esp_err_t bitmans_ble_server_unregister_gatts()
{
    ESP_LOGI(TAG, "Unregistering GATT server");
    // TODO: Add unregistration logic
    return ESP_OK;
}
