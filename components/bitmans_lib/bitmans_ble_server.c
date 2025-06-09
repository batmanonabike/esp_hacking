#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"

#include "bitmans_ble.h"
#include "bitmans_hash_table.h"
#include "bitmans_ble_server.h"

// See: /docs/ble_intro.md
// GATT Server implementation for Bitman's BLE server
static const char *TAG = "bitmans_lib:ble_server";

// The GATTS interface is a unique identifier for each GATT server instance, we get that during registration.
static bitmans_hash_table_t app_cb_table;   // Hash table to map app IDs to GATTS callbacks.
static bitmans_hash_table_t gatts_cb_table; // Hash table to map GATTS interfaces to callbacks.
static bitmans_gaps_callbacks_t *g_pGapCallbacks = NULL;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// GAP (Generic Access Profile) events to about BLE advertising, scanning, connection, and security events.
// Common events include:
// - ESP_GAP_BLE_ADV_START_COMPLETE_EVT: Advertising has started.
// - ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: Advertising has stopped.
// - ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: Scanning has started.
// - ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: Scanning has stopped.
// - ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: Connection parameters updated.
// - ESP_GAP_BLE_SEC_REQ_EVT: Security request from peer device.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html#_CPPv426esp_gap_ble_cb_event_t
static void bitmans_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *pParam)
{
    assert(g_pGapCallbacks != NULL); // call bitmans_ble_gaps_callbacks_init!

    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT");
        g_pGapCallbacks->on_advert_data_set(g_pGapCallbacks, pParam);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_ADV_START_COMPLETE_EVT");
        g_pGapCallbacks->on_advert_start(g_pGapCallbacks, pParam);
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT");
        g_pGapCallbacks->on_advert_stop(g_pGapCallbacks, pParam);
        break;

    default:
        ESP_LOGD(TAG, "Unhandled GAP event: %d", event);
        break;
    }
}

static bitmans_gatts_callbacks_t *bitmans_gatts_callbacks_create_mapping(
    esp_gatt_if_t gatts_if, bitmans_gatts_app_id app_id)
{
    bitmans_gatts_callbacks_t *pCallbacks = NULL;
    esp_err_t err = bitmans_hash_table_get(&app_cb_table, app_id, (void **)&pCallbacks);

    if (err == ESP_OK && pCallbacks != NULL)
    {
        err = bitmans_hash_table_set(&gatts_cb_table, gatts_if, pCallbacks);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Callback registered for appId: %d, gatts_if: %d", app_id, gatts_if);
            pCallbacks->service_handle = 0;
            pCallbacks->gatts_if = gatts_if;
            return pCallbacks;
        }
    }

    ESP_LOGW(TAG, "No callback registered for appId: %d, gatts_if: %d", app_id, gatts_if);
    return NULL;
}

// Note that this type of esp log...
//    W (3601141) BT_BTM: BTM_BleWriteAdvData, Partial data write into ADV
// .. indicates that the packet is not big enough to hold all the advertised data.
// This means that passive scanners which don't listen for an additional scam response packet will only
// see a subset of what you are advertising.
// The order priority is:
//   Flags (mandatory) - Always included first
//   Service UUID - Typically gets priority over the device name
//   Device name - Gets truncated or dropped if space is limited.
// See also: esp_ble_adv_params_t
//
// Stick to max 10 characers for the device name to ensure it fits in the advertising packet IF you also
// advertise a service UUID.
esp_err_t bitmans_gatts_begin_advert_data_set(const char *pszAdvName, uint8_t *pId, uint8_t idLen)
{
    esp_err_t err = ESP_OK;

    // `esp_ble_adv_data_t` describes the advertising data that will be sent while advertising.
    // This is a limited size packet (31 user bytes), so we need to be careful about what we include if
    // we want passive scans to see enough data to determine interest.
    //
    // Active scanners can request additional data via a single `scan response` packet, which is larger
    // and can include more information.

    // Optional data we can include in the advertising data:
    // 'Connection Interval Range' (an extra 6 bytes) in the advertising data...
    //   .min_interval = 0x0006,
    //   .max_interval = 0x0010,
    // The code seems to function fine without this.
    //
    // Appearance is a 16-bit value which indicates the type of device being advertised.
    // This adds 3 bytes in total.
    //   0x0000: Unknown
    //   0x0080: Generic Phone
    //   0x0040: Generic Computer
    //   0x0180: Generic Watch
    //   0x0181: Sports Watch
    //   0x0340: Generic Thermometer
    //   0x0380: Generic Heart Rate Sensor
    //   0x03C0: Generic Blood Pressure
    //   0x0940: Generic Audio Source
    //   0x0941: Generic Audio Sink
    //   0x0942: Microphone
    //   0x0943: Speaker
    //   0x0944: Headphones
    //   0x0945: Portable Audio Player

    // There are also flags that can be set in the advertising data, which indicate the type of device:
    //   ESP_BLE_ADV_FLAG_GEN_DISC: General discoverable mode
    //   ESP_BLE_ADV_FLAG_LIMIT_DISC: For devices that only want to be discoverable for a short time.
    //   ESP_BLE_ADV_FLAG_BREDR_NOT_SPT: Bluetooth classic not supported

    // An *active* scan will get this as part of the advert response.
    //   esp_ble_scan_params_t
    //     .scan_type = BLE_SCAN_TYPE_ACTIVE 

    // Advertisement packet. 
    // For example we might just include the service UUID which means that *passive* scan can find 
    // if they are interested in the service quicker and without handshaking.

    esp_ble_adv_data_t adv_data = {
        .p_service_uuid = pId,
        .service_uuid_len = idLen,
        .set_scan_rsp = false,
        .include_name = false, // Don't include name in adv packet
        .include_txpower = false,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .appearance = 0x0944, // Headphones
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };

    // Set main advertisement data.
    err = esp_ble_gap_config_adv_data(&adv_data);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure advertising data: %s", esp_err_to_name(err));
        return err;
    }

    if (pszAdvName != NULL)
    {
        // We can send one (optional) additional `Scan response` packet.
        // This packet can include more data than the main advertisement packet but won't be seen by
        // passive scanners.
        esp_ble_adv_data_t scan_rsp_data = {
            .set_scan_rsp = true,
            .include_name = true, // Include name in scan response (pszAdvertisedName)
            .include_txpower = false,
            .flag = 0 // No flags in scan response
        };

        err = esp_ble_gap_set_device_name(pszAdvName);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set device name for: %s, %s", pszAdvName, esp_err_to_name(err));
            return err;
        }

        err = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to configure scan response data: %s", esp_err_to_name(err));
            return err;
        }
    }

    ESP_LOGV(TAG, "Advertising data setup succeeded");
    return ESP_OK;
}

esp_err_t bitmans_gatts_begin_advert_data_set128(const char *pszAdvertisedName, bitmans_ble_uuid128_t *pId)
{
    return bitmans_gatts_begin_advert_data_set(pszAdvertisedName, pId->uuid, ESP_UUID_LEN_128);
}

static esp_err_t bitmans_gatts_create_service(esp_gatt_if_t gatts_if, esp_gatt_srvc_id_t *pId)
{
    esp_err_t err = esp_ble_gatts_create_service(gatts_if, pId, 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create GATTS service: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t bitmans_gatts_create_service128(esp_gatt_if_t gatts_if, bitmans_ble_uuid128_t *pId)
{
    esp_gatt_id_t id = {
        .inst_id = 0,
        .uuid = {
            .len = ESP_UUID_LEN_128,
            .uuid = {.uuid128 = {0}}}};
    memcpy(id.uuid.uuid.uuid128, pId->uuid, sizeof(pId->uuid));

    esp_gatt_srvc_id_t service_id = {.id = id, .is_primary = true};

    return bitmans_gatts_create_service(gatts_if, &service_id);
}

esp_err_t bitmans_gatts_start_advertising()
{
    esp_err_t err = esp_ble_gap_start_advertising(&adv_params);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start advertising: %s", esp_err_to_name(err));
        return err;
    }
    else
    {
        ESP_LOGI(TAG, "Advertising started successfully");
    }
    return ESP_OK;
}

// Permissions control what operations a client is allowed to perform on the characteristic value.
//   - ESP_GATT_PERM_READ:   Client can read the value.
//   - ESP_GATT_PERM_WRITE:  Client can write the value.
//   - ESP_GATT_PERM_READ_ENC, ESP_GATT_PERM_WRITE_ENC, etc: Require encryption/authentication.
//   - Combine with | to allow multiple permissions.
//
// Properties describe the supported GATT operations for the characteristic:
//   - ESP_GATT_CHAR_PROP_BIT_READ:      Characteristic can be read.
//   - ESP_GATT_CHAR_PROP_BIT_WRITE:     Characteristic can be written.
//   - ESP_GATT_CHAR_PROP_BIT_NOTIFY:    Server can send notifications to clients.
//   - ESP_GATT_CHAR_PROP_BIT_INDICATE:  Server can send indications (with acknowledgment).
//   - Combine with | to support multiple operations.
//
// Example for Battery Level characteristic:
//   Permissions: ESP_GATT_PERM_READ
//   Properties:  ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY
static esp_err_t bitmans_gatts_create_char(
    esp_gatt_if_t gatts_if, bitmans_gatts_service_handle service_handle,
    esp_bt_uuid_t *pId, esp_gatt_char_prop_t properties, esp_gatt_perm_t permissions)
{
    esp_err_t err = esp_ble_gatts_add_char(service_handle, pId, permissions, properties, NULL, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create characteristic: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t bitmans_gatts_create_char128(
    esp_gatt_if_t gatts_if, bitmans_gatts_service_handle service_handle,
    bitmans_ble_uuid128_t *pId, esp_gatt_char_prop_t properties, esp_gatt_perm_t permissions)
{
    esp_bt_uuid_t char_uuid = {
        .len = ESP_UUID_LEN_128,
        .uuid = {.uuid128 = {0}}};
    memcpy(char_uuid.uuid.uuid128, pId->uuid, sizeof(pId->uuid));
    return bitmans_gatts_create_char(gatts_if, service_handle, &char_uuid, properties, permissions);
}

// The Client Characteristic Configuration Descriptor (CCCD, UUID 0x2902) is required for any
// BLE characteristic that supports notifications or indications.
// It allows each client to enable or disable notifications/indications for itself by writing to this descriptor.
// Without the CCCD, clients cannot subscribe to battery level notifications, even if the characteristic supports them.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gatts.html
esp_err_t bitmans_gatts_add_cccd(uint16_t service_handle, uint16_t char_handle)
{
    // 0x2902 is the standard UUID for CCCD
    esp_bt_uuid_t cccd_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}};

    // Permissions: allow client to read and write the descriptor
    esp_err_t err = esp_ble_gatts_add_char_descr(service_handle, &cccd_uuid,
                                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add CCCD descriptor: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "CCCD descriptor added for service_handle=%d, char_handle=%d", service_handle, char_handle);
    }

    return err;
}

static bitmans_gatts_callbacks_t *bitmans_gatts_callbacks_lookup(esp_gatt_if_t gatts_if)
{
    bitmans_gatts_callbacks_t *pCallbacks = NULL;
    esp_err_t err = bitmans_hash_table_get(&gatts_cb_table, gatts_if, (void **)&pCallbacks);

    if (err == ESP_OK && pCallbacks != NULL)
    {
        ESP_LOGV(TAG, "Got callback for gatts_if: %d", gatts_if);
        return pCallbacks;
    }

    ESP_LOGW(TAG, "No callback registered for gatts_if: %d", gatts_if);
    return NULL;
}

esp_err_t bitmans_gatts_start_service(bitmans_gatts_service_handle service_handle)
{
    esp_err_t err = esp_ble_gatts_start_service(service_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start service: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Service started successfully, service_handle=%d", service_handle);
    }
    return ESP_OK;
}

esp_err_t bitmans_gatts_stop_service(bitmans_gatts_service_handle service_handle)
{
    esp_err_t err = esp_ble_gatts_stop_service(service_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop service: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Service started successfully, service_handle=%d", service_handle);
    }
    return ESP_OK;
}

esp_err_t bitmans_gatts_send_response(
    esp_gatt_if_t gatts_if, uint16_t conn_id, uint32_t trans_id,
    esp_gatt_status_t status, esp_gatt_rsp_t *pResponse)
{
    esp_err_t err = esp_ble_gatts_send_response(gatts_if, conn_id, trans_id, status, pResponse);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send response: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Response sent successfully");
    }
    return ESP_OK;
}

esp_err_t bitmans_gatts_send_uint8(
    esp_gatt_if_t gatts_if, uint16_t handle, uint16_t conn_id, uint32_t trans_id,
    esp_gatt_status_t status, uint8_t value)
{
    esp_gatt_rsp_t response = {0};
    response.attr_value.len = 1;
    response.attr_value.handle = handle;
    response.attr_value.value[0] = value;
    return bitmans_gatts_send_response(gatts_if, conn_id, trans_id, status, &response);
}

// GATTS (GATT Server) events notify about BLE server events.
// These include:
// - ESP_GATTS_REG_EVT: GATT server profile registered, usually where you create your service.
// - ESP_GATTS_CREATE_EVT: Service created, add characteristics here.
// - ESP_GATTS_ADD_CHAR_EVT: Characteristic added, add descriptors (like CCCD) here.
// - ESP_GATTS_ADD_CHAR_DESCR_EVT: Descriptor (such as CCCD) added, store descriptor handle if needed.
// - ESP_GATTS_START_EVT: Service started, begin advertising.
// - ESP_GATTS_CONNECT_EVT: A client has connected.
// - ESP_GATTS_DISCONNECT_EVT: A client has disconnected, often restart advertising.
// - ESP_GATTS_READ_EVT: A client is reading a characteristic or descriptor value.
// - ESP_GATTS_WRITE_EVT: A client is writing to a characteristic or descriptor value.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gatts.html#_CPPv426esp_gatts_cb_event_t

// Order of Operations: The correct order of operations is:
// ESP_GATTS_REG_EVT: Register the GATT application and create the service.
// ESP_GATTS_CREATE_EVT: The service is created. Now add the characteristic.
// ESP_GATTS_ADD_CHAR_EVT: The characteristic is added. Now add descriptors (like CCCD).
// ESP_GATTS_ADD_CHAR_DESCR_EVT: The descriptor is added. Now start the service.
// ESP_GATTS_START_EVT: The service is started. Now start advertising.
static void bitmans_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *pParam)
{
    bitmans_gatts_callbacks_t *pCallbacks = NULL;

    if (event == ESP_GATTS_REG_EVT)
    {
        ESP_LOGI(TAG, "ESP_GATTS_REG_EVT, Service registered");
        pCallbacks = bitmans_gatts_callbacks_create_mapping(gatts_if, pParam->reg.app_id);
        if (pCallbacks != NULL)
            pCallbacks->on_reg(pCallbacks, pParam);
        return;
    }

    pCallbacks = bitmans_gatts_callbacks_lookup(gatts_if);

    switch (event)
    {
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CREATE_EVT, Service created");
        if (pCallbacks != NULL)
        {
            pCallbacks->service_handle = pParam->create.service_handle;
            pCallbacks->on_create(pCallbacks, pParam);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_EVT, Characteristic added");
        if (pCallbacks != NULL)
            pCallbacks->on_add_char(pCallbacks, pParam);
        break;

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_ADD_CHAR_DESCR_EVT, Descriptor added");
        if (pCallbacks != NULL)
            pCallbacks->on_add_char_descr(pCallbacks, pParam);
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_START_EVT, Service started");
        if (pCallbacks != NULL)
            pCallbacks->on_start(pCallbacks, pParam);
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, Client connected");
        if (pCallbacks != NULL)
            pCallbacks->on_connect(pCallbacks, pParam);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, Client disconnected");
        if (pCallbacks != NULL)
            pCallbacks->on_disconnect(pCallbacks, pParam);
        break;

    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_READ_EVT, Read event");
        if (pCallbacks != NULL)
            pCallbacks->on_read(pCallbacks, pParam);
        break;

        // Respond with dummy data
        // {
        //     uint8_t value[4] = {0x42, 0x43, 0x44, 0x45};
        //     esp_gatt_rsp_t rsp = {0};
        //     rsp.attr_value.handle = pParam->read.handle;
        //     rsp.attr_value.len = 4;
        //     memcpy(rsp.attr_value.value, value, 4);
        //     esp_ble_gatts_send_response(gatts_if, pParam->read.conn_id, pParam->read.trans_id, ESP_GATT_OK, &rsp);
        // }
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT, Write event");

        // Optionally process param->write.value
        // esp_ble_gatts_send_response(gatts_if, pParam->write.conn_id, pParam->write.trans_id, ESP_GATT_OK, NULL);
        break;

    case ESP_GATTS_STOP_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_STOP_EVT, Service stopped");
        if (pCallbacks != NULL)
            pCallbacks->on_stop(pCallbacks, pParam);
        break;

    case ESP_GATTS_UNREG_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_UNREG_EVT, unregistering app");
        if (pCallbacks != NULL)
            pCallbacks->on_unreg(pCallbacks, pParam);
        bitmans_hash_table_remove(&gatts_cb_table, gatts_if);
        break;

    default:
        break;
    }
}

esp_err_t bitmans_ble_server_init()
{
    ESP_LOGI(TAG, "Initializing BLE GATT server");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
        return ret;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
        return ret;

    ret = esp_bluedroid_init();
    if (ret)
        return ret;

    ret = esp_bluedroid_enable();
    if (ret)
        return ret;

    ret = esp_ble_gatts_register_callback(bitmans_gatts_event_handler);
    if (ret)
        return ret;

    ret = esp_ble_gap_register_callback(bitmans_gap_event_handler);
    if (ret)
        return ret;

    // Initialize hash tables for GATT callbacks
    ret = bitmans_hash_table_init(&gatts_cb_table, 16, NULL, NULL);
    if (ret)
        return ret;

    return bitmans_hash_table_init(&app_cb_table, 4, NULL, NULL);
}

esp_err_t bitmans_gatts_stop_advertising()
{
    esp_err_t ret = esp_ble_gap_stop_advertising();
    if (ret == ESP_OK)
    {
        ESP_LOGV(TAG, "Stopped advertising: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGE(TAG, "Failed to stop advertising: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t bitmans_ble_server_term()
{
    ESP_LOGI(TAG, "Terminating BLE GATT server");

    bitmans_gatts_stop_advertising();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    bitmans_hash_table_cleanup(&app_cb_table);
    bitmans_hash_table_cleanup(&gatts_cb_table);

    return ESP_OK;
}

void bitman_gatts_no_op(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam)
{
}

void bitmans_ble_gatts_callbacks_init(bitmans_gatts_callbacks_t *pCallbacks, void *pContext)
{
    pCallbacks->service_handle = 0;
    pCallbacks->pContext = pContext;
    pCallbacks->gatts_if = ESP_GATT_IF_NONE;

    if (pCallbacks->on_reg == NULL)
        pCallbacks->on_reg = bitman_gatts_no_op;
    if (pCallbacks->on_read == NULL)
        pCallbacks->on_read = bitman_gatts_no_op;
    if (pCallbacks->on_stop == NULL)
        pCallbacks->on_stop = bitman_gatts_no_op;
    if (pCallbacks->on_start == NULL)
        pCallbacks->on_start = bitman_gatts_no_op;
    if (pCallbacks->on_unreg == NULL)
        pCallbacks->on_unreg = bitman_gatts_no_op;
    if (pCallbacks->on_write == NULL)
        pCallbacks->on_write = bitman_gatts_no_op;
    if (pCallbacks->on_create == NULL)
        pCallbacks->on_create = bitman_gatts_no_op;
    if (pCallbacks->on_connect == NULL)
        pCallbacks->on_connect = bitman_gatts_no_op;
    if (pCallbacks->on_add_char == NULL)
        pCallbacks->on_add_char = bitman_gatts_no_op;
    if (pCallbacks->on_disconnect == NULL)
        pCallbacks->on_disconnect = bitman_gatts_no_op;
    if (pCallbacks->on_add_char_descr == NULL)
        pCallbacks->on_add_char_descr = bitman_gatts_no_op;
}

esp_err_t bitmans_gatts_register(bitmans_gatts_app_id app_id, bitmans_gatts_callbacks_t *pCallbacks, void *pContext)
{
    ESP_LOGI(TAG, "Registering GATT server: %d", app_id);

    if (pCallbacks == NULL)
        return ESP_ERR_INVALID_ARG;

    pCallbacks->pContext = pContext;
    esp_err_t ret = bitmans_hash_table_set(&app_cb_table, app_id, pCallbacks);
    if (ret != ESP_OK)
        return ret;

    ret = esp_ble_gatts_app_register(app_id);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register GATT application: %s", esp_err_to_name(ret)); // Add this error check
        return ret;
    }

    return ESP_OK;
}

esp_err_t bitmans_gatts_unregister(bitmans_gatts_app_id app_id)
{
    ESP_LOGI(TAG, "Unregistering GATT server: %d", app_id);

    bitmans_gatts_callbacks_t *pCallbacks = pCallbacks =
        (bitmans_gatts_callbacks_t *)bitmans_hash_table_try_get(&app_cb_table, app_id);

    if (pCallbacks == NULL)
    {
        ESP_LOGW(TAG, "No callbacks registered for appId: %d", app_id);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret = esp_ble_gatts_app_unregister(pCallbacks->gatts_if);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister GATT application: %s", esp_err_to_name(ret));
        return ret;
    }

    return bitmans_hash_table_remove(&app_cb_table, app_id);
}

void bitmans_gaps_no_op(struct bitmans_gaps_callbacks_t *pCb, esp_ble_gap_cb_param_t *pParam)
{
}

void bitmans_ble_gaps_callbacks_init(bitmans_gaps_callbacks_t *pCb, void *pContext)
{
    assert(pCb != NULL);

    g_pGapCallbacks = pCb;
    g_pGapCallbacks->pContext = pContext;
    if (g_pGapCallbacks->on_advert_stop == NULL)
        g_pGapCallbacks->on_advert_stop = bitmans_gaps_no_op;
    if (g_pGapCallbacks->on_advert_start == NULL)
        g_pGapCallbacks->on_advert_start = bitmans_gaps_no_op;
    if (g_pGapCallbacks->on_advert_data_set == NULL)
        g_pGapCallbacks->on_advert_data_set = bitmans_gaps_no_op;
}
