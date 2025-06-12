#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bat_gatts_fsm(void);

#ifdef __cplusplus
}
#endif

