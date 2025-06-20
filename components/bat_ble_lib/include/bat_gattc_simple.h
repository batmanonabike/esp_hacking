#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BAT_MAX_SCAN_DEVICES 20
#define BAT_MAX_SERVICES 10
#define BAT_MAX_CHARACTERISTICS 20
#define BLE_CLIENT_OPERATION_TIMEOUT_MS 5000

// Define event bits for GATTC state
#define BLE_CLIENT_REGISTERED_BIT      (1 << 0)
#define BLE_CLIENT_CONNECTED_BIT       (1 << 1)
#define BLE_CLIENT_DISCONNECTED_BIT    (1 << 2)
#define BLE_CLIENT_SERVICE_FOUND_BIT   (1 << 3)
#define BLE_CLIENT_CHAR_FOUND_BIT      (1 << 4)
#define BLE_CLIENT_READ_DONE_BIT       (1 << 5)
#define BLE_CLIENT_WRITE_DONE_BIT      (1 << 6)
#define BLE_CLIENT_NOTIFY_REG_BIT      (1 << 7)
#define BLE_CLIENT_NOTIFY_RECV_BIT     (1 << 8)
#define BLE_CLIENT_DESC_FOUND_BIT      (1 << 9)
#define BLE_CLIENT_DESC_WRITE_DONE_BIT (1 << 10)
#define BLE_CLIENT_SCAN_DONE_BIT       (1 << 11)
#define BLE_CLIENT_ERROR_BIT           (1 << 12)

// Forward declaration for client structure
typedef struct bat_gattc_client_s bat_gattc_client_t;

// Callback types
typedef void (*bat_gattc_scan_result_cb_t)(bat_gattc_client_t *pClient, esp_ble_gap_cb_param_t *pParam);
typedef void (*bat_gattc_connected_cb_t)(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam);
typedef void (*bat_gattc_disconnect_cb_t)(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam);
typedef void (*bat_gattc_service_found_cb_t)(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam);
typedef void (*bat_gattc_char_found_cb_t)(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam, esp_gattc_char_elem_t *char_elem);
typedef void (*bat_gattc_descr_found_cb_t)(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam, esp_gattc_descr_elem_t *descr_elem);
typedef void (*bat_gattc_read_cb_t)(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam);
typedef void (*bat_gattc_write_cb_t)(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam);
typedef void (*bat_gattc_notify_cb_t)(bat_gattc_client_t *pClient, esp_ble_gattc_cb_param_t *pParam);

// Callback structure
typedef struct {
    bat_gattc_scan_result_cb_t    onScanResult;
    bat_gattc_connected_cb_t      onConnect;
    bat_gattc_disconnect_cb_t     onDisconnect;
    bat_gattc_service_found_cb_t  onServiceFound;
    bat_gattc_char_found_cb_t     onCharFound;
    bat_gattc_descr_found_cb_t    onDescrFound;
    bat_gattc_read_cb_t           onRead;
    bat_gattc_write_cb_t          onWrite;
    bat_gattc_notify_cb_t         onNotify;
} bat_gattc_callbacks_t;

// Device scan result structure
typedef struct {
    esp_bd_addr_t addr;
    esp_ble_addr_type_t addr_type;
    char name[32];
    int8_t rssi;
    bool has_service_uuid;
    esp_bt_uuid_t service_uuid;
} bat_gattc_scan_result_t;

// Client structure
typedef struct bat_gattc_client_s {
    // User context
    void *pContext;
    
    // Client identification
    uint16_t appId;
    esp_gatt_if_t gattcIf;
    
    // Event handling
    EventGroupHandle_t eventGroup;
    
    // Connection state
    bool isConnected;
    uint16_t connId;
    esp_bd_addr_t remoteBda;
    esp_ble_addr_type_t remoteAddrType;
    
    // Services and characteristics
    esp_bt_uuid_t targetServiceUuid;
    uint16_t serviceStartHandle;
    uint16_t serviceEndHandle;
    
    // Callback functions
    bat_gattc_callbacks_t callbacks;
    
    // Scan results
    uint8_t scanResultCount;
    bat_gattc_scan_result_t scanResults[BAT_MAX_SCAN_DEVICES];
    
    // Characteristics
    uint8_t charCount;
    esp_gattc_char_elem_t chars[BAT_MAX_CHARACTERISTICS];
    esp_bt_uuid_t targetCharUuids[BAT_MAX_CHARACTERISTICS];
    
    // Descriptors (primarily for CCCD)
    uint16_t cccdHandles[BAT_MAX_CHARACTERISTICS];
    
    // Scan parameters
    esp_ble_scan_params_t scanParams;
} bat_gattc_client_t;

/**
 * @brief Initialize the GATTC client
 * 
 * @param pClient Pointer to client structure
 * @param pContext User context pointer
 * @param appId Application ID (must be unique)
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_init(bat_gattc_client_t *pClient, void *pContext, uint16_t appId, int timeoutMs);

/**
 * @brief Configure scan parameters
 * 
 * @param pClient Pointer to client structure
 * @param scanInterval Scan interval
 * @param scanWindow Scan window
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_set_scan_params(bat_gattc_client_t *pClient, uint16_t scanInterval, uint16_t scanWindow);

/**
 * @brief Start scanning for BLE devices
 * 
 * @param pClient Pointer to client structure
 * @param durationSec Scan duration in seconds (0 for continuous)
 * @param pCallbacks Callback functions
 * @param pTargetServiceUuid Target service UUID to filter by (optional, can be NULL)
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_start_scan(bat_gattc_client_t *pClient, uint32_t durationSec, 
                              bat_gattc_callbacks_t *pCallbacks, esp_bt_uuid_t *pTargetServiceUuid, 
                              int timeoutMs);

/**
 * @brief Stop scanning for BLE devices
 * 
 * @param pClient Pointer to client structure
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_stop_scan(bat_gattc_client_t *pClient);

/**
 * @brief Connect to a device from scan results
 * 
 * @param pClient Pointer to client structure
 * @param deviceIndex Index in scan results array
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_connect(bat_gattc_client_t *pClient, uint8_t deviceIndex, int timeoutMs);

/**
 * @brief Connect to a device directly using its address
 * 
 * @param pClient Pointer to client structure
 * @param addr Device address
 * @param addrType Device address type
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_connect_by_addr(bat_gattc_client_t *pClient, esp_bd_addr_t addr, 
                                   esp_ble_addr_type_t addrType, int timeoutMs);

/**
 * @brief Disconnect from current device
 * 
 * @param pClient Pointer to client structure
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_disconnect(bat_gattc_client_t *pClient);

/**
 * @brief Search for a specific service by UUID
 * 
 * @param pClient Pointer to client structure
 * @param serviceUuid Service UUID to search for
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_search_service(bat_gattc_client_t *pClient, esp_bt_uuid_t serviceUuid, int timeoutMs);

/**
 * @brief Search for characteristics within a service
 * 
 * @param pClient Pointer to client structure
 * @param pCharUuids Array of characteristic UUIDs to search for
 * @param numChars Number of characteristics in the array
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_get_characteristics(bat_gattc_client_t *pClient, esp_bt_uuid_t *pCharUuids, 
                                       uint8_t numChars, int timeoutMs);

/**
 * @brief Find the Client Config Descriptor (CCCD) for a characteristic
 * 
 * @param pClient Pointer to client structure
 * @param charIndex Index of the characteristic in the client's chars array
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_get_descriptor(bat_gattc_client_t *pClient, uint8_t charIndex, int timeoutMs);

/**
 * @brief Read a characteristic value
 * 
 * @param pClient Pointer to client structure
 * @param charIndex Index of the characteristic in the client's chars array
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_read_char(bat_gattc_client_t *pClient, uint8_t charIndex, int timeoutMs);

/**
 * @brief Write a value to a characteristic
 * 
 * @param pClient Pointer to client structure
 * @param charIndex Index of the characteristic in the client's chars array
 * @param pValue Value to write
 * @param length Length of the value
 * @param writeType Write type (e.g., with response, no response)
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_write_char(bat_gattc_client_t *pClient, uint8_t charIndex, 
                              uint8_t *pValue, uint16_t length, 
                              esp_gatt_write_type_t writeType, int timeoutMs);

/**
 * @brief Register for notifications/indications from a characteristic
 * 
 * @param pClient Pointer to client structure
 * @param charIndex Index of the characteristic in the client's chars array
 * @param enableNotifications Whether to enable or disable notifications
 * @param enableIndications Whether to enable or disable indications
 * @param timeoutMs Timeout for operations in ms
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_register_for_notify(bat_gattc_client_t *pClient, uint8_t charIndex,
                                       bool enableNotifications, bool enableIndications,
                                       int timeoutMs);

/**
 * @brief De-initialize the GATTC client
 * 
 * @param pClient Pointer to client structure
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_deinit(bat_gattc_client_t *pClient);

/**
 * @brief Reset event flags in the client
 * 
 * @param pClient Pointer to client structure
 */
void bat_gattc_reset_flags(bat_gattc_client_t *pClient);

/**
 * @brief Get a device name from scan results
 * 
 * @param pClient Pointer to client structure
 * @param deviceIndex Index of device in scan results
 * @param nameBuffer Buffer to store name
 * @param bufferSize Size of name buffer
 * @return esp_err_t ESP_OK on success, error otherwise
 */
esp_err_t bat_gattc_get_device_name(bat_gattc_client_t *pClient, uint8_t deviceIndex, 
                                   char *nameBuffer, size_t bufferSize);

#ifdef __cplusplus
}
#endif
