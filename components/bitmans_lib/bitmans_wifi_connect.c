#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs.h"  // Add specific NVS header

#include "bitmans_wifi_logging.h"
#include "bitmans_wifi_connect.h"

static const char *TAG = "bitmans_lib:wifi_connect";

// Default configuration values
#define DEFAULT_WIFI_SSID      "Jelly Star_8503"
#define DEFAULT_WIFI_PASS      "Lorena345"
#define DEFAULT_HEARTBEAT_MS   2000
#define DEFAULT_MAX_MISSED     10

// WiFi event group and bits
static EventGroupHandle_t wifi_event_group = NULL;
static const int WIFI_CONNECTED_BIT = BIT0;

// WiFi configuration and status
static bitmans_wifi_config_t wifi_config;
static bitmans_wifi_status_t current_status = BITMANS_WIFI_DISCONNECTED;
static esp_netif_t *sta_netif = NULL;

// Heartbeat variables
static TaskHandle_t heartbeat_task_handle = NULL;
static volatile bool heartbeat_failed = false;

// User callback for status changes
static void (*user_callback)(bitmans_wifi_status_t) = NULL;

// Private function declarations
static void heartbeat_task(void *pvParameters);
static void connection_monitor_task(void *pvParameters);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// TODO: I think there is a bug somewhere in here that causes the WiFi connection to not retry properly.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#esp32-wi-fi-event-description

/**
 * @brief Event handler for WiFi events
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
        bitmans_wifi_connect();
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        const char * pszReason = bitmans_get_disconnect_reason(disconn->reason);
        ESP_LOGW(TAG, "Disconnected from SSID: %s, reason: %d, %s", wifi_config.ssid, disconn->reason, pszReason);

        // Clear the connected bit when disconnected
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        current_status = BITMANS_WIFI_DISCONNECTED;
        if (user_callback) 
            user_callback(current_status);

        ESP_LOGI(TAG, "Retrying in 5 seconds...");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        bitmans_wifi_connect();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_ip4_addr_t ip_addr = event->ip_info.ip;
        ESP_LOGI(TAG, "Connected, got IP: " IPSTR, IP2STR(&ip_addr));

        heartbeat_failed = false;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        current_status = BITMANS_WIFI_CONNECTED;
        if (user_callback) 
            user_callback(current_status);
    }
}

/**
 * @brief Task to monitor WiFi connection health
 */
static void heartbeat_task(void *pvParameters) 
{
    int missed_heartbeats = 0;

    while (1) 
    {
        if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) 
        {
            ESP_LOGI(TAG, "Heartbeat: Connected to SSID: %s", wifi_config.ssid);
            missed_heartbeats = 0;
        } 
        else 
        {
            missed_heartbeats++;
            ESP_LOGW(TAG, "Heartbeat: Not connected to SSID: %s (missed %d)", wifi_config.ssid, missed_heartbeats);
            if (missed_heartbeats >= wifi_config.max_missed_beats) 
            {
                ESP_LOGE(TAG, "Heartbeat failed %d times. Marking connection as failed.", wifi_config.max_missed_beats);
                heartbeat_failed = true;
            }
        }
        
        // Use the configured heartbeat interval
        vTaskDelay(wifi_config.heartbeat_ms / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Task to monitor connection status and reconnect if necessary
 */
static void connection_monitor_task(void *pvParameters) {
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        
        if (bits & WIFI_CONNECTED_BIT) 
        {
            ESP_LOGI(TAG, "Connected. Monitoring heartbeat...");
            
            // Wait until heartbeat_failed is set
            while (!heartbeat_failed) 
                vTaskDelay(500 / portTICK_PERIOD_MS);
            
            ESP_LOGI(TAG, "Heartbeat failure detected. Disconnecting WiFi.");
            esp_wifi_disconnect();
            heartbeat_failed = false; // Reset for next connection cycle
        }
    }
}

/**
 * @brief Initialize WiFi subsystem and start connection monitoring
 */
esp_err_t bitmans_wifi_init(const bitmans_wifi_config_t *config) 
{
    ESP_LOGI(TAG, "Initializing WiFi connection module");
        
    // Set default config or use user-provided config
    if (config != NULL) 
        memcpy(&wifi_config, config, sizeof(bitmans_wifi_config_t));
    else 
    {
        strcpy(wifi_config.ssid, DEFAULT_WIFI_SSID);
        strcpy(wifi_config.password, DEFAULT_WIFI_PASS);
        wifi_config.auth_mode = WIFI_AUTH_WPA2_PSK;
        wifi_config.heartbeat_ms = DEFAULT_HEARTBEAT_MS;
        wifi_config.max_missed_beats = DEFAULT_MAX_MISSED;
    }
    
    // Create event group for WiFi events
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) 
    {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
        
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));
    
    // Configure WiFi
    wifi_config_t esp_wifi_config = {
        .sta = {
            .threshold.authmode = wifi_config.auth_mode,
        },
    };
    
    // Copy SSID and password to ESP WiFi config
    strncpy((char*)esp_wifi_config.sta.ssid, wifi_config.ssid, sizeof(esp_wifi_config.sta.ssid));
    strncpy((char*)esp_wifi_config.sta.password, wifi_config.password, sizeof(esp_wifi_config.sta.password));
    
    // Set WiFi mode and config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &esp_wifi_config));
    
    // Create tasks for monitoring
    xTaskCreate(heartbeat_task, "heartbeat_task", 2048, NULL, 5, &heartbeat_task_handle);
    xTaskCreate(connection_monitor_task, "conn_monitor", 2048, NULL, 5, NULL);
    
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi initialization complete");
    
    return ESP_OK;
}

/**
 * @brief Get current WiFi status
 */
bitmans_wifi_status_t bitmans_wifi_get_status(void) 
{
    return current_status;
}

/**
 * @brief Get current IP address as string
 */
esp_err_t bitmans_wifi_get_ip(char *ip_str, size_t len) 
{
    if (ip_str == NULL || len < 16)
        return ESP_ERR_INVALID_ARG;
    
    if (current_status != BITMANS_WIFI_CONNECTED) 
    {
        strncpy(ip_str, "0.0.0.0", len);
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(sta_netif, &ip_info);
    if (ret != ESP_OK) 
    {
        strncpy(ip_str, "0.0.0.0", len);
        return ret;
    }
    
    snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

/**
 * @brief Disconnect from WiFi network
 */
esp_err_t bitmans_wifi_disconnect(void) 
{
    return esp_wifi_disconnect();
}

/**
 * @brief Connect to WiFi network
 */
esp_err_t bitmans_wifi_connect(void) 
{
    ESP_LOGI(TAG, "Attempting to connect to SSID: %s", wifi_config.ssid);
    current_status = BITMANS_WIFI_CONNECTING;
    if (user_callback) 
        user_callback(current_status);

    return esp_wifi_connect();
}

/**
 * @brief Update WiFi configuration
 */
esp_err_t bitmans_wifi_update_config(const bitmans_wifi_config_t *config) 
{
    if (config == NULL) 
        return ESP_ERR_INVALID_ARG;
    
    // Update internal config
    memcpy(&wifi_config, config, sizeof(bitmans_wifi_config_t));
    
    // Update ESP WiFi config
    wifi_config_t esp_wifi_config = {
        .sta = {
            .threshold.authmode = wifi_config.auth_mode,
        },
    };
    
    // Copy SSID and password
    strncpy((char*)esp_wifi_config.sta.ssid, wifi_config.ssid, sizeof(esp_wifi_config.sta.ssid));
    strncpy((char*)esp_wifi_config.sta.password, wifi_config.password, sizeof(esp_wifi_config.sta.password));
    
    // Apply new config
    return esp_wifi_set_config(ESP_IF_WIFI_STA, &esp_wifi_config);
}

/**
 * @brief Register callback for status changes
 */
esp_err_t bitmans_wifi_register_callback(void (*callback)(bitmans_wifi_status_t status)) 
{
    if (callback == NULL) 
        return ESP_ERR_INVALID_ARG;
    
    user_callback = callback;
    return ESP_OK;
}

/**
 * @brief Terminate WiFi connection module
 */
esp_err_t bitmans_wifi_term(void) 
{
    ESP_LOGI(TAG, "Terminating WiFi connection module");
    
    // Delete tasks
    if (heartbeat_task_handle != NULL) 
    {
        vTaskDelete(heartbeat_task_handle);
        heartbeat_task_handle = NULL;
    }
    
    // Disconnect and stop WiFi
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Delete event group
    if (wifi_event_group != NULL) 
    {
        vEventGroupDelete(wifi_event_group);
        wifi_event_group = NULL;
    }
    
    // Reset state
    current_status = BITMANS_WIFI_DISCONNECTED;
    user_callback = NULL;
    
    return ESP_OK;
}
