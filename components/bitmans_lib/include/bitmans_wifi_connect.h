#pragma once

#include "esp_err.h"
#include "esp_wifi.h"  // This includes esp_wifi_types.h
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi connection configuration structure
 */
typedef struct {
    char ssid[32];             // WiFi SSID
    char password[64];         // WiFi password
    uint32_t heartbeat_ms;     // Heartbeat interval in milliseconds (default 2000)
    uint8_t max_missed_beats;  // Maximum number of missed heartbeats before marking connection as failed (default 10)
    wifi_auth_mode_t auth_mode; // WiFi authentication mode (default WPA2_PSK)
} bitmans_wifi_config_t;

/**
 * @brief WiFi connection status
 */
typedef enum {
    BITMANS_WIFI_DISCONNECTED,    // Not connected to WiFi
    BITMANS_WIFI_CONNECTING,      // Connection attempt in progress
    BITMANS_WIFI_CONNECTED,       // Connected to WiFi with valid IP
    BITMANS_WIFI_ERROR            // Error state
} bitmans_wifi_status_t;

/**
 * @brief Initialize the WiFi connection functionality
 * 
 * @param config Pointer to WiFi configuration structure. If NULL, will use default settings.
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_wifi_init(const bitmans_wifi_config_t *config);

/**
 * @brief Get the current WiFi connection status
 * 
 * @return bitmans_wifi_status_t Current WiFi status
 */
bitmans_wifi_status_t bitmans_wifi_get_status(void);

/**
 * @brief Get the current IP address as a string
 * 
 * @param ip_str Buffer to store IP address string (at least 16 bytes)
 * @param len Length of the buffer
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_wifi_get_ip(char *ip_str, size_t len);

/**
 * @brief Disconnect from the current WiFi network
 * 
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_wifi_disconnect(void);

/**
 * @brief Connect to the configured WiFi network
 * 
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_wifi_connect(void);

/**
 * @brief Update WiFi configuration
 * 
 * @param config New WiFi configuration
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_wifi_update_config(const bitmans_wifi_config_t *config);

/**
 * @brief Register a callback function for WiFi status changes
 * 
 * @param callback Function pointer to callback
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_wifi_register_callback(void (*callback)(bitmans_wifi_status_t status));

/**
 * @brief Terminate the WiFi connection functionality and free resources
 * 
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_wifi_deinit(void);

#ifdef __cplusplus
}
#endif
