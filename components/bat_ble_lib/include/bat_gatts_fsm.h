#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// GATTS State definitions - States represent the lifecycle of a GATTS server
typedef enum 
{
    BAT_GATTS_STATE_IDLE = 0,          // Initial state before any initialization
    BAT_GATTS_STATE_INITIALIZING,      // BT stack initialization in progress
    BAT_GATTS_STATE_READY,             // BT stack initialized, ready for app registration
    BAT_GATTS_STATE_APP_REGISTERING,   // Application registration in progress
    BAT_GATTS_STATE_APP_REGISTERED,    // Application registered, ready to create service
    BAT_GATTS_STATE_SERVICE_CREATING,  // Service creation in progress
    BAT_GATTS_STATE_SERVICE_CREATED,   // Service created, ready to add characteristics
    BAT_GATTS_STATE_CHAR_ADDING,       // Characteristic addition in progress
    BAT_GATTS_STATE_CHAR_ADDED,        // Characteristics added, ready to add descriptors
    BAT_GATTS_STATE_DESC_ADDING,       // Descriptor addition in progress
    BAT_GATTS_STATE_DESC_ADDED,        // Descriptors added, ready to start service
    BAT_GATTS_STATE_SERVICE_STARTING,  // Service starting in progress
    BAT_GATTS_STATE_SERVICE_STARTED,   // Service started, ready to advertise
    BAT_GATTS_STATE_ADVERTISING,       // Advertising in progress
    BAT_GATTS_STATE_CONNECTED,         // Client connected
    BAT_GATTS_STATE_DISCONNECTING,     // Disconnection in progress
    BAT_GATTS_STATE_SERVICE_STOPPING,  // Service stopping in progress
    BAT_GATTS_STATE_SERVICE_STOPPED,   // Service stopped
    BAT_GATTS_STATE_APP_UNREGISTERING, // Application unregistration in progress
    BAT_GATTS_STATE_ERROR,             // Error state
    BAT_GATTS_STATE_MAX
} bat_gatts_state_t;

// GATTS Event definitions - Events that can trigger state transitions
typedef enum 
{
    BAT_GATTS_EVENT_INIT_REQUEST = 0,     // Request to initialize BLE stack
    BAT_GATTS_EVENT_INIT_COMPLETE,        // BLE stack initialization complete
    BAT_GATTS_EVENT_REGISTER_REQUEST,     // Request to register GATT server app
    BAT_GATTS_EVENT_REGISTER_COMPLETE,    // GATT server app registration complete
    BAT_GATTS_EVENT_CREATE_SERVICE,       // Request to create service
    BAT_GATTS_EVENT_SERVICE_CREATED,      // Service creation complete
    BAT_GATTS_EVENT_ADD_CHAR_REQUEST,     // Request to add characteristic
    BAT_GATTS_EVENT_CHAR_ADDED,           // Characteristic addition complete
    BAT_GATTS_EVENT_ADD_DESC_REQUEST,     // Request to add descriptor
    BAT_GATTS_EVENT_DESC_ADDED,           // Descriptor addition complete
    BAT_GATTS_EVENT_START_SERVICE,        // Request to start service
    BAT_GATTS_EVENT_SERVICE_STARTED,      // Service start complete
    BAT_GATTS_EVENT_START_ADV_REQUEST,    // Request to start advertising
    BAT_GATTS_EVENT_ADV_STARTED,          // Advertising start complete
    BAT_GATTS_EVENT_CONNECT,              // Client connected
    BAT_GATTS_EVENT_DISCONNECT,           // Client disconnected
    BAT_GATTS_EVENT_STOP_ADV_REQUEST,     // Request to stop advertising
    BAT_GATTS_EVENT_ADV_STOPPED,          // Advertising stop complete
    BAT_GATTS_EVENT_STOP_SERVICE,         // Request to stop service
    BAT_GATTS_EVENT_SERVICE_STOPPED,      // Service stop complete
    BAT_GATTS_EVENT_UNREGISTER_REQUEST,   // Request to unregister GATTS app
    BAT_GATTS_EVENT_UNREGISTER_COMPLETE,  // GATTS app unregistration complete
    BAT_GATTS_EVENT_RESET,                // Reset request to go back to IDLE state
    BAT_GATTS_EVENT_ERROR,                // Error occurred
    BAT_GATTS_EVENT_TIMEOUT,              // Timeout occurred
    BAT_GATTS_EVENT_MAX
} bat_gatts_event_t;

// Event bits for synchronization
#define BAT_GATTS_EVENT_PROCESSED_BIT      BIT0
#define BAT_GATTS_TRANSITION_COMPLETE_BIT  BIT1
#define BAT_GATTS_ERROR_BIT                BIT2
#define BAT_GATTS_READY_TO_ADVERTISE_BIT   BIT3
#define BAT_GATTS_CLIENT_CONNECTED_BIT     BIT4
#define BAT_GATTS_CLIENT_DISCONNECTED_BIT  BIT5

// Forward declaration
struct bat_gatts_fsm_context_t;

// State function pointer type
typedef esp_err_t (*bat_gatts_state_func_t)(struct bat_gatts_fsm_context_t *pContext, bat_gatts_event_t event);

// TODO: 
// Function declarations
esp_err_t bat_gatts_fsm_init(void);
esp_err_t bat_gatts_fsm_process_event(bat_gatts_event_t event);
esp_err_t bat_gatts_fsm_get_state(bat_gatts_state_t *pState);

#ifdef __cplusplus
}
#endif

