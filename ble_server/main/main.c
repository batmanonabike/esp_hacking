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
    ERROR_BIT = BIT0,
    GATTS_REGISTER_BIT = BIT1,
    GATTS_START_BIT = BIT2,
    GATTS_UNREGISTER_BIT = BIT3,
    GAPS_STOPADVERTISING_BIT = BIT4,
    GAPS_STARTADVERTISING_BIT = BIT5,
} app_bits;

typedef struct app_context
{
    const char *pszAdvName;
    EventGroupHandle_t ble_events;
    bitmans_ble_uuid128_t char_uuid;
    bitmans_ble_uuid128_t service_uuid;

} app_context;

bool try_handle_error(app_context *pAppContext, esp_err_t err, const char *pszMethod)
{
    if (err == ESP_OK)
        return false;

    ESP_LOGE(TAG, "%s FAILED: %s", pszMethod, esp_err_to_name(err));
    xEventGroupSetBits(pAppContext->ble_events, ERROR_BIT);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gatts callbacks
static void app_on_gatts_reg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_create_service128(pCb->gatts_if, &pAppContext->service_uuid);
    if (try_handle_error(pAppContext, err, "app_on_gatts_reg"))
        return;

    xEventGroupSetBits(pAppContext->ble_events, GATTS_REGISTER_BIT);
}

static void app_on_gatts_create(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // One or more characteristics can be created here.
    app_context *pAppContext = (app_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_create_char128(
        pCb->gatts_if, pCb->service_handle, &pAppContext->char_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
    try_handle_error(pAppContext, err, "app_on_gatts_create");
}

static void app_on_gatts_add_char(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_start_service(pCb->service_handle);
    try_handle_error(pAppContext, err, "app_on_gatts_add_char");
}

static void app_on_gatts_start(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_begin_advert_data_set128(pAppContext->pszAdvName, &pAppContext->service_uuid);
    if (try_handle_error(pAppContext, err, "app_on_gatts_start"))
        return;

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

    esp_err_t err = bitmans_gatts_start_advertising();
    app_context *pAppContext = (app_context *)pCb->pContext;
    try_handle_error(pAppContext, err, "on_gaps_advert_data_set");
}

static void on_gaps_advert_start(bitmans_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    if (pParam->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "Advertising started successfully");
        xEventGroupSetBits(pAppContext->ble_events, GAPS_STARTADVERTISING_BIT);
    }
    else
    {
        ESP_LOGE(TAG, "Advertising start failed, status=0x%x", pParam->adv_start_cmpl.status);
        try_handle_error(pAppContext, ESP_FAIL, "on_gaps_advert_start");
    }
}

static void on_gaps_advert_stop(bitmans_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    if (pParam->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "Advertising stopped successfully");
        xEventGroupSetBits(pAppContext->ble_events, GAPS_STOPADVERTISING_BIT);
    }
    else
    {
        ESP_LOGE(TAG, "Advertising stop failed, status=0x%x", pParam->adv_stop_cmpl.status);
        try_handle_error(pAppContext, ESP_FAIL, "on_gaps_advert_stop");
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

bool handle_flags_error(esp_err_t err, EventBits_t flags)
{
    if (err == ESP_OK && ((flags & ERROR_BIT) == 0))
        return false;

    ESP_LOGE(TAG, "Error: %s", esp_err_to_name(err));
    return true;
}

void app_main(void)
{
    ESP_LOGI(TAG, "App starting");

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

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());

    app_context appContext;
    app_context_init(&appContext);
    bitmans_ble_gaps_callbacks_init(&gaps_callbacks, &appContext);
    bitmans_ble_gatts_callbacks_init(&gatts_callbacks, &appContext);

    EventBits_t flags = 0;
    esp_err_t err = ESP_OK;

    while (true)
    {
#define BITMANS_APP_ID 0x55
        err = bitmans_ble_gatts_register(BITMANS_APP_ID, &gatts_callbacks, &appContext);
        if (handle_flags_error(err, flags))
            break;

        ESP_LOGI(TAG, "Wait for start advertising event");
        err = bitmans_waitbits_forever(appContext.ble_events, GAPS_STARTADVERTISING_BIT | ERROR_BIT, &flags);
        if (handle_flags_error(err, flags))
            break;

        ESP_LOGI(TAG, "Running");
        vTaskDelay(10000 / portTICK_PERIOD_MS);

        err = bitmans_gatts_stop_advertising();
        if (handle_flags_error(err, flags))
            break;

        ESP_LOGI(TAG, "Wait for stop advertising event");
        err = bitmans_waitbits_forever(appContext.ble_events, GAPS_STOPADVERTISING_BIT | ERROR_BIT, &flags);
        if (handle_flags_error(err, flags))
            break;

        err = bitmans_ble_gatts_unregister(BITMANS_APP_ID);
        if (handle_flags_error(err, flags))
            break;

        ESP_LOGI(TAG, "Wait for unregister event");
        err = bitmans_waitbits_forever(appContext.ble_events, GATTS_UNREGISTER_BIT | ERROR_BIT, &flags);
        if (handle_flags_error(err, flags))
            break;

        // We shouldn't need this delay, but there might be some race condition.  From the log...
        // E (1965) BT_BTC: bta_to_btc_uuid UUID len is invalid 56080
        vTaskDelay(70000 / portTICK_PERIOD_MS);
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);

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
