#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_bt_defs.h"
#include "assert.h"

#include "bat_lib.h"
#include "bat_gatts_fsm.h"
#include "bat_ble_server.h"
#include "bat_gatts_fsm_helpers.h"

static const char *TAG = "bat_gatts_fsm";

esp_err_t bat_gatts_fsm(void)
{
    ESP_LOGI(TAG, "GATTS fsm");
    return ESP_OK;
}

