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

esp_err_t bitmans_ble_uuid16_to_uuid128(uint16_t uuid16, bitmans_ble_uuid128_t *);
esp_err_t bitmans_ble_string4_to_uuid128(const char *pszUUID, bitmans_ble_uuid128_t *);
esp_err_t bitmans_ble_string36_to_uuid128(const char *pszUUID, bitmans_ble_uuid128_t *);
esp_err_t bitmans_ble_uuid_match(const esp_bt_uuid_t *, const bitmans_ble_uuid128_t *, bool *pResult);
bool bitmans_ble_uuid_try_match(const esp_bt_uuid_t *pEspId, const bitmans_ble_uuid128_t *pUuid);

#ifdef __cplusplus
}
#endif

