#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

#include "bat_gatts_fsm.h"
#include "bat_ble_server.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bat_ble_lib_init();
esp_err_t bat_ble_lib_deinit();

#ifdef __cplusplus
}
#endif

