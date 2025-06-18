/**
 * Battery Service BLE Server
 * 
 * Implements a standard BLE Battery Service (UUID: 0x180F)
 * with Battery Level characteristic (UUID: 0x2A19)
 * 
 * c:\Users\mbrown\source\repos\_a\esp_hacking_10\ble_battery_server2\main\main.c
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
#include "bat_gatts_simple.h"

static const char *TAG = "ble_battery_server";

#define BATTERY_LEVEL_CHAR_UUID 0x2A19
#define BATTERY_UPDATE_INTERVAL 10000

// Simulation parameters
#define BATTERY_LEVEL_MAX 100
#define BATTERY_LEVEL_MIN 20
#define BATTERY_DISCHARGE_STEP 1
#define BATTERY_RECHARGE_STEP 5

// Battery server context
typedef struct {
    bool discharging;               // Whether battery is discharging or recharging
    uint8_t batteryLevel;           // Current battery level percentage (0-100)
    bool notificationsEnabled;      // Whether notifications are enabled
    TimerHandle_t updateTimer;      // Timer for periodic battery level updates
} app_context_t;

static app_context_t g_appContext = {0};
static bat_gatts_server_t g_server = {0};

static void battery_timer_callback(TimerHandle_t xTimer);
static void start_battery_simulation(bat_gatts_server_t *pServer);
static void stop_battery_simulation(bat_gatts_server_t *pServer);

static void on_connect(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Client connected");
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    // Reset notification state
    pAppContext->notificationsEnabled = false;
    
    // Start battery simulation
    start_battery_simulation(pServer);
}


static void on_disconnect(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Client disconnected, restarting advertising");
    
    // Stop battery simulation
    stop_battery_simulation(pServer);
    
    // Auto restart advertising on disconnect
    bat_ble_gap_start_advertising(pServer->pAdvParams);
}

// Callback for characteristic reads
static void on_read(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    uint16_t handle = pParam->read.handle;
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    // Check if read is for Battery Level characteristic
    if (handle == pServer->charHandles[0]) 
    {
        ESP_LOGI(TAG, "Client read battery level: %d%%", pAppContext->batteryLevel);
    }
}

// Callback for descriptor writes (for CCCD)
static void on_desc_write(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    // Find which descriptor was written
    uint16_t handle = pParam->write.handle;
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    // Check if it's CCCD for Battery Level characteristic
    if (handle == pServer->descrHandles[0]) 
    {
        // CCCD write operations should have 2 bytes of data
        if (pParam->write.len == 2) 
        {
            uint16_t descValue = pParam->write.value[0] | (pParam->write.value[1] << 8);
            
            // Check for notifications bit
            if (descValue & BAT_CCCD_NOTIFICATION) 
            {
                ESP_LOGI(TAG, "Battery Level notifications enabled");
                pAppContext->notificationsEnabled = true;
                
                // Send initial notification with current battery level
                bat_gatts_notify(pServer, 0, &pAppContext->batteryLevel, sizeof(pAppContext->batteryLevel));
            } 
            else 
            {
                ESP_LOGI(TAG, "Battery Level notifications disabled");
                pAppContext->notificationsEnabled = false;
            }
        }
    }
}

// Timer callback function to update simulated battery level
static void battery_timer_callback(TimerHandle_t xTimer)
{
    app_context_t *pAppContext = (app_context_t *)g_server.pContext;
    
    // Update battery level based on current state (discharging or recharging)
    if (pAppContext->discharging) 
    {
        // Discharge battery
        if (pAppContext->batteryLevel > BATTERY_LEVEL_MIN) 
        {
            pAppContext->batteryLevel -= BATTERY_DISCHARGE_STEP;
            if (pAppContext->batteryLevel < BATTERY_LEVEL_MIN) 
            {
                pAppContext->batteryLevel = BATTERY_LEVEL_MIN;
                pAppContext->discharging = false; // Start recharging
            }
        }
    } 
    else 
    {
        // Recharge battery
        if (pAppContext->batteryLevel < BATTERY_LEVEL_MAX) 
        {
            pAppContext->batteryLevel += BATTERY_RECHARGE_STEP;
            if (pAppContext->batteryLevel > BATTERY_LEVEL_MAX) 
            {
                pAppContext->batteryLevel = BATTERY_LEVEL_MAX;
                pAppContext->discharging = true; // Start discharging
            }
        }
    }
    
    ESP_LOGI(TAG, "Battery level updated: %d%%", pAppContext->batteryLevel);
    
    // Send notification if enabled
    if (g_server.isConnected && pAppContext->notificationsEnabled) 
    {
        bat_gatts_notify(&g_server, 0, &pAppContext->batteryLevel, sizeof(pAppContext->batteryLevel));
        ESP_LOGI(TAG, "Battery level notification sent: %d%%", pAppContext->batteryLevel);
    }
}

static void start_battery_simulation(bat_gatts_server_t *pServer)
{
    ESP_LOGI(TAG, "Starting battery simulation");
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    // If timer doesn't exist, create it
    if (pAppContext->updateTimer == NULL) 
    {
        pAppContext->updateTimer = xTimerCreate(
            "battery_timer",
            pdMS_TO_TICKS(BATTERY_UPDATE_INTERVAL),
            pdTRUE, // Auto reload
            0,      // Timer ID not used
            battery_timer_callback
        );
    }
    
    if (pAppContext->updateTimer != NULL) 
        xTimerStart(pAppContext->updateTimer, 0);
}

static void stop_battery_simulation(bat_gatts_server_t *pServer)
{
    ESP_LOGI(TAG, "Stopping battery simulation");
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    if (pAppContext->updateTimer != NULL) 
        xTimerStop(pAppContext->updateTimer, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BLE Battery Service example");
    
    ESP_ERROR_CHECK(bat_lib_init());
    ESP_ERROR_CHECK(bat_blink_init(-1));
    ESP_ERROR_CHECK(bat_ble_lib_init());

    bat_set_blink_mode(BLINK_MODE_FAST);

    g_appContext.updateTimer = NULL;
    g_appContext.discharging = true; // Start in discharge mode
    g_appContext.notificationsEnabled = false;
    g_appContext.batteryLevel = BATTERY_LEVEL_MAX; // Start at 100%
    
    // Define Battery Level characteristic
    bat_gatts_char_config_t charConfig = {
        .uuid = BATTERY_LEVEL_CHAR_UUID,
        .maxLen = sizeof(uint8_t),
        .initValueLen = sizeof(uint8_t),
        .pInitialValue = &g_appContext.batteryLevel,
        .permissions = ESP_GATT_PERM_READ,
        .properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
        .hasIndications = false,
        .hasNotifications = true
    };
    
    // Set up callbacks
    bat_gatts_callbacks2_t callbacks = {
        .onConnect = on_connect,
        .onDisconnect = on_disconnect,
        .onRead = on_read,
        .onDescWrite = on_desc_write
    };
    
    const int timeoutMs = 5000;
    const char * pServiceUuid = "f0debc9a-7856-3412-1234-56785612561C"; 
    ESP_ERROR_CHECK(bat_gatts_init(&g_server, &g_appContext, "Battery Monitor", 0x55, 
                                  pServiceUuid, ESP_BLE_APPEARANCE_GENERIC_THERMOMETER, timeoutMs));
    ESP_ERROR_CHECK(bat_gatts_create_service(&g_server, &charConfig, 1, timeoutMs));
    ESP_ERROR_CHECK(bat_gatts_start(&g_server, &callbacks, timeoutMs));

    ESP_LOGI(TAG, "BLE Battery Service running");
    ESP_LOGI(TAG, "  - Battery Level characteristic (0x%04X): Read, Notify", BATTERY_LEVEL_CHAR_UUID);
    
    bat_set_blink_mode(BLINK_MODE_BREATHING);
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    bat_set_blink_mode(BLINK_MODE_BASIC);

    // Clean up (this will only execute if the loop is exited)
    stop_battery_simulation(&g_server);

    if (g_appContext.updateTimer != NULL)
    {
        xTimerDelete(g_appContext.updateTimer, 0);
        g_appContext.updateTimer = NULL;
    }

    ESP_ERROR_CHECK(bat_gatts_stop(&g_server, timeoutMs));
    ESP_ERROR_CHECK(bat_gatts_deinit(&g_server));
    ESP_ERROR_CHECK(bat_ble_lib_deinit());
    ESP_ERROR_CHECK(bat_blink_deinit());
    ESP_ERROR_CHECK(bat_lib_deinit());

    ESP_LOGI(TAG, "BLE Battery Service example finished");
}
