#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "bat_ble_client.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void bat_debug_esp_ble_resolve_adv_data(bat_scan_result_t *pScanResult);
    void bat_log_ble_scan(bat_scan_result_t *pScanResult, bool ignoreNoAdvertisedName);
    void bat_log_verbose_ble_scan(bat_scan_result_t *pScanResult, bool ignoreNoAdvertisedName);

#ifdef __cplusplus
}
#endif