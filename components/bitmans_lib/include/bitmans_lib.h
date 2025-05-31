#pragma once

#include "esp_err.h"
#include "bitmans_ble.h"
#include "bitmans_blink.h"
#include "bitmans_wifi_logging.h"
#include "bitmans_wifi_connect.h"

#ifdef __cplusplus
extern "C" {
#endif

// BitmansLib core functions
esp_err_t bitmans_lib_init(void);
const char * bitmans_lib_get_version(void);
void bitmans_lib_log_message(const char *message);

#ifdef __cplusplus
}
#endif
