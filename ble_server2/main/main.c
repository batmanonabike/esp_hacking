#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bat_lib.h"
#include "bat_config.h"
#include "bat_ble_lib.h"

static const char *TAG = "ble_server2_app";

static bat_ble_server_t bleServer;

// Callback for write events
static void on_write(void *pContext, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(TAG, "Received write: %.*s", param->write.len, param->write.value);
    
    // Echo back the received data as a notification
    bat_ble_server_notify(&bleServer, 0, param->write.value, param->write.len);
}

void app_main(void)
{
    ESP_LOGI(TAG, "App starting");

    bat_lib_t bat_lib;
    bat_ble_lib_t bat_ble_lib;
    ESP_ERROR_CHECK(bat_lib_init(&bat_lib));
    ESP_ERROR_CHECK(bat_ble_lib_init(bat_lib, &bat_ble_lib));

    // Define our service characteristics
    uint8_t initialValue[] = "Hello BLE";
    bat_ble_char_config_t charConfigs[] = 
    {
        {
            .uuid = 0xFF01,
            .permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
            .maxLen = 100,
            .pInitialValue = initialValue,
            .initValueLen = sizeof(initialValue) - 1
        }
    };
    
    bat_ble_server_callbacks_t callbacks = {
        .onWrite = on_write,
    };
    
    ESP_ERROR_CHECK(bat_ble_server_init2(&bleServer, "ESP32-BLE", 0x55, 0xFF00));
    ESP_ERROR_CHECK(bat_ble_server_create_service(&bleServer, 0xFF00, charConfigs, 1));
    ESP_ERROR_CHECK(bat_ble_server_set_callbacks(&bleServer, NULL, &callbacks));
    ESP_ERROR_CHECK(bat_ble_server_start(&bleServer));    
    ESP_LOGI(TAG, "App started");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(bat_ble_lib_deinit(bat_ble_lib));
    ESP_ERROR_CHECK(bat_lib_deinit(bat_lib));

    ESP_LOGI(TAG, "App exiting");
}
