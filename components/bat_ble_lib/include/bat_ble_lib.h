#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

#include "bat_lib.h"
#include "bat_gatts_fsm.h"
#include "bat_ble_server.h"
#include "bat_gatts_simple.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
    char ch;
} bat_ble_lib_t;

esp_err_t bat_ble_lib_init(bat_lib_t, bat_ble_lib_t *pBleLib);
esp_err_t bat_ble_lib_deinit(bat_ble_lib_t ble_lib);

#ifdef __cplusplus
}
#endif

