#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bitmans_ble_server_init();
esp_err_t bitmans_ble_server_term();
esp_err_t bitmans_ble_server_register_gatts();
esp_err_t bitmans_ble_server_unregister_gatts();

#ifdef __cplusplus
}
#endif
