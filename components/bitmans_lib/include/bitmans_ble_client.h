#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "bitmans_ble.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
    SUMMARY:
    - A BDA (Bluetooth Device Address) is a unique 6-byte address assigned to every Bluetooth device, used to identify devices in BLE operations.
    - This header defines two callback structures for BLE GAP events:
      * bitmans_gap_bda_callbacks_t: For events associated with a specific BDA (e.g., scan result, connection, security request).
      * bitmans_gap_global_callbacks_t: For global events not associated with a specific BDA (e.g., scan start/stop, scan param set).
    - Registration functions are provided for both per-BDA and global callbacks.
    */

    // The GATT Client applications IDs, the application IDs are used to register GATT Client applications.
    typedef enum
    {
        GATTC_APPFIRST = 0,
        GATTC_APP0 = GATTC_APPFIRST,
        GATTC_APP1 = 1,
        GATTC_APP2 = 2,
        GATTC_APP3 = 3,
        GATTC_APP4 = 4,
        GATTC_APPLAST = GATTC_APP4, // Last app ID
    } bitmans_gattc_app_id_t;

#define ADVERTISED_NAME_BUFFER_LEN 32
    typedef struct
    {
        char name[ADVERTISED_NAME_BUFFER_LEN];
    } bitmans_advertised_name_t;

    // `GCC typeof` because the header files doesn\'t expose scan_rst structure directly.
    typedef typeof(((esp_ble_gap_cb_param_t *)0)->scan_rst) bitmans_scan_result_t;

    esp_err_t bitmans_ble_client_init();
    esp_err_t bitmans_ble_client_term();
    esp_err_t bitmans_ble_register_gattc(bitmans_gattc_app_id_t);
    esp_err_t bitmans_ble_unregister_gattc(bitmans_gattc_app_id_t);

    esp_err_t bitmans_ble_client_set_scan_params(); // Effectively initiates scanning.
    esp_err_t bitmans_ble_start_scanning(uint32_t scan_duration_secs);
    esp_err_t bitmans_ble_client_stop_scanning();

    esp_err_t bitmans_ble_client_get_advertised_name(bitmans_scan_result_t *, bitmans_advertised_name_t *);
    bool bitmans_ble_advname_matches(bitmans_scan_result_t *, const char *pszName);
    bool bitmans_ble_client_find_service_uuid(bitmans_scan_result_t *pScanResult, bitmans_ble_uuid128_t * pId);
    
    // Callbacks from scanning and connection management. 
    typedef struct bitmans_gapc_callbacks_t
    {
        void *pContext;
        void (*on_scan_param_set_complete)(struct bitmans_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_scan_start_complete)(struct bitmans_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_scan_stop_complete)(struct bitmans_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);

        // BDA-specific callbacks can be added here if needed
        void (*on_scan_result)(struct bitmans_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_update_conn_params)(struct bitmans_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_sec_req)(struct bitmans_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
    } bitmans_gapc_callbacks_t;
    void bitmans_ble_gapc_callbacks_init(bitmans_gapc_callbacks_t *, void *pContext);

    // GAP BDA-specific callbacks, we might want to associate context with each BDA.
    void bitmans_bda_context_reset(const esp_bd_addr_t *pbda);
    void *bitmans_bda_context_lookup(const esp_bd_addr_t *pbda);
    esp_err_t bitmans_bda_context_set(const esp_bd_addr_t *pbda, void *pContext);


#ifdef __cplusplus
}
#endif
