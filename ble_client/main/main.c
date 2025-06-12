#include "esp_log.h"
#include "bat_lib.h"
#include "bat_config.h"

static const char *TAG = "ble_client_app";

typedef struct app_gap_context
{
    uint32_t scan_duration_secs;
    bat_ble_uuid128_t service_uuid;

} app_gap_context;

void app_context_init(app_gap_context *pContext)
{
    pContext->scan_duration_secs = 0; // Set to 0 for indefinite scanning - we'll stop manually after one sweep
    ESP_ERROR_CHECK(bat_ble_string36_to_uuid128(bat_get_server_id(), &pContext->service_uuid));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// GAP Client Callbacks
void app_on_gapc_scan_param_set_complete(struct bat_gapc_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    app_gap_context *pAppContext = (app_gap_context *)pCb->pContext;
    bat_ble_start_scanning(pAppContext->scan_duration_secs);
}

void app_on_gapc_scan_start_complete(struct bat_gapc_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
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

void app_on_gapc_update_conn_params(struct bat_gapc_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "Connection parameters updated: min_int=%d, max_int=%d, latency=%d, timeout=%d",
             pParam->update_conn_params.min_int,
             pParam->update_conn_params.max_int,
             pParam->update_conn_params.latency,
             pParam->update_conn_params.timeout);

    bat_bda_context_lookup(&pParam->update_conn_params.bda);
}

void app_on_gapc_sec_req(struct bat_gapc_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    bat_bda_context_lookup(&pParam->ble_security.ble_req.bd_addr);
}

// In your scan result handler:
// if (bat_ble_client_find_service_uuid(&scan_result->scan_rst, &pAppContext->service_uuid)) {
//     // Found the device you want
//     memcpy(g_target_bda, scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
//     g_target_addr_type = scan_result->scan_rst.ble_addr_type;
//     esp_ble_gap_stop_scanning(); // Stop scanning before connecting
// }

// // In your scan stop complete handler:
// esp_ble_gattc_open(g_gattc_handles[GATTC_APP0], g_target_bda, g_target_addr_type, true);
// // This results in calls to the GATT Client callbacks, where you can handle the connection establishment.

static bool is_server_recognised(struct bat_gapc_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    app_gap_context *pAppContext = (app_gap_context *)pCb->pContext;

    // Check if the device advertises the expected name
    if (bat_ble_advname_matches(&pParam->scan_rst, "BitmansGATTS_0"))
        return true;

    // Check if the device advertises the custom service UUID
    if (bat_ble_client_find_service_uuid(&pParam->scan_rst, &pAppContext->service_uuid))
        return true;

    return false;
}

void app_on_gapc_scan_result(struct bat_gapc_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    switch (pParam->scan_rst.search_evt)
    {
    // Triggered for each device found during scanning.
    case ESP_GAP_SEARCH_INQ_RES_EVT:
        if (!is_server_recognised(pCb, pParam))
            return;

        // Next we vould call esp_ble_gap_stop_scanning() and then esp_ble_gattc_open()

        // Use comprehensive logging for detailed advertising packet analysis
        ESP_LOGI(TAG, "=== Using Comprehensive BLE Logging ===");
        bat_log_verbose_ble_scan(&pParam->scan_rst, false);

        // Enable debug logging to investigate esp_ble_resolve_adv_data zero-length issues
        // ESP_LOGI(TAG, "=== Debug Analysis ===");
        // bat_debug_esp_ble_resolve_adv_data(&pParam->scan_rst);

        ESP_LOGI(TAG, "Device with custom service UUID found. BDA: %02x:%02x:%02x:%02x:%02x:%02x",
                 pParam->scan_rst.bda[0], pParam->scan_rst.bda[1],
                 pParam->scan_rst.bda[2], pParam->scan_rst.bda[3],
                 pParam->scan_rst.bda[4], pParam->scan_rst.bda[5]);

        // Store BDA for connection if needed:
        // memcpy(g_target_bda, scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
        // g_target_addr_type = scan_result->scan_rst.ble_addr_type;

        // esp_err_t err = bat_bda_context_set(&scan_result->scan_rst.bda, NULL); // TODO!
        // if (err == ESP_OK)
        bat_ble_client_stop_scanning(); // Stop scanning before connecting

        // The connection attempt should ideally happen in ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT
        break;

    // Triggered when one complete scan sweep is finished
    case ESP_GAP_SEARCH_INQ_CMPL_EVT:
        ESP_LOGI(TAG, "Scan inquiry complete - one sweep finished. Stopping scan.");
        bat_ble_client_stop_scanning();
        break;

    default:
        break;
    }
}

void app_on_gapc_scan_stop_complete(struct bat_gapc_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
    // If you stopped scanning to connect, initiate connection here
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application");

    ESP_ERROR_CHECK(bat_lib_init());
    ESP_ERROR_CHECK(bat_ble_client_init());
    ESP_ERROR_CHECK(bat_ble_register_gattc(GATTC_APP0));

    app_gap_context app_context;
    bat_gapc_callbacks_t gap_callbacks = {
        .pContext = NULL,
        .on_sec_req = app_on_gapc_sec_req,
        .on_scan_result = app_on_gapc_scan_result,
        .on_scan_stop_complete = app_on_gapc_scan_stop_complete,
        .on_update_conn_params = app_on_gapc_update_conn_params,
        .on_scan_start_complete = app_on_gapc_scan_start_complete,
        .on_scan_param_set_complete = app_on_gapc_scan_param_set_complete,
    };
    app_context_init(&app_context);
    bat_ble_gapc_callbacks_init(&gap_callbacks, &app_context);

    ESP_ERROR_CHECK(bat_blink_init(-1));
    bat_set_blink_mode(BLINK_MODE_SLOW);

    ESP_LOGI(TAG, "Initialising application");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    bat_set_blink_mode(BLINK_MODE_BREATHING);
    ESP_ERROR_CHECK(bat_ble_client_set_scan_params());
    for (int counter = 20; counter > 0; counter--)
    {
        ESP_LOGI(TAG, "Running application: %d", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_ERROR_CHECK(bat_ble_client_stop_scanning());

    bat_set_blink_mode(BLINK_MODE_FAST);
    ESP_LOGI(TAG, "Uninitialising application");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    bat_set_blink_mode(BLINK_MODE_VERY_FAST);
    ESP_LOGI(TAG, "Exiting application");
    vTaskDelay(5000 / portTICK_PERIOD_MS);    
	
	bat_blink_deinit();
    bat_ble_unregister_gattc(GATTC_APP0);
    bat_ble_client_deinit();
}
