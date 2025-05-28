#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

// Include our custom component header
#include "provide_lib.h"

static const char *TAG = "ConsumeLib";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ConsumeLib application");

    // Initialize our ProvideLib component
    esp_err_t ret = provide_lib_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ProvideLib");
        return;
    }

    // Get the ProvideLib version
    const char *version = provide_lib_get_version();
    ESP_LOGI(TAG, "ProvideLib version: %s", version);

    // Use ProvideLib to log messages
    provide_lib_log_message("Hello from ConsumeLib!");

    // Main loop
    int counter = 0;
    while (1) {
        char message[64];
        snprintf(message, sizeof(message), "Counter value: %d", counter++);
        
        // Use our shared component to log the message
        provide_lib_log_message(message);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
