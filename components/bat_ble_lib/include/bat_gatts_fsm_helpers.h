#pragma once

#include "bat_gatts_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *bat_gatts_state_to_string(bat_gatts_state_t state);
const char *bat_gatts_event_to_string(bat_gatts_event_t event);

#ifdef __cplusplus
}
#endif

