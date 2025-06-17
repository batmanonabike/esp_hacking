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

static const char *TAG = "ble_cccd_server";

// Define service UUID
// Custom service UUID: f0debc9a-7856-3412-1234-56789abcdef0
#define APP_SERVICE_UUID "f0debc9a-7856-3412-1234-56789abcdef0"

// Server context
typedef struct {
    uint8_t counterValues[3];
    bool indicationsEnabled[3];
    bool notificationsEnabled[3];
    TimerHandle_t updateTimers[3];
} app_context_t;

static bat_gatts_server_t g_server = {0};

// Forward declarations
static void counter_timer_callback(TimerHandle_t xTimer);
static void stop_notification_timers(bat_gatts_server_t *pServer);
static void start_notification_timers(bat_gatts_server_t *pServer);

// Callback for client connections
static void on_connect(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Client connected");
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;

    // Reset notification/indication state
    for (int i = 0; i < 3; i++) 
    {
        pAppContext->indicationsEnabled[i] = false;
        pAppContext->notificationsEnabled[i] = false;
    }
}

// Callback for client disconnections
static void on_disconnect(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Client disconnected, restarting advertising");
    
    // Stop notification timers
    stop_notification_timers(pServer);
    
    // Auto restart advertising on disconnect
    bat_ble_gap_start_advertising(pServer->pAdvParams);
}

// Callback for characteristic reads
static void on_read(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    // Find which characteristic was read
    uint16_t handle = pParam->read.handle;
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    for (int i = 0; i < pServer->numChars; i++) 
    {
        if (handle == pServer->charHandles[i]) 
        {
            ESP_LOGI(TAG, "Read request for characteristic %d, value: %d",
                     i, pAppContext->counterValues[i]);

            // The BLE stack automatically responds with the current value
            break;
        }
    }
}

// Callback for characteristic writes
static void on_write(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    // Find which characteristic was written
    uint16_t handle = pParam->write.handle;
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    for (int i = 0; i < pServer->numChars; i++) 
    {
        if (handle == pServer->charHandles[i]) 
        {
            if (pParam->write.len > 0) 
            {
                pAppContext->counterValues[i] = pParam->write.value[0];
                ESP_LOGI(TAG, "Characteristic %d written with value: %d",
                         i, pAppContext->counterValues[i]);

                // Respond to the write if needed
                if (pParam->write.need_rsp) 
                {
                    esp_ble_gatts_send_response(pServer->gattsIf, pParam->write.conn_id,
                                             pParam->write.trans_id, ESP_GATT_OK, NULL);
                }
            }
            break;
        }
    }
}

// Callback for descriptor writes (especially CCCD)
static void on_desc_write(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam)
{
    // Find which descriptor was written
    uint16_t handle = pParam->write.handle;
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
   
    for (int i = 0; i < pServer->descrsAdded; i++) 
    {
        if (handle == pServer->descrHandles[i]) 
        {
            if (pParam->write.len == 2) 
            {
                uint16_t descValue = pParam->write.value[0] | (pParam->write.value[1] << 8);
                ESP_LOGI(TAG, "CCCD %d written with value: 0x%04x", i, descValue);
                
                // Check for notifications (bit 0) and indications (bit 1)
                pAppContext->notificationsEnabled[i] = (descValue & BAT_CCCD_NOTIFICATION) != 0;
                pAppContext->indicationsEnabled[i] = (descValue & BAT_CCCD_INDICATION) != 0;

                ESP_LOGI(TAG, "Characteristic %d - Notifications: %s, Indications: %s",
                         i,
                         pAppContext->notificationsEnabled[i] ? "Enabled" : "Disabled",
                         pAppContext->indicationsEnabled[i] ? "Enabled" : "Disabled");

                // Start or restart the notification timers if any notification is enabled
                bool anyNotificationOrIndication = false;
                for (int j = 0; j < 3; j++) 
                {
                    if (pAppContext->notificationsEnabled[j] || pAppContext->indicationsEnabled[j]) 
                    {
                        anyNotificationOrIndication = true;
                        break;
                    }
                }
                
                if (anyNotificationOrIndication) 
                    start_notification_timers(pServer);
                else 
                    stop_notification_timers(pServer);
            }
            break;
        }
    }
}

// Timer callback function to send notifications/indications
static void counter_timer_callback(TimerHandle_t xTimer)
{
    int charIndex = (int)pvTimerGetTimerID(xTimer);
    app_context_t *pAppContext = (app_context_t *)g_server.pContext;
    
    if (charIndex >= 0 && charIndex < 3) 
    {
        // Increment counter
        pAppContext->counterValues[charIndex]++;
        
        // Send notification if enabled
        if (pAppContext->notificationsEnabled[charIndex]) 
        {
            ESP_LOGI(TAG, "Sending notification for char %d with value %d", 
                     charIndex, pAppContext->counterValues[charIndex]);
            
            bat_gatts_notify(&g_server, charIndex, 
                          &pAppContext->counterValues[charIndex], 1);
        }
        // Send indication if enabled
        else if (pAppContext->indicationsEnabled[charIndex]) 
        {
            ESP_LOGI(TAG, "Sending indication for char %d with value %d", 
                     charIndex, pAppContext->counterValues[charIndex]);

            bat_gatts_indicate(&g_server, charIndex, 
                           &pAppContext->counterValues[charIndex], 1);
        }
    }
}

static void start_notification_timers(bat_gatts_server_t *pServer)
{
    ESP_LOGI(TAG, "Starting notification timers");
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    // Create and start timers for each characteristic with different periods
    for (int i = 0; i < 3; i++) 
    {
        // If timer doesn't exist, create it
        if (pAppContext->updateTimers[i] == NULL) 
        {
            // Use different periods for each timer (1s, 2s, 3s)
            TickType_t period = pdMS_TO_TICKS((i + 1) * 1000);
            
            char timerName[16];
            snprintf(timerName, sizeof(timerName), "UpdateTimer%d", i);
            
            pAppContext->updateTimers[i] = xTimerCreate(
                timerName,
                period,
                pdTRUE,  // auto-reload
                (void*)i, // timer ID is the characteristic index
                counter_timer_callback
            );
        }

        if (pAppContext->updateTimers[i] != NULL) 
            xTimerStart(pAppContext->updateTimers[i], 0);
    }
}

static void stop_notification_timers(bat_gatts_server_t *pServer)
{
    ESP_LOGI(TAG, "Stopping notification timers");
    app_context_t *pAppContext = (app_context_t *)pServer->pContext;
    
    for (int i = 0; i < 3; i++) {
        if (pAppContext->updateTimers[i] != NULL) {
            xTimerStop(pAppContext->updateTimers[i], 0);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BLE APP Server example");
    
    // Initialize the libraries
    ESP_ERROR_CHECK(bat_lib_init());
    ESP_ERROR_CHECK(bat_ble_lib_init());
    
    // Initialize counters
    app_context_t *pAppContext = (app_context_t *)g_server.pContext;
    for (int i = 0; i < 3; i++) 
        pAppContext->counterValues[i] = 10 * (i + 1); // Start with 10, 20, 30
    
    // Define our service characteristics
    bat_gatts_char_config_t charConfigs[3] = 
    {
        {
            .uuid = 0xFF01,
            .permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .properties = ESP_GATT_CHAR_PROP_BIT_READ | 
                          ESP_GATT_CHAR_PROP_BIT_WRITE | 
                          ESP_GATT_CHAR_PROP_BIT_NOTIFY,
            .maxLen = 1,
            .pInitialValue = &pAppContext->counterValues[0],
            .initValueLen = 1,
            .hasNotifications = true,
            .hasIndications = false,
        },
        {
            .uuid = 0xFF02,
            .permissions = ESP_GATT_PERM_READ,
            .properties = ESP_GATT_CHAR_PROP_BIT_READ | 
                          ESP_GATT_CHAR_PROP_BIT_INDICATE,
            .maxLen = 1,
            .pInitialValue = &pAppContext->counterValues[1],
            .initValueLen = 1,
            .hasNotifications = false,
            .hasIndications = true,
        },
        {
            .uuid = 0xFF03,
            .permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .properties = ESP_GATT_CHAR_PROP_BIT_READ | 
                          ESP_GATT_CHAR_PROP_BIT_WRITE | 
                          ESP_GATT_CHAR_PROP_BIT_NOTIFY | 
                          ESP_GATT_CHAR_PROP_BIT_INDICATE,
            .maxLen = 1,
            .pInitialValue = &pAppContext->counterValues[2],
            .initValueLen = 1,
            .hasNotifications = true,
            .hasIndications = true,
        }
    };
    
    // Set up callbacks
    bat_gatts_callbacks2_t callbacks = {
        .onRead = on_read,
        .onWrite = on_write,
        .onConnect = on_connect,
        .onDescWrite = on_desc_write,
        .onDisconnect = on_disconnect,
    };

    // Initialize the BLE server
    const int timeoutMs = 5000;
    ESP_ERROR_CHECK(bat_gatts_init(&g_server, NULL, "CCCD Demo", 0x55, 
                                APP_SERVICE_UUID, 0x0940, timeoutMs));
    
    // Create the service with characteristics and CCCDs
    ESP_ERROR_CHECK(bat_gatts_create_service(&g_server, charConfigs, 3, timeoutMs));

    // Start the GATT server and begin advertising
    ESP_ERROR_CHECK(bat_gatts_start(&g_server, &callbacks, timeoutMs));

    ESP_LOGI(TAG, "BLE CCCD server running");
    ESP_LOGI(TAG, "  - Char 1 (0xFF01): Read, Write, Notify");
    ESP_LOGI(TAG, "  - Char 2 (0xFF02): Read, Indicate");
    ESP_LOGI(TAG, "  - Char 3 (0xFF03): Read, Write, Notify, Indicate");
    
    // Main loop
    while (1)
    {
        // Nothing to do here - everything is handled by callbacks and timers
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Clean up
    stop_notification_timers(&g_server);

    for (int i = 0; i < 3; i++) 
    {
        if (pAppContext->updateTimers[i] != NULL) 
            xTimerDelete(pAppContext->updateTimers[i], 0);
    }

    ESP_ERROR_CHECK(bat_gatts_stop(&g_server, timeoutMs));
    ESP_ERROR_CHECK(bat_gatts_deinit(&g_server));
    ESP_ERROR_CHECK(bat_ble_lib_deinit());
    ESP_ERROR_CHECK(bat_lib_deinit());

    ESP_LOGI(TAG, "BLE APP server example finished");
}
