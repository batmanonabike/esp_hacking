#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "bitmans_blink.h"

static const char *TAG = "bitmans_lib:blink";

// Blink related variables
static int led_gpio = GPIO_NUM_2; // Default LED pin
static bool is_breathing_mode = false;
static TaskHandle_t blink_task_handle = NULL;
static QueueHandle_t blink_mode_queue = NULL;
static blink_mode_t current_blink_mode = BLINK_MODE_NONE;

// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html#introduction
static void configure_led_pwm_for_breathing_effect(void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = led_gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&ledc_channel);
}

// Task to handle LED blinking based on current mode
static void blink_task(void *pvParameters)
{
    int led_state = 0;
    int fade_value = 0;
    int fade_direction = 1;
    blink_mode_t mode_update;

    // Set up the LED pin
    gpio_reset_pin(led_gpio);
    gpio_set_direction(led_gpio, GPIO_MODE_OUTPUT);
    
    while (1) {
        // Check if there's a new mode
        if (xQueueReceive(blink_mode_queue, &mode_update, 0) == pdTRUE) 
        {
            current_blink_mode = mode_update;
            ESP_LOGI(TAG, "Blink mode changed to %d", current_blink_mode);

            // Reset states when mode changes
            led_state = 0;
            fade_value = 0;
            fade_direction = 1;
            
            if (current_blink_mode == BLINK_MODE_BREATHING) 
            {
                is_breathing_mode = true;
                configure_led_pwm_for_breathing_effect();
            } 
            // Switch from breathing to regular GPIO if needed
            else if (is_breathing_mode) 
            {
                is_breathing_mode = false;
                gpio_reset_pin(led_gpio);
                gpio_set_direction(led_gpio, GPIO_MODE_OUTPUT);
            }
        }

        // Handle blinking based on current mode
        switch (current_blink_mode) {
            case BLINK_MODE_NONE:
                gpio_set_level(led_gpio, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case BLINK_MODE_ON:
                gpio_set_level(led_gpio, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            case BLINK_MODE_BASIC:
                gpio_set_level(led_gpio, led_state);
                led_state = !led_state;
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
                
            case BLINK_MODE_SLOW:
                gpio_set_level(led_gpio, led_state);
                led_state = !led_state;
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case BLINK_MODE_MEDIUM:
                gpio_set_level(led_gpio, led_state);
                led_state = !led_state;
                vTaskDelay(pdMS_TO_TICKS(300));
                break;
                
            case BLINK_MODE_FAST:
                gpio_set_level(led_gpio, led_state);
                led_state = !led_state;
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            case BLINK_MODE_BREATHING:
                // Implement breathing effect using PWM
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, fade_value);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                
                // Update fade value for next iteration
                fade_value += (fade_direction * 10);
                if (fade_value >= 1023) {
                    fade_value = 1023;
                    fade_direction = -1;
                } else if (fade_value <= 0) {
                    fade_value = 0;
                    fade_direction = 1;
                }
                
                vTaskDelay(pdMS_TO_TICKS(20)); // Shorter delay for smoother breathing
                break;
                
            default:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}

esp_err_t bitmans_blink_init(int gpio_pin)
{
    // Set GPIO pin (use default if -1 is passed)
    if (gpio_pin >= 0) 
        led_gpio = gpio_pin;
    
    ESP_LOGI(TAG, "Initializing blink system on GPIO %d", led_gpio);
    
    // Create queue for mode changes
    blink_mode_queue = xQueueCreate(5, sizeof(blink_mode_t));
    if (blink_mode_queue == NULL) 
    {
        ESP_LOGE(TAG, "Failed to create blink mode queue");
        return ESP_FAIL;
    }
    
    // Create blink task
    BaseType_t task_created = xTaskCreate(
        blink_task,
        "blink_task",
        2048,
        NULL,
        5,
        &blink_task_handle
    );
    
    if (task_created != pdPASS) 
    {
        ESP_LOGE(TAG, "Failed to create blink task");
        vQueueDelete(blink_mode_queue);
        blink_mode_queue = NULL;
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t bitmans_set_blink_mode(blink_mode_t mode)
{
    if (blink_mode_queue == NULL) 
    {
        ESP_LOGE(TAG, "Blink system not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Send new mode to the queue
    if (xQueueSend(blink_mode_queue, &mode, 0) != pdTRUE) 
    {
        ESP_LOGW(TAG, "Failed to queue blink mode change");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

blink_mode_t bitmans_get_blink_mode(void)
{
    return current_blink_mode;
}

/**
 * @brief Terminate the LED blink functionality and free resources
 * 
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t bitmans_blink_term(void)
{
    if (blink_task_handle == NULL) 
    {
        ESP_LOGW(TAG, "Blink system not initialized or already terminated");
        return ESP_ERR_INVALID_STATE;
    }

    // Set LED off before cleanup
    if (!is_breathing_mode) 
        gpio_set_level(led_gpio, 0);
    else 
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } 

    vTaskDelete(blink_task_handle);
    blink_task_handle = NULL;

    if (blink_mode_queue != NULL) 
    {
        vQueueDelete(blink_mode_queue);
        blink_mode_queue = NULL;
    }
    
    ESP_LOGI(TAG, "Blink system terminated");
    return ESP_OK;
}
