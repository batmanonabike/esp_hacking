#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "bat_lib.h"

static const char *TAG = "bat_lib";
static const char *VERSION = "1.0.2";

esp_err_t bat_lib_init(bat_lib_t *pLib)
{
    ESP_LOGI(TAG, "Initializing %s version %s", TAG, VERSION);

    // Initialize Non-Volatile Storage (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

void bat_lib_log_message(const char *message)
{
    ESP_LOGI(TAG, "User message: %s", message);
}

esp_err_t bat_lib_deinit(bat_lib_t lib)
{
    ESP_LOGI(TAG, "Deinitializing %s", TAG);
    return ESP_OK;
}

const char *bat_lib_get_version(void)
{
    return VERSION;
}

esp_err_t bat_waitbits(
    EventGroupHandle_t ble_events, int bit,
    TickType_t xTicksToWait, EventBits_t *pResult)
{
    TickType_t start = xTaskGetTickCount();

    BaseType_t xClearOnExit = pdTRUE;
    BaseType_t xWaitForAllBits = pdFALSE;
    EventBits_t bits = xEventGroupWaitBits(ble_events, bit, xClearOnExit, xWaitForAllBits, xTicksToWait);

    TickType_t end = xTaskGetTickCount();
    TickType_t waited = end - start;

    if (bits == 0)
        return ESP_ERR_TIMEOUT; // No bits were set within the timeout

    ESP_LOGI(TAG, "waitbits: waited %lu ticks (%.2f seconds, %lu ms)",
             (unsigned long)waited,
             waited / (float)configTICK_RATE_HZ,
             (unsigned long)(waited * 1000 / configTICK_RATE_HZ));

    if (pResult != NULL)
        *pResult = bits; // Store the result if provided
    return ESP_OK;
}

esp_err_t bat_waitbits_forever(EventGroupHandle_t ble_events, int bit, EventBits_t *pResult)
{
    return bat_waitbits(ble_events, bit, portMAX_DELAY, pResult);
}

void _bat_error_check_restart(esp_err_t rc, const char *file, int line, const char *function, const char *expression)
{
    printf("ESP_ERROR_CHECK_RESTART failed: esp_err_t 0x%x", rc);
    printf(" (%s)", esp_err_to_name(rc));
    printf(" at %p\n", __builtin_return_address(0));
    printf("file: \"%s\" line %d\nfunc: %s\nexpression: %s\n", file, line, function, expression);

    printf("This message will self destruct in 5 seconds...\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
}
