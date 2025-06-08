#pragma once

#include "esp_err.h"
#include "freertos/event_groups.h"

#include "bitmans_ble.h"
#include "bitmans_blink.h"
#include "bitmans_ble_client.h"
#include "bitmans_ble_server.h"
#include "bitmans_wifi_logging.h"
#include "bitmans_wifi_connect.h"
#include "bitmans_ble_client_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

// BitmansLib core functions
esp_err_t bitmans_lib_init(void);
const char * bitmans_lib_get_version(void);
void bitmans_lib_log_message(const char *message);
esp_err_t bitmans_waitbits_forever(EventGroupHandle_t, int register_bit,  EventBits_t *);
esp_err_t bitmans_waitbits(EventGroupHandle_t, int register_bit,  TickType_t, EventBits_t *);

void _bitmans_error_check_restart(esp_err_t, const char *, int, const char *, const char *);

#ifdef __cplusplus
}
#endif

// The was coppied from esp_err.h, but modified to restart the system instead of aborting.
#define ESP_ERROR_CHECK_RESTART(x) do {                                 \
        esp_err_t err_rc_ = (x);                                        \
        if (unlikely(err_rc_ != ESP_OK)) {                              \
            _bitmans_error_check_restart(err_rc_, __FILE__, __LINE__,   \
                                     __ASSERT_FUNC, #x);                \
        }                                                               \
    } while(0)
    