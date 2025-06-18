#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bat_lib.h"
#include "bat_config.h"
#include "bat_ble_lib.h"
#include "bat_gatts_simple.h"

static const char *TAG = "ble_bob_app";

static const char *bob_insults[] = {
    // 1234567890123456789012345]
    "Tiny Bob strikes again!",
    "Bob: fun-sized fool",
    "Bob has a small penis",
    "Bob: tiny tool, big mouth",
    "Bob's dick is microscopic",
    "Bob: small man, small dick",
    "Bob: mini member moron",
    "Bob's package is pathetic",
    "Bob: little prick",
    "Bob: size matters, you lose",
    "Bob: micro penis energy",
    "Bob: needle dick energy"};

#define NUM_BOB_INSULTS (sizeof(bob_insults) / sizeof(bob_insults[0]))

const char *pick_random_bob_insult()
{
    uint32_t random_val = esp_random();
    int idx = random_val % NUM_BOB_INSULTS;
    return bob_insults[idx];
}

void app_main(void)
{
    ESP_LOGI(TAG, "App starting");

    ESP_ERROR_CHECK(bat_lib_init());
    ESP_ERROR_CHECK(bat_ble_lib_init());
    ESP_ERROR_CHECK(bat_blink_init(-1));

    bat_gatts_server_t bleServer = {0};

    const int timeoutMs = 5000; 
    const char * pAdvName = pick_random_bob_insult();
    const char * pServiceUuid = "f0debc9a-7856-3412-1234-56785612561B"; 
    ESP_ERROR_CHECK(bat_gatts_init(&bleServer, NULL, pAdvName, 0x55, pServiceUuid, 0x0080, timeoutMs));
    ESP_ERROR_CHECK(bat_gatts_create_service(&bleServer, NULL, 0, timeoutMs));
    ESP_ERROR_CHECK(bat_gatts_start(&bleServer, NULL, timeoutMs));   

    ESP_LOGI(TAG, "App running");
    bat_set_blink_mode(BLINK_MODE_BREATHING);
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    bat_gatts_stop(&bleServer, timeoutMs);
    bat_gatts_deinit(&bleServer);
    bat_ble_lib_deinit();
    bat_lib_deinit();

    ESP_LOGI(TAG, "App exiting");
}
