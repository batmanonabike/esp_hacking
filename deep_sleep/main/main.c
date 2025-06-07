// --- ESP32 Sleep Modes ---
// Light Sleep: CPU paused, memory retained, fast wakeup, less power saving.
// Deep Sleep: Most of chip powered off, only RTC memory/peripherals retained. Wakeup causes reset.
// Use RTC_DATA_ATTR to persist variables across deep sleep cycles.
//
// ULP (Ultra Low Power) Coprocessor:
// The ESP32 has a tiny ULP coprocessor that can run while the main CPU is in deep sleep.
// - ULP can perform simple tasks (e.g., ADC sampling, GPIO monitoring) with very low power usage.
// - ULP can wake up the main CPU if a condition is met (e.g., sensor threshold, pin change).
// - Useful for battery-powered applications that need to monitor sensors or pins without waking the main CPU.
// - ULP programs are written in a special assembly or with helper macros in C.
// See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ulp.html
//
// This example demonstrates deep sleep with timer wakeup and RTC memory usage.
//
// app_main() logic:
//   1. On first boot or after deep sleep, check wakeup reason.
//   2. If woke from timer, log for 3 seconds, then go to deep sleep again.
//   3. If fresh boot or other wakeup, go to deep sleep for 5 seconds.
//   4. Loop forever: sleep, wake, log, sleep...

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/portmacro.h" // Add this for portTICK_PERIOD_MS

static const char *TAG = "deep_sleep";

// RTC memory variables: persist across deep sleep cycles
RTC_DATA_ATTR static int wake_count = 0;          // Counts wakeups
RTC_DATA_ATTR static int64_t last_sleep_time = 0; // Stores last sleep timestamp (in microseconds)

void app_main(void)
{
    // Get wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    int64_t now = esp_timer_get_time(); // microseconds since initialization of the ESP Timer.

    // Log startup time (since power-on or last reset)
    ESP_LOGI(TAG, "Startup time: %lld ms since boot", now / 1000);

    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
    {
        wake_count++;
        int64_t slept_us = 0;
        if (last_sleep_time != 0 && now > last_sleep_time)
        {
            slept_us = now - last_sleep_time;
            ESP_LOGI(TAG, "Device was asleep for: %lld ms", slept_us / 1000);
        }
        else
        {
            ESP_LOGI(TAG, "First boot or unknown sleep duration");
        }

        ESP_LOGI(TAG, "Woke from timer! Wake count: %d", wake_count);
        ESP_LOGI(TAG, "Last sleep started at: %lld us", last_sleep_time);

        // Log for 3 seconds
        int log_time = 0;
        while (log_time < 3000)
        {
            ESP_LOGI(TAG, "Awake! Logging... (%d/3)", (log_time / 1000) + 1);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            log_time += 1000;
        }

        ESP_LOGI(TAG, "Going to deep sleep for 5 seconds...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Short delay for log flush

        last_sleep_time = esp_timer_get_time(); // Save timestamp before sleep
        esp_sleep_enable_timer_wakeup(5000000); // 5 seconds in microseconds
        esp_deep_sleep_start();
    }
    else
    {
        // Called on first boot or other wakeup reasons
        ESP_LOGI(TAG, "Boot or other wakeup (reason: %d). Going to deep sleep for 5 seconds...", wakeup_reason);

        wake_count = 0;
        last_sleep_time = esp_timer_get_time(); // Save timestamp before sleep
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Short delay for log flush
        esp_sleep_enable_timer_wakeup(5000000); // 5 seconds in microseconds
        
        // esp_deep_sleep_disable_rom_logging(); // Disable ROM logging lowers power consumption
        esp_deep_sleep_start();
    }
    // Code never reaches here due to deep sleep reset
}

// --- RTC Memory Usage ---
// Use RTC_DATA_ATTR to persist variables (e.g., counters, flags) across deep sleep cycles.
// See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/deep_sleep.html#rtc-memory
//
// --- Wakeup Methods ---
// Use esp_sleep_enable_timer_wakeup(), esp_sleep_enable_ext0_wakeup(), etc., to configure wakeup sources.
// See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html

// Note: The ESP32 does not have a battery-backed real-time clock (RTC) by default.
// - It can keep track of elapsed time (using esp_timer_get_time or RTC timer) during deep sleep.
// - To know the actual date/time (wall clock), you must:
//     * Synchronize with an NTP server over WiFi (fetch time from the internet)
//     * Use an external RTC chip with battery backup (e.g., DS3231)
//     * Manually set the time after each boot or wakeup
// - After deep sleep, only elapsed time is available unless you restore the real time from NTP or an external RTC.
// - For most battery-powered applications, tracking elapsed time is sufficient, but for logging real timestamps, NTP or an external RTC is required.
