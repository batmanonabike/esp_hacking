#include <inttypes.h>  
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi_types_generic.h"

#include "bitmans_wifi_logging.h"

static const char *TAG = "bitmans_lib:wifi_logging";

// Auth mode names for readable output
static const char* bitmans_get_auth_mode_name(wifi_auth_mode_t auth_mode) {
    switch (auth_mode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA_WPA2_PSK";
        case WIFI_AUTH_ENTERPRISE: return "ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2_WPA3_PSK";
        case WIFI_AUTH_WAPI_PSK: return "WAPI_PSK";
        case WIFI_AUTH_OWE: return "OWE";
        case WIFI_AUTH_WPA3_ENT_192: return "WPA3_ENT_192";
        case WIFI_AUTH_WPA3_EXT_PSK: return "WPA3_EXT_PSK";
        case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE: return "WPA3_EXT_PSK_MIXED";
        case WIFI_AUTH_DPP: return "DPP";
        default: return "UNKNOWN";
    }
}

// Convert binary MAC address to string
static char* bitmans_mac_to_str(const uint8_t* mac, char* mac_str) {
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return mac_str;
}

// Helper to print error reason codes for disconnection
const char* bitmans_get_disconnect_reason(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
        case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
        default: return "OTHER";
    }
}

void bitmans_log_wifi_ready(void) {
    ESP_LOGI(TAG, "WiFi ready");
}

void bitmans_log_scan_done(wifi_event_sta_scan_done_t *event_data) {
    ESP_LOGI(TAG, "Scan done: status=%"PRIu32", found %d APs, scan_id=%d", 
        event_data->status, event_data->number, event_data->scan_id);
}

void bitmans_log_sta_start(void) {
    ESP_LOGI(TAG, "Station started");
}

void bitmans_log_sta_stop(void) {
    ESP_LOGI(TAG, "Station stopped");
}

void bitmans_log_sta_connected(wifi_event_sta_connected_t *event_data) {
    char ssid[33] = {0};
    char bssid_str[18] = {0};
    
    memcpy(ssid, event_data->ssid, event_data->ssid_len);
    ssid[event_data->ssid_len] = '\0';
    
    ESP_LOGI(TAG, "Station connected to AP - SSID: %s, BSSID: %s, Channel: %d, Auth mode: %s, AID: %d",
        ssid, 
        bitmans_mac_to_str(event_data->bssid, bssid_str),
        event_data->channel,
        bitmans_get_auth_mode_name(event_data->authmode),
        event_data->aid);
}

void bitmans_log_sta_disconnected(wifi_event_sta_disconnected_t *event_data) {
    char ssid[33] = {0};
    char bssid_str[18] = {0};
    
    memcpy(ssid, event_data->ssid, event_data->ssid_len);
    ssid[event_data->ssid_len] = '\0';
    
    ESP_LOGI(TAG, "Station disconnected from AP - SSID: %s, BSSID: %s, Reason: %s (%d), RSSI: %d",
        ssid, 
        bitmans_mac_to_str(event_data->bssid, bssid_str),
        bitmans_get_disconnect_reason(event_data->reason),
        event_data->reason,
        event_data->rssi);
}

void bitmans_log_sta_authmode_change(wifi_event_sta_authmode_change_t *event_data) {
    ESP_LOGI(TAG, "Auth mode changed from %s to %s",
        bitmans_get_auth_mode_name(event_data->old_mode),
        bitmans_get_auth_mode_name(event_data->new_mode));
}

void bitmans_log_sta_wps_er_success(wifi_event_sta_wps_er_success_t *event_data) {
    ESP_LOGI(TAG, "WPS succeeded in enrollee mode, received %d AP credentials", event_data->ap_cred_cnt);
    
    for (int i = 0; i < event_data->ap_cred_cnt; i++) {
        ESP_LOGI(TAG, "  AP %d - SSID: %s", i + 1, event_data->ap_cred[i].ssid);
    }
}

void bitmans_log_sta_wps_er_failed(int reason) {
    const char* reason_str = "UNKNOWN";
    
    switch (reason) {
        case WPS_FAIL_REASON_NORMAL:
            reason_str = "NORMAL";
            break;
        case WPS_FAIL_REASON_RECV_M2D:
            reason_str = "RECEIVED_M2D";
            break;
        case WPS_FAIL_REASON_RECV_DEAUTH:
            reason_str = "RECEIVED_DEAUTH";
            break;
    }
    
    ESP_LOGI(TAG, "WPS failed in enrollee mode, reason: %s", reason_str);
}

void bitmans_log_sta_wps_er_timeout(void) {
    ESP_LOGI(TAG, "WPS timeout in enrollee mode");
}

void bitmans_log_sta_wps_er_pin(wifi_event_sta_wps_er_pin_t *event_data) {
    ESP_LOGI(TAG, "WPS PIN in enrollee mode: %c%c%c%c%c%c%c%c", 
        event_data->pin_code[0], event_data->pin_code[1], 
        event_data->pin_code[2], event_data->pin_code[3], 
        event_data->pin_code[4], event_data->pin_code[5], 
        event_data->pin_code[6], event_data->pin_code[7]);
}

void bitmans_log_sta_wps_er_pbc_overlap(void) {
    ESP_LOGI(TAG, "WPS PBC overlap in enrollee mode");
}

void bitmans_log_ap_start(void) {
    ESP_LOGI(TAG, "Soft-AP started");
}

void bitmans_log_ap_stop(void) {
    ESP_LOGI(TAG, "Soft-AP stopped");
}

void bitmans_log_ap_staconnected(wifi_event_ap_staconnected_t *event_data) {
    char mac_str[18] = {0};
    
    ESP_LOGI(TAG, "Station connected to Soft-AP - MAC: %s, AID: %d, Is mesh child: %s",
        bitmans_mac_to_str(event_data->mac, mac_str),
        event_data->aid,
        event_data->is_mesh_child ? "Yes" : "No");
}

void bitmans_log_ap_stadisconnected(wifi_event_ap_stadisconnected_t *event_data) {
    char mac_str[18] = {0};
    
    ESP_LOGI(TAG, "Station disconnected from Soft-AP - MAC: %s, AID: %d, Is mesh child: %s, Reason: %d",
        bitmans_mac_to_str(event_data->mac, mac_str),
        event_data->aid,
        event_data->is_mesh_child ? "Yes" : "No",
        event_data->reason);
}

void bitmans_log_ap_probereqrecved(wifi_event_ap_probe_req_rx_t *event_data) {
    char mac_str[18] = {0};
    
    ESP_LOGI(TAG, "Probe request received - RSSI: %d, MAC: %s",
        event_data->rssi,
        bitmans_mac_to_str(event_data->mac, mac_str));
}

void bitmans_log_ftm_report(wifi_event_ftm_report_t *event_data) {
    char mac_str[18] = {0};
    const char* status_str = "UNKNOWN";
    
    switch (event_data->status) {
        case FTM_STATUS_SUCCESS: status_str = "SUCCESS"; break;
        case FTM_STATUS_UNSUPPORTED: status_str = "UNSUPPORTED"; break;
        case FTM_STATUS_CONF_REJECTED: status_str = "CONFIG_REJECTED"; break;
        case FTM_STATUS_NO_RESPONSE: status_str = "NO_RESPONSE"; break;
        case FTM_STATUS_FAIL: status_str = "FAIL"; break;
        case FTM_STATUS_NO_VALID_MSMT: status_str = "NO_VALID_MEASUREMENT"; break;
        case FTM_STATUS_USER_TERM: status_str = "USER_TERMINATED"; break;
    }
    
    ESP_LOGI(TAG, "FTM report - Peer MAC: %s, Status: %s, RTT Raw: %"PRIu32" ns, RTT Est: %"PRIu32" ns, Distance Est: %"PRIu32" cm, Entries: %u",
        bitmans_mac_to_str(event_data->peer_mac, mac_str),
        status_str,
        event_data->rtt_raw,
        event_data->rtt_est,
        event_data->dist_est,
        event_data->ftm_report_num_entries);
}

void bitmans_log_sta_bss_rssi_low(wifi_event_bss_rssi_low_t *event_data) {
    ESP_LOGI(TAG, "BSS RSSI low - RSSI: %"PRId32"", event_data->rssi);
}

void bitmans_log_action_tx_status(wifi_event_action_tx_status_t *event_data) {
    char da_str[18] = {0};
    const char* if_str = (event_data->ifx == WIFI_IF_STA) ? "STA" : 
                         (event_data->ifx == WIFI_IF_AP) ? "AP" : "Unknown";
    
    ESP_LOGI(TAG, "Action TX status - Interface: %s, Context: %"PRIu32", DA: %s, Status: %u",
        if_str,
        event_data->context,
        bitmans_mac_to_str(event_data->da, da_str),
        event_data->status);
}

void bitmans_log_roc_done(wifi_event_roc_done_t *event_data) {
    ESP_LOGI(TAG, "ROC done - Context: %"PRIu32"", event_data->context);
}

void bitmans_log_sta_beacon_timeout(void) {
    ESP_LOGI(TAG, "Station beacon timeout");
}

void bitmans_log_connectionless_module_wake_interval_start(void) {
    ESP_LOGI(TAG, "Connectionless module wake interval start");
}

void bitmans_log_ap_wps_rg_success(wifi_event_ap_wps_rg_success_t *event_data) {
    char mac_str[18] = {0};
    
    ESP_LOGI(TAG, "AP WPS succeeded in registrar mode - Enrollee MAC: %s",
        bitmans_mac_to_str(event_data->peer_macaddr, mac_str));
}

void bitmans_log_ap_wps_rg_failed(wifi_event_ap_wps_rg_fail_reason_t *event_data) {
    char mac_str[18] = {0};
    const char* reason_str = "UNKNOWN";
    
    switch (event_data->reason) {
        case WPS_AP_FAIL_REASON_NORMAL:
            reason_str = "NORMAL";
            break;
        case WPS_AP_FAIL_REASON_CONFIG:
            reason_str = "CONFIG";
            break;
        case WPS_AP_FAIL_REASON_AUTH:
            reason_str = "AUTH";
            break;
        case WPS_AP_FAIL_REASON_MAX:
            reason_str = "MAX_REASON";
            break;
        default:
            reason_str = "UNKNOWN";
            break;
    }
    
    ESP_LOGI(TAG, "AP WPS failed in registrar mode - Reason: %s, Enrollee MAC: %s",
        reason_str,
        bitmans_mac_to_str(event_data->peer_macaddr, mac_str));
}

void bitmans_log_ap_wps_rg_timeout(void) {
    ESP_LOGI(TAG, "AP WPS timeout in registrar mode");
}

void bitmans_log_ap_wps_rg_pin(wifi_event_ap_wps_rg_pin_t *event_data) {
    ESP_LOGI(TAG, "AP WPS PIN in registrar mode: %c%c%c%c%c%c%c%c", 
        event_data->pin_code[0], event_data->pin_code[1], 
        event_data->pin_code[2], event_data->pin_code[3], 
        event_data->pin_code[4], event_data->pin_code[5], 
        event_data->pin_code[6], event_data->pin_code[7]);
}

void bitmans_log_ap_wps_rg_pbc_overlap(void) {
    ESP_LOGI(TAG, "AP WPS PBC overlap in registrar mode");
}

void bitmans_log_itwt_setup(void) {
    ESP_LOGI(TAG, "iTWT setup");
}

void bitmans_log_itwt_teardown(void) {
    ESP_LOGI(TAG, "iTWT teardown");
}

void bitmans_log_itwt_probe(void) {
    ESP_LOGI(TAG, "iTWT probe");
}

void bitmans_log_itwt_suspend(void) {
    ESP_LOGI(TAG, "iTWT suspend");
}

void bitmans_log_twt_wakeup(void) {
    ESP_LOGI(TAG, "TWT wakeup");
}

void bitmans_log_btwt_setup(void) {
    ESP_LOGI(TAG, "bTWT setup");
}

void bitmans_log_btwt_teardown(void) {
    ESP_LOGI(TAG, "bTWT teardown");
}

void bitmans_log_nan_started(void) {
    ESP_LOGI(TAG, "NAN Discovery started");
}

void bitmans_log_nan_stopped(void) {
    ESP_LOGI(TAG, "NAN Discovery stopped");
}

void bitmans_log_nan_svc_match(wifi_event_nan_svc_match_t *event_data) {
    char mac_str[18] = {0};
    
    ESP_LOGI(TAG, "NAN Service Discovery match - Subscribe ID: %u, Publish ID: %u, Publisher MAC: %s, Update Pub ID: %s",
        event_data->subscribe_id,
        event_data->publish_id,
        bitmans_mac_to_str(event_data->pub_if_mac, mac_str),
        event_data->update_pub_id ? "Yes" : "No");
}

void bitmans_log_nan_replied(wifi_event_nan_replied_t *event_data) {
    char mac_str[18] = {0};
    
    ESP_LOGI(TAG, "Replied to NAN peer - Publish ID: %u, Subscribe ID: %u, Subscriber MAC: %s",
        event_data->publish_id,
        event_data->subscribe_id,
        bitmans_mac_to_str(event_data->sub_if_mac, mac_str));
}

void bitmans_log_nan_receive(wifi_event_nan_receive_t *event_data) {
    char mac_str[18] = {0};
    
    ESP_LOGI(TAG, "Received NAN Follow-up - Instance ID: %u, Peer Instance ID: %u, Peer MAC: %s, Peer Service Info: %s",
        event_data->inst_id,
        event_data->peer_inst_id,
        bitmans_mac_to_str(event_data->peer_if_mac, mac_str),
        event_data->peer_svc_info);
}

void bitmans_log_ndp_indication(wifi_event_ndp_indication_t *event_data) {
    char nmi_str[18] = {0};
    char ndi_str[18] = {0};
    
    ESP_LOGI(TAG, "NDP Indication - Publish ID: %u, NDP ID: %u, Peer NMI: %s, Peer NDI: %s, Service Info: %s",
        event_data->publish_id,
        event_data->ndp_id,
        bitmans_mac_to_str(event_data->peer_nmi, nmi_str),
        bitmans_mac_to_str(event_data->peer_ndi, ndi_str),
        event_data->svc_info);
}

void bitmans_log_ndp_confirm(wifi_event_ndp_confirm_t *event_data) {
    char nmi_str[18] = {0};
    char ndi_str[18] = {0};
    char own_ndi_str[18] = {0};
    
    ESP_LOGI(TAG, "NDP Confirm - Status: %u, NDP ID: %u, Peer NMI: %s, Peer NDI: %s, Own NDI: %s, Service Info: %s",
        event_data->status,
        event_data->ndp_id,
        bitmans_mac_to_str(event_data->peer_nmi, nmi_str),
        bitmans_mac_to_str(event_data->peer_ndi, ndi_str),
        bitmans_mac_to_str(event_data->own_ndi, own_ndi_str),
        event_data->svc_info);
}

void bitmans_log_ndp_terminated(wifi_event_ndp_terminated_t *event_data) {
    char ndi_str[18] = {0};
    
    ESP_LOGI(TAG, "NDP Terminated - Reason: %u, NDP ID: %u, Initiator NDI: %s",
        event_data->reason,
        event_data->ndp_id,
        bitmans_mac_to_str(event_data->init_ndi, ndi_str));
}

void bitmans_log_home_channel_change(wifi_event_home_channel_change_t *event_data) {
    const char* bitmans_get_sec_chan_str(wifi_second_chan_t chan) {
        switch (chan) {
            case WIFI_SECOND_CHAN_NONE: return "NONE";
            case WIFI_SECOND_CHAN_ABOVE: return "ABOVE";
            case WIFI_SECOND_CHAN_BELOW: return "BELOW";
            default: return "UNKNOWN";
        }
    }
    
    ESP_LOGI(TAG, "Home channel changed - Old: %u+%s, New: %u+%s",
        event_data->old_chan,
        bitmans_get_sec_chan_str(event_data->old_snd),
        event_data->new_chan,
        bitmans_get_sec_chan_str(event_data->new_snd));
}

void bitmans_log_sta_neighbor_rep(wifi_event_neighbor_report_t *event_data) {
    ESP_LOGI(TAG, "Station Neighbor Report received - Report length: %u bytes", 
        event_data->report_len);
}

// Main event handler to process all WiFi events
void bitmans_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base != WIFI_EVENT) {
        return;
    }

    switch (event_id) {
        case WIFI_EVENT_WIFI_READY:
            bitmans_log_wifi_ready();
            break;
        case WIFI_EVENT_SCAN_DONE:
            bitmans_log_scan_done((wifi_event_sta_scan_done_t *)event_data);
            break;
        case WIFI_EVENT_STA_START:
            bitmans_log_sta_start();
            break;
        case WIFI_EVENT_STA_STOP:
            bitmans_log_sta_stop();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            bitmans_log_sta_connected((wifi_event_sta_connected_t *)event_data);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            bitmans_log_sta_disconnected((wifi_event_sta_disconnected_t *)event_data);
            break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            bitmans_log_sta_authmode_change((wifi_event_sta_authmode_change_t *)event_data);
            break;
        case WIFI_EVENT_STA_WPS_ER_SUCCESS:
            bitmans_log_sta_wps_er_success((wifi_event_sta_wps_er_success_t *)event_data);
            break;
        case WIFI_EVENT_STA_WPS_ER_FAILED:
            bitmans_log_sta_wps_er_failed(*(int *)event_data);
            break;
        case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
            bitmans_log_sta_wps_er_timeout();
            break;
        case WIFI_EVENT_STA_WPS_ER_PIN:
            bitmans_log_sta_wps_er_pin((wifi_event_sta_wps_er_pin_t *)event_data);
            break;
        case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP:
            bitmans_log_sta_wps_er_pbc_overlap();
            break;
        case WIFI_EVENT_AP_START:
            bitmans_log_ap_start();
            break;
        case WIFI_EVENT_AP_STOP:
            bitmans_log_ap_stop();
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            bitmans_log_ap_staconnected((wifi_event_ap_staconnected_t *)event_data);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            bitmans_log_ap_stadisconnected((wifi_event_ap_stadisconnected_t *)event_data);
            break;
        case WIFI_EVENT_AP_PROBEREQRECVED:
            bitmans_log_ap_probereqrecved((wifi_event_ap_probe_req_rx_t *)event_data);
            break;
        case WIFI_EVENT_FTM_REPORT:
            bitmans_log_ftm_report((wifi_event_ftm_report_t *)event_data);
            break;
        case WIFI_EVENT_STA_BSS_RSSI_LOW:
            bitmans_log_sta_bss_rssi_low((wifi_event_bss_rssi_low_t *)event_data);
            break;
        case WIFI_EVENT_ACTION_TX_STATUS:
            bitmans_log_action_tx_status((wifi_event_action_tx_status_t *)event_data);
            break;
        case WIFI_EVENT_ROC_DONE:
            bitmans_log_roc_done((wifi_event_roc_done_t *)event_data);
            break;
        case WIFI_EVENT_STA_BEACON_TIMEOUT:
            bitmans_log_sta_beacon_timeout();
            break;
        case WIFI_EVENT_CONNECTIONLESS_MODULE_WAKE_INTERVAL_START:
            bitmans_log_connectionless_module_wake_interval_start();
            break;
        case WIFI_EVENT_AP_WPS_RG_SUCCESS:
            bitmans_log_ap_wps_rg_success((wifi_event_ap_wps_rg_success_t *)event_data);
            break;
        case WIFI_EVENT_AP_WPS_RG_FAILED:
            bitmans_log_ap_wps_rg_failed((wifi_event_ap_wps_rg_fail_reason_t *)event_data);
            break;
        case WIFI_EVENT_AP_WPS_RG_TIMEOUT:
            bitmans_log_ap_wps_rg_timeout();
            break;
        case WIFI_EVENT_AP_WPS_RG_PIN:
            bitmans_log_ap_wps_rg_pin((wifi_event_ap_wps_rg_pin_t *)event_data);
            break;
        case WIFI_EVENT_AP_WPS_RG_PBC_OVERLAP:
            bitmans_log_ap_wps_rg_pbc_overlap();
            break;
        case WIFI_EVENT_ITWT_SETUP:
            bitmans_log_itwt_setup();
            break;
        case WIFI_EVENT_ITWT_TEARDOWN:
            bitmans_log_itwt_teardown();
            break;
        case WIFI_EVENT_ITWT_PROBE:
            bitmans_log_itwt_probe();
            break;
        case WIFI_EVENT_ITWT_SUSPEND:
            bitmans_log_itwt_suspend();
            break;
        case WIFI_EVENT_TWT_WAKEUP:
            bitmans_log_twt_wakeup();
            break;
        case WIFI_EVENT_BTWT_SETUP:
            bitmans_log_btwt_setup();
            break;
        case WIFI_EVENT_BTWT_TEARDOWN:
            bitmans_log_btwt_teardown();
            break;
        case WIFI_EVENT_NAN_STARTED:
            bitmans_log_nan_started();
            break;
        case WIFI_EVENT_NAN_STOPPED:
            bitmans_log_nan_stopped();
            break;
        case WIFI_EVENT_NAN_SVC_MATCH:
            bitmans_log_nan_svc_match((wifi_event_nan_svc_match_t *)event_data);
            break;
        case WIFI_EVENT_NAN_REPLIED:
            bitmans_log_nan_replied((wifi_event_nan_replied_t *)event_data);
            break;
        case WIFI_EVENT_NAN_RECEIVE:
            bitmans_log_nan_receive((wifi_event_nan_receive_t *)event_data);
            break;
        case WIFI_EVENT_NDP_INDICATION:
            bitmans_log_ndp_indication((wifi_event_ndp_indication_t *)event_data);
            break;
        case WIFI_EVENT_NDP_CONFIRM:
            bitmans_log_ndp_confirm((wifi_event_ndp_confirm_t *)event_data);
            break;
        case WIFI_EVENT_NDP_TERMINATED:
            bitmans_log_ndp_terminated((wifi_event_ndp_terminated_t *)event_data);
            break;
        case WIFI_EVENT_HOME_CHANNEL_CHANGE:
            bitmans_log_home_channel_change((wifi_event_home_channel_change_t *)event_data);
            break;        
        case WIFI_EVENT_STA_NEIGHBOR_REP:
            bitmans_log_sta_neighbor_rep((wifi_event_neighbor_report_t *)event_data);
            break;
        default:
            ESP_LOGI(TAG, "Unhandled WiFi event: %"PRId32"", event_id);
            break;
    }
}

// Store the event handler instance for proper cleanup later
static esp_event_handler_instance_t wifi_log_handler_instance = NULL;

esp_err_t bitmans_register_wifi_eventlog_handler(void) 
{
    if (wifi_log_handler_instance != NULL) 
        return ESP_OK;

    esp_err_t ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
        &bitmans_wifi_event_handler, NULL, &wifi_log_handler_instance);
    
    if (ret == ESP_OK) 
        ESP_LOGI(TAG, "Successfully registered WiFi event logging handler");
    else 
        ESP_LOGE(TAG, "Failed to register WiFi event logging handler: %s", esp_err_to_name(ret));

    return ret;
}

// Add a cleanup function for proper deregistration
void bitmans_unregister_wifi_eventlog_handler(void) 
{
    if (wifi_log_handler_instance != NULL) 
    {
        esp_err_t ret = esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_log_handler_instance);
            
        if (ret != ESP_OK) 
            ESP_LOGE(TAG, "Failed to unregister WiFi event logging handler: %s", esp_err_to_name(ret));
        else 
        {
            ESP_LOGI(TAG, "Successfully unregistered WiFi event logging handler");
            wifi_log_handler_instance = NULL;
        }
    }
}
