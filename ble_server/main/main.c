#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bat_lib.h"
#include "bat_config.h"

static const char *TAG = "ble_server_app";

typedef enum app_bits
{
    ERROR_BIT = BIT0,
    GATTS_STOP_BIT = BIT1,
    GATTS_READY_TO_START_BIT = BIT2,
    GAPS_STOP_ADVERTISING_BIT = BIT3,
    GAPS_START_ADVERTISING_BIT = BIT4,
} app_bits;

typedef struct app_context
{
    const char *pszAdvName;
    const char *pszAdvNameBase;
    EventGroupHandle_t ble_events;
    bat_ble_uuid128_t char_uuid;
    bat_ble_uuid128_t service_uuid;

} app_context;

void app_context_init(app_context *pContext)
{
    pContext->pszAdvName = NULL;
    pContext->ble_events = xEventGroupCreate();
    pContext->pszAdvNameBase = bat_get_advertname();

    ESP_ERROR_CHECK(bat_ble_string36_to_uuid128(bat_get_char_id(), &pContext->char_uuid));
    ESP_ERROR_CHECK(bat_ble_string36_to_uuid128(bat_get_server_id(), &pContext->service_uuid));
}

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
static void app_on_gatts_reg(bat_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    esp_err_t err = bat_gatts_create_service128(pCb->gatts_if, &pAppContext->service_uuid);
    try_handle_error(pAppContext, err, "app_on_gatts_reg");
}

static void app_on_gatts_create(bat_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // One or more characteristics can be created here.
    app_context *pAppContext = (app_context *)pCb->pContext;
    esp_err_t err = bat_gatts_create_char128(
        pCb->gatts_if, pCb->service_handle, &pAppContext->char_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
    try_handle_error(pAppContext, err, "app_on_gatts_create");
}

static void app_on_gatts_add_char(bat_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    // esp_err_t err = bat_gatts_start_service(pCb->service_handle);
    // try_handle_error(pAppContext, err, "app_on_gatts_add_char");
    xEventGroupSetBits(pAppContext->ble_events, GATTS_READY_TO_START_BIT);
}

static void app_on_gatts_start(bat_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;

    esp_err_t err = bat_gatts_begin_advert_data_set128(pAppContext->pszAdvName, &pAppContext->service_uuid);
    try_handle_error(pAppContext, err, "app_on_gatts_start");
    return;
}

static void app_on_gatts_stop(bat_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    xEventGroupSetBits(pAppContext->ble_events, GATTS_STOP_BIT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gap callbacks
static void on_gaps_advert_data_set(bat_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Advertising data set, starting advertising");

    esp_err_t err = bat_gatts_start_advertising();
    app_context *pAppContext = (app_context *)pCb->pContext;
    try_handle_error(pAppContext, err, "on_gaps_advert_data_set");
}

static void on_gaps_advert_start(bat_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    if (pParam->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "Advertising started successfully");
        xEventGroupSetBits(pAppContext->ble_events, GAPS_START_ADVERTISING_BIT);
    }
    else
    {
        ESP_LOGE(TAG, "Advertising start failed, status=0x%x", pParam->adv_start_cmpl.status);
        try_handle_error(pAppContext, ESP_FAIL, "on_gaps_advert_start");
    }
}

static void on_gaps_advert_stop(bat_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    if (pParam->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "Advertising stopped successfully");
        xEventGroupSetBits(pAppContext->ble_events, GAPS_STOP_ADVERTISING_BIT);
    }
    else
    {
        ESP_LOGE(TAG, "Advertising stop failed, status=0x%x", pParam->adv_stop_cmpl.status);
        try_handle_error(pAppContext, ESP_FAIL, "on_gaps_advert_stop");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool handle_flags_error(esp_err_t err, EventBits_t flags, int stage)
{
    if (err == ESP_OK && ((flags & ERROR_BIT) == 0) && flags != 0)
        return false;

    if (err != ESP_OK)
        ESP_LOGE(TAG, "Error: %s, stage: %d", esp_err_to_name(err), stage);
    else
        ESP_LOGE(TAG, "Error: Flags=0x%08lx, stage: %d", (unsigned long)flags, stage);

    return true;
}

esp_err_t run_gatts_service(bat_gatts_callbacks_t *pGattsCallbacks, const char * pszAdvName, uint32_t runtimeMs)
{
    EventBits_t flags = 0;
    app_context *pAppContext = (app_context *)pGattsCallbacks->pContext;
    pAppContext->pszAdvName = pszAdvName;
    xEventGroupClearBits(pAppContext->ble_events, 0x00FFFFFF); // Top byte is reserved.

    bat_set_blink_mode(BLINK_MODE_BASIC);
    esp_err_t err = bat_gatts_start_service(pGattsCallbacks->service_handle);
    if (handle_flags_error(err, ~ERROR_BIT, 100))
        return ESP_FAIL;

    ESP_LOGI(TAG, "Wait for start advertising event");
    err = bat_waitbits_forever(pAppContext->ble_events, GAPS_START_ADVERTISING_BIT | ERROR_BIT, &flags);
    if (handle_flags_error(err, flags, 200))
        return ESP_FAIL;

    bat_set_blink_mode(BLINK_MODE_BREATHING);
    ESP_LOGI(TAG, "Running");
    vTaskDelay(runtimeMs / portTICK_PERIOD_MS);

    bat_set_blink_mode(BLINK_MODE_SLOW);
    err = bat_gatts_stop_advertising();
    if (handle_flags_error(err, ~ERROR_BIT, 300))
        return ESP_FAIL;

    ESP_LOGI(TAG, "Wait for stop advertising event");
    err = bat_waitbits_forever(pAppContext->ble_events, GAPS_STOP_ADVERTISING_BIT | ERROR_BIT, &flags);
    if (handle_flags_error(err, flags, 400))
        return ESP_FAIL;

    err = bat_gatts_stop_service(pGattsCallbacks->service_handle);
    if (handle_flags_error(err, ~ERROR_BIT, 500))
        return ESP_FAIL;

    ESP_LOGI(TAG, "Wait for stop service event");
    err = bat_waitbits_forever(pAppContext->ble_events, GATTS_STOP_BIT | ERROR_BIT, &flags);
    if (handle_flags_error(err, flags, 600))
        return ESP_FAIL;

    // We shouldn't need this delay, but there might be some race condition.  From the log...
    // E (1965) BT_BTC: bta_to_btc_uuid UUID len is invalid 56080
    // vTaskDelay(2000 / portTICK_PERIOD_MS);

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "App starting");

    bat_gatts_callbacks_t gatts_callbacks = {
        .on_reg = app_on_gatts_reg,
        .on_stop = app_on_gatts_stop,
        .on_start = app_on_gatts_start,
        .on_create = app_on_gatts_create,
        .on_add_char = app_on_gatts_add_char,
    };

    bat_gaps_callbacks_t gaps_callbacks = {
        .on_advert_stop = on_gaps_advert_stop,
        .on_advert_start = on_gaps_advert_start,
        .on_advert_data_set = on_gaps_advert_data_set,
    };

    bat_lib_t bat_lib;
    ESP_ERROR_CHECK(bat_lib_init(&bat_lib));
    ESP_ERROR_CHECK(bat_blink_init(-1));
    ESP_ERROR_CHECK(bat_ble_server_init());

    app_context appContext;
    app_context_init(&appContext);
    bat_ble_gaps_callbacks_init(&gaps_callbacks, &appContext);
    bat_ble_gatts_callbacks_init(&gatts_callbacks, &appContext);

#define BAT_APP_ID 0x55
    EventBits_t flags = 0;
    ESP_ERROR_CHECK(bat_gatts_register(BAT_APP_ID, &gatts_callbacks, &appContext));
    esp_err_t err = bat_waitbits_forever(appContext.ble_events, GATTS_READY_TO_START_BIT | ERROR_BIT, &flags);
    if (!handle_flags_error(err, flags, 50))
    {
        char szAdvName[64];
        for (int n = 0;; ++n)
        {
            snprintf(szAdvName, sizeof(szAdvName), "%s_%d", appContext.pszAdvNameBase, n);
            err = run_gatts_service(&gatts_callbacks, szAdvName, 1000 * 60 * 60);
            if (err != ESP_OK)
                break;
        }
    }

    ESP_LOGI(TAG, "Exiting soon");
    bat_set_blink_mode(BLINK_MODE_VERY_FAST);
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "App restarting");
    esp_restart();
}
