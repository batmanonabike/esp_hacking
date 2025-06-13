#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bat_lib.h"
#include "bat_config.h"
#include "bat_ble_lib.h"

static const char *TAG = "ble_server2_app";

void app_main(void)
{
    ESP_LOGI(TAG, "App starting");

    bat_lib_t bat_lib;
    bat_ble_lib_t bat_ble_lib;
    ESP_ERROR_CHECK(bat_lib_init(&bat_lib));
    ESP_ERROR_CHECK(bat_ble_lib_init(bat_lib, &bat_ble_lib));

    ESP_LOGI(TAG, "App started");
    ESP_ERROR_CHECK(bat_ble_lib_deinit(bat_ble_lib));
    ESP_ERROR_CHECK(bat_lib_deinit(bat_lib));

    ESP_LOGI(TAG, "App exiting");
}
