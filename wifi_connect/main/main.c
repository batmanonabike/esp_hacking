#include "esp_log.h"
#include "bat_lib.h"

static const char *TAG = "wifi_connect_app";

void wifi_status_callback(bat_wifi_status_t status) 
{
    switch (status) {
        case BAT_WIFI_DISCONNECTED:
            ESP_LOGI(TAG, "WiFiStatus: Disconnected");
            bat_set_blink_mode(BLINK_MODE_NONE);
            break;
        case BAT_WIFI_CONNECTING:
            ESP_LOGI(TAG, "WiFiStatus: Connecting");
            bat_set_blink_mode(BLINK_MODE_FAST);
            break;
        case BAT_WIFI_CONNECTED:
            ESP_LOGI(TAG, "WiFiStatus: Connected");
            bat_set_blink_mode(BLINK_MODE_BREATHING);
            break;
        case BAT_WIFI_ERROR:
            ESP_LOGI(TAG, "WiFiStatus: Error");
            bat_set_blink_mode(BLINK_MODE_SLOW);
            break;
        default:
            ESP_LOGI(TAG, "WiFiStatus: Unknown");
            bat_set_blink_mode(BLINK_MODE_SLOW);
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting %s application", TAG);

    bat_wifi_config_t wifi_config = {
        .ssid = "Jelly Star_8503",
        .password = "Lorena345",
        .heartbeat_ms = 2000,          // 2 seconds
        .max_missed_beats = 10,
        .auth_mode = WIFI_AUTH_WPA2_PSK
    };
    
    bat_lib_t bat_lib;
    ESP_ERROR_CHECK(bat_lib_init(&bat_lib));
    ESP_ERROR_CHECK(bat_blink_init(-1));
    
    bat_set_blink_mode(BLINK_MODE_NONE);

    ESP_ERROR_CHECK(bat_wifi_register_callback(wifi_status_callback));
    ESP_ERROR_CHECK(bat_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(bat_register_wifi_eventlog_handler());

    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI("HEAP", "Available heap: %d bytes", free_heap);

    size_t min_free_heap = esp_get_minimum_free_heap_size();
    ESP_LOGI("HEAP", "Minimum free heap since boot: %d bytes", min_free_heap); 
    
    char ip_str[16];
    while (1) 
    {
        bat_wifi_status_t status = bat_wifi_get_status();
        ESP_LOGI(TAG, "Checking WiFi connection status: %d", status);
        
        if (status == BAT_WIFI_CONNECTED) 
        {
            if (bat_wifi_get_ip(ip_str, sizeof(ip_str)) == ESP_OK) 
            {
                ESP_LOGI(TAG, "Current IP address: %s", ip_str);
            }
        }
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    // Cleanup (this part won't be reached in this example)
    bat_wifi_deinit();
    bat_blink_deinit();
}
