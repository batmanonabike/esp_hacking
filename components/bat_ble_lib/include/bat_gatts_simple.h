#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BAT_MAX_CHARACTERISTICS 10
#define BLE_OPERATION_TIMEOUT_MS 5000

typedef struct bat_gatts_server_s bat_gatts_server_t; 
typedef void (*bat_gatts_callback_t)(bat_gatts_server_t *pServer, esp_ble_gatts_cb_param_t *pParam);

typedef struct 
{
    bat_gatts_callback_t onRead;
    bat_gatts_callback_t onWrite;
    bat_gatts_callback_t onConnect;
    bat_gatts_callback_t onDisconnect;   
} bat_gatts_callbacks2_t;

typedef struct bat_gatts_server_s
{
    void *pContext;

    // Server identification
    int appearance;
    uint16_t appId;
    char deviceName[32];
    esp_bt_uuid_t serviceId;
    esp_ble_adv_params_t * pAdvParams;
    uint8_t raw_uuid[ESP_UUID_LEN_128];

    // Service information
    uint8_t numChars;
    uint16_t serviceHandle;
    uint16_t charHandles[BAT_MAX_CHARACTERISTICS];

    // Connection state
    bool isConnected;
    uint16_t connId;
    esp_gatt_if_t gattsIf;

    // Event handling
    EventGroupHandle_t eventGroup;

    // Callbacks
    bat_gatts_callbacks2_t callbacks;

} bat_gatts_server_t;

typedef struct 
{
    uint16_t uuid;
    uint16_t maxLen;
    uint16_t initValueLen;
    uint8_t *pInitialValue;
    esp_gatt_perm_t permissions;
    esp_gatt_char_prop_t properties;
} bat_gatts_char_config_t;

esp_err_t bat_gatts_init(bat_gatts_server_t *, void *, const char*, uint16_t appId, const char*, int, int);
esp_err_t bat_gatts_create_service(bat_gatts_server_t *, bat_gatts_char_config_t *pCharConfigs, uint8_t numChars, int);
esp_err_t bat_gatts_start(bat_gatts_server_t *, bat_gatts_callbacks2_t *pCbs, int timeoutMs);
esp_err_t bat_gatts_stop(bat_gatts_server_t *, int);
esp_err_t bat_gatts_deinit(bat_gatts_server_t *);

void bat_gatts_reset_flags(bat_gatts_server_t *);
esp_err_t bat_gatts_notify(bat_gatts_server_t *, uint16_t charIndex, uint8_t *pData, uint16_t dataLen);

#ifdef __cplusplus
}
#endif
