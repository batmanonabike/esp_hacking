#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bitmans_lib.h"
#include "bitmans_config.h"
#include "bitmans_ble_server.h"

static const char *TAG = "ble_server_app";

typedef enum app_bits
{
    GATTS_START_BIT = BIT0,
    GATTS_REGISTER_BIT = BIT1,
    GATTS_UNREGISTER_BIT = BIT2,
} app_bits;

typedef struct app_context
{
    const char *pszAdvName;
    EventGroupHandle_t ble_events;
    bitmans_ble_uuid128_t char_uuid;
    bitmans_ble_uuid128_t service_uuid;

} app_context;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gatts callbacks
static void app_on_gatts_reg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    bitmans_gatts_create_service128(pCb->gatts_if, &pAppContext->service_uuid);
    xEventGroupSetBits(pAppContext->ble_events, GATTS_REGISTER_BIT);
}

static void app_on_gatts_create(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // One or more characteristics can be created here.
    app_context *pAppContext = (app_context *)pCb->pContext;
    bitmans_gatts_create_char128(
        pCb->gatts_if, pCb->service_handle, &pAppContext->char_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
}

static void app_on_gatts_add_char(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_start_service(pCb->service_handle);
}

static void app_on_gatts_start(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    bitmans_gatts_begin_advert_data_set128(pAppContext->pszAdvName, &pAppContext->service_uuid);
    xEventGroupSetBits(pAppContext->ble_events, GATTS_START_BIT);
}

static void app_on_gatts_unreg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    xEventGroupSetBits(pAppContext->ble_events, GATTS_UNREGISTER_BIT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gap callbacks
static void on_gaps_advert_data_set(bitmans_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Advertising data set, starting advertising");
    bitmans_gatts_start_advertising();
}

static void on_gaps_advert_start(bitmans_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    if (pParam->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "Advertising started successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Advertising start failed, status=0x%x", pParam->adv_start_cmpl.status);
    }
}

static void on_gaps_advert_stop(bitmans_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    if (pParam->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "Advertising stopped successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Advertising stop failed, status=0x%x", pParam->adv_stop_cmpl.status);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void app_context_term(app_context *pContext)
{
    pContext->pszAdvName = NULL;
    if (pContext->ble_events != NULL)
    {
        vEventGroupDelete(pContext->ble_events);
        pContext->ble_events = NULL;
    }

    memset(&pContext->char_uuid, 0, sizeof(pContext->char_uuid));
    memset(&pContext->service_uuid, 0, sizeof(pContext->service_uuid));
}

void app_context_init(app_context *pContext)
{
    pContext->ble_events = xEventGroupCreate();
    pContext->pszAdvName = bitmans_get_advertname();

    ESP_ERROR_CHECK(bitmans_ble_string36_to_uuid128(bitmans_get_char_id(), &pContext->char_uuid));
    ESP_ERROR_CHECK(bitmans_ble_string36_to_uuid128(bitmans_get_server_id(), &pContext->service_uuid));
}

void app_main(void)
{
#define BITMANS_APP_ID 0x55

    bitmans_gatts_callbacks_t gatts_callbacks = {
        .on_reg = app_on_gatts_reg,
        .on_unreg = app_on_gatts_unreg,
        .on_start = app_on_gatts_start,
        .on_create = app_on_gatts_create,
        .on_add_char = app_on_gatts_add_char,
    };

    bitmans_gaps_callbacks_t gaps_callbacks = {
        .on_advert_stop = on_gaps_advert_stop,
        .on_advert_start = on_gaps_advert_start,
        .on_advert_data_set = on_gaps_advert_data_set,
    };

    ESP_LOGI(TAG, "App starting");

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());

    app_context appContext;
    app_context_init(&appContext);
    bitmans_ble_gaps_callbacks_init(&gaps_callbacks, &appContext);
    bitmans_ble_gatts_callbacks_init(&gatts_callbacks, &appContext);

    while (true)
    {
        esp_err_t ret = bitmans_ble_gatts_register(BITMANS_APP_ID, &gatts_callbacks, &appContext);
        ESP_LOGI(TAG, "bitmans_ble_gatts_register: %s", esp_err_to_name(ret));

        // ESP_LOGI(TAG, "Wait for register event");
        // ESP_ERROR_CHECK(bitmans_waitbits_forever(appContext.ble_events, GATTS_REGISTER_BIT, NULL));
        // ESP_ERROR_CHECK_RESTART(bitmans_waitbits_forever(appContext.ble_events, GATTS_REGISTER_BIT, NULL));

        ESP_LOGI(TAG, "Wait for start event");
        ESP_ERROR_CHECK(bitmans_waitbits_forever(appContext.ble_events, GATTS_START_BIT, NULL));

        // ESP_LOGI(TAG, "WAITING");
        // vTaskDelay(10000 / portTICK_PERIOD_MS);

        ret = bitmans_ble_gatts_unregister(BITMANS_APP_ID);
        ESP_LOGI(TAG, "bitmans_ble_gatts_unregister: %s", esp_err_to_name(ret));

        // ESP_GATTS_UNREG_EVT
        // vTaskDelay(5000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Wait for unregister event");
        ESP_ERROR_CHECK(bitmans_waitbits_forever(appContext.ble_events, GATTS_UNREGISTER_BIT, NULL));
        // ESP_ERROR_CHECK_RESTART(bitmans_waitbits_forever(appContext.ble_events, GATTS_UNREGISTER_BIT, NULL));

        // We shouldn't need this delay, but there might be some race condition.  From the log...
        // E (1965) BT_BTC: bta_to_btc_uuid UUID len is invalid 56080
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    vTaskDelay(60000 / portTICK_PERIOD_MS);

    // ESP_LOGI(TAG, "Stop advertising");
    // bitmans_gatts_stop_advertising();
    // vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Term BLE");
    bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    bitmans_ble_server_term();

    app_context_term(&appContext);
    ESP_LOGI(TAG, "App terminated");

    esp_restart();
}
