#include "esp_log.h"
#include "bitmans_lib.h"
#include "bitmans_ble_server.h"

static const char *TAG = "ble_server_app";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BLE server application");
    
    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());
    ESP_ERROR_CHECK(bitmans_ble_server_register_gatts());

    ESP_LOGI(TAG, "BLE server running");
    for (int counter = 200; counter > 0; counter--) 
    {
        ESP_LOGI(TAG, "Running BLE server: %d", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Uninitialising BLE server");
    bitmans_ble_server_unregister_gatts();
    bitmans_ble_server_term();
}
