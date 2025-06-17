#ifndef BAT_UUID_H
#define BAT_UUID_H

#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"
#include "esp_err.h"

/**
 * @brief Converts UUID string to esp_bt_uuid_t (128-bit)
 * 
 * Parses UUID string in the format "f0debc9a-7856-3412-1234-56785612561A" 
 * and stores it in the provided uuid structure.
 * 
 * @param uuid_str String representation of UUID (format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
 * @param uuid     Pointer to esp_bt_uuid_t to store the result
 * 
 * @return ESP_OK if successful, otherwise appropriate error
 */
esp_err_t bat_uuid_from_string(const char *uuid_str, esp_bt_uuid_t *uuid);

/**
 * @brief Creates a 16-bit UUID
 * 
 * @param uuid_16  16-bit UUID value
 * @param uuid     Pointer to esp_bt_uuid_t to store the result
 * 
 * @return ESP_OK if successful
 */
esp_err_t bat_uuid_from_16bit(uint16_t uuid_16, esp_bt_uuid_t *uuid);

/**
 * @brief Creates a 32-bit UUID
 * 
 * @param uuid_32  32-bit UUID value
 * @param uuid     Pointer to esp_bt_uuid_t to store the result
 * 
 * @return ESP_OK if successful
 */
esp_err_t bat_uuid_from_32bit(uint32_t uuid_32, esp_bt_uuid_t *uuid);

/**
 * @brief Compares two UUIDs for equality
 * 
 * @param uuid1    First UUID to compare
 * @param uuid2    Second UUID to compare
 * 
 * @return true if UUIDs are equal, false otherwise
 */
bool bat_uuid_equal(const esp_bt_uuid_t *uuid1, const esp_bt_uuid_t *uuid2);

/**
 * @brief Converts a UUID to string representation
 * 
 * @param uuid     Pointer to esp_bt_uuid_t to convert
 * @param str      Buffer to store string representation
 * @param len      Length of the buffer (should be at least 37 bytes for 128-bit UUID)
 * 
 * @return ESP_OK if successful, otherwise appropriate error
 */
esp_err_t bat_uuid_to_string(const esp_bt_uuid_t *uuid, char *str, size_t len);

/**
 * @brief Formats UUID for logging, with appropriate prefix based on UUID length
 * 
 * @param uuid     Pointer to esp_bt_uuid_t to format
 * @param buffer   Buffer to store formatted string (should be at least 45 bytes)
 * @param buf_size Size of the buffer
 * 
 * @return Pointer to the formatted string (same as buffer)
 */
const char *bat_uuid_to_log_string(const esp_bt_uuid_t *uuid, char *buffer, size_t buf_size);

#endif /* BAT_UUID_H */