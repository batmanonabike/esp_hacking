#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bat_lib.h"
#include "bat_config.h"
#include "bat_ble_lib.h"

static const char *TAG = "ble_restart_server_app";
static bat_ble_server_t bleServer = {0};

void app_main(void)
{    
    ESP_ERROR_CHECK(bat_lib_init());    
    ESP_ERROR_CHECK(bat_ble_lib_init());
    ESP_ERROR_CHECK(bat_blink_init(-1));
    
    ESP_LOGI(TAG, "App starting");
    bat_set_blink_mode(BLINK_MODE_BASIC);

    const int timeoutMs = 5000; 
    const char * pServiceUuid = "f0debc9a-7856-3412-1234-56785612561B"; 

    ESP_ERROR_CHECK(bat_ble_server_init2(&bleServer, NULL, "Martyn", 0x55, pServiceUuid, 0x0944, timeoutMs));
    ESP_ERROR_CHECK(bat_ble_server_create_service(&bleServer, NULL, 0, timeoutMs));
    
    ESP_LOGI(TAG, "App running");
    while (1)
    {
        bat_ble_server_reset_flags(&bleServer);

        ESP_LOGI(TAG, "Service starting...");
        bat_set_blink_mode(BLINK_MODE_FAST);
        ESP_ERROR_CHECK(bat_ble_server_start(&bleServer, NULL, timeoutMs));  
        //vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "Service started, advertising...");
        bat_set_blink_mode(BLINK_MODE_BREATHING);
        vTaskDelay(pdMS_TO_TICKS(3000));

        ESP_LOGI(TAG, "Service stopping soon...");
        bat_set_blink_mode(BLINK_MODE_VERY_FAST);
        bat_ble_server_stop(&bleServer, timeoutMs);
        //vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "Service stopped, restarting soon...");
        bat_set_blink_mode(BLINK_MODE_NONE);
        //vTaskDelay(pdMS_TO_TICKS(10000));
    }

    bat_ble_server_stop(&bleServer, timeoutMs);
    bat_ble_server_deinit(&bleServer);
    bat_ble_lib_deinit();
    bat_lib_deinit();

    ESP_LOGI(TAG, "App restarting");
    esp_restart();
}
