#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "esp_bt_defs.h"
#include "assert.h"

#include "bat_lib.h"
#include "bat_uuid.h"
#include "bat_ble_lib.h"
#include "bat_ble_client.h"

static const char *TAG = "bat_ble_client";

esp_err_t bat_ble_gattc_app_register(uint16_t app_id)
{
    ESP_LOGI(TAG, "Registering GATT client app with ID %d", app_id);
    
    esp_err_t ret = esp_ble_gattc_app_register(app_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register GATT client app with ID %d, error: %s", app_id, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "GATT client app registered with ID %d successfully", app_id);
    return ESP_OK;
}

esp_err_t bat_ble_gattc_open(esp_gatt_if_t gattc_if, esp_bd_addr_t peer_addr, esp_ble_addr_type_t addr_type, bool is_direct)
{
    assert(peer_addr != NULL);
    
    char addr_str[18];
    sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", 
            peer_addr[0], peer_addr[1], peer_addr[2], 
            peer_addr[3], peer_addr[4], peer_addr[5]);
    
    ESP_LOGI(TAG, "Opening connection to device %s (addr_type=%d, is_direct=%d)", 
             addr_str, addr_type, is_direct);
    
    esp_err_t ret = esp_ble_gattc_open(gattc_if, peer_addr, addr_type, is_direct);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open connection to device %s, error: %s", addr_str, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Connection request to device %s sent successfully", addr_str);
    return ESP_OK;
}

esp_err_t bat_ble_gattc_close(esp_gatt_if_t gattc_if, uint16_t conn_id)
{
    ESP_LOGI(TAG, "Closing connection with conn_id %d", conn_id);
    
    esp_err_t ret = esp_ble_gattc_close(gattc_if, conn_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to close connection with conn_id %d, error: %s", conn_id, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Connection with conn_id %d closed successfully", conn_id);
    return ESP_OK;
}

esp_err_t bat_ble_gap_set_scan_params(esp_ble_scan_params_t *scan_params)
{
    assert(scan_params != NULL);
    
    ESP_LOGI(TAG, "Setting scan parameters: interval=%d, window=%d, scan_type=%d, addr_type=%d, filter_policy=%d",
             scan_params->scan_interval, scan_params->scan_window,
             scan_params->scan_type, scan_params->own_addr_type,
             scan_params->scan_filter_policy);
             
    esp_err_t ret = esp_ble_gap_set_scan_params(scan_params);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set scan parameters, error: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Scan parameters set successfully");
    return ESP_OK;
}

esp_err_t bat_ble_gap_start_scanning(uint32_t duration, bool is_continue)
{
    ESP_LOGI(TAG, "Starting BLE scan (duration=%lu seconds, is_continue=%d)", 
             duration, is_continue);
             
    esp_err_t ret = esp_ble_gap_start_scanning(duration);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start scanning, error: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "BLE scan started successfully");
    return ESP_OK;
}

esp_err_t bat_ble_gap_stop_scanning(void)
{
    ESP_LOGI(TAG, "Stopping BLE scan");
    
    esp_err_t ret = esp_ble_gap_stop_scanning();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop scanning, error: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "BLE scan stopped successfully");
    return ESP_OK;
}

esp_err_t bat_ble_gattc_search_service(esp_gatt_if_t gattc_if, uint16_t conn_id, esp_bt_uuid_t *filter_uuid)
{
    if (filter_uuid != NULL)
    {
        char uuid_str[45] = {0}; 
        bat_uuid_to_log_string(filter_uuid, uuid_str, sizeof(uuid_str));
        ESP_LOGI(TAG, "Searching for service with UUID %s on conn_id %d", uuid_str, conn_id);
    }
    else
    {
        ESP_LOGI(TAG, "Searching for all services on conn_id %d", conn_id);
    }
    

    esp_err_t ret = esp_ble_gattc_search_service(gattc_if, conn_id, filter_uuid);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start service discovery on conn_id %d, error: %s", 
                 conn_id, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Service discovery started on conn_id %d", conn_id);
    return ESP_OK;
}

esp_err_t bat_ble_gattc_read_char(esp_gatt_if_t gattc_if, uint16_t conn_id, 
                                 uint16_t handle, esp_gatt_auth_req_t auth_req)
{
    ESP_LOGI(TAG, "Reading characteristic with handle 0x%04x on conn_id %d (auth_req=%d)", 
             handle, conn_id, auth_req);
             
    esp_err_t ret = esp_ble_gattc_read_char(gattc_if, conn_id, handle, auth_req);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read characteristic with handle 0x%04x, error: %s", 
                 handle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Read request for characteristic handle 0x%04x sent successfully", handle);
    return ESP_OK;
}

esp_err_t bat_ble_gattc_read_char_descr(esp_gatt_if_t gattc_if, uint16_t conn_id, 
                                       uint16_t handle, esp_gatt_auth_req_t auth_req)
{
    ESP_LOGI(TAG, "Reading descriptor with handle 0x%04x on conn_id %d (auth_req=%d)", 
             handle, conn_id, auth_req);
             
    esp_err_t ret = esp_ble_gattc_read_char_descr(gattc_if, conn_id, handle, auth_req);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read descriptor with handle 0x%04x, error: %s", 
                 handle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Read request for descriptor handle 0x%04x sent successfully", handle);
    return ESP_OK;
}

static void log_debug_data(const uint8_t *value, uint16_t value_len)
{
    if (value == NULL || value_len == 0)
        return;
    
    char data_str[64] = {0};
    for (int i = 0; i < value_len && i < 8; i++)
        sprintf(data_str + (i * 3), "%02x ", value[i]);
    
    if (value_len > 8)
        strcat(data_str, "...");

    ESP_LOGD(TAG, "Data to write: %s", data_str);
}

esp_err_t bat_ble_gattc_write_char(esp_gatt_if_t gattc_if, uint16_t conn_id,
                                  uint16_t handle, uint16_t value_len,
                                  uint8_t *value, esp_gatt_write_type_t write_type,
                                  esp_gatt_auth_req_t auth_req)
{    assert(value != NULL || value_len == 0);
    
    ESP_LOGI(TAG, "Writing to characteristic with handle 0x%04x, length %d, write_type=%d, auth_req=%d", 
             handle, value_len, write_type, auth_req);
             
    log_debug_data(value, value_len);
    
    esp_err_t ret = esp_ble_gattc_write_char(gattc_if, conn_id, handle, value_len,
                                           value, write_type, auth_req);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write to characteristic with handle 0x%04x, error: %s", 
                 handle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Write request to characteristic handle 0x%04x sent successfully", handle);
    return ESP_OK;
}

esp_err_t bat_ble_gattc_write_char_descr(esp_gatt_if_t gattc_if, uint16_t conn_id,
                                        uint16_t handle, uint16_t value_len,
                                        uint8_t *value, esp_gatt_write_type_t write_type,
                                        esp_gatt_auth_req_t auth_req)
{    assert(value != NULL || value_len == 0);
    
    ESP_LOGI(TAG, "Writing to descriptor with handle 0x%04x, length %d, write_type=%d, auth_req=%d", 
             handle, value_len, write_type, auth_req);
             
    log_debug_data(value, value_len);
    
    esp_err_t ret = esp_ble_gattc_write_char_descr(gattc_if, conn_id, handle, value_len,
                                                 value, write_type, auth_req);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write to descriptor with handle 0x%04x, error: %s", 
                 handle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Write request to descriptor handle 0x%04x sent successfully", handle);
    return ESP_OK;
}

esp_err_t bat_ble_gattc_register_for_notify(esp_gatt_if_t gattc_if, esp_bd_addr_t server_bda, 
                                           uint16_t handle, bool register_for_notify)
{
    assert(server_bda != NULL);
    
    char addr_str[18];
    sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", 
            server_bda[0], server_bda[1], server_bda[2], 
            server_bda[3], server_bda[4], server_bda[5]);
    
    ESP_LOGI(TAG, "%s for notifications/indications for handle 0x%04x, device %s",
             register_for_notify ? "Registering" : "Unregistering", handle, addr_str);
             
    esp_err_t ret = esp_ble_gattc_register_for_notify(gattc_if, server_bda, handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to %s for notifications with handle 0x%04x, error: %s", 
                 register_for_notify ? "register" : "unregister", handle, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Successfully %s for notifications with handle 0x%04x",
             register_for_notify ? "registered" : "unregistered", handle);
             
    return ESP_OK;
}

esp_gatt_status_t bat_ble_gattc_get_char_by_uuid(esp_gatt_if_t gattc_if, uint16_t conn_id, 
                                              uint16_t start_handle, uint16_t end_handle, 
                                              esp_bt_uuid_t char_uuid, 
                                              esp_gattc_char_elem_t *result, 
                                              uint16_t *count)
{
    assert(result != NULL);
    assert(count != NULL);
    
    char uuid_str[45] = {0};
    bat_uuid_to_log_string(&char_uuid, uuid_str, sizeof(uuid_str));
    
    ESP_LOGI(TAG, "Getting characteristic with UUID %s, range [0x%04x-0x%04x], conn_id %d", 
             uuid_str, start_handle, end_handle, conn_id);
             
    esp_gatt_status_t status = esp_ble_gattc_get_char_by_uuid(gattc_if, conn_id, 
                                                            start_handle, end_handle, 
                                                            char_uuid, result, count);
    if (status != ESP_GATT_OK)
    {
        ESP_LOGE(TAG, "Failed to get characteristic with UUID %s, error: %d", uuid_str, status);
        return status;
    }
    
    ESP_LOGI(TAG, "Found %d characteristic(s) with UUID %s, handle: 0x%04x", 
             *count, uuid_str, (*count > 0) ? result->char_handle : 0);
    return status;
}

esp_gatt_status_t bat_ble_gattc_get_descr_by_uuid(esp_gatt_if_t gattc_if, uint16_t conn_id, 
                                               uint16_t start_handle, uint16_t end_handle, 
                                               esp_bt_uuid_t char_uuid, esp_bt_uuid_t descr_uuid, 
                                               esp_gattc_descr_elem_t *result, uint16_t *count)
{
    assert(result != NULL);
    assert(count != NULL);
    
    char char_uuid_str[45] = {0};
    char descr_uuid_str[45] = {0};
    bat_uuid_to_log_string(&char_uuid, char_uuid_str, sizeof(char_uuid_str));
    bat_uuid_to_log_string(&descr_uuid, descr_uuid_str, sizeof(descr_uuid_str));
    
    ESP_LOGI(TAG, "Getting descriptor with UUID %s for characteristic UUID %s, range [0x%04x-0x%04x], conn_id %d", 
             descr_uuid_str, char_uuid_str, start_handle, end_handle, conn_id);
             
    esp_gatt_status_t status = esp_ble_gattc_get_descr_by_uuid(gattc_if, conn_id, 
                                                            start_handle, end_handle, 
                                                            char_uuid, descr_uuid, 
                                                            result, count);
    if (status != ESP_GATT_OK)
    {
        ESP_LOGE(TAG, "Failed to get descriptor with UUID %s for characteristic %s, error: %d", 
                 descr_uuid_str, char_uuid_str, status);
        return status;
    }
    
    ESP_LOGI(TAG, "Found %d descriptor(s) with UUID %s, handle: 0x%04x", 
             *count, descr_uuid_str, (*count > 0) ? result->handle : 0);
    return status;
}

esp_err_t bat_ble_gattc_app_unregister(esp_gatt_if_t gattc_if)
{
    ESP_LOGI(TAG, "Unregistering GATT client with interface %d", gattc_if);
    
    esp_err_t ret = esp_ble_gattc_app_unregister(gattc_if);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister GATT client with interface %d, error: %s", 
                 gattc_if, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "GATT client with interface %d unregistered successfully", gattc_if);
    return ESP_OK;
}
