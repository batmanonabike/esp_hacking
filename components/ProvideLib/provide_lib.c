#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "provide_lib.h"

static const char *TAG = "ProvideLib";
static const char *VERSION = "1.0.0";

esp_err_t provide_lib_init(void)
{
    ESP_LOGI(TAG, "Initializing ProvideLib version %s", VERSION);
    return ESP_OK;
}

void provide_lib_log_message(const char *message)
{
    ESP_LOGI(TAG, "User message: %s", message);
}

const char *provide_lib_get_version(void)
{
    return VERSION;
}
