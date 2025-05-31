#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bitmans_ble_init();
esp_err_t bitmans_ble_term();
void bitmans_ble_start_scan(void);

#ifdef __cplusplus
}
#endif

