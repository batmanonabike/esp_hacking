#include "esp_log.h"
#include "bitmans_lib.h"
#include "bitmans_config.h"

static const char *TAG = "ble_client_app";

typedef struct app_gap_context
{
    uint32_t scan_duration_secs;
    bitmans_ble_uuid128_t service_uuid;
    
} app_gap_context;

void app_on_scan_param_set_complete(struct bitmans_gap_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    bitmans_ble_set_scan_params();
}

void app_on_scan_start_complete(struct bitmans_gap_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    if (pParam->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
    {
        ESP_LOGE(TAG, "Scan start failed, error status = %x", pParam->scan_start_cmpl.status);
    }
    else
    {
        ESP_LOGI(TAG, "Scan started successfully.");
    }
}

void app_on_update_conn_params(struct bitmans_gap_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Connection parameters updated: min_int=%d, max_int=%d, latency=%d, timeout=%d",
             pParam->update_conn_params.min_int,
             pParam->update_conn_params.max_int,
             pParam->update_conn_params.latency,
             pParam->update_conn_params.timeout);

    bitmans_bda_context_lookup(&pParam->update_conn_params.bda);
}

void app_on_sec_req(struct bitmans_gap_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    bitmans_bda_context_lookup(&pParam->ble_security.ble_req.bd_addr);
}

// In your scan result handler:
// if (bitmans_ble_find_service_uuid(&scan_result->scan_rst, &pAppContext->service_uuid)) {
//     // Found the device you want
//     memcpy(g_target_bda, scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
//     g_target_addr_type = scan_result->scan_rst.ble_addr_type;
//     esp_ble_gap_stop_scanning(); // Stop scanning before connecting
// }

// // In your scan stop complete handler:
// esp_ble_gattc_open(g_gattc_handles[GATTC_APP0], g_target_bda, g_target_addr_type, true);
// // This results in calls to the GATT Client callbacks, where you can handle the connection establishment.

void app_on_scan_result(struct bitmans_gap_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    app_gap_context *pAppContext = (app_gap_context *)pCb->pContext;
    esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)pParam;

    switch (scan_result->scan_rst.search_evt)
    {
    // Triggered for each device found during scanning.
    case ESP_GAP_SEARCH_INQ_RES_EVT: 
        bitmans_log_ble_scan(&scan_result->scan_rst, true);

        // Here you would check if this is the server you want to connect to
        // For example, by checking the advertised name or service UUID
        // If it is, you would call esp_ble_gap_stop_scanning() and then esp_ble_gattc_open()
        if (bitmans_ble_find_service_uuid(&scan_result->scan_rst, &pAppContext->service_uuid))
        {
            ESP_LOGI(TAG, "Device with custom service UUID found. BDA: %02x:%02x:%02x:%02x:%02x:%02x",
                     scan_result->scan_rst.bda[0], scan_result->scan_rst.bda[1],
                     scan_result->scan_rst.bda[2], scan_result->scan_rst.bda[3],
                     scan_result->scan_rst.bda[4], scan_result->scan_rst.bda[5]);

            // Store BDA for connection if needed:
            // memcpy(g_target_bda, scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
            // g_target_addr_type = scan_result->scan_rst.ble_addr_type;

            esp_err_t err = bitmans_bda_context_set(&scan_result->scan_rst.bda, NULL); // TODO!
            if (err == ESP_OK)
                esp_ble_gap_stop_scanning();

            // The connection attempt should ideally happen in ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT
        }
        break;

    default:
        break;
    }
}

void on_scan_stop_complete(struct bitmans_gap_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    // If you stopped scanning to connect, initiate connection here
}

void app_context_init(app_gap_context *pContext)
{
    pContext->scan_duration_secs = 30; // Default scan duration
    ESP_ERROR_CHECK(bitmans_ble_string36_to_uuid128(bitmans_get_server_id(), &pContext->service_uuid));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application");

    ESP_ERROR_CHECK(bitmans_lib_init());
    ESP_ERROR_CHECK(bitmans_ble_client_init());
    ESP_ERROR_CHECK(bitmans_ble_client_register_gattc(GATTC_APP0));

    app_gap_context app_context;
    bitmans_gap_callbacks_t gap_callbacks = {
        .pContext = NULL,
        .on_sec_req = app_on_sec_req,
        .on_scan_result = app_on_scan_result,
        .on_update_conn_params = app_on_update_conn_params,
        .on_scan_start_complete = app_on_scan_start_complete,
        .on_scan_param_set_complete = app_on_scan_param_set_complete,
    };
    app_context_init(&app_context);
    bitmans_ble_gap_callbacks_init(&gap_callbacks, &app_context);

    ESP_ERROR_CHECK(bitmans_blink_init(-1));
    bitmans_set_blink_mode(BLINK_MODE_SLOW);

    ESP_LOGI(TAG, "Initialising application");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    bitmans_set_blink_mode(BLINK_MODE_BREATHING);
    ESP_ERROR_CHECK(bitmans_ble_set_scan_params());
    for (int counter = 20; counter > 0; counter--)
    {
        ESP_LOGI(TAG, "Running application: %d", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_ERROR_CHECK(bitmans_ble_stop_scanning());

    bitmans_set_blink_mode(BLINK_MODE_FAST);
    ESP_LOGI(TAG, "Uninitialising application");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    bitmans_set_blink_mode(BLINK_MODE_VERY_FAST);
    ESP_LOGI(TAG, "Exiting application");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    bitmans_blink_term();
    bitmans_ble_client_unregister_gattc(GATTC_APP0);
    bitmans_ble_client_term();
}
