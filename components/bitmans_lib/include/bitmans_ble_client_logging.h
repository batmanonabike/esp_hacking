#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "bitmans_ble_client.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void bitmans_log_ble_scan(bitmans_scan_result_t *pScanResult, bool ignoreNoAdvertisedName);

#ifdef __cplusplus
}
#endif