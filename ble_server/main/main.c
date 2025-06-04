#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bitmans_lib.h"
#include "bitmans_ble_server.h"

static const char *TAG = "ble_server_app";

typedef struct bitmans_gatts_context
{
    int UNREGISTER_BIT; // Event bit for unregistering
    const char *pszAdvName;   // Advertised name for the BLE device

    EventGroupHandle_t ble_events;
    bitmans_ble_uuid_t char_uuid;
    bitmans_ble_uuid_t service_uuid;

} bitmans_gatts_context;

static void bitman_gatts_on_reg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_create_service(pCb->gatts_if, &pAppContext->service_uuid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create service: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_create(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // One or more characteristics can be created here.
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_create_characteristic(
        pCb->gatts_if, pCb->service_handle, &pAppContext->service_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create characteristic: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_add_char(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = esp_ble_gatts_start_service(pCb->service_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start service: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_start(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_advertise(pAppContext->pszAdvName, &pAppContext->service_uuid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start advertising: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_unreg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    xEventGroupSetBits(pAppContext->ble_events, pAppContext->UNREGISTER_BIT);
}

void bitmans_gatts_context_init(bitmans_gatts_context *pContext,
    const char *pszAdvName, const char *pszServerUUID, const char *pszCharUUID)
{
    pContext->UNREGISTER_BIT = BIT0;
    pContext->pszAdvName = pszAdvName;
    pContext->ble_events = xEventGroupCreate();
    bitmans_ble_string_to_uuid(pszCharUUID, &pContext->char_uuid);
    bitmans_ble_string_to_uuid(pszServerUUID, &pContext->service_uuid);
}

void bitmans_gatts_context_term(bitmans_gatts_context *pContext)
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

void app_main(void)
{
    bitmans_gatts_context appContext;
    bitmans_gatts_callbacks_t appCallbacks = {
        .on_reg = bitman_gatts_on_reg,
        .on_unreg = bitman_gatts_on_unreg,
        .on_start = bitman_gatts_on_start,
        .on_create = bitman_gatts_on_create,
        .on_add_char = bitman_gatts_on_add_char,
    };

    const char *pszAdvName = "BitmansGATTS";;
    const char *pszCharUUID = "f0debc9a-3412-7856-1234-56785601efcd";
    const char *pszServerUUID = "f0debc9a-7856-3412-1234-567856125612";

    ESP_LOGI(TAG, "Starting BLE server application");

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());
    bitmans_ble_gatts_callbacks_init(&appCallbacks, &appContext);
    bitmans_gatts_context_init(&appContext, pszAdvName, pszServerUUID, pszCharUUID);

#define BITMANS_APP_ID 0x55
    ESP_LOGI(TAG, "Register BLE server (1)");
    ESP_ERROR_CHECK(bitmans_ble_gatts_register(BITMANS_APP_ID, &appCallbacks, &appContext));

    // ESP_LOGI(TAG, "BLE server running");
    // for (int counter = 10; counter > 0; counter--)
    // {
    //     ESP_LOGI(TAG, "Running BLE server: %d", counter);
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    ESP_LOGI(TAG, "BLE server running");
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "BLE stop advertising");
    esp_ble_gap_stop_advertising();
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Unregister BLE (1)");
    bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    vTaskDelay(30000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Wait for unregister event (1)");
    xEventGroupWaitBits(appContext.ble_events, appContext.UNREGISTER_BIT,
        pdTRUE,         /* Clear the bits before returning */
        pdFALSE,        /* Don't wait for both bits, either bit will do */
        portMAX_DELAY); /* Wait forever */

    ESP_LOGI(TAG, "Register BLE server (2)");
    bitmans_ble_gatts_register(BITMANS_APP_ID, &appCallbacks, &appContext);
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Unregister BLE (2)");
    bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    // vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Wait for unregister event (2)");
    xEventGroupWaitBits(appContext.ble_events, appContext.UNREGISTER_BIT,
        pdTRUE,         /* Clear the bits before returning */
        pdFALSE,        /* Don't wait for both bits, either bit will do */
        portMAX_DELAY); /* Wait forever */

    ESP_LOGI(TAG, "Uninitialising BLE server (2)");
    bitmans_ble_server_term();

    bitmans_gatts_context_term(&appContext);
    ESP_LOGI(TAG, "BLE server application terminated");
}
