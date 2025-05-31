#include "esp_log.h"
#include "bitmans_lib.h"

static const char *TAG = "ble_connect_app";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application");
    
    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_init());
    ESP_ERROR_CHECK(bitmans_blink_init(-1));
    bitmans_set_blink_mode(BLINK_MODE_SLOW);

    ESP_LOGI(TAG, "Initialising application");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    bitmans_set_blink_mode(BLINK_MODE_BREATHING);
    for (int counter = 20; counter > 0; counter--) 
    {
        ESP_LOGI(TAG, "Running application: %d", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    bitmans_set_blink_mode(BLINK_MODE_FAST);
    ESP_LOGI(TAG, "Uninitialising application");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    
    bitmans_set_blink_mode(BLINK_MODE_VERY_FAST);
    ESP_LOGI(TAG, "Exiting application");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    bitmans_blink_term();
    bitmans_ble_term();
}
