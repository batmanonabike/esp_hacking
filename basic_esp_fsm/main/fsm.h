#ifndef FSM_H
#define FSM_H

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// State definitions
typedef enum 
{
    FSM_STATE_DISCONNECTED = 0,
    FSM_STATE_CONNECTING,
    FSM_STATE_CONNECTED,
    FSM_STATE_DISCONNECTING,
    FSM_STATE_MAX
} fsm_state_t;

// Event definitions
typedef enum 
{
    FSM_EVENT_CONNECT_REQUEST = 0,
    FSM_EVENT_CONNECTION_SUCCESS,
    FSM_EVENT_CONNECTION_FAILED,
    FSM_EVENT_DISCONNECT_REQUEST,
    FSM_EVENT_CONNECTION_LOST,
    FSM_EVENT_TIMEOUT,
    FSM_EVENT_MAX
} fsm_event_t;

// Forward declaration
struct fsm_context_t;

// State function pointer type
typedef esp_err_t (*fsm_state_func_t)(struct fsm_context_t *pContext, fsm_event_t event);

// State information structure
typedef struct 
{
    char szConnectionId[32];        // Connection identifier
    uint32_t ulConnectionAttempts;  // Number of connection attempts
    uint32_t ulConnectedTime;       // Time connected in seconds
    uint32_t ulDataBytesSent;       // Data bytes sent
    uint32_t ulDataBytesReceived;   // Data bytes received
    bool bIsSecure;                 // Security status
} fsm_state_info_t;

// Callback structure for state change notifications
typedef struct 
{
    void *pContext;
    void (*on_state_changed)(struct fsm_context_t *pFsmContext, fsm_state_t oldState, fsm_state_t newState);
    void (*on_event_processed)(struct fsm_context_t *pFsmContext, fsm_event_t event, esp_err_t result);
    void (*on_connection_data)(struct fsm_context_t *pFsmContext, const char *pData, size_t dataLen);
} fsm_callbacks_t;

// Main FSM context structure
typedef struct fsm_context_t 
{
    fsm_state_t currentState;                           // Current state
    fsm_state_func_t stateHandlers[FSM_STATE_MAX];      // Function pointers for each state
    fsm_state_info_t stateInfo;                         // State information
    fsm_callbacks_t callbacks;                          // Callback functions
    EventGroupHandle_t eventGroup;                      // Event group for synchronization
    const char *szTag;                                  // Log tag
} fsm_context_t;

// Event bits for synchronization
#define FSM_EVENT_PROCESSED_BIT     BIT0
#define FSM_TRANSITION_COMPLETE_BIT BIT1
#define FSM_ERROR_BIT               BIT2

// Function declarations
esp_err_t fsm_init(fsm_context_t *pContext, const char *szTag);
esp_err_t fsm_term(fsm_context_t *pContext);
esp_err_t fsm_set_callbacks(fsm_context_t *pContext, const fsm_callbacks_t *pCallbacks);
esp_err_t fsm_process_event(fsm_context_t *pContext, fsm_event_t event);
esp_err_t fsm_get_current_state(fsm_context_t *pContext, fsm_state_t *pState);
const char *fsm_state_to_string(fsm_state_t state);
const char *fsm_event_to_string(fsm_event_t event);

// State handler function declarations
esp_err_t fsm_state_disconnected_handler(fsm_context_t *pContext, fsm_event_t event);
esp_err_t fsm_state_connecting_handler(fsm_context_t *pContext, fsm_event_t event);
esp_err_t fsm_state_connected_handler(fsm_context_t *pContext, fsm_event_t event);
esp_err_t fsm_state_disconnecting_handler(fsm_context_t *pContext, fsm_event_t event);

#endif // FSM_H
