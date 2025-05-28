#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "bitmans_lib.h"

static const char *TAG = "BitmansLib";
static const char *VERSION = "1.0.0";

esp_err_t bitmans_lib_init(void)
{
    ESP_LOGI(TAG, "Initializing BitmansLib version %s", VERSION);
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
