#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "bat_lib.h"
#include "bat_ble_lib.h"

static const char *TAG = "bat_ble_server";

esp_err_t bat_ble_server_test(void)
{
    ESP_LOGI(TAG, "Testing BLE server functionality...");
    return ESP_OK;
}
