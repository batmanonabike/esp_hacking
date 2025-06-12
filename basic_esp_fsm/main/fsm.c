#include "fsm.h"
#include <string.h>
#include <stdio.h>
#include "freertos/task.h"

// Default no-op callback functions
static void default_state_changed_callback(fsm_context_t *pFsmContext, fsm_state_t oldState, fsm_state_t newState)
{
    // No-op
}

static void default_event_processed_callback(fsm_context_t *pFsmContext, fsm_event_t event, esp_err_t result)
{
    // No-op
}

static void default_connection_data_callback(fsm_context_t *pFsmContext, const char *pData, size_t dataLen)
{
    // No-op
}

// Helper function to transition states
static esp_err_t fsm_transition_to_state(fsm_context_t *pContext, fsm_state_t newState)
{
    if (pContext == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (newState >= FSM_STATE_MAX) 
    {
        ESP_LOGE(pContext->szTag, "Invalid state transition to %d", newState);
        return ESP_ERR_INVALID_ARG;
    }

    fsm_state_t oldState = pContext->currentState;
    pContext->currentState = newState;

    ESP_LOGI(pContext->szTag, "State transition: %s -> %s", 
             fsm_state_to_string(oldState), 
             fsm_state_to_string(newState));

    // Notify callback
    if (pContext->callbacks.on_state_changed != NULL) 
    {
        pContext->callbacks.on_state_changed(pContext, oldState, newState);
    }

    // Set transition complete bit
    xEventGroupSetBits(pContext->eventGroup, FSM_TRANSITION_COMPLETE_BIT);

    return ESP_OK;
}

// Initialize the FSM
esp_err_t fsm_init(fsm_context_t *pContext, const char *szTag)
{
    if (pContext == NULL || szTag == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize context
    memset(pContext, 0, sizeof(fsm_context_t));
    pContext->szTag = szTag;
    pContext->currentState = FSM_STATE_DISCONNECTED;

    // Set up state handlers using function pointers
    pContext->stateHandlers[FSM_STATE_DISCONNECTED] = fsm_state_disconnected_handler;
    pContext->stateHandlers[FSM_STATE_CONNECTING] = fsm_state_connecting_handler;
    pContext->stateHandlers[FSM_STATE_CONNECTED] = fsm_state_connected_handler;
    pContext->stateHandlers[FSM_STATE_DISCONNECTING] = fsm_state_disconnecting_handler;

    // Initialize state information
    snprintf(pContext->stateInfo.szConnectionId, sizeof(pContext->stateInfo.szConnectionId), "CONN_%08X", (unsigned int)pContext);
    pContext->stateInfo.ulConnectionAttempts = 0;
    pContext->stateInfo.ulConnectedTime = 0;
    pContext->stateInfo.ulDataBytesSent = 0;
    pContext->stateInfo.ulDataBytesReceived = 0;
    pContext->stateInfo.bIsSecure = false;

    // Initialize callbacks with no-op functions
    pContext->callbacks.on_state_changed = default_state_changed_callback;
    pContext->callbacks.on_event_processed = default_event_processed_callback;
    pContext->callbacks.on_connection_data = default_connection_data_callback;

    // Create event group
    pContext->eventGroup = xEventGroupCreate();
    if (pContext->eventGroup == NULL) 
    {
        ESP_LOGE(szTag, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(szTag, "FSM initialized in state: %s", fsm_state_to_string(pContext->currentState));
    return ESP_OK;
}

// Terminate the FSM
esp_err_t fsm_deinit(fsm_context_t *pContext)
{
    if (pContext == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(pContext->szTag, "Terminating FSM");

    // Clean up event group
    if (pContext->eventGroup != NULL) 
    {
        vEventGroupDelete(pContext->eventGroup);
        pContext->eventGroup = NULL;
    }

    // Clear context
    memset(pContext, 0, sizeof(fsm_context_t));

    return ESP_OK;
}

// Set callbacks
esp_err_t fsm_set_callbacks(fsm_context_t *pContext, const fsm_callbacks_t *pCallbacks)
{
    if (pContext == NULL || pCallbacks == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Copy callbacks, keeping defaults for NULL pointers
    if (pCallbacks->on_state_changed != NULL) 
    {
        pContext->callbacks.on_state_changed = pCallbacks->on_state_changed;
    }
    
    if (pCallbacks->on_event_processed != NULL) 
    {
        pContext->callbacks.on_event_processed = pCallbacks->on_event_processed;
    }
    
    if (pCallbacks->on_connection_data != NULL) 
    {
        pContext->callbacks.on_connection_data = pCallbacks->on_connection_data;
    }

    pContext->callbacks.pContext = pCallbacks->pContext;

    return ESP_OK;
}

// Process an event
esp_err_t fsm_process_event(fsm_context_t *pContext, fsm_event_t event)
{
    if (pContext == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (event >= FSM_EVENT_MAX) 
    {
        ESP_LOGE(pContext->szTag, "Invalid event: %d", event);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(pContext->szTag, "Processing event: %s in state: %s", 
             fsm_event_to_string(event), 
             fsm_state_to_string(pContext->currentState));

    // Clear previous event bits
    xEventGroupClearBits(pContext->eventGroup, FSM_EVENT_PROCESSED_BIT | FSM_ERROR_BIT);

    // Call the appropriate state handler using function pointer
    fsm_state_func_t stateHandler = pContext->stateHandlers[pContext->currentState];
    esp_err_t result = ESP_ERR_INVALID_STATE;
    
    if (stateHandler != NULL) 
    {
        result = stateHandler(pContext, event);
    }
    else 
    {
        ESP_LOGE(pContext->szTag, "No handler for state: %s", fsm_state_to_string(pContext->currentState));
    }

    // Set appropriate event bits
    if (result == ESP_OK) 
    {
        xEventGroupSetBits(pContext->eventGroup, FSM_EVENT_PROCESSED_BIT);
    }
    else 
    {
        xEventGroupSetBits(pContext->eventGroup, FSM_ERROR_BIT);
    }

    // Notify callback
    if (pContext->callbacks.on_event_processed != NULL) 
    {
        pContext->callbacks.on_event_processed(pContext, event, result);
    }

    return result;
}

// Get current state
esp_err_t fsm_get_current_state(fsm_context_t *pContext, fsm_state_t *pState)
{
    if (pContext == NULL || pState == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    *pState = pContext->currentState;
    return ESP_OK;
}

// Convert state to string
const char *fsm_state_to_string(fsm_state_t state)
{
    switch (state) 
    {
        case FSM_STATE_DISCONNECTED:   return "DISCONNECTED";
        case FSM_STATE_CONNECTING:     return "CONNECTING";
        case FSM_STATE_CONNECTED:      return "CONNECTED";
        case FSM_STATE_DISCONNECTING:  return "DISCONNECTING";
        default:                       return "UNKNOWN";
    }
}

// Convert event to string
const char *fsm_event_to_string(fsm_event_t event)
{
    switch (event) 
    {
        case FSM_EVENT_CONNECT_REQUEST:    return "CONNECT_REQUEST";
        case FSM_EVENT_CONNECTION_SUCCESS: return "CONNECTION_SUCCESS";
        case FSM_EVENT_CONNECTION_FAILED:  return "CONNECTION_FAILED";
        case FSM_EVENT_DISCONNECT_REQUEST: return "DISCONNECT_REQUEST";
        case FSM_EVENT_CONNECTION_LOST:    return "CONNECTION_LOST";
        case FSM_EVENT_TIMEOUT:            return "TIMEOUT";
        default:                           return "UNKNOWN";
    }
}

// State handler implementations

// DISCONNECTED state handler
esp_err_t fsm_state_disconnected_handler(fsm_context_t *pContext, fsm_event_t event)
{
    if (pContext == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event) 
    {
        case FSM_EVENT_CONNECT_REQUEST:
            ESP_LOGI(pContext->szTag, "Received connect request in DISCONNECTED state");
            pContext->stateInfo.ulConnectionAttempts++;
            pContext->stateInfo.bIsSecure = false; // Reset security flag
            return fsm_transition_to_state(pContext, FSM_STATE_CONNECTING);

        case FSM_EVENT_CONNECTION_SUCCESS:
        case FSM_EVENT_CONNECTION_FAILED:
        case FSM_EVENT_DISCONNECT_REQUEST:
        case FSM_EVENT_CONNECTION_LOST:
            ESP_LOGW(pContext->szTag, "Ignoring event %s in DISCONNECTED state", 
                     fsm_event_to_string(event));
            return ESP_OK;

        case FSM_EVENT_TIMEOUT:
            ESP_LOGD(pContext->szTag, "Timeout in DISCONNECTED state - no action needed");
            return ESP_OK;

        default:
            ESP_LOGE(pContext->szTag, "Unhandled event %s in DISCONNECTED state", 
                     fsm_event_to_string(event));
            return ESP_ERR_INVALID_ARG;
    }
}

// CONNECTING state handler
esp_err_t fsm_state_connecting_handler(fsm_context_t *pContext, fsm_event_t event)
{
    if (pContext == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event) 
    {
        case FSM_EVENT_CONNECTION_SUCCESS:
            ESP_LOGI(pContext->szTag, "Connection established successfully");
            pContext->stateInfo.ulConnectedTime = 0; // Reset connected time
            pContext->stateInfo.bIsSecure = true;    // Set security flag
            pContext->stateInfo.ulDataBytesSent = 0;
            pContext->stateInfo.ulDataBytesReceived = 0;
            return fsm_transition_to_state(pContext, FSM_STATE_CONNECTED);

        case FSM_EVENT_CONNECTION_FAILED:
        case FSM_EVENT_TIMEOUT:
            ESP_LOGW(pContext->szTag, "Connection failed/timeout - returning to DISCONNECTED");
            return fsm_transition_to_state(pContext, FSM_STATE_DISCONNECTED);

        case FSM_EVENT_DISCONNECT_REQUEST:
            ESP_LOGI(pContext->szTag, "Disconnect requested during connection attempt");
            return fsm_transition_to_state(pContext, FSM_STATE_DISCONNECTING);

        case FSM_EVENT_CONNECT_REQUEST:
            ESP_LOGW(pContext->szTag, "Already connecting - ignoring additional connect request");
            return ESP_OK;

        case FSM_EVENT_CONNECTION_LOST:
            ESP_LOGW(pContext->szTag, "Connection lost during connection attempt");
            return fsm_transition_to_state(pContext, FSM_STATE_DISCONNECTED);

        default:
            ESP_LOGE(pContext->szTag, "Unhandled event %s in CONNECTING state", 
                     fsm_event_to_string(event));
            return ESP_ERR_INVALID_ARG;
    }
}

// CONNECTED state handler
esp_err_t fsm_state_connected_handler(fsm_context_t *pContext, fsm_event_t event)
{
    if (pContext == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event) 
    {
        case FSM_EVENT_DISCONNECT_REQUEST:
            ESP_LOGI(pContext->szTag, "Disconnect requested in CONNECTED state");
            return fsm_transition_to_state(pContext, FSM_STATE_DISCONNECTING);

        case FSM_EVENT_CONNECTION_LOST:
            ESP_LOGW(pContext->szTag, "Connection lost unexpectedly");
            return fsm_transition_to_state(pContext, FSM_STATE_DISCONNECTED);

        case FSM_EVENT_TIMEOUT:
            ESP_LOGD(pContext->szTag, "Timeout in CONNECTED state - updating connected time");
            pContext->stateInfo.ulConnectedTime++;
            // Simulate some data transfer
            pContext->stateInfo.ulDataBytesSent += 10;
            pContext->stateInfo.ulDataBytesReceived += 15;
            
            // Notify data callback with simulated data
            if (pContext->callbacks.on_connection_data != NULL) 
            {
                char szData[64];
                snprintf(szData, sizeof(szData), "KeepAlive-%lu", pContext->stateInfo.ulConnectedTime);
                pContext->callbacks.on_connection_data(pContext, szData, strlen(szData));
            }
            return ESP_OK;

        case FSM_EVENT_CONNECT_REQUEST:
            ESP_LOGW(pContext->szTag, "Already connected - ignoring connect request");
            return ESP_OK;

        case FSM_EVENT_CONNECTION_SUCCESS:
            ESP_LOGD(pContext->szTag, "Already connected - ignoring connection success");
            return ESP_OK;

        case FSM_EVENT_CONNECTION_FAILED:
            ESP_LOGW(pContext->szTag, "Connection failed event in CONNECTED state");
            return fsm_transition_to_state(pContext, FSM_STATE_DISCONNECTED);

        default:
            ESP_LOGE(pContext->szTag, "Unhandled event %s in CONNECTED state", 
                     fsm_event_to_string(event));
            return ESP_ERR_INVALID_ARG;
    }
}

// DISCONNECTING state handler
esp_err_t fsm_state_disconnecting_handler(fsm_context_t *pContext, fsm_event_t event)
{
    if (pContext == NULL) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event) 
    {
        case FSM_EVENT_CONNECTION_LOST:
        case FSM_EVENT_TIMEOUT:
            ESP_LOGI(pContext->szTag, "Disconnection completed");
            // Log final statistics
            ESP_LOGI(pContext->szTag, "Connection stats - Attempts: %lu, Connected time: %lu sec, Sent: %lu bytes, Received: %lu bytes",
                     pContext->stateInfo.ulConnectionAttempts,
                     pContext->stateInfo.ulConnectedTime,
                     pContext->stateInfo.ulDataBytesSent,
                     pContext->stateInfo.ulDataBytesReceived);
            return fsm_transition_to_state(pContext, FSM_STATE_DISCONNECTED);

        case FSM_EVENT_CONNECT_REQUEST:
            ESP_LOGW(pContext->szTag, "Connect request during disconnection - will be queued");
            return ESP_OK;

        case FSM_EVENT_DISCONNECT_REQUEST:
            ESP_LOGD(pContext->szTag, "Already disconnecting - ignoring additional disconnect request");
            return ESP_OK;

        case FSM_EVENT_CONNECTION_SUCCESS:
            ESP_LOGW(pContext->szTag, "Unexpected connection success during disconnection");
            return fsm_transition_to_state(pContext, FSM_STATE_CONNECTED);

        case FSM_EVENT_CONNECTION_FAILED:
            ESP_LOGD(pContext->szTag, "Connection failed during disconnection - completing disconnect");
            return fsm_transition_to_state(pContext, FSM_STATE_DISCONNECTED);

        default:
            ESP_LOGE(pContext->szTag, "Unhandled event %s in DISCONNECTING state", 
                     fsm_event_to_string(event));
            return ESP_ERR_INVALID_ARG;
    }
}
