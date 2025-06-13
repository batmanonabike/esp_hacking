#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// 1. Create a Server Context Structure
// First, create a context structure to maintain server state and configuration:
#define BAT_MAX_CHARACTERISTICS 10
typedef void (*bat_ble_server_cb_t)(void *pContext, esp_ble_gatts_cb_param_t *pParam);

typedef struct {
    void *pContext;
    bat_ble_server_cb_t onRead;
    bat_ble_server_cb_t onWrite;
    bat_ble_server_cb_t onConnect;
    bat_ble_server_cb_t onDisconnect;   
} bat_ble_server_callbacks_t;

typedef struct
{
    // Server identification
    uint16_t appId;
    char deviceName[32];

    // Service information
    uint16_t serviceHandle;
    uint16_t charHandles[BAT_MAX_CHARACTERISTICS];
    uint8_t numChars;

    // Connection state
    bool isConnected;
    uint16_t connId;
    esp_gatt_if_t gattsIf;

    // Event handling
    EventGroupHandle_t eventGroup;

    // Callbacks
    bat_ble_server_callbacks_t callbacks;

} bat_ble_server_t;

typedef struct {
    uint16_t uuid;
    esp_gatt_perm_t permissions;
    esp_gatt_char_prop_t properties;
    uint16_t maxLen;
    uint8_t *pInitialValue;
    uint16_t initValueLen;
} bat_ble_char_config_t;

esp_err_t bat_ble_server_init2(bat_ble_server_t *pServer, const char* deviceName, uint16_t appId, uint16_t serviceUuid);
esp_err_t bat_ble_server_start(bat_ble_server_t *pServer);
esp_err_t bat_ble_server_stop(bat_ble_server_t *pServer);
esp_err_t bat_ble_server_notify(bat_ble_server_t *pServer, uint16_t charIndex, uint8_t *pData, uint16_t dataLen);
esp_err_t bat_ble_server_set_callbacks(bat_ble_server_t *pServer, void *pContext, bat_ble_server_callbacks_t *pCallbacks);
esp_err_t bat_ble_server_create_service(bat_ble_server_t *pServer, uint16_t serviceUuid, bat_ble_char_config_t *pCharConfigs, uint8_t numChars);

#ifdef __cplusplus
}
#endif

