#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "bitmans_lib.h"

static const char *TAG = "Blinky";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Blinky application");

    esp_err_t ret = bitmans_lib_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BitmansLib");
        return;
    }

    const char *version = bitmans_lib_get_version();
    ESP_LOGI(TAG, "BitmansLib version: %s", version);

    bitmans_blink_init(-1);

    while (1) 
    {
        bitmans_lib_log_message("BLINK_MODE_SLOW");
        bitmans_set_blink_mode(BLINK_MODE_SLOW);
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        bitmans_lib_log_message("BLINK_MODE_MEDIUM");
        bitmans_set_blink_mode(BLINK_MODE_MEDIUM);
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        bitmans_lib_log_message("BLINK_MODE_FAST");
        bitmans_set_blink_mode(BLINK_MODE_FAST);
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        bitmans_lib_log_message("BLINK_MODE_BASIC");
        bitmans_set_blink_mode(BLINK_MODE_BASIC);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        
        bitmans_lib_log_message("BLINK_MODE_BREATHING");
        bitmans_set_blink_mode(BLINK_MODE_BREATHING);
        vTaskDelay(10000 / portTICK_PERIOD_MS);

        bitmans_lib_log_message("BLINK_MODE_ON");
        bitmans_set_blink_mode(BLINK_MODE_ON);
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        bitmans_lib_log_message("BLINK_MODE_NONE");
        bitmans_set_blink_mode(BLINK_MODE_NONE);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    bitmans_blink_term();
}
