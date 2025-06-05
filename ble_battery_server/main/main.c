#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bitmans_lib.h"
#include "bitmans_ble_server.h"

static const char *TAG = "ble_battery_server_app";

typedef struct bitmans_gatts_context
{
    const char *pszAdvName;
    bitmans_ble_uuid128_t char_uuid;
    bitmans_ble_uuid128_t service_uuid;

} bitmans_gatts_context;

static void bitman_gatts_on_reg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_create_service128(pCb->gatts_if, &pAppContext->service_uuid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create service: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_create(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // Here we create the characteristic for the battery level.
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_create_char128(
        pCb->gatts_if, pCb->service_handle, &pAppContext->char_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);

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
    esp_err_t err = bitmans_gatts_advertise128(pAppContext->pszAdvName, &pAppContext->service_uuid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start advertising: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_read(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // if (pParam->read.handle == battery_level_handle) {
    //     uint8_t battery_level = 42; // Simulated value
    //     esp_ble_gatts_send_response(
    //         pCb->gatts_if, pParam->read.conn_id, pParam->read.trans_id,
    //         ESP_GATT_OK, &battery_level_rsp);
    // }
}

static void bitman_gatts_on_unreg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
}

void bitmans_gatts_context_term(bitmans_gatts_context *pContext)
{
    pContext->pszAdvName = NULL;
    memset(&pContext->char_uuid, 0, sizeof(pContext->char_uuid));
    memset(&pContext->service_uuid, 0, sizeof(pContext->service_uuid));
}

void bitmans_gatts_context_init(bitmans_gatts_context *pContext, const char *pszAdvName)
{
    pContext->pszAdvName = pszAdvName;

    // https://www.bluetooth.com/specifications/gatt/characteristics/
    const char * pszBatteryLevel = "2A19";
    const char * pszBatteryService = "180F";
    ESP_ERROR_CHECK(bitmans_ble_string4_to_uuid128(pszBatteryLevel, &pContext->char_uuid));
    ESP_ERROR_CHECK(bitmans_ble_string4_to_uuid128(pszBatteryService, &pContext->service_uuid));
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

    const char *pszAdvName = "Bitmans Battery";;
    ESP_LOGI(TAG, "Starting BLE server application");

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());
    bitmans_ble_gatts_callbacks_init(&appCallbacks, &appContext);
    bitmans_gatts_context_init(&appContext, pszAdvName);

#define BITMANS_APP_ID 0x56
    ESP_LOGI(TAG, "Register BLE server");
    ESP_ERROR_CHECK(bitmans_ble_gatts_register(BITMANS_APP_ID, &appCallbacks, &appContext));

    ESP_LOGI(TAG, "BLE server running");
    for (int counter = 180; counter > 0; counter--)
    {
        ESP_LOGI(TAG, "Running BLE server: %d", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "BLE server running");
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "BLE stop advertising");
    esp_ble_gap_stop_advertising();
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Unregister BLE");
    bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    vTaskDelay(30000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Uninitialising BLE server");
    bitmans_ble_server_term();

    bitmans_gatts_context_term(&appContext);
    ESP_LOGI(TAG, "BLE server application terminated");
}
