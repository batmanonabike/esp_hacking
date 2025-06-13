#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

#include "bat_gatts_fsm.h"
#include "bat_ble_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
} bat_ble_lib_t;

esp_err_t bat_ble_lib_init();
esp_err_t bat_ble_lib_deinit();

esp_err_t bat_ble_init(bat_ble_lib_t *pLib);
esp_err_t bat_ble_deinit(bat_ble_lib_t lib);

#ifdef __cplusplus
}
#endif

