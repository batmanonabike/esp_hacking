#pragma once

#include "esp_err.h"

#include "bat_ble.h"
#include "bat_blink.h"
#include "bat_ble_client.h"
#include "bat_ble_server.h"
#include "bat_wifi_logging.h"
#include "bat_wifi_connect.h"
#include "bat_ble_client_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

// BitmansLib core functions
esp_err_t bat_lib_init(void);
const char * bat_lib_get_version(void);
void bat_lib_log_message(const char *message);
esp_err_t bat_waitbits_forever(EventGroupHandle_t, int register_bit,  EventBits_t *);
esp_err_t bat_waitbits(EventGroupHandle_t, int register_bit,  TickType_t, EventBits_t *);

void _bat_error_check_restart(esp_err_t, const char *, int, const char *, const char *);

#ifdef __cplusplus
}
#endif

// The was copied from esp_err.h, but modified to restart the system instead of aborting.
#define ESP_ERROR_CHECK_RESTART(x) do {                                 \
        esp_err_t err_rc_ = (x);                                        \
        if (unlikely(err_rc_ != ESP_OK)) {                              \
            _bat_error_check_restart(err_rc_, __FILE__, __LINE__,   \
                                     __ASSERT_FUNC, #x);                \
        }                                                               \
    } while(0)
    