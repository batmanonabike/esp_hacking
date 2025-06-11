#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "fsm.h"

static const char *TAG = "FSM_DEMO";

// Application context structure
typedef struct 
{
    fsm_context_t fsm;
    TaskHandle_t eventTaskHandle;
    TaskHandle_t timeoutTaskHandle;
    bool bRunning;
    uint32_t ulEventCounter;
} app_context_t;

static app_context_t g_appContext = {0};

// Callback function implementations
static void on_state_changed_callback(fsm_context_t *pFsmContext, fsm_state_t oldState, fsm_state_t newState)
{
    ESP_LOGI(TAG, "=== STATE CHANGE CALLBACK ===");
    ESP_LOGI(TAG, "Transition: %s -> %s", fsm_state_to_string(oldState), fsm_state_to_string(newState));
    ESP_LOGI(TAG, "Connection ID: %s", pFsmContext->stateInfo.szConnectionId);
    ESP_LOGI(TAG, "Connection attempts: %lu", pFsmContext->stateInfo.ulConnectionAttempts);
    ESP_LOGI(TAG, "Is secure: %s", pFsmContext->stateInfo.bIsSecure ? "Yes" : "No");
    ESP_LOGI(TAG, "=============================");
}

static void on_event_processed_callback(fsm_context_t *pFsmContext, fsm_event_t event, esp_err_t result)
{
    // app_context_t *pAppContext = (app_context_t *)pFsmContext; // Context passed back
    
    ESP_LOGD(TAG, "Event processed: %s, Result: %s", 
             fsm_event_to_string(event), 
             esp_err_to_name(result));
             
    if (result != ESP_OK) 
    {
        ESP_LOGE(TAG, "Event processing failed: %s", esp_err_to_name(result));
    }
}

static void on_connection_data_callback(fsm_context_t *pFsmContext, const char *pData, size_t dataLen)
{
    ESP_LOGI(TAG, "=== CONNECTION DATA ===");
    ESP_LOGI(TAG, "Received data (%d bytes): %.*s", (int)dataLen, (int)dataLen, pData);
    ESP_LOGI(TAG, "Total sent: %lu bytes, Total received: %lu bytes", 
             pFsmContext->stateInfo.ulDataBytesSent,
             pFsmContext->stateInfo.ulDataBytesReceived);
    ESP_LOGI(TAG, "=====================");
}

// Task to generate random events
static void event_generator_task(void *pvParameters)
{
    app_context_t *pAppContext = (app_context_t *)pvParameters;
    
    // Event sequence for demonstration
    fsm_event_t eventSequence[] = 
    {
        FSM_EVENT_CONNECT_REQUEST,      // Start connection
        FSM_EVENT_CONNECTION_SUCCESS,   // Connection succeeds
        FSM_EVENT_TIMEOUT,              // Some activity while connected
        FSM_EVENT_TIMEOUT,              // More activity
        FSM_EVENT_DISCONNECT_REQUEST,   // Request disconnect
        FSM_EVENT_CONNECTION_LOST,      // Complete disconnection
        
        FSM_EVENT_CONNECT_REQUEST,      // Try again
        FSM_EVENT_CONNECTION_FAILED,    // This time it fails
        
        FSM_EVENT_CONNECT_REQUEST,      // Try once more
        FSM_EVENT_CONNECTION_SUCCESS,   // Success
        FSM_EVENT_CONNECTION_LOST,      // Unexpected disconnection
    };
    
    const size_t numEvents = sizeof(eventSequence) / sizeof(eventSequence[0]);
    size_t eventIndex = 0;
    
    ESP_LOGI(TAG, "Event generator task started");
    
    while (pAppContext->bRunning && eventIndex < numEvents) 
    {
        // Wait between events
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        if (!pAppContext->bRunning) break;
        
        fsm_event_t event = eventSequence[eventIndex];
        
        ESP_LOGI(TAG, ">>> Generating event #%lu: %s <<<", 
                 ++pAppContext->ulEventCounter, 
                 fsm_event_to_string(event));
                 
        esp_err_t result = fsm_process_event(&pAppContext->fsm, event);
        if (result != ESP_OK) 
        {
            ESP_LOGE(TAG, "Failed to process event: %s", esp_err_to_name(result));
        }
        
        eventIndex++;
        
        // Show current state info
        fsm_state_t currentState;
        if (fsm_get_current_state(&pAppContext->fsm, &currentState) == ESP_OK) 
        {
            ESP_LOGI(TAG, "Current state: %s", fsm_state_to_string(currentState));
        }
    }
    
    ESP_LOGI(TAG, "Event generator task completed");
    pAppContext->eventTaskHandle = NULL;
    vTaskDelete(NULL);
}

// Task to generate timeout events for connected state
static void timeout_generator_task(void *pvParameters)
{
    app_context_t *pAppContext = (app_context_t *)pvParameters;
    
    ESP_LOGI(TAG, "Timeout generator task started");
    
    while (pAppContext->bRunning) 
    {
        // Check every 2 seconds
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        if (!pAppContext->bRunning) break;
        
        // Only send timeout events when connected
        fsm_state_t currentState;
        if (fsm_get_current_state(&pAppContext->fsm, &currentState) == ESP_OK) 
        {
            if (currentState == FSM_STATE_CONNECTED) 
            {
                ESP_LOGD(TAG, "Sending keepalive timeout event");
                esp_err_t result = fsm_process_event(&pAppContext->fsm, FSM_EVENT_TIMEOUT);
                if (result != ESP_OK) 
                {
                    ESP_LOGE(TAG, "Failed to process timeout event: %s", esp_err_to_name(result));
                }
            }
            else if (currentState == FSM_STATE_CONNECTING) 
            {
                // Simulate connection timeout after some time
                static int connectingCount = 0;
                connectingCount++;
                if (connectingCount > 3) // After 6 seconds of connecting
                {
                    ESP_LOGI(TAG, "Simulating connection timeout");
                    fsm_process_event(&pAppContext->fsm, FSM_EVENT_TIMEOUT);
                    connectingCount = 0;
                }
            }
            else if (currentState == FSM_STATE_DISCONNECTING) 
            {
                // Simulate disconnection completion
                static int disconnectingCount = 0;
                disconnectingCount++;
                if (disconnectingCount > 2) // After 4 seconds of disconnecting
                {
                    ESP_LOGI(TAG, "Simulating disconnection completion");
                    fsm_process_event(&pAppContext->fsm, FSM_EVENT_TIMEOUT);
                    disconnectingCount = 0;
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "Timeout generator task completed");
    pAppContext->timeoutTaskHandle = NULL;
    vTaskDelete(NULL);
}

// Initialize the application
static esp_err_t app_init(void)
{
    ESP_LOGI(TAG, "Initializing FSM Demo Application");
    
    // Initialize application context
    memset(&g_appContext, 0, sizeof(app_context_t));
    g_appContext.bRunning = true;
    g_appContext.ulEventCounter = 0;
    
    // Initialize the FSM
    esp_err_t ret = fsm_init(&g_appContext.fsm, "FSM");
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to initialize FSM: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set up callbacks
    fsm_callbacks_t callbacks = 
    {
        .pContext = &g_appContext,
        .on_state_changed = on_state_changed_callback,
        .on_event_processed = on_event_processed_callback,
        .on_connection_data = on_connection_data_callback
    };
    
    ret = fsm_set_callbacks(&g_appContext.fsm, &callbacks);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to set FSM callbacks: %s", esp_err_to_name(ret));
        fsm_term(&g_appContext.fsm);
        return ret;
    }
    
    ESP_LOGI(TAG, "FSM Demo Application initialized successfully");
    return ESP_OK;
}

// Cleanup the application
static void app_cleanup(void)
{
    ESP_LOGI(TAG, "Cleaning up FSM Demo Application");
    
    g_appContext.bRunning = false;
    
    // Wait for tasks to complete
    if (g_appContext.eventTaskHandle != NULL) 
    {
        ESP_LOGI(TAG, "Waiting for event task to complete...");
        while (g_appContext.eventTaskHandle != NULL) 
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    if (g_appContext.timeoutTaskHandle != NULL) 
    {
        ESP_LOGI(TAG, "Waiting for timeout task to complete...");
        while (g_appContext.timeoutTaskHandle != NULL) 
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    // Terminate FSM
    fsm_term(&g_appContext.fsm);
    
    ESP_LOGI(TAG, "FSM Demo Application cleanup completed");
}

// Main application entry point
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 Finite State Machine Demo ===");
    ESP_LOGI(TAG, "This demo showcases a connection-based FSM with function pointers");
    ESP_LOGI(TAG, "States: DISCONNECTED -> CONNECTING -> CONNECTED -> DISCONNECTING");
    ESP_LOGI(TAG, "Events: CONNECT_REQUEST, CONNECTION_SUCCESS, CONNECTION_FAILED,");
    ESP_LOGI(TAG, "        DISCONNECT_REQUEST, CONNECTION_LOST, TIMEOUT");
    
    ESP_LOGI(TAG, "===============================================");
    ESP_LOGI(TAG, "Starting Basic FSM Demo");
    
    // Initialize application
    esp_err_t ret = app_init();
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Application initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Show initial state
    fsm_state_t initialState;
    if (fsm_get_current_state(&g_appContext.fsm, &initialState) == ESP_OK) 
    {
        ESP_LOGI(TAG, "Starting in state: %s", fsm_state_to_string(initialState));
    }
    
    // Create event generator task
    BaseType_t taskResult = xTaskCreate(
        event_generator_task,
        "event_gen",
        4096,
        &g_appContext,
        5,
        &g_appContext.eventTaskHandle
    );
    
    if (taskResult != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to create event generator task");
        app_cleanup();
        return;
    }
    
    // Create timeout generator task
    taskResult = xTaskCreate(
        timeout_generator_task,
        "timeout_gen",
        4096,
        &g_appContext,
        4,
        &g_appContext.timeoutTaskHandle
    );
    
    if (taskResult != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to create timeout generator task");
        app_cleanup();
        return;
    }
    
    ESP_LOGI(TAG, "Tasks created successfully - demo is running");
    
    // Main loop - monitor the system
    uint32_t loopCount = 0;
    while (g_appContext.bRunning) 
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        loopCount++;
        
        ESP_LOGI(TAG, "=== System Status (Loop #%lu) ===", loopCount);
        
        fsm_state_t currentState;
        if (fsm_get_current_state(&g_appContext.fsm, &currentState) == ESP_OK) 
        {
            ESP_LOGI(TAG, "Current State: %s", fsm_state_to_string(currentState));
            ESP_LOGI(TAG, "Connection ID: %s", g_appContext.fsm.stateInfo.szConnectionId);
            ESP_LOGI(TAG, "Total Events Processed: %lu", g_appContext.ulEventCounter);
            ESP_LOGI(TAG, "Connection Attempts: %lu", g_appContext.fsm.stateInfo.ulConnectionAttempts);
            
            if (currentState == FSM_STATE_CONNECTED) 
            {
                ESP_LOGI(TAG, "Connected Time: %lu seconds", g_appContext.fsm.stateInfo.ulConnectedTime);
                ESP_LOGI(TAG, "Data Sent: %lu bytes", g_appContext.fsm.stateInfo.ulDataBytesSent);
                ESP_LOGI(TAG, "Data Received: %lu bytes", g_appContext.fsm.stateInfo.ulDataBytesReceived);
            }
        }
        
        ESP_LOGI(TAG, "==============================");
        
        // Check if tasks are still running
        if (g_appContext.eventTaskHandle == NULL && g_appContext.timeoutTaskHandle == NULL) 
        {
            ESP_LOGI(TAG, "All tasks completed - stopping demo");
            break;
        }
        
        // Run for maximum 60 seconds
        if (loopCount >= 12) 
        {
            ESP_LOGI(TAG, "Demo time limit reached - stopping");
            break;
        }
    }
    
    // Cleanup
    app_cleanup();
    
    ESP_LOGI(TAG, "=== FSM Demo Completed ===");
    ESP_LOGI(TAG, "Final Statistics:");
    ESP_LOGI(TAG, "Total Events: %lu", g_appContext.ulEventCounter);
    ESP_LOGI(TAG, "Connection Attempts: %lu", g_appContext.fsm.stateInfo.ulConnectionAttempts);
    ESP_LOGI(TAG, "==========================");
}
