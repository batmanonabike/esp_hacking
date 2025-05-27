#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define WIFI_SSID      "Jelly Star_8503"
#define WIFI_PASS      "Lorena345"
#define CONNECTED_TIME_SEC 5

static const char *TAG = "WifiConnect";
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static volatile bool heartbeat_failed = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Attempting to connect to SSID: %s", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Disconnected from SSID: %s, reason: %d", WIFI_SSID, disconn->reason);

        // Clear the connected bit when disconnected
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);

        switch (disconn->reason) {
            case WIFI_REASON_AUTH_EXPIRE:
                ESP_LOGW(TAG, "Authentication expired.");
                break;
            case WIFI_REASON_AUTH_FAIL:
                ESP_LOGW(TAG, "Authentication failed. Check password.");
                break;
            case WIFI_REASON_NO_AP_FOUND:
                ESP_LOGW(TAG, "AP not found. Check SSID.");
                break;
            case WIFI_REASON_ASSOC_LEAVE:
                ESP_LOGW(TAG, "Station has disassociated.");
                break;
            default:
                ESP_LOGW(TAG, "Other disconnect reason: %d", disconn->reason);
                break;
        }

        ESP_LOGI(TAG, "Retrying in 5 seconds...");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Attempting to connect to SSID: %s", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_ip4_addr_t ip_addr = event->ip_info.ip;
        ESP_LOGI(TAG, "Connected, got IP: " IPSTR, IP2STR(&ip_addr));

        heartbeat_failed = false;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                       ESP_EVENT_ANY_ID,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                       IP_EVENT_STA_GOT_IP,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       &instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void heartbeat_task(void *pvParameters) {
    int missed_heartbeats = 0;
    const int max_missed_heartbeats = 10;

    while (1) {
        if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Heartbeat: Connected to SSID: %s", WIFI_SSID);
            missed_heartbeats = 0;
            // heartbeat_failed = false; // Reset heartbeat_failed on successful connection
        } else {
            missed_heartbeats++;
            ESP_LOGW(TAG, "Heartbeat: Not connected to SSID: %s (missed %d)", WIFI_SSID, missed_heartbeats);
            if (missed_heartbeats >= max_missed_heartbeats) {
                ESP_LOGE(TAG, "Heartbeat failed %d times. Marking connection as failed.", max_missed_heartbeats);
                heartbeat_failed = true;
            }
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS); // 2 seconds heartbeat interval
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();

    xTaskCreate(heartbeat_task, "heartbeat_task", 2048, NULL, 5, NULL);

    // Stay connected until heartbeat fails
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected. Monitoring heartbeat...");
            // Wait until heartbeat_failed is set
            while (!heartbeat_failed) {
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
            ESP_LOGI(TAG, "Heartbeat failure detected. Disconnecting WiFi.");
            esp_wifi_disconnect();
            // heartbeat_failed = false; // Reset for next connection cycle
        }
    }
}
