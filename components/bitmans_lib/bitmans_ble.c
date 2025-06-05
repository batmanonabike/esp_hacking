#include <string.h>
#include "esp_log.h"
#include "bitmans_ble.h"

static const char *TAG = "bitmans_lib:ble";

static int8_t bitmans_ble_hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1; // Invalid hex character
}

static esp_err_t bitmans_ble_parse_uuid(const char *psz, uint8_t *pValue)
{
    int8_t high = bitmans_ble_hex_value(psz[0]);
    int8_t low = bitmans_ble_hex_value(psz[1]);

    if (high == -1 || low == -1)
    {
        ESP_LOGE(TAG, "Invalid hexadecimal characters: %c%c", psz[0], psz[1]);
        return ESP_ERR_INVALID_ARG;
    }

    *pValue = (high << 4) | low;
    return ESP_OK;
}

esp_err_t bitmans_ble_string_to_uuid128(const char *psz, bitmans_ble_uuid128_t *pId)
{
    if (psz == NULL || pId == NULL)
    {
        ESP_LOGE(TAG, "Null pointer provided.");
        return ESP_ERR_INVALID_STATE;
    }

    // Validate length and hyphen positions
    if (strlen(psz) != 36)
    {
        ESP_LOGE(TAG, "Invalid UUID string length: %s", psz);
        return ESP_ERR_INVALID_STATE;
    }

    if (psz[8] != '-' || psz[13] != '-' || psz[18] != '-' || psz[23] != '-')
    {
        ESP_LOGE(TAG, "Invalid UUID string format: %s", psz);
        return ESP_ERR_INVALID_STATE;
    }

    const char *pszMarker = psz;
    uint8_t big_endian[ESP_UUID_LEN_128];

    for (int i = 0; i < ESP_UUID_LEN_128; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            pszMarker++; // Skip hyphens

        esp_err_t err = bitmans_ble_parse_uuid(pszMarker, &big_endian[i]);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to parse UUID string component at index %d.", i);
            return err;
        }
        pszMarker += 2;
    }

    // Reverse the byte order to get the little-endian representation
    // and store it in the char array of the output struct.
    for (int i = 0; i < ESP_UUID_LEN_128; ++i)
        pId->uuid[i] = big_endian[ESP_UUID_LEN_128 - 1 - i];

    return ESP_OK;
}
