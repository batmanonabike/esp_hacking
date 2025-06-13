#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "bat_lib.h"

static const char *TAG = "blink_app";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting %s application", TAG);

    bat_lib_t bat_lib;
    ESP_ERROR_CHECK(bat_lib_init(&bat_lib));
    ESP_ERROR_CHECK(bat_blink_init(-1));

    while (1) 
    {
        bat_lib_log_message("BLINK_MODE_SLOW");
        bat_set_blink_mode(BLINK_MODE_SLOW);
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        bat_lib_log_message("BLINK_MODE_MEDIUM");
        bat_set_blink_mode(BLINK_MODE_MEDIUM);
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        bat_lib_log_message("BLINK_MODE_FAST");
        bat_set_blink_mode(BLINK_MODE_FAST);
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        bat_lib_log_message("BLINK_MODE_BASIC");
        bat_set_blink_mode(BLINK_MODE_BASIC);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        
        bat_lib_log_message("BLINK_MODE_BREATHING");
        bat_set_blink_mode(BLINK_MODE_BREATHING);
        vTaskDelay(10000 / portTICK_PERIOD_MS);

        bat_lib_log_message("BLINK_MODE_ON");
        bat_set_blink_mode(BLINK_MODE_ON);
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        bat_lib_log_message("BLINK_MODE_NONE");
        bat_set_blink_mode(BLINK_MODE_NONE);
        vTaskDelay(5000 / portTICK_PERIOD_MS);    
	}

    bat_blink_deinit();
}
