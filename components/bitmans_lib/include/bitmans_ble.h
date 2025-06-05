#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t uuid[ESP_UUID_LEN_128];
} bitmans_ble_uuid128_t;

esp_err_t bitmans_ble_string_to_uuid128(const char *pszUUID, bitmans_ble_uuid128_t *);

#ifdef __cplusplus
}
#endif

