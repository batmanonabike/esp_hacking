#include "esp_log.h"
#include "bitmans_lib.h"
#include "bitmans_ble_server.h"

static bitmans_ble_uuid_t g_char_uuid;
static bitmans_ble_uuid_t g_service_uuid;

static const char *TAG = "ble_server_app";
static const char *BITMANS_ADV_NAME = "BitmansBLE2";

static void bitman_gatts_on_reg(bitmans_gatts_callbacks_t *pCb, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    esp_err_t err = bitmans_gatts_create_service(gatts_if, &g_service_uuid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create service: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_create(bitmans_gatts_callbacks_t *pCb, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    // One or more characteristics can be created here.
    esp_err_t err = bitmans_gatts_create_characteristic(
        gatts_if, pParam->create.service_handle, &g_service_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create characteristic: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_add_char(bitmans_gatts_callbacks_t *pCb, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    esp_err_t err = esp_ble_gatts_start_service(pCb->service_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start service: %s", esp_err_to_name(err));
    }
}

static void bitman_gatts_on_start(bitmans_gatts_callbacks_t *pCb, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    esp_err_t err = bitmans_gatts_advertise(BITMANS_ADV_NAME, &g_service_uuid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start advertising: %s", esp_err_to_name(err));
    }
}

static bitmans_gatts_callbacks_t bitmans_gatts_callbacks = {
    .service_handle = 0,
    .on_reg = bitman_gatts_on_reg,
    .on_create = bitman_gatts_on_create,
    .on_add_char = bitman_gatts_on_add_char,
    .on_start = bitman_gatts_on_start,
    .on_connect = bitman_gatt_no_op,
    .on_disconnect = bitman_gatt_no_op,
    .on_read = bitman_gatt_no_op,
    .on_write = bitman_gatt_no_op,
    .on_unreg = bitman_gatt_no_op
};

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BLE server application");

    bitmans_ble_string_to_uuid("f0debc9a-7856-3412-1234-567856125612", &g_service_uuid);
    bitmans_ble_string_to_uuid("f0debc9a-3412-7856-1234-56785601efcd", &g_char_uuid);

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_server_init());

#define BITMANS_APP_ID 0x55
    ESP_ERROR_CHECK(bitmans_ble_gatts_register(BITMANS_APP_ID, &bitmans_gatts_callbacks));

    ESP_LOGI(TAG, "BLE server running");
    for (int counter = 200; counter > 0; counter--)
    {
        ESP_LOGI(TAG, "Running BLE server: %d", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Uninitialising BLE server");
    bitmans_ble_gatts_unregister(BITMANS_APP_ID);
    bitmans_ble_server_term();
}
