#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t uuid[ESP_UUID_LEN_128];
} bitmans_ble_uuid_t;

/**
 * @brief Converts a 128-bit UUID string to a 16-byte array (little-endian)
 *        and stores it in the bitmans_ble_uuid_t struct.
 *
 * The UUID string must be in the format "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" (36 characters).
 * The output byte array (in pUUID->uuid) will have the byte order reversed
 * compared to the string representation, which is common for 128-bit service UUIDs
 * in BLE advertising data.
 *
 * @param pszUUID The null-terminated UUID string.
 * @param pUUID Pointer to a bitmans_ble_uuid_t struct where the converted UUID will be stored.
 * @return true if conversion was successful, false otherwise (e.g., invalid format, null pointers).
 */
esp_err_t bitmans_ble_string_to_uuid(const char *pszUUID, bitmans_ble_uuid_t *pUUID);

#ifdef __cplusplus
}
#endif

