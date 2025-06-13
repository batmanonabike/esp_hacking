#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_bt_defs.h"
#include "esp_gatts_api.h"
#include "assert.h"

#include "bat_lib.h"
#include "bat_gatts_fsm.h"
#include "bat_ble_server.h"
#include "bat_gatts_fsm_helpers.h"

static const char *TAG = "bat_gatts_fsm";

/**
 * @brief GATT Server event handler
 * 
 * Handles callback events from the ESP32 BLE GATT Server API.
 * See ESP-IDF documentation for details:
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gatts.html
 * 
 * @param event The GATT Server event type
 * @param gatts_if GATT Server interface type
 * @param pParam Pointer to callback parameters
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t bat_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    ESP_LOGI(TAG, "GATTS event: %d, gatts_if: %d", event, gatts_if);
    
    if (pParam == NULL) 
    {
        ESP_LOGE(TAG, "Invalid GATTS callback parameters (NULL)");
        return ESP_ERR_INVALID_ARG;
    }

    switch (event) 
    {
        // Registration events (typically first)
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_REG_EVT received, app_id: %d, status: %d", 
                    pParam->reg.app_id, pParam->reg.status);
            break;
            
        // Service creation events
        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CREATE_EVT received, status: %d, service_handle: %d", 
                    pParam->create.status, pParam->create.service_handle);
            break;

        case ESP_GATTS_ADD_INCL_SRVC_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_ADD_INCL_SRVC_EVT received, status: %d, service_handle: %d", 
                    pParam->add_incl_srvc.status, pParam->add_incl_srvc.service_handle);
            break;
            
        case ESP_GATTS_ADD_CHAR_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_EVT received, status: %d, attr_handle: %d, service_handle: %d", 
                    pParam->add_char.status, pParam->add_char.attr_handle, pParam->add_char.service_handle);
            break;
            
        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_DESCR_EVT received, status: %d, attr_handle: %d", 
                    pParam->add_char_descr.status, pParam->add_char_descr.attr_handle);
            break;
        
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CREAT_ATTR_TAB_EVT received, status: %d, num_handle: %d", 
                    pParam->add_attr_tab.status, pParam->add_attr_tab.num_handle);
            break;
            
        // Service start/stop events
        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_START_EVT received, status: %d, service_handle: %d", 
                    pParam->start.status, pParam->start.service_handle);
            break;
            
        case ESP_GATTS_STOP_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_STOP_EVT received, status: %d, service_handle: %d", 
                    pParam->stop.status, pParam->stop.service_handle);
            break;
            
        // Connection events
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT received, conn_id: %d", 
                    pParam->connect.conn_id);
            ESP_LOGI(TAG, "Remote device address: %02x:%02x:%02x:%02x:%02x:%02x",
                    pParam->connect.remote_bda[0], pParam->connect.remote_bda[1],
                    pParam->connect.remote_bda[2], pParam->connect.remote_bda[3],
                    pParam->connect.remote_bda[4], pParam->connect.remote_bda[5]);
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT received, conn_id: %d, reason: 0x%x", 
                    pParam->disconnect.conn_id, pParam->disconnect.reason);
            break;
            
        // MTU exchange event
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT received, conn_id: %d, mtu: %d", 
                    pParam->mtu.conn_id, pParam->mtu.mtu);
            break;
            
        // Data exchange events
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_READ_EVT received, conn_id: %d, handle: %d", 
                    pParam->read.conn_id, pParam->read.handle);
            break;
            
        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT received, conn_id: %d, handle: %d, len: %d", 
                    pParam->write.conn_id, pParam->write.handle, pParam->write.len);
            if (pParam->write.is_prep)
            {
                ESP_LOGI(TAG, "Write is prepare write operation");
            }
            break;
            
        case ESP_GATTS_EXEC_WRITE_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT received, conn_id: %d, exec_write_flag: %d", 
                    pParam->exec_write.conn_id, pParam->exec_write.exec_write_flag);
            break;
            
        case ESP_GATTS_RESPONSE_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_RESPONSE_EVT received, status: %d, handle: %d", 
                    pParam->rsp.status, pParam->rsp.handle);
            break;
            
        // Confirmation events
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT received, conn_id: %d, handle: %d, status: %d", 
                    pParam->conf.conn_id, pParam->conf.handle, pParam->conf.status);
            break;
            
        // Other events
        case ESP_GATTS_SET_ATTR_VAL_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_SET_ATTR_VAL_EVT received, status: %d", 
                    pParam->set_attr_val.status);
            break;
            
        case ESP_GATTS_SEND_SERVICE_CHANGE_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_SEND_SERVICE_CHANGE_EVT received, status: %d", 
                    pParam->service_change.status);
            break;
            
        case ESP_GATTS_OPEN_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_OPEN_EVT received, status: %d", pParam->open.status);
            break;
            
        case ESP_GATTS_CLOSE_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CLOSE_EVT received, status: %d", pParam->close.status);
            break;
            
        case ESP_GATTS_LISTEN_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_LISTEN_EVT received");
            break;
            
        case ESP_GATTS_CONGEST_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONGEST_EVT received, conn_id: %d, congested: %d", 
                    pParam->congest.conn_id, pParam->congest.congested);
            break;
            
        // Clean-up events
        case ESP_GATTS_DELETE_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_DELETE_EVT received, status: %d", pParam->del.status);
            break;
            
        case ESP_GATTS_UNREG_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_UNREG_EVT received");
            break;
            
        default:
            ESP_LOGW(TAG, "Unhandled GATTS event: %d", event);
            break;
    }
    
    return ESP_OK;
}

esp_err_t bat_gatts_fsm(void)
{
    ESP_LOGI(TAG, "GATTS fsm");
    return ESP_OK;
}
