#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ProvideLib
 * 
 * @return ESP_OK on success, other error codes on failure
 */
esp_err_t provide_lib_init(void);

/**
 * @brief Log a message using ProvideLib
 * 
 * @param message The message to log
 */
void provide_lib_log_message(const char *message);

/**
 * @brief Get the version of ProvideLib
 * 
 * @return const char* The version string
 */
const char *provide_lib_get_version(void);

#ifdef __cplusplus
}
#endif
