#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bat_lib.h"
#include "bat_config.h"
#include "bat_ble_lib.h"
#include "bat_gatts_simple.h"

static const char *TAG = "ble_server2_app";

static bat_gatts_server_t bleServer = {0};

// Callback for write events
static void on_write(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Received write: %.*s", pParam->write.len, pParam->write.value);
    
    // Echo back the received data as a notification
    bat_gatts_notify(pServer, 0, pParam->write.value, pParam->write.len);
}

static void on_disconnect(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    // Auto restart advertising on disconnect
    bat_ble_gap_start_advertising(pServer->pAdvParams);
}

void app_main(void)
{
    ESP_LOGI(TAG, "App starting");

    ESP_ERROR_CHECK(bat_lib_init());
    ESP_ERROR_CHECK(bat_ble_lib_init());

    // Define our service characteristics
    uint8_t initialValue[] = "Hello BLE";
    bat_gatts_char_config_t charConfigs[] = 
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
    
    bat_gatts_callbacks2_t callbacks = {
        .onWrite = on_write,
        .onDisconnect = on_disconnect,
    };

    const int timeoutMs = 5000; 
    const char * pServiceUuid = "f0debc9a-7856-3412-1234-56785612561A"; 
    ESP_ERROR_CHECK(bat_gatts_init(&bleServer, NULL, "Martyn", 0x55, pServiceUuid, 0x0944, timeoutMs));
    //ESP_ERROR_CHECK(bat_gatts_init(&bleServer, NULL, NULL, 0x55, pServiceUuid, 0x0180, timeoutMs));
    ESP_ERROR_CHECK(bat_gatts_create_service(&bleServer, charConfigs, 1, timeoutMs));
    ESP_ERROR_CHECK(bat_gatts_start(&bleServer, &callbacks, timeoutMs));   

    ESP_LOGI(TAG, "App running");
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(bat_gatts_deinit(&bleServer));
    ESP_ERROR_CHECK(bat_ble_lib_deinit());
    ESP_ERROR_CHECK(bat_lib_deinit());

    ESP_LOGI(TAG, "App exiting");
}
