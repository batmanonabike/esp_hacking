#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "bitmans_lib.h"

static const char *TAG = "WifiConnect2";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting WifiConnect2 application");

    esp_err_t ret = bitmans_lib_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BitmansLib");
        return;
    }

    // Get the ProvideLib version
    const char *version = bitmans_lib_get_version();
    ESP_LOGI(TAG, "BitmansLib version: %s", version);

    bitmans_lib_log_message("Hello from WifiConnect2!");

    // Main loop
    int counter = 0;
    while (1) {
        char message[64];
        snprintf(message, sizeof(message), "Counter value: %d", counter++);
        
        // Use our shared component to log the message
        bitmans_lib_log_message(message);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
