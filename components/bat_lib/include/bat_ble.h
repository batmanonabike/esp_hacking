#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t uuid[ESP_UUID_LEN_128];
} bat_ble_uuid128_t;

typedef uint16_t bat_ble_uuid16_t;

esp_err_t bat_ble_uuid16_to_uuid128(bat_ble_uuid16_t, bat_ble_uuid128_t *);
esp_err_t bat_ble_string4_to_uuid128(const char *pszUUID, bat_ble_uuid128_t *);
esp_err_t bat_ble_string36_to_uuid128(const char *pszUUID, bat_ble_uuid128_t *);
esp_err_t bat_ble_uuid_match(const esp_bt_uuid_t *, const bat_ble_uuid128_t *, bool *pResult);

void bat_ble_log_uuid128(const char *context, const uint8_t *uuid_bytes);
bool bat_ble_uuid_try_match(const esp_bt_uuid_t *pEspId, const bat_ble_uuid128_t *pUuid);

#ifdef __cplusplus
}
#endif

