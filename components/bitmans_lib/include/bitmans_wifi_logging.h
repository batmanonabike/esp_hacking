#pragma once

#include "esp_wifi_types_generic.h"

#ifdef __cplusplus
extern "C" {
#endif

// Helper functions to log specific WiFi events
void bitmans_log_wifi_ready(void);
void bitmans_log_scan_done(wifi_event_sta_scan_done_t *event_data);
void bitmans_log_sta_start(void);
void bitmans_log_sta_stop(void);
void bitmans_log_sta_connected(wifi_event_sta_connected_t *event_data);
void bitmans_log_sta_disconnected(wifi_event_sta_disconnected_t *event_data);
void bitmans_log_sta_authmode_change(wifi_event_sta_authmode_change_t *event_data);
void bitmans_log_sta_wps_er_success(wifi_event_sta_wps_er_success_t *event_data);
void bitmans_log_sta_wps_er_failed(int reason);
void bitmans_log_sta_wps_er_timeout(void);
void bitmans_log_sta_wps_er_pin(wifi_event_sta_wps_er_pin_t *event_data);
void bitmans_log_sta_wps_er_pbc_overlap(void);
void bitmans_log_ap_start(void);
void bitmans_log_ap_stop(void);
void bitmans_log_ap_staconnected(wifi_event_ap_staconnected_t *event_data);
void bitmans_log_ap_stadisconnected(wifi_event_ap_stadisconnected_t *event_data);
void bitmans_log_ap_probereqrecved(wifi_event_ap_probe_req_rx_t *event_data);
void bitmans_log_ftm_report(wifi_event_ftm_report_t *event_data);
void bitmans_log_sta_bss_rssi_low(wifi_event_bss_rssi_low_t *event_data);
void bitmans_log_action_tx_status(wifi_event_action_tx_status_t *event_data);
void bitmans_log_roc_done(wifi_event_roc_done_t *event_data);
void bitmans_log_sta_beacon_timeout(void);
void bitmans_log_connectionless_module_wake_interval_start(void);
void bitmans_log_ap_wps_rg_success(wifi_event_ap_wps_rg_success_t *event_data);
void bitmans_log_ap_wps_rg_failed(wifi_event_ap_wps_rg_fail_reason_t *event_data);
void bitmans_log_ap_wps_rg_timeout(void);
void bitmans_log_ap_wps_rg_pin(wifi_event_ap_wps_rg_pin_t *event_data);
void bitmans_log_ap_wps_rg_pbc_overlap(void);
void bitmans_log_itwt_setup(void);
void bitmans_log_itwt_teardown(void);
void bitmans_log_itwt_probe(void);
void bitmans_log_itwt_suspend(void);
void bitmans_log_twt_wakeup(void);
void bitmans_log_btwt_setup(void);
void bitmans_log_btwt_teardown(void);
void bitmans_log_nan_started(void);
void bitmans_log_nan_stopped(void);
void bitmans_log_nan_svc_match(wifi_event_nan_svc_match_t *event_data);
void bitmans_log_nan_replied(wifi_event_nan_replied_t *event_data);
void bitmans_log_nan_receive(wifi_event_nan_receive_t *event_data);
void bitmans_log_ndp_indication(wifi_event_ndp_indication_t *event_data);
void bitmans_log_ndp_confirm(wifi_event_ndp_confirm_t *event_data);
void bitmans_log_ndp_terminated(wifi_event_ndp_terminated_t *event_data);
void bitmans_log_home_channel_change(wifi_event_home_channel_change_t *event_data);
void bitmans_log_sta_neighbor_rep(wifi_event_neighbor_report_t *event_data);
void bitmans_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void bitmans_unregister_wifi_eventlog_handler(void);
esp_err_t bitmans_register_wifi_eventlog_handler(void);
const char * bitmans_get_disconnect_reason(uint8_t reason);

#ifdef __cplusplus
}
#endif
