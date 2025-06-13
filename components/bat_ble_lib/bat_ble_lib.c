#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"

#include "bat_ble_lib.h"

static const char *TAG = "bat_ble_lib";
static const char *VERSION = "1.0.0";

esp_err_t bat_ble_lib_init()
{
    ESP_LOGI(TAG, "Initializing %s version %s", TAG, VERSION);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
        return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
        return ret;

    ret = esp_bluedroid_init();
    if (ret)
        return ret;

    ret = esp_bluedroid_enable();
    if (ret)
        return ret;

    return ESP_OK;
}

esp_err_t bat_ble_lib_deinit()
{
    ESP_LOGI(TAG, "DeInitializing BLE");

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    return ESP_OK;
}
