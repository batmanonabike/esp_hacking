#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// Event -> State
// 1).  Registration:
// ESP_GATTS_REG_EVT -> BAT_SERVICE_REGISTERED
//   Call esp_ble_gatts_create_service()
//
// 2).  Service creation:
// ESP_GATTS_CREATE_EVT -> BAT_SERVICE_CREATING
//   Call esp_ble_gatts_add_char() for each characteristic
//
// 3).  Characteristic addition:
// ESP_GATTS_ADD_CHAR_EVT -> BAT_SERVICE_CREATING
//   Call esp_ble_gatts_add_char_descr() for each descriptor
//
// 4).  Descriptor addition:
// ESP_GATTS_ADD_CHAR_DESCR_EVT -> BAT_SERVICE_CREATING
//   Call esp_ble_gatts_start_service() to start the service
//
// 5).  Service start:
// ESP_GATTS_START_EVT -> BAT_SERVICE_STARTED
//   Call esp_ble_gap_start_advertising() to start advertising
//
// 6).  Advertising:
// ESP_GATTS_START_ADV_EVT -> BAT_SERVICE_ADVERTISING
//
// 7).  Client connection:
//  ESP_GATTS_CONNECT_EVT -> BAT_SERVICE_CONNECTED
//    Call esp_ble_gap_stop_advertising() to stop advertising
//
// 8).  Client disconnection:
//  ESP_GATTS_DISCONNECT_EVT -> BAT_SERVICE_DISCONNECTED
//
// 9).  Service stop:
// ESP_GATTS_STOP_EVT -> BAT_SERVICE_STOPPED
//
// 10). Unregistration: 
// ESP_GATTS_UNREG_EVT -> BAT_SERVICE_UNREGISTERED
//
// 11). Error handling:
// ESP_GATTS_ERROR_EVT -> BAT_SERVICE_ERROR
//
// 12). Timeout handling:
// ESP_GATTS_TIMEOUT_EVT -> BAT_SERVICE_TIMEOUT 
//
// 13). Reset handling:
// ESP_GATTS_RESET_EVT -> BAT_SERVICE_IDLE
//
// 14). Transition complete:
// ESP_GATTS_TRANSITION_COMPLETE_EVT -> BAT_SERVICE_TRANSITION_COMPLETE
//
// 15). Event processed:
// ESP_GATTS_EVENT_PROCESSED_EVT -> BAT_SERVICE_EVENT_PROCESSED
//
// 16). Ready to advertise:
// ESP_GATTS_READY_TO_ADVERTISE_EVT -> BAT_SERVICE_READY_TO_ADVERTIS

#ifdef __cplusplus
}
#endif

