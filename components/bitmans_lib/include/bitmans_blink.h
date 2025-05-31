#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Blink modes to indicate different ESP states
 */
typedef enum {
    BLINK_MODE_NONE,       // LED always off
    BLINK_MODE_BASIC,      // Regular on/off pattern (500ms on, 500ms off)
    BLINK_MODE_SLOW,       // Slow blinking (1000ms on, 1000ms off)
    BLINK_MODE_MEDIUM,     // Medium speed blinking (300ms on, 300ms off)
    BLINK_MODE_FAST,       // Fast blinking (100ms on, 100ms off)
    BLINK_MODE_VERY_FAST,  // Very fast blinking (50ms on, 50ms off)
    BLINK_MODE_BREATHING,  // Gradual fade in and out,
    BLINK_MODE_ON          // LED always on
} blink_mode_t;

/**
 * @brief Initialize the LED blink functionality
 * 
 * @param gpio_pin GPIO pin number for the LED (defaults to GPIO_NUM_2 if set to -1)
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_blink_init(int gpio_pin);

/**
 * @brief Set the current blink mode
 * 
 * @param mode The blink mode to set
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_set_blink_mode(blink_mode_t mode);

/**
 * @brief Get the current blink mode
 * 
 * @return blink_mode_t The current blink mode
 */
blink_mode_t bitmans_get_blink_mode(void);


/**
 * @brief Terminate the LED blink functionality and free resources
 * 
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_blink_term(void);

#ifdef __cplusplus
}
#endif
