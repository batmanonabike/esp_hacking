#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bat_lib.h"

static const char *TAG = "ble_bob_app";

typedef struct app_context
{
    const char *pszAdvName;
} app_context;

static esp_err_t app_create_service(esp_gatt_if_t gatts_if)
{
    esp_gatt_id_t id = {
        .inst_id = 0,
        .uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid = {.uuid16 = 0x1800} // Generic Access, or any 16-bit value
        }};
    esp_gatt_srvc_id_t service_id = {.id = id, .is_primary = true};

    esp_err_t err = esp_ble_gatts_create_service(gatts_if, &service_id, 4);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create GATTS service: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gatts callbacks
static void app_on_gatts_reg(bat_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_create_service(pCb->gatts_if);
}

static void app_on_gatts_create(bat_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bat_gatts_start_service(pCb->service_handle);
}

static void app_on_gatts_start(bat_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    bat_gatts_begin_advert_data_set(pAppContext->pszAdvName, NULL, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gap callbacks
static void on_gaps_advert_data_set(bat_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Advertising data set, starting advertising");
    bat_gatts_start_advertising();
}

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

    bat_lib_t bat_lib;
    ESP_ERROR_CHECK(bat_lib_init(&bat_lib));
    ESP_ERROR_CHECK(bat_blink_init(-1));
    ESP_ERROR_CHECK(bat_ble_server_init());

    bat_gatts_callbacks_t gatts_callbacks = {
        .on_reg = app_on_gatts_reg,
        .on_start = app_on_gatts_start,
        .on_create = app_on_gatts_create,
    };
    
    app_context appContext = { .pszAdvName = pick_random_bob_insult() };
    bat_gaps_callbacks_t gaps_callbacks = { .on_advert_data_set = on_gaps_advert_data_set};

    bat_ble_gaps_callbacks_init(&gaps_callbacks, &appContext);
    bat_ble_gatts_callbacks_init(&gatts_callbacks, &appContext);

#define BAT_APP_ID 0x57
    ESP_LOGI(TAG, "Register Gatts");
    ESP_ERROR_CHECK(bat_gatts_register(BAT_APP_ID, &gatts_callbacks, &appContext));

    bat_set_blink_mode(BLINK_MODE_BREATHING);
    ESP_LOGI(TAG, "App running");
    while (1)
        vTaskDelay(1000 / portTICK_PERIOD_MS);
}
