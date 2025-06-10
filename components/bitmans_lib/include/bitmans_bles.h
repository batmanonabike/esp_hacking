#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_bt_defs.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "bitmans_ble.h"
#include "bitmans_hash_table.h"

#ifdef __cplusplus
extern "C" {
#endif

// Constants
#define BITMANS_BLES_MAX_SERVICES 8
#define BITMANS_BLES_MAX_CHARACTERISTICS 16

// Forward declarations
typedef struct bitmans_bles_server_t bitmans_bles_server_t;
typedef struct bitmans_bles_service_t bitmans_bles_service_t;
typedef struct bitmans_bles_characteristic_t bitmans_bles_characteristic_t;

// Server-wide states
typedef enum {
    BITMANS_BLES_STATE_IDLE,
    BITMANS_BLES_STATE_INITIALIZING,
    BITMANS_BLES_STATE_READY,
    BITMANS_BLES_STATE_REGISTERING_APPS,
    BITMANS_BLES_STATE_CREATING_SERVICES,
    BITMANS_BLES_STATE_ADDING_CHARACTERISTICS,
    BITMANS_BLES_STATE_ADDING_DESCRIPTORS,
    BITMANS_BLES_STATE_STARTING_SERVICES,
    BITMANS_BLES_STATE_SETTING_ADV_DATA,
    BITMANS_BLES_STATE_ADVERTISING,
    BITMANS_BLES_STATE_CONNECTED,
    BITMANS_BLES_STATE_STOPPING_ADVERTISING,
    BITMANS_BLES_STATE_STOPPING_SERVICES,
    BITMANS_BLES_STATE_DELETING_SERVICES,
    BITMANS_BLES_STATE_UNREGISTERING_APPS,
    BITMANS_BLES_STATE_ERROR
} bitmans_bles_state_t;

// Individual service states
typedef enum {
    BITMANS_BLES_SERVICE_STATE_DEFINED,
    BITMANS_BLES_SERVICE_STATE_REGISTERING,
    BITMANS_BLES_SERVICE_STATE_REGISTERED,
    BITMANS_BLES_SERVICE_STATE_CREATING,
    BITMANS_BLES_SERVICE_STATE_CREATED,
    BITMANS_BLES_SERVICE_STATE_ADDING_CHARS,
    BITMANS_BLES_SERVICE_STATE_CHARS_ADDED,
    BITMANS_BLES_SERVICE_STATE_ADDING_DESCRIPTORS,
    BITMANS_BLES_SERVICE_STATE_DESCRIPTORS_ADDED,
    BITMANS_BLES_SERVICE_STATE_STARTING,
    BITMANS_BLES_SERVICE_STATE_STARTED,
    BITMANS_BLES_SERVICE_STATE_STOPPING,
    BITMANS_BLES_SERVICE_STATE_STOPPED,
    BITMANS_BLES_SERVICE_STATE_DELETING,
    BITMANS_BLES_SERVICE_STATE_ERROR
} bitmans_bles_service_state_t;

// Error types for detailed error handling
typedef enum {
    BITMANS_BLES_ERROR_NONE,
    BITMANS_BLES_ERROR_INIT_FAILED,
    BITMANS_BLES_ERROR_APP_REGISTER_FAILED,
    BITMANS_BLES_ERROR_SERVICE_CREATE_FAILED,
    BITMANS_BLES_ERROR_CHAR_ADD_FAILED,
    BITMANS_BLES_ERROR_DESCRIPTOR_ADD_FAILED,
    BITMANS_BLES_ERROR_SERVICE_START_FAILED,
    BITMANS_BLES_ERROR_ADV_CONFIG_FAILED,
    BITMANS_BLES_ERROR_ADV_START_FAILED,
    BITMANS_BLES_ERROR_TIMEOUT,
    BITMANS_BLES_ERROR_INVALID_STATE,
    BITMANS_BLES_ERROR_NO_MEMORY,
    BITMANS_BLES_ERROR_INTERNAL
} bitmans_bles_error_t;

// Event types for user callbacks
typedef enum {
    BITMANS_BLES_EVENT_SERVER_READY,
    BITMANS_BLES_EVENT_SERVICE_READY,
    BITMANS_BLES_EVENT_ADVERTISING_STARTED,
    BITMANS_BLES_EVENT_ADVERTISING_STOPPED,
    BITMANS_BLES_EVENT_CLIENT_CONNECTED,
    BITMANS_BLES_EVENT_CLIENT_DISCONNECTED,
    BITMANS_BLES_EVENT_READ_REQUEST,
    BITMANS_BLES_EVENT_WRITE_REQUEST,
    BITMANS_BLES_EVENT_NOTIFY_ENABLED,
    BITMANS_BLES_EVENT_NOTIFY_DISABLED,
    BITMANS_BLES_EVENT_ERROR
} bitmans_bles_event_type_t;

// Characteristic definition
typedef struct {
    bitmans_ble_uuid128_t uuid;
    esp_gatt_char_prop_t properties;
    esp_gatt_perm_t permissions;
    bool add_cccd;
    const char *name;
    uint16_t max_length;        // Maximum data length for this characteristic
    uint8_t *initial_value;     // Initial value (optional)
    uint16_t initial_value_len; // Length of initial value
} bitmans_bles_char_def_t;

// Service definition
typedef struct {
    bitmans_ble_uuid128_t uuid;
    const char *name;
    uint8_t app_id;  // Unique app ID for this service
    
    // Characteristics array
    bitmans_bles_char_def_t *characteristics;
    uint8_t characteristic_count;
    
    // Service behavior
    bool auto_start;          // Automatically start service after all chars/descriptors added
    bool include_in_adv;      // Include this service UUID in advertising data
} bitmans_bles_service_def_t;

// Server configuration
typedef struct {
    const char *device_name;
    uint16_t appearance;           // BLE appearance value
    uint32_t min_conn_interval;    // Connection interval min (units of 1.25ms)
    uint32_t max_conn_interval;    // Connection interval max (units of 1.25ms)
    uint16_t adv_int_min;         // Advertising interval min
    uint16_t adv_int_max;         // Advertising interval max
    
    // Task configuration
    uint32_t task_stack_size;     // Stack size for server task
    uint8_t task_priority;        // Priority for server task
    uint32_t event_queue_size;    // Size of event queue
    
    // Timeout configuration
    uint32_t operation_timeout_ms; // Timeout for async operations
} bitmans_bles_config_t;

// Event data structures
typedef struct {
    bitmans_bles_service_t *service;
} bitmans_bles_event_service_ready_t;

typedef struct {
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
} bitmans_bles_event_connected_t;

typedef struct {
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    esp_gatt_conn_reason_t reason;
} bitmans_bles_event_disconnected_t;

typedef struct {
    bitmans_bles_service_t *service;
    bitmans_bles_characteristic_t *characteristic;
    uint16_t conn_id;
    uint32_t trans_id;
    uint16_t offset;
} bitmans_bles_event_read_request_t;

typedef struct {
    bitmans_bles_service_t *service;
    bitmans_bles_characteristic_t *characteristic;
    uint16_t conn_id;
    uint32_t trans_id;
    uint16_t offset;
    uint8_t *data;
    uint16_t len;
    bool need_rsp;
} bitmans_bles_event_write_request_t;

typedef struct {
    bitmans_bles_service_t *service;
    bitmans_bles_characteristic_t *characteristic;
    uint16_t conn_id;
    bool enabled;
} bitmans_bles_event_notify_t;

typedef struct {
    bitmans_bles_error_t error_code;
    bitmans_bles_state_t state;
    const char *description;
    esp_err_t esp_error;
} bitmans_bles_event_error_t;

// Union of all event data
typedef union {
    bitmans_bles_event_service_ready_t service_ready;
    bitmans_bles_event_connected_t connected;
    bitmans_bles_event_disconnected_t disconnected;
    bitmans_bles_event_read_request_t read_request;
    bitmans_bles_event_write_request_t write_request;
    bitmans_bles_event_notify_t notify;
    bitmans_bles_event_error_t error;
} bitmans_bles_event_data_t;

// Event structure passed to user callbacks
typedef struct {
    bitmans_bles_event_type_t type;
    bitmans_bles_event_data_t data;
    void *user_context;
} bitmans_bles_event_t;

// User callback function type
typedef void (*bitmans_bles_event_callback_t)(const bitmans_bles_event_t *event);

// Periodic callback function type (called in server task context)
typedef void (*bitmans_bles_periodic_callback_t)(void *user_context);

// User callbacks structure
typedef struct {
    bitmans_bles_event_callback_t event_callback;       // Main event callback
    bitmans_bles_periodic_callback_t periodic_callback; // Optional periodic callback
    uint32_t periodic_interval_ms;                      // Interval for periodic callback (0 = disabled)
    void *user_context;                                 // User context passed to callbacks
} bitmans_bles_callbacks_t;

// Runtime characteristic structure (internal)
struct bitmans_bles_characteristic_t {
    bitmans_bles_char_def_t definition;
    uint16_t handle;
    uint16_t cccd_handle;
    bitmans_bles_service_t *service;  // Back reference
};

// Runtime service structure (internal)
struct bitmans_bles_service_t {
    bitmans_bles_service_def_t definition;
    bitmans_bles_service_state_t state;
    esp_gatt_if_t gatts_if;
    uint16_t service_handle;
    
    // Runtime characteristics
    bitmans_bles_characteristic_t *characteristics;
    
    // State tracking
    uint8_t current_char_index;
    uint8_t current_descr_index;
    
    bitmans_bles_server_t *server;  // Back reference
};

// Main server structure (mostly internal)
struct bitmans_bles_server_t {
    bitmans_bles_state_t state;
    bitmans_bles_config_t config;
    bitmans_bles_callbacks_t callbacks;
    
    // Services
    bitmans_bles_service_t *services;
    uint8_t service_count;
    uint8_t current_service_index;
    
    // Connection tracking
    uint16_t conn_id;
    esp_bd_addr_t remote_bda;
    bool is_connected;
    
    // Task and synchronization
    TaskHandle_t server_task_handle;
    QueueHandle_t event_queue;
    EventGroupHandle_t operation_events;
    
    // Error tracking
    bitmans_bles_error_t last_error;
    esp_err_t last_esp_error;
    
    // Hash tables for quick lookups
    bitmans_hash_table_t gatts_if_to_service;  // Maps gatts_if -> service
    bitmans_hash_table_t handle_to_char;       // Maps handle -> characteristic
    
    // Operation timeout
    uint32_t operation_timeout_ms;
    
    // Flags
    bool stop_requested;
    bool advertising_enabled;
};

// Default configuration
#define BITMANS_BLES_DEFAULT_CONFIG() { \
    .device_name = "ESP32 BLE Server", \
    .appearance = 0x0000, \
    .min_conn_interval = 0x0006, \
    .max_conn_interval = 0x0010, \
    .adv_int_min = 0x20, \
    .adv_int_max = 0x40, \
    .task_stack_size = 4096, \
    .task_priority = 5, \
    .event_queue_size = 32, \
    .operation_timeout_ms = 10000 \
}

// API Functions

/**
 * @brief Initialize the BLE server
 * @param config Server configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_init(const bitmans_bles_config_t *config);

/**
 * @brief Set user callbacks
 * @param callbacks User callback functions and context
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_set_callbacks(const bitmans_bles_callbacks_t *callbacks);

/**
 * @brief Add a service definition to the server
 * @param service_def Service definition
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_add_service(const bitmans_bles_service_def_t *service_def);

/**
 * @brief Start the BLE server (register apps, create services, etc.)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_start(void);

/**
 * @brief Stop the BLE server gracefully
 * @param timeout_ms Timeout in milliseconds
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_stop(uint32_t timeout_ms);

/**
 * @brief Start advertising
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_start_advertising(void);

/**
 * @brief Stop advertising
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_stop_advertising(void);

/**
 * @brief Send a response to a read/write request
 * @param conn_id Connection ID
 * @param trans_id Transaction ID
 * @param status GATT status
 * @param handle Attribute handle
 * @param data Response data (can be NULL for write responses)
 * @param len Data length
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_send_response(uint16_t conn_id, uint32_t trans_id, 
                                     esp_gatt_status_t status, uint16_t handle,
                                     const uint8_t *data, uint16_t len);

/**
 * @brief Send a notification to connected client
 * @param characteristic Characteristic to notify
 * @param data Notification data
 * @param len Data length
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_send_notification(bitmans_bles_characteristic_t *characteristic,
                                         const uint8_t *data, uint16_t len);

/**
 * @brief Send an indication to connected client
 * @param characteristic Characteristic to indicate
 * @param data Indication data
 * @param len Data length
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bitmans_bles_send_indication(bitmans_bles_characteristic_t *characteristic,
                                       const uint8_t *data, uint16_t len);

/**
 * @brief Get current server state
 * @return bitmans_bles_state_t Current state
 */
bitmans_bles_state_t bitmans_bles_get_state(void);

/**
 * @brief Get last error information
 * @param error_code Output: Error code
 * @param esp_error Output: ESP-IDF error code
 * @return const char* Error description string
 */
const char* bitmans_bles_get_last_error(bitmans_bles_error_t *error_code, esp_err_t *esp_error);

/**
 * @brief Check if server is connected to a client
 * @return bool True if connected
 */
bool bitmans_bles_is_connected(void);

/**
 * @brief Get connection information
 * @param conn_id Output: Connection ID
 * @param remote_bda Output: Remote device address
 * @return bool True if connected and info retrieved
 */
bool bitmans_bles_get_connection_info(uint16_t *conn_id, esp_bd_addr_t *remote_bda);

/**
 * @brief Clear error state and attempt recovery
 * @return esp_err_t ESP_OK if recovery successful
 */
esp_err_t bitmans_bles_clear_error(void);

/**
 * @brief Get service by name
 * @param name Service name
 * @return bitmans_bles_service_t* Pointer to service or NULL if not found
 */
bitmans_bles_service_t* bitmans_bles_get_service_by_name(const char *name);

/**
 * @brief Get characteristic by name within a service
 * @param service Service to search in
 * @param name Characteristic name
 * @return bitmans_bles_characteristic_t* Pointer to characteristic or NULL if not found
 */
bitmans_bles_characteristic_t* bitmans_bles_get_characteristic_by_name(
    bitmans_bles_service_t *service, const char *name);

/**
 * @brief Get characteristic by handle
 * @param handle Attribute handle
 * @return bitmans_bles_characteristic_t* Pointer to characteristic or NULL if not found
 */
bitmans_bles_characteristic_t* bitmans_bles_get_characteristic_by_handle(uint16_t handle);

/**
 * @brief Cleanup and deinitialize the BLE server
 */
void bitmans_bles_deinit(void);

#ifdef __cplusplus
}
#endif
