#include <string.h> 
#include "esp_log.h"
#include "bitmans_ble.h"

static const char *TAG = "bitmans_lib:ble";

static esp_err_t bitmans_ble_parse_uuid(const char *psz, uint8_t *pValue)
{
    char *pszEnd;
    unsigned long val = strtol(psz, &pszEnd, 16);

    if (val > 255 || pszEnd == psz || (pszEnd - psz) != 2)
        return ESP_ERR_INVALID_STATE;

    *pValue = (uint8_t)val;
    return ESP_OK;
}

esp_err_t bitmans_ble_string_to_uuid(const char *pszUUID, bitmans_ble_uuid_t *pUUID)
{
    if (pszUUID == NULL || pUUID == NULL)
    {
        ESP_LOGE(TAG, "Null pointer provided.");
        return ESP_ERR_INVALID_STATE;
    }

    // Validate length and hyphen positions
    if (strlen(pszUUID) != 36)
    {
        ESP_LOGE(TAG, "Invalid UUID string length: %s", pszUUID);
        return ESP_ERR_INVALID_STATE;
    }

    if (pszUUID[8] != '-' || pszUUID[13] != '-' || pszUUID[18] != '-' || pszUUID[23] != '-')
    {
        ESP_LOGE(TAG, "Invalid UUID string format: %s", pszUUID);
        return ESP_ERR_INVALID_STATE;
    }

    const char *psz = pszUUID;
    uint8_t big_endian[ESP_UUID_LEN_128];

    for (int i = 0; i < ESP_UUID_LEN_128; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            psz++; // Skip hyphens

        esp_err_t err = bitmans_ble_parse_uuid(psz, &big_endian[i]);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to parse UUID string component at index %d.", i);
            return err;
        }
        psz += 2;
    }

    // Reverse the byte order to get the little-endian representation
    // and store it in the char array of the output struct.
    for (int i = 0; i < ESP_UUID_LEN_128; ++i)
        pUUID->uuid[i] = big_endian[ESP_UUID_LEN_128 - 1 - i];

    return ESP_OK;
}
