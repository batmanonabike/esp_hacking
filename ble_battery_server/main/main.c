#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "bitmans_lib.h"
#include "bitmans_config.h"

static const char *TAG = "ble_battery_server_app";

typedef struct app_context
{
    const char *pszAdvName;
    uint16_t battery_level_handle;
    bitmans_ble_uuid128_t battery_level_uuid;
    bitmans_ble_uuid128_t battery_service_uuid;

} app_context;

// Our battery service uses:
// 0x180F - Battery Service UUID (specific to battery)
// 0x2A19 - Battery Level Characteristic UUID (specific to battery)
// 0x2902 - CCCD (generic - same UUID used by heart rate monitors, thermometers, etc.)
//
// The 0x2902 descriptor is simply a mechanism that allows clients to subscribe to notifications.
// It's a general BLE feature that the battery service uses, but it's not specifically a battery thing.
static void app_context_init(app_context *pContext)
{
    pContext->battery_level_handle = 0;
    pContext->pszAdvName = "Bitmans Battery";

    const char *pszBatteryLevelId = bitmans_get_battery_level_id();
    const char *pszBatteryServiceId = bitmans_get_battery_server_id();
    ESP_ERROR_CHECK(bitmans_ble_string4_to_uuid128(pszBatteryLevelId, &pContext->battery_level_uuid));
    ESP_ERROR_CHECK(bitmans_ble_string4_to_uuid128(pszBatteryServiceId, &pContext->battery_service_uuid));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gatts callbacks
static void app_on_gatts_reg(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    bitmans_gatts_create_service128(pCb->gatts_if, &pAppContext->battery_service_uuid);
}

static void app_on_gatts_create(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    // Here we create the characteristic for the battery level.
    app_context *pAppContext = (app_context *)pCb->pContext;
    bitmans_gatts_create_char128(
        pCb->gatts_if, pCb->service_handle, &pAppContext->battery_level_uuid,
        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY, // TODO: handle notifications
        ESP_GATT_PERM_READ);
}

static void app_on_gatts_add_char(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    if (bitmans_ble_uuid_try_match(&pParam->add_char.char_uuid, &pAppContext->battery_level_uuid))
    {
        pAppContext->battery_level_handle = pParam->add_char.attr_handle;
        ESP_LOGI(TAG, "Battery level characteristic added with handle: %d", pAppContext->battery_level_handle);

        // Add CCCD so clients can enable notifications
        bitmans_gatts_add_cccd(pCb->service_handle, pAppContext->battery_level_handle);
    }

    // TODO: delete/restart service if we fail to add the characteristic???
}

static void app_on_add_char_desc(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_start_service(pCb->service_handle);
}

static void app_on_gatts_start(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    bitmans_gatts_begin_advert_data_set128(pAppContext->pszAdvName, &pAppContext->battery_service_uuid);
}

static void app_on_gatts_connect(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_stop_advertising();

    // You might want to cache the connection ID for later use for notifications,
    // to send responses to the client, to manage the connection state,
    // or to manage the connection state.
}

static void app_on_gatts_read(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;

    // This is a read request for the battery level characteristic
    if (pParam->read.handle == pAppContext->battery_level_handle)
    {
        uint8_t battery_level = 42; // Simulated value
        ESP_LOGI(TAG, "Sending battery level response: %d", battery_level);

        bitmans_gatts_send_uint8(
            pCb->gatts_if,
            pParam->read.handle, pParam->read.conn_id, pParam->read.trans_id,
            ESP_GATT_OK, battery_level);
    }
}

static void app_on_gatts_disconnect(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
    app_context *pAppContext = (app_context *)pCb->pContext;
    bitmans_gatts_begin_advert_data_set128(pAppContext->pszAdvName, &pAppContext->battery_service_uuid);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gap callbacks
static void on_gaps_advert_data_set(bitmans_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Advertising data set, starting advertising");
    bitmans_gatts_start_advertising();
}

////////////////////////////////////////////////////////////////////////////////////////////
// As it stands: this  is a correct, minimal, read-only fake battery service.
// Testable with Bluetooth LE Explorer (Windows) or similar app.
// TODO: To be “fully functioning pretend battery”:
// Add a simulated battery level that changes over time.
// Implement notification logic and CCCD write handling.
// Add a timer to update the battery level and send notifications if a client is subscribed.
// Handle client connection/pairing.
////////////////////////////////////////////////////////////////////////////////////////////
void app_main(void)
{
    ESP_LOGI(TAG, "App starting");

    bitmans_gatts_callbacks_t gatts_callbacks = {
        .on_reg = app_on_gatts_reg,
        .on_read = app_on_gatts_read,
        .on_start = app_on_gatts_start,
        .on_create = app_on_gatts_create,
        .on_connect = app_on_gatts_connect,
        .on_add_char = app_on_gatts_add_char,
        .on_disconnect = app_on_gatts_disconnect,
        .on_add_char_descr = app_on_add_char_desc,
    };

    bitmans_gaps_callbacks_t gaps_callbacks = {
        .on_advert_data_set = on_gaps_advert_data_set,
    };

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_blink_init(-1));
    ESP_ERROR_CHECK(bitmans_ble_server_init());

    app_context appContext;
    app_context_init(&appContext);
    bitmans_ble_gaps_callbacks_init(&gaps_callbacks, &appContext);
    bitmans_ble_gatts_callbacks_init(&gatts_callbacks, &appContext);

#define BITMANS_APP_ID 0x56
    ESP_LOGI(TAG, "Register Gatts");
    ESP_ERROR_CHECK(bitmans_gatts_register(BITMANS_APP_ID, &gatts_callbacks, &appContext));

    bitmans_set_blink_mode(BLINK_MODE_BREATHING);
    ESP_LOGI(TAG, "App running");
    for (int counter = 180; counter > 0; counter--)
    {
        ESP_LOGI(TAG, "App counter: %d", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Exiting soon");
    bitmans_set_blink_mode(BLINK_MODE_VERY_FAST);
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "App restarting");
    esp_restart();
}
