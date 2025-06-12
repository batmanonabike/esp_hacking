#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "bat_ble.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
    SUMMARY:
    - A BDA (Bluetooth Device Address) is a unique 6-byte address assigned to every Bluetooth device, used to identify devices in BLE operations.
    - This header defines two callback structures for BLE GAP events:
      * bat_gap_bda_callbacks_t: For events associated with a specific BDA (e.g., scan result, connection, security request).
      * bat_gap_global_callbacks_t: For global events not associated with a specific BDA (e.g., scan start/stop, scan param set).
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
    } bat_gattc_app_id_t;

#define ADVERTISED_NAME_BUFFER_LEN 32
    typedef struct
    {
        char name[ADVERTISED_NAME_BUFFER_LEN];
    } bat_advertised_name_t;

    // `GCC typeof` because the header files doesn\'t expose scan_rst structure directly.
    typedef typeof(((esp_ble_gap_cb_param_t *)0)->scan_rst) bat_scan_result_t;    
	
	esp_err_t bat_ble_client_init();
    esp_err_t bat_ble_client_deinit();
    esp_err_t bat_ble_register_gattc(bat_gattc_app_id_t);
    esp_err_t bat_ble_unregister_gattc(bat_gattc_app_id_t);

    esp_err_t bat_ble_client_set_scan_params(); // Effectively initiates scanning.
    esp_err_t bat_ble_start_scanning(uint32_t scan_duration_secs);
    esp_err_t bat_ble_client_stop_scanning();

    esp_err_t bat_ble_client_get_advertised_name(bat_scan_result_t *, bat_advertised_name_t *);
    bool bat_ble_advname_matches(bat_scan_result_t *, const char *pszName);
    bool bat_ble_client_find_service_uuid(bat_scan_result_t *pScanResult, bat_ble_uuid128_t * pId);
    
    // Callbacks from scanning and connection management. 
    typedef struct bat_gapc_callbacks_t
    {
        void *pContext;
        void (*on_scan_param_set_complete)(struct bat_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_scan_start_complete)(struct bat_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_scan_stop_complete)(struct bat_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);

        // BDA-specific callbacks can be added here if needed
        void (*on_scan_result)(struct bat_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_update_conn_params)(struct bat_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
        void (*on_sec_req)(struct bat_gapc_callbacks_t *, esp_ble_gap_cb_param_t *);
    } bat_gapc_callbacks_t;
    void bat_ble_gapc_callbacks_init(bat_gapc_callbacks_t *, void *pContext);

    // GAP BDA-specific callbacks, we might want to associate context with each BDA.
    void bat_bda_context_reset(const esp_bd_addr_t *pbda);
    void *bat_bda_context_lookup(const esp_bd_addr_t *pbda);
    esp_err_t bat_bda_context_set(const esp_bd_addr_t *pbda, void *pContext);


#ifdef __cplusplus
}
#endif
