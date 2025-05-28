#pragma once

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bitmans_lib_init(void);
const char * bitmans_lib_get_version(void);
void bitmans_lib_log_message(const char *message);

#ifdef __cplusplus
}
#endif
