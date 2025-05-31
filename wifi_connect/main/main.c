#include "esp_log.h"
#include "bitmans_lib.h"

static const char *TAG = "wifi_connect_app";

void wifi_status_callback(bitmans_wifi_status_t status) 
{
    switch (status) {
        case BITMANS_WIFI_DISCONNECTED:
            ESP_LOGI(TAG, "WiFiStatus: Disconnected");
            bitmans_set_blink_mode(BLINK_MODE_NONE);
            break;
        case BITMANS_WIFI_CONNECTING:
            ESP_LOGI(TAG, "WiFiStatus: Connecting");
            bitmans_set_blink_mode(BLINK_MODE_FAST);
            break;
        case BITMANS_WIFI_CONNECTED:
            ESP_LOGI(TAG, "WiFiStatus: Connected");
            bitmans_set_blink_mode(BLINK_MODE_BREATHING);
            break;
        case BITMANS_WIFI_ERROR:
            ESP_LOGI(TAG, "WiFiStatus: Error");
            bitmans_set_blink_mode(BLINK_MODE_SLOW);
            break;
        default:
            ESP_LOGI(TAG, "WiFiStatus: Unknown");
            bitmans_set_blink_mode(BLINK_MODE_SLOW);
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting %s application", TAG);

    bitmans_wifi_config_t wifi_config = {
        .ssid = "Jelly Star_8503",
        .password = "Lorena345",
        .heartbeat_ms = 2000,          // 2 seconds
        .max_missed_beats = 10,
        .auth_mode = WIFI_AUTH_WPA2_PSK
    };
    
    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_blink_init(-1));
    
    bitmans_set_blink_mode(BLINK_MODE_NONE);

    ESP_ERROR_CHECK(bitmans_wifi_register_callback(wifi_status_callback));
    ESP_ERROR_CHECK(bitmans_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(bitmans_register_wifi_eventlog_handler());

    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI("HEAP", "Available heap: %d bytes", free_heap);

    size_t min_free_heap = esp_get_minimum_free_heap_size();
    ESP_LOGI("HEAP", "Minimum free heap since boot: %d bytes", min_free_heap); 
    
    char ip_str[16];
    while (1) 
    {
        bitmans_wifi_status_t status = bitmans_wifi_get_status();
        ESP_LOGI(TAG, "Checking WiFi connection status: %d", status);
        
        if (status == BITMANS_WIFI_CONNECTED) 
        {
            if (bitmans_wifi_get_ip(ip_str, sizeof(ip_str)) == ESP_OK) 
            {
                ESP_LOGI(TAG, "Current IP address: %s", ip_str);
            }
        }
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    
    // Cleanup (this part won't be reached in this example)
    bitmans_wifi_term();
    bitmans_blink_term();
}
