#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bitmans_lib.h"
#include "bitmans_ble_server.h"

static bitmans_ble_uuid_t s_char_uuid;
static bitmans_ble_uuid_t s_service_uuid;

static const char *TAG = "ble_server_app";
static const char *BITMANS_ADV_NAME = "Bob is a pillock";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_ble_events;

/* The event group allows multiple bits for each event, but we only care about one event
 * - are we unregistering? */
const int UNREGISTER_BIT = BIT0;

static void bitman_gatts_on_reg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    esp_err_t err = bitmans_gatts_create_service(pCb->gatts_if, &s_service_uuid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create service: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_create(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // One or more characteristics can be created here.
    esp_err_t err = bitmans_gatts_create_characteristic(
        pCb->gatts_if, pParam->create.service_handle, &s_service_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create characteristic: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_add_char(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    esp_err_t err = esp_ble_gatts_start_service(pCb->service_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start service: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_start(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    esp_err_t err = bitmans_gatts_advertise(BITMANS_ADV_NAME, &s_service_uuid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start advertising: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_unreg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Unregister called");
    xEventGroupSetBits(s_ble_events, UNREGISTER_BIT);
}

static bitmans_gatts_callbacks_t bitmans_gatts_callbacks = {
    .service_handle = 0,
    .on_reg = bitman_gatts_on_reg,
    .on_create = bitman_gatts_on_create,
    .on_add_char = bitman_gatts_on_add_char,
    .on_start = bitman_gatts_on_start,
    .on_connect = bitman_gatts_no_op,
    .on_disconnect = bitman_gatts_no_op,
    .on_read = bitman_gatts_no_op,
    .on_write = bitman_gatts_no_op,
    .on_unreg = bitman_gatts_on_unreg
};

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BLE server application");

    bitmans_ble_string_to_uuid("f0debc9a-7856-3412-1234-567856125612", &s_service_uuid);
    bitmans_ble_string_to_uuid("f0debc9a-3412-7856-1234-56785601efcd", &s_char_uuid);

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());

    s_ble_events = xEventGroupCreate();

#define BITMANS_APP_ID 0x55
    ESP_LOGI(TAG, "Register BLE server (1)");
    ESP_ERROR_CHECK(bitmans_ble_gatts_register(BITMANS_APP_ID, &bitmans_gatts_callbacks));

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
    xEventGroupWaitBits(s_ble_events, UNREGISTER_BIT,
        pdTRUE, /* Clear the bits before returning */
        pdFALSE, /* Don't wait for both bits, either bit will do */
        portMAX_DELAY); /* Wait forever */

    ESP_LOGI(TAG, "Register BLE server (2)");
    bitmans_ble_gatts_register(BITMANS_APP_ID, &bitmans_gatts_callbacks);
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Unregister BLE (2)");
    bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    //vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Wait for unregister event (2)");
    xEventGroupWaitBits(s_ble_events, UNREGISTER_BIT,
        pdTRUE, /* Clear the bits before returning */
        pdFALSE, /* Don't wait for both bits, either bit will do */
        portMAX_DELAY); /* Wait forever */

    ESP_LOGI(TAG, "Uninitialising BLE server (2)");
    bitmans_ble_server_term();
}
