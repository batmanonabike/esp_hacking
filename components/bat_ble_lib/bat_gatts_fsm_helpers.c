#include <stdio.h>
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
#include "bat_gatts_fsm.h"
#include "bat_ble_server.h"

// static const char *TAG = "bat_gatts_fsm_helpers";

const char *bat_gatts_state_to_string(bat_gatts_state_t state)
{
    switch (state)
    {
        case BAT_GATTS_STATE_IDLE:              return "IDLE";
        case BAT_GATTS_STATE_INITIALIZING:      return "INITIALIZING";
        case BAT_GATTS_STATE_READY:             return "READY";
        case BAT_GATTS_STATE_APP_REGISTERING:   return "APP_REGISTERING";
        case BAT_GATTS_STATE_APP_REGISTERED:    return "APP_REGISTERED";
        case BAT_GATTS_STATE_SERVICE_CREATING:  return "SERVICE_CREATING";
        case BAT_GATTS_STATE_SERVICE_CREATED:   return "SERVICE_CREATED";
        case BAT_GATTS_STATE_CHAR_ADDING:       return "CHAR_ADDING";
        case BAT_GATTS_STATE_CHAR_ADDED:        return "CHAR_ADDED";
        case BAT_GATTS_STATE_DESC_ADDING:       return "DESC_ADDING";
        case BAT_GATTS_STATE_DESC_ADDED:        return "DESC_ADDED";
        case BAT_GATTS_STATE_SERVICE_STARTING:  return "SERVICE_STARTING";
        case BAT_GATTS_STATE_SERVICE_STARTED:   return "SERVICE_STARTED";
        case BAT_GATTS_STATE_ADVERTISING:       return "ADVERTISING";
        case BAT_GATTS_STATE_CONNECTED:         return "CONNECTED";
        case BAT_GATTS_STATE_DISCONNECTING:     return "DISCONNECTING";
        case BAT_GATTS_STATE_SERVICE_STOPPING:  return "SERVICE_STOPPING";
        case BAT_GATTS_STATE_SERVICE_STOPPED:   return "SERVICE_STOPPED";
        case BAT_GATTS_STATE_APP_UNREGISTERING: return "APP_UNREGISTERING";
        case BAT_GATTS_STATE_ERROR:             return "ERROR";
        default:                                return "UNKNOWN_STATE";
    }
}

const char *bat_gatts_event_to_string(bat_gatts_event_t event)
{
    switch (event)
    {
        case BAT_GATTS_EVENT_INIT_REQUEST:      return "INIT_REQUEST";
        case BAT_GATTS_EVENT_INIT_COMPLETE:     return "INIT_COMPLETE";
        case BAT_GATTS_EVENT_REGISTER_REQUEST:  return "REGISTER_REQUEST";
        case BAT_GATTS_EVENT_REGISTER_COMPLETE: return "REGISTER_COMPLETE";
        case BAT_GATTS_EVENT_CREATE_SERVICE:    return "CREATE_SERVICE";
        case BAT_GATTS_EVENT_SERVICE_CREATED:   return "SERVICE_CREATED";
        case BAT_GATTS_EVENT_ADD_CHAR_REQUEST:  return "ADD_CHAR_REQUEST";
        case BAT_GATTS_EVENT_CHAR_ADDED:        return "CHAR_ADDED";
        case BAT_GATTS_EVENT_ADD_DESC_REQUEST:  return "ADD_DESC_REQUEST";
        case BAT_GATTS_EVENT_DESC_ADDED:        return "DESC_ADDED";
        case BAT_GATTS_EVENT_START_SERVICE:     return "START_SERVICE";
        case BAT_GATTS_EVENT_SERVICE_STARTED:   return "SERVICE_STARTED";
        case BAT_GATTS_EVENT_START_ADV_REQUEST: return "START_ADV_REQUEST";
        case BAT_GATTS_EVENT_ADV_STARTED:       return "ADV_STARTED";
        case BAT_GATTS_EVENT_CONNECT:           return "CONNECT";
        case BAT_GATTS_EVENT_DISCONNECT:        return "DISCONNECT";
        case BAT_GATTS_EVENT_STOP_ADV_REQUEST:  return "STOP_ADV_REQUEST";
        case BAT_GATTS_EVENT_ADV_STOPPED:       return "ADV_STOPPED";
        case BAT_GATTS_EVENT_STOP_SERVICE:      return "STOP_SERVICE";
        case BAT_GATTS_EVENT_SERVICE_STOPPED:   return "SERVICE_STOPPED";
        case BAT_GATTS_EVENT_UNREGISTER_REQUEST: return "UNREGISTER_REQUEST";
        case BAT_GATTS_EVENT_UNREGISTER_COMPLETE: return "UNREGISTER_COMPLETE";
        case BAT_GATTS_EVENT_RESET:             return "RESET";
        case BAT_GATTS_EVENT_ERROR:             return "ERROR";
        case BAT_GATTS_EVENT_TIMEOUT:           return "TIMEOUT";
        default:                                return "UNKNOWN_EVENT";
    }
}
