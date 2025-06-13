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

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/portmacro.h" 

#include "bat_lib.h"

static const char *TAG = "deep_sleep";

// --- RTC Memory Usage ---
// Use RTC_DATA_ATTR to persist variables (e.g., counters, flags) across deep sleep cycles.
// See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/deep_sleep.html#rtc-memory
RTC_DATA_ATTR static int rtc_wake_count = 0;             // Counts wakeups

void app_first_boot(esp_sleep_wakeup_cause_t wakeup_reason)
{
    bat_set_blink_mode(BLINK_MODE_BASIC);
    ESP_LOGI(TAG, "Boot or other wakeup (reason: %d). Going to deep sleep for 5 seconds...", wakeup_reason);

    rtc_wake_count = 0;
    vTaskDelay(5000 / portTICK_PERIOD_MS);
}

void app_wake_from_timer()
{
    bat_set_blink_mode(BLINK_MODE_BREATHING);

    rtc_wake_count++;
    ESP_LOGI(TAG, "Woke from timer! Wake count: %d", rtc_wake_count);

    const int max = 5;
    for (int n = 0; n < max; ++n)
    {
        ESP_LOGI(TAG, "Awake! Logging... (%d/%d)", n + 1, max);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    int64_t now = esp_timer_get_time(); // microseconds since initialization of the ESP Timer.
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    bat_lib_t bat_lib;
    ESP_ERROR_CHECK(bat_lib_init(&bat_lib));
    ESP_ERROR_CHECK(bat_blink_init(-1));

    // Log startup time (since power-on or last reset)
    ESP_LOGI(TAG, "Startup time: %lld ms since boot", now / 1000);

    if (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER)
        app_first_boot(wakeup_reason);
    else
        app_wake_from_timer();

    ESP_LOGI(TAG, "Going to deep sleep for 5 seconds...");
    bat_set_blink_mode(BLINK_MODE_VERY_FAST);
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Ensure logs are flushed before deep sleep
    esp_sleep_enable_timer_wakeup(5000000); // 5 seconds in microseconds
    esp_deep_sleep_disable_rom_logging();   // Disable ROM logging lowers power consumption
    esp_deep_sleep_start();    
	
	// Code never reaches here due to deep sleep reset
    /* bat_blink_deinit(); */
}
