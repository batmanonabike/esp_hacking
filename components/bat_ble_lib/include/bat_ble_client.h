#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register a GATT client application with the stack
 *
 * @param app_id Application ID, used to identify the client
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_app_register(uint16_t app_id);

/**
 * @brief Open a connection to a BLE server
 *
 * @param gattc_if GATT client interface
 * @param peer_addr Bluetooth address of the peer device
 * @param addr_type Address type of the peer device
 * @param is_direct Whether to use direct connection or background connection procedure
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_open(esp_gatt_if_t gattc_if, esp_bd_addr_t peer_addr, esp_ble_addr_type_t addr_type, bool is_direct);

/**
 * @brief Close a connection to a BLE server
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_close(esp_gatt_if_t gattc_if, uint16_t conn_id);

/**
 * @brief Configure BLE scanning parameters
 *
 * @param scan_params Scan parameters
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gap_set_scan_params(esp_ble_scan_params_t *scan_params);

/**
 * @brief Start BLE scanning
 *
 * @param duration Scan duration in seconds, 0 means scan indefinitely
 * @param is_continue Whether this is continuing a previous scan
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gap_start_scanning(uint32_t duration, bool is_continue);

/**
 * @brief Stop BLE scanning
 *
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gap_stop_scanning(void);

/**
 * @brief Search for services in the connected server
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @param filter_uuid UUID to filter services (pass NULL for all services)
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_search_service(esp_gatt_if_t gattc_if, uint16_t conn_id, esp_bt_uuid_t *filter_uuid);

/**
 * @brief Read characteristic value
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @param handle Characteristic handle
 * @param auth_req Authentication requirement
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_read_char(esp_gatt_if_t gattc_if, uint16_t conn_id, 
                                 uint16_t handle, esp_gatt_auth_req_t auth_req);

/**
 * @brief Read descriptor value
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @param handle Descriptor handle
 * @param auth_req Authentication requirement
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_read_char_descr(esp_gatt_if_t gattc_if, uint16_t conn_id, 
                                       uint16_t handle, esp_gatt_auth_req_t auth_req);

/**
 * @brief Write characteristic value
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @param handle Characteristic handle
 * @param value_len Length of the value to write
 * @param value Value to write
 * @param write_type Write type (normal write, no response, prepare, etc)
 * @param auth_req Authentication requirement
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_write_char(esp_gatt_if_t gattc_if, uint16_t conn_id,
                                  uint16_t handle, uint16_t value_len,
                                  uint8_t *value, esp_gatt_write_type_t write_type,
                                  esp_gatt_auth_req_t auth_req);

/**
 * @brief Write descriptor value
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @param handle Descriptor handle
 * @param value_len Length of the value to write
 * @param value Value to write
 * @param write_type Write type (normal write, no response, prepare, etc)
 * @param auth_req Authentication requirement
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_write_char_descr(esp_gatt_if_t gattc_if, uint16_t conn_id,
                                        uint16_t handle, uint16_t value_len,
                                        uint8_t *value, esp_gatt_write_type_t write_type,
                                        esp_gatt_auth_req_t auth_req);

/**
 * @brief Register for notifications/indications from a characteristic
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @param handle Characteristic handle
 * @param register_for_notify True to register, false to unregister
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_register_for_notify(esp_gatt_if_t gattc_if, esp_bd_addr_t server_bda, 
                                           uint16_t handle, bool register_for_notify);

/**
 * @brief Get a characteristic by UUID within a specified handle range
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @param start_handle Start handle for the search range
 * @param end_handle End handle for the search range
 * @param char_uuid UUID of the characteristic to find
 * @param result Pointer to store the found characteristic
 * @param count Pointer to store the count of found characteristics (input max count, output actual count)
 * @return esp_gatt_status_t ESP_GATT_OK on success, appropriate error code otherwise
 */
esp_gatt_status_t bat_ble_gattc_get_char_by_uuid(esp_gatt_if_t gattc_if, uint16_t conn_id, 
                                              uint16_t start_handle, uint16_t end_handle, 
                                              esp_bt_uuid_t char_uuid, 
                                              esp_gattc_char_elem_t *result, 
                                              uint16_t *count);

/**
 * @brief Get a descriptor by UUID within a characteristic
 *
 * @param gattc_if GATT client interface
 * @param conn_id Connection ID
 * @param start_handle Start handle for the search range
 * @param end_handle End handle for the search range
 * @param char_uuid UUID of the characteristic containing the descriptor
 * @param descr_uuid UUID of the descriptor to find
 * @param result Pointer to store the found descriptor
 * @param count Pointer to store the count of found descriptors (input max count, output actual count)
 * @return esp_gatt_status_t ESP_GATT_OK on success, appropriate error code otherwise
 */
esp_gatt_status_t bat_ble_gattc_get_descr_by_uuid(esp_gatt_if_t gattc_if, uint16_t conn_id, 
                                               uint16_t start_handle, uint16_t end_handle, 
                                               esp_bt_uuid_t char_uuid, esp_bt_uuid_t descr_uuid, 
                                               esp_gattc_descr_elem_t *result, uint16_t *count);

/**
 * @brief Unregister GATT client application
 *
 * @param gattc_if GATT client interface
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise
 */
esp_err_t bat_ble_gattc_app_unregister(esp_gatt_if_t gattc_if);

#ifdef __cplusplus
}
#endif
