/**
 * Battery Service BLE Client
 * 
 * Connects to a BLE device with the Battery Service (UUID: 0x180F)
 * and reads/subscribes to the Battery Level characteristic (UUID: 0x2A19)
 * 
 * c:\Users\mbrown\source\repos\_a\esp_hacking_12\ble_battery_client\main\main.c
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "bat_lib.h"
#include "bat_config.h"
#include "bat_ble_lib.h"
#include "bat_gattc_simple.h"
#include "bat_uuid.h"

static const char *TAG = "ble_battery_client";

// Standard Battery Service UUID
#define BATTERY_SERVICE_UUID 0x180F
#define BATTERY_LEVEL_CHAR_UUID 0x2A19
#define SCAN_DURATION_SECONDS 10

// Client context
typedef struct 
{
    bat_gattc_client_t client;          // BLE GATTC client
    bool connected;                     // Connection state
    uint8_t batteryLevel;               // Last received battery level
    int8_t selectedDeviceIndex;         // Selected device index (-1 if none)
    TimerHandle_t refreshTimer;         // Timer for periodic battery reads
    bool notificationsEnabled;          // Whether notifications are enabled
} app_context_t;

static app_context_t g_appContext = {0};

// Forward declarations
static void start_scan(app_context_t *pContext);
static void connect_to_battery_server(app_context_t *pContext);
static void battery_level_read_task(void *pvParameter);

// Callback handlers
static void on_scan_result(bat_gattc_client_t *pClient, esp_ble_gap_cb_param_t *pParam)
{
    app_context_t *pContext = (app_context_t*)pClient->pContext;
    
    // Get the device name
    char name[32] = {0};
    bat_gattc_get_device_name(pClient, pClient->scanResultCount - 1, name, sizeof(name));
    
    // Check if this looks like our battery server
    if (strstr(name, "Battery") != NULL) 
    {
        ESP_LOGI(TAG, "Found potential battery server: %s (index %d)", 
                name, pClient->scanResultCount - 1);
        
        // If we haven't selected a device yet, select this one
        if (pContext->selectedDeviceIndex < 0) 
        {
            pContext->selectedDeviceIndex = pClient->scanResultCount - 1;
            ESP_LOGI(TAG, "Auto-selecting device %s (index %d)", name, pContext->selectedDeviceIndex);
            
            // Stop scanning since we found what we want
            bat_gattc_stop_scan(pClient);
        }
    }
}

static void on_connect(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam)
{
    app_context_t *pContext = (app_context_t*)pClient->pContext;
    pContext->connected = true;
    
    // Get device name
    char name[32] = {0};
    bat_gattc_get_device_name(pClient, pContext->selectedDeviceIndex, name, sizeof(name));
    
    ESP_LOGI(TAG, "Connected to %s", name);
    bat_set_blink_mode(BLINK_MODE_BREATHING);
}

static void on_disconnect(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam)
{
    app_context_t *pContext = (app_context_t*)pClient->pContext;
    pContext->connected = false;
    
    ESP_LOGI(TAG, "Disconnected from server, restarting scan...");
    bat_set_blink_mode(BLINK_MODE_FAST);
    
    // Stop the refresh timer if it's running
    if (pContext->refreshTimer != NULL) 
    {
        xTimerStop(pContext->refreshTimer, 0);
    }
    
    // Restart scan after a short delay
    vTaskDelay(pdMS_TO_TICKS(1000));
    start_scan(pContext);
}

static void on_read(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam)
{
    app_context_t *pContext = (app_context_t*)pClient->pContext;
    
    // Check if this is a battery level read
    if (pParam->read.value_len == 1) 
    {
        uint8_t batteryLevel = pParam->read.value[0];
        pContext->batteryLevel = batteryLevel;
        
        ESP_LOGI(TAG, "Battery level: %d%%", batteryLevel);
    }
}

static void on_notify(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam)
{
    app_context_t *pContext = (app_context_t*)pClient->pContext;
    
    // Check if this is a battery level notification
    if (pParam->notify.value_len == 1) 
    {
        uint8_t batteryLevel = pParam->notify.value[0];
        pContext->batteryLevel = batteryLevel;
        
        ESP_LOGI(TAG, "Battery level notification: %d%%", batteryLevel);
    }
}

// Timer callback for periodic battery reads
static void battery_refresh_timer_cb(TimerHandle_t xTimer)
{
    // Create a task to read the battery level
    // (This is needed because we can't call GATTC functions from timer context)
    xTaskCreate(battery_level_read_task, "battery_read", 4096, NULL, 5, NULL);
}

// Task to read the battery level
static void battery_level_read_task(void *pvParameter)
{
    if (g_appContext.connected) 
    {
        ESP_LOGI(TAG, "Reading battery level...");
        bat_gattc_read_char(&g_appContext.client, 0, 5000);
    }
    vTaskDelete(NULL);
}

// Start scanning for BLE devices
static void start_scan(app_context_t *pContext)
{
    ESP_LOGI(TAG, "Starting scan for BLE devices");

    // Reset state
    pContext->selectedDeviceIndex = -1;
    pContext->connected = false;
    
    // Register callbacks
    bat_gattc_callbacks_t callbacks = {
        .onScanResult = on_scan_result,
        .onConnect = on_connect,
        .onDisconnect = on_disconnect,
        .onRead = on_read,
        .onNotify = on_notify
    };
    
    // Set up scan parameters - active scan with medium intervals
    bat_gattc_set_scan_params(&pContext->client, 0x50, 0x30);
    
    // Set up a filter for Battery Service (0x180F)
    esp_bt_uuid_t serviceUuid;
    bat_uuid_from_16bit(BATTERY_SERVICE_UUID, &serviceUuid);
    
    // Start the scan
    esp_err_t ret = bat_gattc_start_scan(&pContext->client, SCAN_DURATION_SECONDS, 
                                       &callbacks, &serviceUuid, 5000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(ret));
    }
    
    // Wait a bit longer than the scan duration
    vTaskDelay(pdMS_TO_TICKS((SCAN_DURATION_SECONDS + 1) * 1000));
    
    // If we've selected a device, try to connect to it
    if (pContext->selectedDeviceIndex >= 0) {
        connect_to_battery_server(pContext);
    } else {
        ESP_LOGW(TAG, "No battery service devices found in scan");
        // Try again
        vTaskDelay(pdMS_TO_TICKS(1000));
        start_scan(pContext);
    }
}

// Connect to the selected battery server
static void connect_to_battery_server(app_context_t *pContext)
{
    bat_gattc_client_t *pClient = &pContext->client;
    
    // Get the device name
    char name[32] = {0};
    bat_gattc_get_device_name(pClient, pContext->selectedDeviceIndex, name, sizeof(name));
    
    ESP_LOGI(TAG, "Connecting to battery server: %s", name);
    esp_err_t ret = bat_gattc_connect(pClient, pContext->selectedDeviceIndex, 5000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to device: %s", esp_err_to_name(ret));
        pContext->selectedDeviceIndex = -1;
        vTaskDelay(pdMS_TO_TICKS(1000));
        start_scan(pContext);
        return;
    }
    
    // Search for the Battery Service
    esp_bt_uuid_t serviceUuid;
    bat_uuid_from_16bit(BATTERY_SERVICE_UUID, &serviceUuid);
    
    ESP_LOGI(TAG, "Searching for Battery Service (UUID: 0x%04X)", BATTERY_SERVICE_UUID);
    ret = bat_gattc_search_service(pClient, serviceUuid, 5000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to find Battery Service: %s", esp_err_to_name(ret));
        bat_gattc_disconnect(pClient);
        return;
    }
    
    // Search for the Battery Level characteristic
    esp_bt_uuid_t charUuid;
    bat_uuid_from_16bit(BATTERY_LEVEL_CHAR_UUID, &charUuid);
    
    ESP_LOGI(TAG, "Searching for Battery Level characteristic (UUID: 0x%04X)", 
             BATTERY_LEVEL_CHAR_UUID);
    ret = bat_gattc_get_characteristics(pClient, &charUuid, 1, 5000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to find Battery Level characteristic: %s", esp_err_to_name(ret));
        bat_gattc_disconnect(pClient);
        return;
    }
    
    // Get the CCCD
    ret = bat_gattc_get_descriptor(pClient, 0, 5000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to find CCCD, may not support notifications");
    }
    
    // Read the initial battery level
    ESP_LOGI(TAG, "Reading initial battery level");
    ret = bat_gattc_read_char(pClient, 0, 5000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read battery level: %s", esp_err_to_name(ret));
    }
    
    // Register for notifications if CCCD was found
    if (pClient->cccdHandles[0] != 0) {
        ESP_LOGI(TAG, "Enabling battery level notifications");
        ret = bat_gattc_register_for_notify(pClient, 0, true, false, 5000);
        
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to enable notifications: %s", esp_err_to_name(ret));
            // Fall back to periodic polling
            pContext->notificationsEnabled = false;
        } else {
            pContext->notificationsEnabled = true;
        }
    }
    
    // If notifications aren't enabled, set up a timer to poll the battery level
    if (!pContext->notificationsEnabled) {
        ESP_LOGI(TAG, "Setting up periodic battery level reads");
        
        // Create a timer to read the battery level every 5 seconds
        if (pContext->refreshTimer == NULL) {
            pContext->refreshTimer = xTimerCreate(
                "battery_refresh",
                pdMS_TO_TICKS(5000),
                pdTRUE,  // Auto-reload
                (void*)0,
                battery_refresh_timer_cb);
        }
        
        if (pContext->refreshTimer != NULL) {
            xTimerStart(pContext->refreshTimer, 0);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BLE Battery Client");
    
    // Initialize the application context
    memset(&g_appContext, 0, sizeof(app_context_t));
    g_appContext.selectedDeviceIndex = -1;
    g_appContext.refreshTimer = NULL;
    
    // Initialize the libraries
    ESP_ERROR_CHECK(bat_lib_init());
    ESP_ERROR_CHECK(bat_blink_init(-1));
    ESP_ERROR_CHECK(bat_ble_lib_init());
    
    // Set up blinking pattern to indicate scanning
    bat_set_blink_mode(BLINK_MODE_FAST);
    
    // Initialize the GATTC client
    ESP_ERROR_CHECK(bat_gattc_init(&g_appContext.client, &g_appContext, 0x55, 5000));
    
    // Start scanning for devices
    start_scan(&g_appContext);
    
    // Main loop
    while (1) {
        // Keep the application running
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Cleanup (this point is not normally reached)
    if (g_appContext.refreshTimer != NULL) {
        xTimerStop(g_appContext.refreshTimer, 0);
        xTimerDelete(g_appContext.refreshTimer, 0);
    }
    
    if (g_appContext.connected) {
        bat_gattc_disconnect(&g_appContext.client);
    }
    
    bat_gattc_deinit(&g_appContext.client);
    bat_ble_lib_deinit();
    bat_blink_deinit();
    bat_lib_deinit();
    
    ESP_LOGI(TAG, "Battery Client terminated");
}
