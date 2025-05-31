#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "bitmans_lib.h"

static const char *TAG = "bitmans_lib";
static const char *VERSION = "1.0.2";

esp_err_t bitmans_lib_init(void)
{
    ESP_LOGI(TAG, "Initializing bitmans_lib version %s", VERSION);
    
    // Initialize Non-Volatile Storage (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

void bitmans_lib_log_message(const char *message)
{
    ESP_LOGI(TAG, "User message: %s", message);
}

const char * bitmans_lib_get_version(void)
{
    return VERSION;
}
