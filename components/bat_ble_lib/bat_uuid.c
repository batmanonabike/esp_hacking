#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>
#include "esp_log.h"
#include "bat_uuid.h"

static const char *TAG = "bat_uuid";

esp_err_t bat_uuid_from_string(const char *uuid_str, esp_bt_uuid_t *uuid)
{
    if (uuid_str == NULL || uuid == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters: uuid_str or uuid is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate string length (36 chars for standard UUID format)
    size_t len = strlen(uuid_str);
    if (len != 36)
    {
        ESP_LOGE(TAG, "Invalid UUID string length: %d (expected 36)", len);
        return ESP_ERR_INVALID_ARG;
    }

    // Check for expected hyphens
    if (uuid_str[8] != '-' || uuid_str[13] != '-' || uuid_str[18] != '-' || uuid_str[23] != '-')
    {
        ESP_LOGE(TAG, "Invalid UUID format, expected hyphens at positions 8, 13, 18, 23");
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize the 128-bit UUID
    uuid->len = ESP_UUID_LEN_128;
    uint8_t *uuid128 = uuid->uuid.uuid128;

    // Used to track current byte position in uuid array
    int uuid_byte_index = 0;

    // Parse the string character by character
    for (int i = 0; i < 36; i++)
    {
        // Skip hyphens
        if (uuid_str[i] == '-')
        {
            continue;
        }

        // Process hex characters in pairs to form bytes
        if (uuid_byte_index < ESP_UUID_LEN_128 && i + 1 < 36 && uuid_str[i + 1] != '-')
        {
            char hex[3] = {uuid_str[i], uuid_str[i + 1], 0};

            // Validate hex characters
            for (int j = 0; j < 2; j++)
            {
                if (!isxdigit((unsigned char)hex[j]))
                {
                    ESP_LOGE(TAG, "Invalid hex character in UUID: %c", hex[j]);
                    return ESP_ERR_INVALID_ARG;
                }
            }

            // Convert hex to byte
            unsigned int byte_val;
            sscanf(hex, "%02x", &byte_val);

            // Store in reverse order for ESP32 BLE stack
            // UUID format in memory: Little-endian for first three groups, then big-endian
            if (uuid_byte_index < 4)
            {
                // First group (time_low): little-endian
                uuid128[3 - uuid_byte_index] = (uint8_t)byte_val;
            }
            else if (uuid_byte_index < 6)
            {
                // Second group (time_mid): little-endian
                uuid128[5 - (uuid_byte_index - 4)] = (uint8_t)byte_val;
            }
            else if (uuid_byte_index < 8)
            {
                // Third group (time_high_and_version): little-endian
                uuid128[7 - (uuid_byte_index - 6)] = (uint8_t)byte_val;
            }
            else
            {
                // Fourth and fifth groups: big-endian
                uuid128[uuid_byte_index] = (uint8_t)byte_val;
            }

            uuid_byte_index++;
            i++; // Skip the next character as we've processed it
        }
    }

    ESP_LOGD(TAG, "UUID parsed successfully");
    return ESP_OK;
}

esp_err_t bat_uuid_from_16bit(uint16_t uuid_16, esp_bt_uuid_t *uuid)
{
    if (uuid == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameter: uuid is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uuid->len = ESP_UUID_LEN_16;
    uuid->uuid.uuid16 = uuid_16;

    ESP_LOGD(TAG, "Created 16-bit UUID: 0x%04x", uuid_16);
    return ESP_OK;
}

esp_err_t bat_uuid_from_32bit(uint32_t uuid_32, esp_bt_uuid_t *uuid)
{
    if (uuid == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameter: uuid is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uuid->len = ESP_UUID_LEN_32;
    uuid->uuid.uuid32 = uuid_32;

    ESP_LOGD(TAG, "Created 32-bit UUID: 0x%08" PRIx32, uuid_32);
    return ESP_OK;
}

bool bat_uuid_equal(const esp_bt_uuid_t *uuid1, const esp_bt_uuid_t *uuid2)
{
    if (uuid1 == NULL || uuid2 == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters: uuid1 or uuid2 is NULL");
        return false;
    }

    // If lengths are different, UUIDs cannot be equal
    if (uuid1->len != uuid2->len)
    {
        return false;
    }

    // Compare based on UUID length
    switch (uuid1->len)
    {
    case ESP_UUID_LEN_16:
        return uuid1->uuid.uuid16 == uuid2->uuid.uuid16;

    case ESP_UUID_LEN_32:
        return uuid1->uuid.uuid32 == uuid2->uuid.uuid32;

    case ESP_UUID_LEN_128:
        return memcmp(uuid1->uuid.uuid128, uuid2->uuid.uuid128, ESP_UUID_LEN_128) == 0;

    default:
        ESP_LOGE(TAG, "Invalid UUID length: %d", uuid1->len);
        return false;
    }
}

esp_err_t bat_uuid_to_string(const esp_bt_uuid_t *uuid, char *str, size_t len)
{
    if (uuid == NULL || str == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters: uuid or str is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (len < 37)
    { // 36 chars for UUID + null terminator
        ESP_LOGE(TAG, "Buffer too small for UUID string");
        return ESP_ERR_INVALID_SIZE;
    }

    switch (uuid->len)
    {
    case ESP_UUID_LEN_16:
        snprintf(str, len, "%04x", uuid->uuid.uuid16);
        return ESP_OK;

    case ESP_UUID_LEN_32:
        snprintf(str, len, "%08" PRIx32, uuid->uuid.uuid32);
        return ESP_OK;
 
    case ESP_UUID_LEN_128:
    {
        const uint8_t *u = uuid->uuid.uuid128;

        // Format as standard UUID string with correct byte order
        snprintf(str, len,
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 u[3], u[2], u[1], u[0],                  // time_low (little-endian)
                 u[5], u[4],                              // time_mid (little-endian)
                 u[7], u[6],                              // time_hi_and_version (little-endian)
                 u[8], u[9],                              // clock_seq_hi_and_reserved, clock_seq_low (big-endian)
                 u[10], u[11], u[12], u[13], u[14], u[15] // node (big-endian)
        );
        return ESP_OK;
    }

    default:
        ESP_LOGE(TAG, "Invalid UUID length: %d", uuid->len);
        return ESP_ERR_INVALID_ARG;
    }
}

/**
 * @brief Formats UUID for logging, with appropriate prefix based on UUID length
 * 
 * This function formats UUIDs in a standardized way for logging:
 * - 16-bit UUIDs: "0xXXXX"
 * - 32-bit UUIDs: "0xXXXXXXXX"
 * - 128-bit UUIDs: "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
 * 
 * @param uuid     Pointer to esp_bt_uuid_t to format
 * @param buffer   Buffer to store formatted string
 * @param buf_size Size of the buffer (should be at least 45 bytes)
 * 
 * @return Pointer to the formatted string (same as buffer)
 */
const char *bat_uuid_to_log_string(const esp_bt_uuid_t *uuid, char *buffer, size_t buf_size)
{
    if (uuid == NULL || buffer == NULL || buf_size < 12) 
    {
        // Minimal size needed for "Invalid UUID" + null
        if (buffer != NULL && buf_size >= 12) 
        {
            strncpy(buffer, "Invalid UUID", buf_size - 1);
            buffer[buf_size - 1] = '\0';
        }
        return buffer;
    }
    
    // Clear the buffer
    memset(buffer, 0, buf_size);
    
    switch (uuid->len) 
    {
        case ESP_UUID_LEN_16:
            // Format: 0xXXXX
            snprintf(buffer, buf_size, "0x%04x", uuid->uuid.uuid16);
            break;
            
        case ESP_UUID_LEN_32:
            // Format: 0xXXXXXXXX
            snprintf(buffer, buf_size, "0x%08" PRIx32, uuid->uuid.uuid32);
            break;
            
        case ESP_UUID_LEN_128:
            // Just use the existing to_string function for 128-bit
            if (bat_uuid_to_string(uuid, buffer, buf_size) != ESP_OK) 
            {
                strncpy(buffer, "Invalid 128-bit UUID", buf_size - 1);
                buffer[buf_size - 1] = '\0';
            }
            break;
            
        default:
            snprintf(buffer, buf_size, "Invalid UUID len=%d", uuid->len);
            break;
    }
    
    return buffer;
}