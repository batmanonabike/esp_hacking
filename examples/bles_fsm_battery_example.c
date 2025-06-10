/**
 * @file bles_fsm_battery_example.c
 * @brief Example demonstrating the FSM-based BLE server with battery service
 * 
 * This example shows how to use the new bitmans_bles FSM-based API to create
 * a BLE server with a battery service. The FSM handles all the complex async
 * operations automatically, providing a much cleaner user experience.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bitmans_bles.h"

static const char *TAG = "battery_example";

// Battery level simulation
static uint8_t battery_level = 100;
static TimerHandle_t battery_timer = NULL;

// Service and characteristic handles (populated by FSM)
static uint16_t battery_service_handle = 0;
static uint16_t battery_level_char_handle = 0;

// Battery Service UUID (standard BLE service)
static const uint8_t BATTERY_SERVICE_UUID[] = {0x0F, 0x18}; // 0x180F
static const uint8_t BATTERY_LEVEL_CHAR_UUID[] = {0x19, 0x2A}; // 0x2A19

/**
 * Battery timer callback - simulates battery drain
 */
static void battery_timer_callback(TimerHandle_t xTimer)
{
    if (battery_level > 0) {
        battery_level--;
        ESP_LOGI(TAG, "Battery level: %d%%", battery_level);
        
        // Notify connected clients of battery level change
        bitmans_bles_notify_characteristic(battery_level_char_handle, &battery_level, 1);
    }
    
    if (battery_level == 0) {
        ESP_LOGW(TAG, "Battery depleted! Stopping timer.");
        xTimerStop(xTimer, 0);
    }
}

/**
 * BLE server event callback - handles all server events
 */
static void ble_server_event_callback(bitmans_bles_event_t *event)
{
    switch (event->type) {
        case BITMANS_BLES_EVENT_SERVER_READY:
            ESP_LOGI(TAG, "✅ BLE Server ready - all services configured successfully!");
            break;
            
        case BITMANS_BLES_EVENT_ADVERTISING_STARTED:
            ESP_LOGI(TAG, "📡 Advertising started - device is discoverable");
            break;
            
        case BITMANS_BLES_EVENT_CLIENT_CONNECTED:
            ESP_LOGI(TAG, "🔗 Client connected from: " ESP_BD_ADDR_STR, 
                     ESP_BD_ADDR_HEX(event->data.connection.remote_bda));
            // Start battery simulation when client connects
            if (battery_timer) {
                xTimerStart(battery_timer, 0);
            }
            break;
            
        case BITMANS_BLES_EVENT_CLIENT_DISCONNECTED:
            ESP_LOGI(TAG, "🔌 Client disconnected");
            // Stop battery simulation when client disconnects
            if (battery_timer) {
                xTimerStop(battery_timer, 0);
            }
            break;
            
        case BITMANS_BLES_EVENT_CHARACTERISTIC_READ:
            ESP_LOGI(TAG, "📖 Characteristic read - handle: %d", 
                     event->data.characteristic_access.handle);
            break;
            
        case BITMANS_BLES_EVENT_CHARACTERISTIC_WRITE:
            ESP_LOGI(TAG, "✏️  Characteristic write - handle: %d, length: %d", 
                     event->data.characteristic_access.handle,
                     event->data.characteristic_access.length);
            break;
            
        case BITMANS_BLES_EVENT_NOTIFICATION_ENABLED:
            ESP_LOGI(TAG, "🔔 Notifications enabled for handle: %d", 
                     event->data.notification.handle);
            break;
            
        case BITMANS_BLES_EVENT_NOTIFICATION_DISABLED:
            ESP_LOGI(TAG, "🔕 Notifications disabled for handle: %d", 
                     event->data.notification.handle);
            break;
            
        case BITMANS_BLES_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ BLE Server error: %s (state: %d)", 
                     event->data.error.description,
                     event->data.error.state);
            // In a real application, you might want to restart the server
            break;
            
        default:
            ESP_LOGD(TAG, "Unhandled BLE event: %d", event->type);
            break;
    }
}

/**
 * Read callback for battery level characteristic
 */
static esp_err_t battery_level_read_callback(uint16_t handle, uint8_t *data, uint16_t *length, uint16_t max_length)
{
    if (max_length < 1) {
        return ESP_ERR_NO_MEM;
    }
    
    data[0] = battery_level;
    *length = 1;
    
    ESP_LOGI(TAG, "Battery level read: %d%%", battery_level);
    return ESP_OK;
}

/**
 * Write callback for battery level characteristic (for testing - not typical)
 */
static esp_err_t battery_level_write_callback(uint16_t handle, const uint8_t *data, uint16_t length)
{
    if (length == 1 && data[0] <= 100) {
        battery_level = data[0];
        ESP_LOGI(TAG, "Battery level set to: %d%%", battery_level);
        return ESP_OK;
    }
    
    ESP_LOGW(TAG, "Invalid battery level write: length=%d, value=%d", length, data ? data[0] : 0);
    return ESP_ERR_INVALID_ARG;
}

/**
 * Initialize the BLE server with battery service
 */
static esp_err_t init_ble_server(void)
{
    esp_err_t ret;
    
    // 1. Define the battery service characteristics
    bitmans_bles_char_def_t battery_characteristics[] = {
        {
            .uuid = {
                .len = 2,
                .uuid16 = 0x2A19  // Battery Level UUID
            },
            .properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
            .permissions = ESP_GATT_PERM_READ,
            .read_callback = battery_level_read_callback,
            .write_callback = battery_level_write_callback,  // For testing purposes
            .user_data = NULL
        }
    };
    
    // 2. Define the battery service
    bitmans_bles_service_def_t services[] = {
        {
            .uuid = {
                .len = 2,
                .uuid16 = 0x180F  // Battery Service UUID
            },
            .characteristics = battery_characteristics,
            .char_count = sizeof(battery_characteristics) / sizeof(battery_characteristics[0]),
            .user_data = NULL
        }
    };
    
    // 3. Configure server callbacks
    bitmans_bles_callbacks_t callbacks = {
        .event_callback = ble_server_event_callback,
        .user_context = NULL
    };
    
    // 4. Configure server settings
    bitmans_bles_config_t config = {
        .device_name = "ESP32-Battery",
        .appearance = ESP_BLE_APPEARANCE_GENERIC_WATCH,  // Watch-like device for battery
        .services = services,
        .service_count = sizeof(services) / sizeof(services[0]),
        .callbacks = callbacks,
        .auto_start_advertising = true,
        .advertising_interval_min = 0x20,  // 20ms
        .advertising_interval_max = 0x40,  // 40ms
        .connection_interval_min = 0x10,   // 20ms  
        .connection_interval_max = 0x20,   // 40ms
        .max_connections = 1
    };
    
    // 5. Initialize the BLE server (FSM handles everything!)
    ret = bitmans_bles_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 6. Start the server (FSM will handle all async operations)
    ret = bitmans_bles_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "BLE server initialization complete - FSM will handle the rest!");
    return ESP_OK;
}

/**
 * Main application task
 */
void app_main(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "🚀 Starting FSM-based BLE Battery Server Example");
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create battery simulation timer
    battery_timer = xTimerCreate(
        "battery_timer",
        pdMS_TO_TICKS(5000),  // 5 second intervals
        pdTRUE,               // Auto-reload
        NULL,                 // Timer ID
        battery_timer_callback
    );
    
    if (!battery_timer) {
        ESP_LOGE(TAG, "Failed to create battery timer");
        return;
    }
    
    // Initialize and start BLE server
    ret = init_ble_server();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE server initialization failed");
        return;
    }
    
    ESP_LOGI(TAG, "✅ Application started successfully!");
    ESP_LOGI(TAG, "💡 The FSM will automatically handle:");
    ESP_LOGI(TAG, "   - Service registration");
    ESP_LOGI(TAG, "   - Characteristic creation");  
    ESP_LOGI(TAG, "   - Advertising setup");
    ESP_LOGI(TAG, "   - State transitions");
    ESP_LOGI(TAG, "   - Error recovery");
    ESP_LOGI(TAG, "📱 Connect with a BLE scanner app to see battery service!");
    
    // Main loop - in a real application you might do other work here
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        // Periodically log server status
        bitmans_bles_state_t state = bitmans_bles_get_state();
        ESP_LOGI(TAG, "📊 Server state: %d, Battery: %d%%", state, battery_level);
        
        // Example of how to gracefully restart if needed
        #if 0
        if (some_error_condition) {
            ESP_LOGI(TAG, "🔄 Restarting BLE server...");
            bitmans_bles_stop();
            vTaskDelay(pdMS_TO_TICKS(1000));
            bitmans_bles_start();
        }
        #endif
    }
}
