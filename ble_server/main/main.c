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
    GATTS_REGISTER_BIT = BIT0,   // Register event
    GATTS_UNREGISTER_BIT = BIT1, // Unregister event
} app_bits;

typedef struct app_gatts_context
{
    const char *pszAdvName; // Advertised name for the BLE device
    EventGroupHandle_t ble_events;
    bitmans_ble_uuid128_t char_uuid;
    bitmans_ble_uuid128_t service_uuid;

} app_gatts_context;

static void app_on_reg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_gatts_context *pAppContext = (app_gatts_context *)pCb->pContext;
    bitmans_gatts_create_service128(pCb->gatts_if, &pAppContext->service_uuid);
    xEventGroupSetBits(pAppContext->ble_events, GATTS_REGISTER_BIT);
}

static void app_on_create(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // One or more characteristics can be created here.
    app_gatts_context *pAppContext = (app_gatts_context *)pCb->pContext;
    bitmans_gatts_create_char128(
        pCb->gatts_if, pCb->service_handle, &pAppContext->char_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
}

static void app_on_add_char(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_start_service(pCb->service_handle);
}

static void app_on_start(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_gatts_context *pAppContext = (app_gatts_context *)pCb->pContext;
    bitmans_gatts_begin_advertise128(pAppContext->pszAdvName, &pAppContext->service_uuid);
}

static void app_on_unreg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_gatts_context *pAppContext = (app_gatts_context *)pCb->pContext;
    xEventGroupSetBits(pAppContext->ble_events, GATTS_UNREGISTER_BIT);
}

void app_context_term(app_gatts_context *pContext)
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

void app_context_init(app_gatts_context *pContext)
{
    pContext->ble_events = xEventGroupCreate();
    pContext->pszAdvName = bitmans_get_advertname();

    ESP_ERROR_CHECK(bitmans_ble_string36_to_uuid128(bitmans_get_char_id(), &pContext->char_uuid));
    ESP_ERROR_CHECK(bitmans_ble_string36_to_uuid128(bitmans_get_server_id(), &pContext->service_uuid));
}

////////////////////////////////////////////////////////////////////////////////////////
// TODO: Come back to this example.  The issue is that the BLE server does not restart
// advertising after the first time it is unregistered.
////////////////////////////////////////////////////////////////////////////////////////
void app_main(void)
{
#define BITMANS_APP_ID 0x55

    app_gatts_context appContext;
    bitmans_gatts_callbacks_t callbacks = {
        .on_reg = app_on_reg,
        .on_unreg = app_on_unreg,
        .on_start = app_on_start,
        .on_create = app_on_create,
        .on_add_char = app_on_add_char,
    };

    ESP_LOGI(TAG, "App starting");

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());
    app_context_init(&appContext);
    bitmans_ble_gatts_callbacks_init(&callbacks, &appContext);

    while (true)
    {
        esp_err_t ret = bitmans_ble_gatts_register(BITMANS_APP_ID, &callbacks, &appContext);
        ESP_LOGI(TAG, "bitmans_ble_gatts_register: %s", esp_err_to_name(ret));

        ESP_LOGI(TAG, "Wait for register event");
        ESP_ERROR_CHECK(bitmans_waitbits_forever(appContext.ble_events, GATTS_REGISTER_BIT, NULL));
        //ESP_ERROR_CHECK_RESTART(bitmans_waitbits_forever(appContext.ble_events, GATTS_REGISTER_BIT, NULL));

        // ESP_LOGI(TAG, "WAITING");
        // vTaskDelay(10000 / portTICK_PERIOD_MS);

        ret = bitmans_ble_gatts_unregister(BITMANS_APP_ID);
        ESP_LOGI(TAG, "bitmans_ble_gatts_unregister: %s", esp_err_to_name(ret));

        // ESP_GATTS_UNREG_EVT
        // vTaskDelay(5000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Wait for unregister event");
        ESP_ERROR_CHECK(bitmans_waitbits_forever(appContext.ble_events, GATTS_UNREGISTER_BIT, NULL));
        //ESP_ERROR_CHECK_RESTART(bitmans_waitbits_forever(appContext.ble_events, GATTS_UNREGISTER_BIT, NULL));

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    vTaskDelay(60000 / portTICK_PERIOD_MS);
    // // esp_restart();

    // ESP_LOGI(TAG, "Stop advertising");
    // bitmans_gatts_stop_advertising();
    // vTaskDelay(10000 / portTICK_PERIOD_MS);

    // ESP_LOGI(TAG, "Unregister Gatts (1)");
    // bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    // vTaskDelay(30000 / portTICK_PERIOD_MS);

    // ESP_LOGI(TAG, "Wait for unregister event (1)");
    // xEventGroupWaitBits(appContext.ble_events, appContext.UNREGISTER_BIT,
    //     pdTRUE,         /* Clear the bits before returning */
    //     pdFALSE,        /* Don't wait for both bits, either bit will do */
    //     portMAX_DELAY); /* Wait forever */

    // ESP_LOGI(TAG, "Register Gatts (2)");
    // bitmans_ble_gatts_register(BITMANS_APP_ID, &callbacks, &appContext);
    // vTaskDelay(10000 / portTICK_PERIOD_MS);

    // ESP_LOGI(TAG, "Unregister Gatts (2)");
    // bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    // // vTaskDelay(10000 / portTICK_PERIOD_MS);

    // ESP_LOGI(TAG, "Wait for unregister event (2)");
    // xEventGroupWaitBits(appContext.ble_events, appContext.UNREGISTER_BIT,
    //     pdTRUE,         /* Clear the bits before returning */
    //     pdFALSE,        /* Don't wait for both bits, either bit will do */
    //     portMAX_DELAY); /* Wait forever */

    ESP_LOGI(TAG, "Term BLE");
    bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    bitmans_ble_server_term();

    app_context_term(&appContext);
    ESP_LOGI(TAG, "App terminated");
}
