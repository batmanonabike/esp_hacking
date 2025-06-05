#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bitmans_lib.h"

static const char *TAG = "ble_battery_server_app";

typedef struct bitmans_gatts_context
{
    const char *pszAdvName;
    uint16_t battery_level_handle;
    bitmans_ble_uuid128_t battery_level_uuid;
    bitmans_ble_uuid128_t battery_service_uuid;

} bitmans_gatts_context;

static void bitman_gatts_on_reg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_create_service128(pCb->gatts_if, &pAppContext->battery_service_uuid);
}

static void bitman_gatts_on_create(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // Here we create the characteristic for the battery level.
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_create_char128(
        pCb->gatts_if, pCb->service_handle, &pAppContext->battery_level_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, //TODO: handle notifications
        ESP_GATT_PERM_READ);
}

static void bitman_gatts_on_add_char(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    if (bitmans_ble_uuid_try_match(&pParam->add_char.char_uuid, &pAppContext->battery_level_uuid))
    {
        pAppContext->battery_level_handle = pParam->add_char.attr_handle;
        ESP_LOGI(TAG, "Battery level characteristic added with handle: %d", pAppContext->battery_level_handle);
    }

    esp_err_t err = bitmans_gatts_start_service(pCb->service_handle);
}

static void bitman_gatts_on_start(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_begin_advertise128(pAppContext->pszAdvName, &pAppContext->battery_service_uuid);
}

static void bitman_gatts_on_connect(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    bitmans_gatts_stop_advertising();

    // You might want to cache the connection ID for later use for notifications,
    // to send responses to the client, to manage the connection state,
    // or to manage the connection state.
}

static void bitman_gatts_on_read(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;

    // This is a read request for the battery level characteristic
    if (pParam->read.handle == pAppContext->battery_level_handle)
    {
        uint8_t battery_level = 42; // Simulated value        
        ESP_LOGI(TAG, "Sending battery level response: %d", battery_level);

        bitmans_gatts_send_uint8(pCb->gatts_if, 
            pParam->read.handle, pParam->read.conn_id, pParam->read.trans_id,
            ESP_GATT_OK, battery_level);
    }
}

static void bitman_gatts_on_disconnect(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
    esp_err_t err = bitmans_gatts_begin_advertise128(pAppContext->pszAdvName, &pAppContext->battery_service_uuid);
}

static void bitman_gatts_on_unreg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_context *pAppContext = (bitmans_gatts_context *)pCb->pContext;
}

void bitmans_gatts_context_term(bitmans_gatts_context *pContext)
{
    pContext->pszAdvName = NULL;
    pContext->battery_level_handle = 0;
    memset(&pContext->battery_level_uuid, 0, sizeof(pContext->battery_level_uuid));
    memset(&pContext->battery_service_uuid, 0, sizeof(pContext->battery_service_uuid));
}

void bitmans_gatts_context_init(bitmans_gatts_context *pContext, const char *pszAdvName)
{
    // https://www.bluetooth.com/specifications/gatt/characteristics/
    pContext->pszAdvName = pszAdvName;
    pContext->battery_level_handle = 0;
    ESP_ERROR_CHECK(bitmans_ble_string4_to_uuid128("2A19", &pContext->battery_level_uuid));
    ESP_ERROR_CHECK(bitmans_ble_string4_to_uuid128("180F", &pContext->battery_service_uuid));
}

void app_main(void)
{
    bitmans_gatts_context appContext;
    bitmans_gatts_callbacks_t appCallbacks = {
        .on_reg = bitman_gatts_on_reg,
        .on_read = bitman_gatts_on_read,
        .on_unreg = bitman_gatts_on_unreg,
        .on_start = bitman_gatts_on_start,
        .on_create = bitman_gatts_on_create,
        .on_connect = bitman_gatts_on_connect,
        .on_add_char = bitman_gatts_on_add_char,
        .on_disconnect = bitman_gatts_on_disconnect,
    };

    ESP_LOGI(TAG, "Starting BLE server application");

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());
    bitmans_gatts_context_init(&appContext, "Bitmans Battery");
    bitmans_ble_gatts_callbacks_init(&appCallbacks, &appContext);

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
    bitmans_gatts_stop_advertising();
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Unregister BLE");
    bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    vTaskDelay(30000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Uninitialising BLE server");
    bitmans_ble_server_term();

    bitmans_gatts_context_term(&appContext);
    ESP_LOGI(TAG, "BLE server application terminated");
}
