#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "bat_ble_lib.h"

static const char *TAG = "bat_ble_lib";
static const char *VERSION = "1.0.0";

esp_err_t bat_ble_lib_init()
{
    ESP_LOGI(TAG, "Initializing %s version %s", TAG, VERSION);
    return ESP_OK;
}

esp_err_t bat_ble_lib_deinit()
{
    ESP_LOGI(TAG, "Deinitializing %s", TAG);
    return ESP_OK;
}
