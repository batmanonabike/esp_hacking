#include "bitmans_bles.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "bitmans_bles";

// Event group bits for operation synchronization
#define BLES_OP_COMPLETE_BIT BIT0
#define BLES_OP_ERROR_BIT BIT1
#define BLES_STOP_COMPLETE_BIT BIT2

// Internal event types for task communication
typedef enum
{
    BITMANS_BLES_INTERNAL_EVENT_STATE_CHANGED,
    BITMANS_BLES_INTERNAL_EVENT_ERROR,
    BITMANS_BLES_INTERNAL_EVENT_OPERATION_COMPLETE,
    BITMANS_BLES_INTERNAL_EVENT_STOP_REQUESTED,
    BITMANS_BLES_INTERNAL_EVENT_USER_CALLBACK
} bitmans_bles_internal_event_type_t;

typedef struct
{
    bitmans_bles_internal_event_type_t type;
    union
    {
        bitmans_bles_state_t new_state;
        bitmans_bles_event_t user_event;
        struct
        {
            bitmans_bles_error_t error_code;
            esp_err_t esp_error;
            const char *description;
        } error;
    } data;
} bitmans_bles_internal_event_t;

// Global server instance (singleton pattern to minimize globals)
static bitmans_bles_server_t g_server = {0};
static bool g_server_initialized = false;

// Forward declarations
static void bitmans_bles_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void bitmans_bles_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static esp_err_t bitmans_bles_fsm_transition(bitmans_bles_state_t new_state);
static void bitmans_bles_server_task(void *pvParameters);
static esp_err_t bitmans_bles_send_user_event(bitmans_bles_event_type_t type, bitmans_bles_event_data_t *data);
static esp_err_t bitmans_bles_begin_stop_sequence(void);
static esp_err_t bitmans_bles_auto_advance(void);
static esp_err_t bitmans_bles_register_next_app(void);
static esp_err_t bitmans_bles_create_next_service(void);
static esp_err_t bitmans_bles_add_next_characteristic(void);
static esp_err_t bitmans_bles_add_next_descriptor(void);
static esp_err_t bitmans_bles_start_next_service(void);
static esp_err_t bitmans_bles_setup_advertising(void);
static const char *bitmans_bles_state_to_string(bitmans_bles_state_t state);
static const char *bitmans_bles_error_to_string(bitmans_bles_error_t error);

// Utility functions
static void bitmans_bles_set_error(bitmans_bles_error_t error_code, esp_err_t esp_error, const char *description)
{
    g_server.last_error = error_code;
    g_server.last_esp_error = esp_error;

    ESP_LOGE(TAG, "BLE Server Error: %s (ESP: %s)", description, esp_err_to_name(esp_error));

    // Send error event to user
    bitmans_bles_event_data_t event_data = {0};
    event_data.error.error_code = error_code;
    event_data.error.state = g_server.state;
    event_data.error.description = description;
    event_data.error.esp_error = esp_error;

    bitmans_bles_send_user_event(BITMANS_BLES_EVENT_ERROR, &event_data);

    // Transition to error state
    bitmans_bles_fsm_transition(BITMANS_BLES_STATE_ERROR);
}

static esp_err_t bitmans_bles_wait_for_operation(uint32_t timeout_ms)
{
    if (!g_server.operation_events)
    {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(
        g_server.operation_events,
        BLES_OP_COMPLETE_BIT | BLES_OP_ERROR_BIT,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for any bit
        pdMS_TO_TICKS(timeout_ms));

    if (bits & BLES_OP_ERROR_BIT)
    {
        return ESP_FAIL;
    }
    else if (bits & BLES_OP_COMPLETE_BIT)
    {
        return ESP_OK;
    }
    else
    {
        return ESP_ERR_TIMEOUT;
    }
}

static void bitmans_bles_signal_operation_complete(bool success)
{
    if (g_server.operation_events)
    {
        xEventGroupSetBits(g_server.operation_events,
                           success ? BLES_OP_COMPLETE_BIT : BLES_OP_ERROR_BIT);
    }
}

static esp_err_t bitmans_bles_send_user_event(bitmans_bles_event_type_t type, bitmans_bles_event_data_t *data)
{
    if (!g_server.event_queue)
    {
        return ESP_ERR_INVALID_STATE;
    }

    bitmans_bles_internal_event_t internal_event = {
        .type = BITMANS_BLES_INTERNAL_EVENT_USER_CALLBACK,
        .data.user_event = {
            .type = type,
            .user_context = g_server.callbacks.user_context}};

    if (data)
    {
        internal_event.data.user_event.data = *data;
    }

    BaseType_t result = xQueueSend(g_server.event_queue, &internal_event, pdMS_TO_TICKS(100));
    return (result == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}

// FSM state transition with validation
static esp_err_t bitmans_bles_fsm_transition(bitmans_bles_state_t new_state)
{
    bitmans_bles_state_t old_state = g_server.state;

    ESP_LOGD(TAG, "FSM Transition: %s -> %s",
             bitmans_bles_state_to_string(old_state),
             bitmans_bles_state_to_string(new_state));

    // Validate state transitions
    bool valid_transition = true;
    switch (old_state)
    {
    case BITMANS_BLES_STATE_IDLE:
        valid_transition = (new_state == BITMANS_BLES_STATE_INITIALIZING);
        break;
    case BITMANS_BLES_STATE_INITIALIZING:
        valid_transition = (new_state == BITMANS_BLES_STATE_READY || new_state == BITMANS_BLES_STATE_ERROR);
        break;
    case BITMANS_BLES_STATE_READY:
        valid_transition = (new_state == BITMANS_BLES_STATE_REGISTERING_APPS || new_state == BITMANS_BLES_STATE_ERROR);
        break;
    default:
        // Allow transitions to ERROR state from any state
        if (new_state == BITMANS_BLES_STATE_ERROR)
        {
            valid_transition = true;
        }
        break;
    }

    if (!valid_transition)
    {
        ESP_LOGE(TAG, "Invalid state transition: %s -> %s",
                 bitmans_bles_state_to_string(old_state),
                 bitmans_bles_state_to_string(new_state));
        return ESP_ERR_INVALID_STATE;
    }

    g_server.state = new_state;

    // Handle stop request in any state
    if (g_server.stop_requested && new_state != BITMANS_BLES_STATE_ERROR)
    {
        ESP_LOGI(TAG, "Stop requested, beginning stop sequence");
        g_server.stop_requested = false;
        return bitmans_bles_begin_stop_sequence();
    }

    // Auto-advance through states
    esp_err_t result = bitmans_bles_auto_advance();
    if (result != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INTERNAL, result, "Auto-advance failed");
        return result;
    }

    return ESP_OK;
}

static esp_err_t bitmans_bles_auto_advance(void)
{
    switch (g_server.state)
    {
    case BITMANS_BLES_STATE_READY:
        if (g_server.service_count > 0)
        {
            g_server.current_service_index = 0;
            return bitmans_bles_fsm_transition(BITMANS_BLES_STATE_REGISTERING_APPS);
        }
        break;

    case BITMANS_BLES_STATE_REGISTERING_APPS:
        return bitmans_bles_register_next_app();

    case BITMANS_BLES_STATE_CREATING_SERVICES:
        return bitmans_bles_create_next_service();

    case BITMANS_BLES_STATE_ADDING_CHARACTERISTICS:
        return bitmans_bles_add_next_characteristic();

    case BITMANS_BLES_STATE_ADDING_DESCRIPTORS:
        return bitmans_bles_add_next_descriptor();

    case BITMANS_BLES_STATE_STARTING_SERVICES:
        return bitmans_bles_start_next_service();

    case BITMANS_BLES_STATE_SETTING_ADV_DATA:
        return bitmans_bles_setup_advertising();

    case BITMANS_BLES_STATE_ADVERTISING:
        // Notify user that server is ready
        bitmans_bles_send_user_event(BITMANS_BLES_EVENT_SERVER_READY, NULL);
        break;

    default:
        break;
    }

    return ESP_OK;
}

// Service management functions
static esp_err_t bitmans_bles_register_next_app(void)
{
    if (g_server.current_service_index >= g_server.service_count)
    {
        // All apps registered, move to creating services
        g_server.current_service_index = 0;
        return bitmans_bles_fsm_transition(BITMANS_BLES_STATE_CREATING_SERVICES);
    }

    bitmans_bles_service_t *service = &g_server.services[g_server.current_service_index];
    service->state = BITMANS_BLES_SERVICE_STATE_REGISTERING;

    ESP_LOGI(TAG, "Registering app ID %d for service '%s'",
             service->definition.app_id, service->definition.name);

    esp_err_t ret = esp_ble_gatts_app_register(service->definition.app_id);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_APP_REGISTER_FAILED, ret, "Failed to register GATT app");
        return ret;
    }

    return ESP_OK;
}

static esp_err_t bitmans_bles_create_next_service(void)
{
    if (g_server.current_service_index >= g_server.service_count)
    {
        // All services created, move to adding characteristics
        g_server.current_service_index = 0;
        return bitmans_bles_fsm_transition(BITMANS_BLES_STATE_ADDING_CHARACTERISTICS);
    }

    bitmans_bles_service_t *service = &g_server.services[g_server.current_service_index];
    if (service->state != BITMANS_BLES_SERVICE_STATE_REGISTERED)
    {
        g_server.current_service_index++;
        return bitmans_bles_create_next_service();
    }

    service->state = BITMANS_BLES_SERVICE_STATE_CREATING;

    ESP_LOGI(TAG, "Creating service '%s'", service->definition.name);

    // Create ESP-IDF service ID structure
    esp_gatt_srvc_id_t service_id = {
        .is_primary = true,
        .id = {
            .inst_id = 0,
            .uuid = {
                .len = ESP_UUID_LEN_128,
                .uuid = {.uuid128 = {0}}}}};

    memcpy(service_id.id.uuid.uuid.uuid128, service->definition.uuid.uuid, 16);

    esp_err_t ret = esp_ble_gatts_create_service(service->gatts_if, &service_id,
                                                 (service->definition.characteristic_count * 3) + 4);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_SERVICE_CREATE_FAILED, ret, "Failed to create service");
        return ret;
    }

    return ESP_OK;
}

static esp_err_t bitmans_bles_add_next_characteristic(void)
{
    // Find next service that needs characteristics added
    while (g_server.current_service_index < g_server.service_count)
    {
        bitmans_bles_service_t *service = &g_server.services[g_server.current_service_index];

        if (service->state == BITMANS_BLES_SERVICE_STATE_CREATED)
        {
            service->state = BITMANS_BLES_SERVICE_STATE_ADDING_CHARS;
            service->current_char_index = 0;
        }

        if (service->state == BITMANS_BLES_SERVICE_STATE_ADDING_CHARS)
        {
            if (service->current_char_index < service->definition.characteristic_count)
            {
                // Add this characteristic
                bitmans_bles_char_def_t *char_def = &service->definition.characteristics[service->current_char_index];

                ESP_LOGI(TAG, "Adding characteristic '%s' to service '%s'",
                         char_def->name, service->definition.name);

                esp_bt_uuid_t char_uuid = {
                    .len = ESP_UUID_LEN_128,
                    .uuid = {.uuid128 = {0}}};
                memcpy(char_uuid.uuid.uuid128, char_def->uuid.uuid, 16);

                esp_err_t ret = esp_ble_gatts_add_char(service->service_handle, &char_uuid,
                                                       char_def->permissions, char_def->properties,
                                                       NULL, NULL);
                if (ret != ESP_OK)
                {
                    bitmans_bles_set_error(BITMANS_BLES_ERROR_CHAR_ADD_FAILED, ret, "Failed to add characteristic");
                    return ret;
                }

                return ESP_OK;
            }
            else
            {
                // This service is done with characteristics
                service->state = BITMANS_BLES_SERVICE_STATE_CHARS_ADDED;
                g_server.current_service_index++;
            }
        }
        else
        {
            g_server.current_service_index++;
        }
    }

    // All characteristics added, move to descriptors
    g_server.current_service_index = 0;
    return bitmans_bles_fsm_transition(BITMANS_BLES_STATE_ADDING_DESCRIPTORS);
}

static esp_err_t bitmans_bles_add_next_descriptor(void)
{
    // Find next characteristic that needs CCCD
    while (g_server.current_service_index < g_server.service_count)
    {
        bitmans_bles_service_t *service = &g_server.services[g_server.current_service_index];

        if (service->state == BITMANS_BLES_SERVICE_STATE_CHARS_ADDED)
        {
            service->state = BITMANS_BLES_SERVICE_STATE_ADDING_DESCRIPTORS;
            service->current_descr_index = 0;
        }

        if (service->state == BITMANS_BLES_SERVICE_STATE_ADDING_DESCRIPTORS)
        {
            while (service->current_descr_index < service->definition.characteristic_count)
            {
                bitmans_bles_char_def_t *char_def = &service->definition.characteristics[service->current_descr_index];

                if (char_def->add_cccd)
                {
                    ESP_LOGI(TAG, "Adding CCCD for characteristic '%s'", char_def->name);

                    esp_bt_uuid_t cccd_uuid = {
                        .len = ESP_UUID_LEN_16,
                        .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}};

                    esp_err_t ret = esp_ble_gatts_add_char_descr(service->service_handle, &cccd_uuid,
                                                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                                 NULL, NULL);
                    if (ret != ESP_OK)
                    {
                        bitmans_bles_set_error(BITMANS_BLES_ERROR_DESCRIPTOR_ADD_FAILED, ret, "Failed to add CCCD");
                        return ret;
                    }

                    service->current_descr_index++;
                    return ESP_OK;
                }
                service->current_descr_index++;
            }

            // This service is done with descriptors
            service->state = BITMANS_BLES_SERVICE_STATE_DESCRIPTORS_ADDED;
            g_server.current_service_index++;
        }
        else
        {
            g_server.current_service_index++;
        }
    }

    // All descriptors added, move to starting services
    g_server.current_service_index = 0;
    return bitmans_bles_fsm_transition(BITMANS_BLES_STATE_STARTING_SERVICES);
}

static esp_err_t bitmans_bles_start_next_service(void)
{
    // Find next service to start
    while (g_server.current_service_index < g_server.service_count)
    {
        bitmans_bles_service_t *service = &g_server.services[g_server.current_service_index];

        if (service->state == BITMANS_BLES_SERVICE_STATE_DESCRIPTORS_ADDED && service->definition.auto_start)
        {
            service->state = BITMANS_BLES_SERVICE_STATE_STARTING;

            ESP_LOGI(TAG, "Starting service '%s'", service->definition.name);

            esp_err_t ret = esp_ble_gatts_start_service(service->service_handle);
            if (ret != ESP_OK)
            {
                bitmans_bles_set_error(BITMANS_BLES_ERROR_SERVICE_START_FAILED, ret, "Failed to start service");
                return ret;
            }

            return ESP_OK;
        }

        g_server.current_service_index++;
    }

    // All services started, move to advertising setup
    return bitmans_bles_fsm_transition(BITMANS_BLES_STATE_SETTING_ADV_DATA);
}

static esp_err_t bitmans_bles_setup_advertising(void)
{
    ESP_LOGI(TAG, "Setting up advertising data");

    // Configure advertising data
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = false, // We'll put name in scan response
        .include_txpower = false,
        .min_interval = g_server.config.min_conn_interval,
        .max_interval = g_server.config.max_conn_interval,
        .appearance = g_server.config.appearance,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 0,        .p_service_uuid = NULL,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };

    // Add service UUIDs to advertising data with size validation
    static uint8_t service_uuids[16 * BITMANS_BLES_MAX_SERVICES];
    uint8_t uuid_count = 0;
    uint32_t estimated_packet_size = 3; // Flags (mandatory)

    // Calculate device name size if included
    if (adv_data.include_name && g_server.config.device_name) {
        estimated_packet_size += 2 + strlen(g_server.config.device_name);
    }

    // Calculate appearance size if included
    if (adv_data.appearance != 0) {
        estimated_packet_size += 4; // 2 byte header + 2 byte value
    }

    // Add service UUIDs while checking packet size limits
    for (int i = 0; i < g_server.service_count && uuid_count < BITMANS_BLES_MAX_SERVICES; i++)
    {
        if (g_server.services[i].definition.include_in_adv)
        {
            uint32_t new_size = estimated_packet_size + 2 + ((uuid_count + 1) * 16);
            
            if (new_size > 31) {
                ESP_LOGW(TAG, "Cannot fit service %d in advertising packet (would be %lu bytes, max 31)", 
                         i, new_size);
                ESP_LOGI(TAG, "Consider using scan response or 16-bit UUIDs for additional services");
                break; // Stop adding UUIDs to prevent packet overflow
            }
            
            memcpy(&service_uuids[uuid_count * 16], g_server.services[i].definition.uuid.uuid, 16);
            uuid_count++;
        }
    }

    if (uuid_count > 0)
    {
        adv_data.service_uuid_len = uuid_count * 16;
        adv_data.p_service_uuid = service_uuids;
        estimated_packet_size += 2 + (uuid_count * 16);
    }

    ESP_LOGI(TAG, "Advertising packet: %d services, estimated %lu bytes", uuid_count, estimated_packet_size);

    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_ADV_CONFIG_FAILED, ret, "Failed to configure advertising data");
        return ret;
    }

    // Configure scan response data with device name
    esp_ble_adv_data_t scan_rsp_data = {
        .set_scan_rsp = true,
        .include_name = true,
        .include_txpower = false,
        .appearance = 0,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 0,
        .p_service_uuid = NULL,
        .flag = 0,
    };

    ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_ADV_CONFIG_FAILED, ret, "Failed to configure scan response data");
        return ret;
    }

    return ESP_OK;
}

// GATTS event handler
static void bitmans_bles_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGD(TAG, "GATTS event: %d, gatts_if: %d", event, gatts_if);

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "GATTS_REG_EVT, app_id: %d, status: %d", param->reg.app_id, param->reg.status);

        if (param->reg.status == ESP_GATT_OK)
        {
            // Find service with this app_id
            for (int i = 0; i < g_server.service_count; i++)
            {
                if (g_server.services[i].definition.app_id == param->reg.app_id)
                {
                    g_server.services[i].gatts_if = gatts_if;
                    g_server.services[i].state = BITMANS_BLES_SERVICE_STATE_REGISTERED;

                    // Add to hash table for quick lookup
                    bitmans_hash_table_set(&g_server.gatts_if_to_service, gatts_if, &g_server.services[i]);

                    ESP_LOGI(TAG, "Service '%s' registered with gatts_if: %d",
                             g_server.services[i].definition.name, gatts_if);
                    break;
                }
            }

            // Continue with next app registration
            g_server.current_service_index++;
            bitmans_bles_register_next_app();
        }
        else
        {
            bitmans_bles_set_error(BITMANS_BLES_ERROR_APP_REGISTER_FAILED, param->reg.status, "GATTS app registration failed");
        }
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "GATTS_CREATE_EVT, status: %d, service_handle: %d", param->create.status, param->create.service_handle);

        if (param->create.status == ESP_GATT_OK)
        {
            // Find service with this gatts_if
            bitmans_bles_service_t *service = (bitmans_bles_service_t *)bitmans_hash_table_try_get(&g_server.gatts_if_to_service, gatts_if);
            if (service)
            {
                service->service_handle = param->create.service_handle;
                service->state = BITMANS_BLES_SERVICE_STATE_CREATED;

                ESP_LOGI(TAG, "Service '%s' created with handle: %d", service->definition.name, service->service_handle);

                // Send service ready event
                bitmans_bles_event_data_t event_data = {0};
                event_data.service_ready.service = service;
                bitmans_bles_send_user_event(BITMANS_BLES_EVENT_SERVICE_READY, &event_data);
            }

            // Continue with next service creation
            g_server.current_service_index++;
            bitmans_bles_create_next_service();
        }
        else
        {
            bitmans_bles_set_error(BITMANS_BLES_ERROR_SERVICE_CREATE_FAILED, param->create.status, "Service creation failed");
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "GATTS_ADD_CHAR_EVT, status: %d, attr_handle: %d", param->add_char.status, param->add_char.attr_handle);

        if (param->add_char.status == ESP_GATT_OK)
        {
            // Find service with this gatts_if
            bitmans_bles_service_t *service = (bitmans_bles_service_t *)bitmans_hash_table_try_get(&g_server.gatts_if_to_service, gatts_if);
            if (service && service->current_char_index < service->definition.characteristic_count)
            {
                // Store characteristic handle
                bitmans_bles_characteristic_t *characteristic = &service->characteristics[service->current_char_index];
                characteristic->handle = param->add_char.attr_handle;

                // Add to hash table for quick lookup
                bitmans_hash_table_set(&g_server.handle_to_char, param->add_char.attr_handle, characteristic);

                ESP_LOGI(TAG, "Characteristic '%s' added with handle: %d",
                         characteristic->definition.name, characteristic->handle);

                service->current_char_index++;
            }

            // Continue adding characteristics
            bitmans_bles_add_next_characteristic();
        }
        else
        {
            bitmans_bles_set_error(BITMANS_BLES_ERROR_CHAR_ADD_FAILED, param->add_char.status, "Characteristic add failed");
        }
        break;

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(TAG, "GATTS_ADD_CHAR_DESCR_EVT, status: %d, attr_handle: %d", param->add_char_descr.status, param->add_char_descr.attr_handle);

        if (param->add_char_descr.status == ESP_GATT_OK)
        {
            // Find service and store CCCD handle
            bitmans_bles_service_t *service = (bitmans_bles_service_t *)bitmans_hash_table_try_get(&g_server.gatts_if_to_service, gatts_if);
            if (service && service->current_descr_index > 0)
            {
                bitmans_bles_characteristic_t *characteristic = &service->characteristics[service->current_descr_index - 1];
                characteristic->cccd_handle = param->add_char_descr.attr_handle;

                ESP_LOGI(TAG, "CCCD added for characteristic '%s' with handle: %d",
                         characteristic->definition.name, characteristic->cccd_handle);
            }

            // Continue adding descriptors
            bitmans_bles_add_next_descriptor();
        }
        else
        {
            bitmans_bles_set_error(BITMANS_BLES_ERROR_DESCRIPTOR_ADD_FAILED, param->add_char_descr.status, "Descriptor add failed");
        }
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "GATTS_START_EVT, status: %d, service_handle: %d", param->start.status, param->start.service_handle);

        if (param->start.status == ESP_GATT_OK)
        {
            // Find service and mark as started
            bitmans_bles_service_t *service = (bitmans_bles_service_t *)bitmans_hash_table_try_get(&g_server.gatts_if_to_service, gatts_if);
            if (service)
            {
                service->state = BITMANS_BLES_SERVICE_STATE_STARTED;
                ESP_LOGI(TAG, "Service '%s' started", service->definition.name);
            }

            // Continue starting services
            g_server.current_service_index++;
            bitmans_bles_start_next_service();
        }
        else
        {
            bitmans_bles_set_error(BITMANS_BLES_ERROR_SERVICE_START_FAILED, param->start.status, "Service start failed");
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "GATTS_CONNECT_EVT, conn_id: %d", param->connect.conn_id);

        g_server.conn_id = param->connect.conn_id;
        memcpy(g_server.remote_bda, param->connect.remote_bda, ESP_BD_ADDR_LEN);
        g_server.is_connected = true;

        // Stop advertising when connected
        if (g_server.advertising_enabled)
        {
            esp_ble_gap_stop_advertising();
        }

        // Send connected event
        bitmans_bles_event_data_t event_data = {0};
        event_data.connected.conn_id = param->connect.conn_id;
        memcpy(event_data.connected.remote_bda, param->connect.remote_bda, ESP_BD_ADDR_LEN);
        bitmans_bles_send_user_event(BITMANS_BLES_EVENT_CLIENT_CONNECTED, &event_data);

        bitmans_bles_fsm_transition(BITMANS_BLES_STATE_CONNECTED);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "GATTS_DISCONNECT_EVT, conn_id: %d, reason: %d", param->disconnect.conn_id, param->disconnect.reason);

        // Send disconnected event
        bitmans_bles_event_data_t event_data = {0};
        event_data.disconnected.conn_id = param->disconnect.conn_id;
        memcpy(event_data.disconnected.remote_bda, g_server.remote_bda, ESP_BD_ADDR_LEN);
        event_data.disconnected.reason = param->disconnect.reason;
        bitmans_bles_send_user_event(BITMANS_BLES_EVENT_CLIENT_DISCONNECTED, &event_data);

        g_server.is_connected = false;
        g_server.conn_id = 0;
        memset(g_server.remote_bda, 0, ESP_BD_ADDR_LEN);

        // Restart advertising
        bitmans_bles_fsm_transition(BITMANS_BLES_STATE_ADVERTISING);
        esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
            .adv_int_min = g_server.config.adv_int_min,
            .adv_int_max = g_server.config.adv_int_max,
            .adv_type = ADV_TYPE_IND,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL,
            .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        });
        break;

    case ESP_GATTS_READ_EVT:
    {
        ESP_LOGI(TAG, "GATTS_READ_EVT, conn_id: %d, trans_id: %d, handle: %d",
                 param->read.conn_id, param->read.trans_id, param->read.handle);

        // Find characteristic
        bitmans_bles_characteristic_t *characteristic = (bitmans_bles_characteristic_t *)bitmans_hash_table_try_get(&g_server.handle_to_char, param->read.handle);
        if (characteristic)
        {
            // Send read request event to user
            bitmans_bles_event_data_t event_data = {0};
            event_data.read_request.service = characteristic->service;
            event_data.read_request.characteristic = characteristic;
            event_data.read_request.conn_id = param->read.conn_id;
            event_data.read_request.trans_id = param->read.trans_id;
            event_data.read_request.offset = param->read.offset;
            bitmans_bles_send_user_event(BITMANS_BLES_EVENT_READ_REQUEST, &event_data);
        }
        else
        {
            // Send error response
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                        ESP_GATT_READ_NOT_PERMIT, NULL);
        }
        break;
    }

    case ESP_GATTS_WRITE_EVT:
    {
        ESP_LOGI(TAG, "GATTS_WRITE_EVT, conn_id: %d, trans_id: %d, handle: %d, len: %d",
                 param->write.conn_id, param->write.trans_id, param->write.handle, param->write.len);

        // Find characteristic
        bitmans_bles_characteristic_t *characteristic = (bitmans_bles_characteristic_t *)bitmans_hash_table_try_get(&g_server.handle_to_char, param->write.handle);
        if (characteristic)
        {
            // Check if this is a CCCD write
            if (param->write.handle == characteristic->cccd_handle)
            {
                // CCCD write - handle notifications enable/disable
                bool notifications_enabled = (param->write.len >= 2 && (param->write.value[0] & 0x01));

                bitmans_bles_event_data_t event_data = {0};
                event_data.notify.service = characteristic->service;
                event_data.notify.characteristic = characteristic;
                event_data.notify.conn_id = param->write.conn_id;
                event_data.notify.enabled = notifications_enabled;

                bitmans_bles_send_user_event(notifications_enabled ? BITMANS_BLES_EVENT_NOTIFY_ENABLED : BITMANS_BLES_EVENT_NOTIFY_DISABLED, &event_data);
            }
            else
            {
                // Regular characteristic write
                bitmans_bles_event_data_t event_data = {0};
                event_data.write_request.service = characteristic->service;
                event_data.write_request.characteristic = characteristic;
                event_data.write_request.conn_id = param->write.conn_id;
                event_data.write_request.trans_id = param->write.trans_id;
                event_data.write_request.offset = param->write.offset;
                event_data.write_request.data = param->write.value;
                event_data.write_request.len = param->write.len;
                event_data.write_request.need_rsp = param->write.need_rsp;
                bitmans_bles_send_user_event(BITMANS_BLES_EVENT_WRITE_REQUEST, &event_data);
            }

            // Send response if needed
            if (param->write.need_rsp)
            {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                            ESP_GATT_OK, NULL);
            }
        }
        else
        {
            // Send error response
            if (param->write.need_rsp)
            {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                            ESP_GATT_WRITE_NOT_PERMIT, NULL);
            }
        }
        break;
    }

    default:
        ESP_LOGD(TAG, "Unhandled GATTS event: %d", event);
        break;
    }
}

// GAP event handler
static void bitmans_bles_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGD(TAG, "GAP event: %d", event);

    switch (event)
    {
    case ESP_BLE_GAP_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "ADV_DATA_SET_COMPLETE_EVT, status: %d", param->adv_data_cmpl.status);

        if (param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            bitmans_bles_signal_operation_complete(true);
        }
        else
        {
            bitmans_bles_set_error(BITMANS_BLES_ERROR_ADV_CONFIG_FAILED, param->adv_data_cmpl.status, "Advertising data setup failed");
            bitmans_bles_signal_operation_complete(false);
        }
        break;

    case ESP_BLE_GAP_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "SCAN_RSP_DATA_SET_COMPLETE_EVT, status: %d", param->scan_rsp_data_cmpl.status);

        if (param->scan_rsp_data_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            // Both adv data and scan response are set, start advertising
            esp_ble_adv_params_t adv_params = {
                .adv_int_min = g_server.config.adv_int_min,
                .adv_int_max = g_server.config.adv_int_max,
                .adv_type = ADV_TYPE_IND,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            };

            esp_err_t ret = esp_ble_gap_start_advertising(&adv_params);
            if (ret != ESP_OK)
            {
                bitmans_bles_set_error(BITMANS_BLES_ERROR_ADV_START_FAILED, ret, "Failed to start advertising");
            }
        }
        else
        {
            bitmans_bles_set_error(BITMANS_BLES_ERROR_ADV_CONFIG_FAILED, param->scan_rsp_data_cmpl.status, "Scan response data setup failed");
        }
        break;

    case ESP_BLE_GAP_ADV_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "ADV_START_COMPLETE_EVT, status: %d", param->adv_start_cmpl.status);

        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            g_server.advertising_enabled = true;
            bitmans_bles_fsm_transition(BITMANS_BLES_STATE_ADVERTISING);
            bitmans_bles_send_user_event(BITMANS_BLES_EVENT_ADVERTISING_STARTED, NULL);
        }
        else
        {
            bitmans_bles_set_error(BITMANS_BLES_ERROR_ADV_START_FAILED, param->adv_start_cmpl.status, "Advertising start failed");
        }
        break;

    case ESP_BLE_GAP_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "ADV_STOP_COMPLETE_EVT, status: %d", param->adv_stop_cmpl.status);

        g_server.advertising_enabled = false;
        bitmans_bles_send_user_event(BITMANS_BLES_EVENT_ADVERTISING_STOPPED, NULL);
        bitmans_bles_signal_operation_complete(param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS);
        break;

    default:
        ESP_LOGD(TAG, "Unhandled GAP event: %d", event);
        break;
    }
}

// Server task for handling user callbacks and periodic work
static void bitmans_bles_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "BLE Server task started");

    bitmans_bles_internal_event_t internal_event;
    TickType_t periodic_ticks = portMAX_DELAY;
    TickType_t last_wake_time = xTaskGetTickCount();

    if (g_server.callbacks.periodic_interval_ms > 0)
    {
        periodic_ticks = pdMS_TO_TICKS(g_server.callbacks.periodic_interval_ms);
    }

    while (!g_server.stop_requested)
    {
        // Wait for events or periodic timeout
        if (xQueueReceive(g_server.event_queue, &internal_event, periodic_ticks) == pdTRUE)
        {
            switch (internal_event.type)
            {
            case BITMANS_BLES_INTERNAL_EVENT_USER_CALLBACK:
                if (g_server.callbacks.event_callback)
                {
                    g_server.callbacks.event_callback(&internal_event.data.user_event);
                }
                break;

            case BITMANS_BLES_INTERNAL_EVENT_STOP_REQUESTED:
                ESP_LOGI(TAG, "Stop requested in server task");
                goto task_exit;

            default:
                ESP_LOGD(TAG, "Unhandled internal event: %d", internal_event.type);
                break;
            }
        }

        // Handle periodic callback
        if (g_server.callbacks.periodic_callback &&
            g_server.callbacks.periodic_interval_ms > 0 &&
            (g_server.state == BITMANS_BLES_STATE_ADVERTISING || g_server.state == BITMANS_BLES_STATE_CONNECTED))
        {

            vTaskDelayUntil(&last_wake_time, periodic_ticks);
            g_server.callbacks.periodic_callback(g_server.callbacks.user_context);
        }
    }

task_exit:
    ESP_LOGI(TAG, "BLE Server task exiting");
    if (g_server.operation_events)
    {
        xEventGroupSetBits(g_server.operation_events, BLES_STOP_COMPLETE_BIT);
    }
    vTaskDelete(NULL);
}

// Stop sequence implementation
static esp_err_t bitmans_bles_begin_stop_sequence(void)
{
    ESP_LOGI(TAG, "Beginning stop sequence from state %s", bitmans_bles_state_to_string(g_server.state));

    // Stop advertising first
    if (g_server.advertising_enabled)
    {
        esp_ble_gap_stop_advertising();
        bitmans_bles_fsm_transition(BITMANS_BLES_STATE_STOPPING_ADVERTISING);
        return ESP_OK;
    }

    // Stop services
    if (g_server.state >= BITMANS_BLES_STATE_STARTING_SERVICES)
    {
        for (int i = 0; i < g_server.service_count; i++)
        {
            if (g_server.services[i].state == BITMANS_BLES_SERVICE_STATE_STARTED)
            {
                g_server.services[i].state = BITMANS_BLES_SERVICE_STATE_STOPPING;
                esp_ble_gatts_stop_service(g_server.services[i].service_handle);
            }
        }
        bitmans_bles_fsm_transition(BITMANS_BLES_STATE_STOPPING_SERVICES);
        return ESP_OK;
    }

    // Delete services
    if (g_server.state >= BITMANS_BLES_STATE_CREATING_SERVICES)
    {
        for (int i = 0; i < g_server.service_count; i++)
        {
            if (g_server.services[i].service_handle != 0)
            {
                g_server.services[i].state = BITMANS_BLES_SERVICE_STATE_DELETING;
                esp_ble_gatts_delete_service(g_server.services[i].service_handle);
            }
        }
        bitmans_bles_fsm_transition(BITMANS_BLES_STATE_DELETING_SERVICES);
        return ESP_OK;
    }

    // Unregister apps
    if (g_server.state >= BITMANS_BLES_STATE_REGISTERING_APPS)
    {
        for (int i = 0; i < g_server.service_count; i++)
        {
            if (g_server.services[i].gatts_if != ESP_GATT_IF_NONE)
            {
                esp_ble_gatts_app_unregister(g_server.services[i].gatts_if);
            }
        }
        bitmans_bles_fsm_transition(BITMANS_BLES_STATE_UNREGISTERING_APPS);
        return ESP_OK;
    }

    // Already stopped or in error state
    return ESP_OK;
}

// String conversion functions
static const char *bitmans_bles_state_to_string(bitmans_bles_state_t state)
{
    switch (state)
    {
    case BITMANS_BLES_STATE_IDLE:
        return "IDLE";
    case BITMANS_BLES_STATE_INITIALIZING:
        return "INITIALIZING";
    case BITMANS_BLES_STATE_READY:
        return "READY";
    case BITMANS_BLES_STATE_REGISTERING_APPS:
        return "REGISTERING_APPS";
    case BITMANS_BLES_STATE_CREATING_SERVICES:
        return "CREATING_SERVICES";
    case BITMANS_BLES_STATE_ADDING_CHARACTERISTICS:
        return "ADDING_CHARACTERISTICS";
    case BITMANS_BLES_STATE_ADDING_DESCRIPTORS:
        return "ADDING_DESCRIPTORS";
    case BITMANS_BLES_STATE_STARTING_SERVICES:
        return "STARTING_SERVICES";
    case BITMANS_BLES_STATE_SETTING_ADV_DATA:
        return "SETTING_ADV_DATA";
    case BITMANS_BLES_STATE_ADVERTISING:
        return "ADVERTISING";
    case BITMANS_BLES_STATE_CONNECTED:
        return "CONNECTED";
    case BITMANS_BLES_STATE_STOPPING_ADVERTISING:
        return "STOPPING_ADVERTISING";
    case BITMANS_BLES_STATE_STOPPING_SERVICES:
        return "STOPPING_SERVICES";
    case BITMANS_BLES_STATE_DELETING_SERVICES:
        return "DELETING_SERVICES";
    case BITMANS_BLES_STATE_UNREGISTERING_APPS:
        return "UNREGISTERING_APPS";
    case BITMANS_BLES_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

static const char *bitmans_bles_error_to_string(bitmans_bles_error_t error)
{
    switch (error)
    {
    case BITMANS_BLES_ERROR_NONE:
        return "NONE";
    case BITMANS_BLES_ERROR_INIT_FAILED:
        return "INIT_FAILED";
    case BITMANS_BLES_ERROR_APP_REGISTER_FAILED:
        return "APP_REGISTER_FAILED";
    case BITMANS_BLES_ERROR_SERVICE_CREATE_FAILED:
        return "SERVICE_CREATE_FAILED";
    case BITMANS_BLES_ERROR_CHAR_ADD_FAILED:
        return "CHAR_ADD_FAILED";
    case BITMANS_BLES_ERROR_DESCRIPTOR_ADD_FAILED:
        return "DESCRIPTOR_ADD_FAILED";
    case BITMANS_BLES_ERROR_SERVICE_START_FAILED:
        return "SERVICE_START_FAILED";
    case BITMANS_BLES_ERROR_ADV_CONFIG_FAILED:
        return "ADV_CONFIG_FAILED";
    case BITMANS_BLES_ERROR_ADV_START_FAILED:
        return "ADV_START_FAILED";
    case BITMANS_BLES_ERROR_TIMEOUT:
        return "TIMEOUT";
    case BITMANS_BLES_ERROR_INVALID_STATE:
        return "INVALID_STATE";
    case BITMANS_BLES_ERROR_NO_MEMORY:
        return "NO_MEMORY";
    case BITMANS_BLES_ERROR_INTERNAL:
        return "INTERNAL";
    default:
        return "UNKNOWN";
    }
}

// Public API Implementation

esp_err_t bitmans_bles_init(const bitmans_bles_config_t *config)
{
    if (g_server_initialized)
    {
        ESP_LOGW(TAG, "BLE Server already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config)
    {
        ESP_LOGE(TAG, "Config cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing BLE Server");

    // Clear server structure
    memset(&g_server, 0, sizeof(g_server));

    // Copy configuration
    g_server.config = *config;
    g_server.state = BITMANS_BLES_STATE_IDLE;
    g_server.last_error = BITMANS_BLES_ERROR_NONE;

    // Initialize BLE stack
    esp_err_t ret = esp_bt_controller_init(&(esp_bt_controller_config_t)BT_CONTROLLER_INIT_CONFIG_DEFAULT());
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "BT controller init failed");
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "BT controller enable failed");
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "Bluedroid init failed");
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "Bluedroid enable failed");
        return ret;
    }

    // Set device name
    ret = esp_ble_gap_set_device_name(config->device_name);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "Set device name failed");
        return ret;
    }

    // Register event handlers
    ret = esp_ble_gatts_register_callback(bitmans_bles_gatts_event_handler);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "GATTS callback register failed");
        return ret;
    }

    ret = esp_ble_gap_register_callback(bitmans_bles_gap_event_handler);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "GAP callback register failed");
        return ret;
    }

    // Initialize hash tables
    ret = bitmans_hash_table_init(&g_server.gatts_if_to_service, 16, NULL, NULL);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "Hash table init failed");
        return ret;
    }

    ret = bitmans_hash_table_init(&g_server.handle_to_char, 64, NULL, NULL);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ret, "Hash table init failed");
        return ret;
    }

    // Create event queue
    g_server.event_queue = xQueueCreate(config->event_queue_size, sizeof(bitmans_bles_internal_event_t));
    if (!g_server.event_queue)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ESP_ERR_NO_MEM, "Event queue creation failed");
        return ESP_ERR_NO_MEM;
    }

    // Create event group for operation synchronization
    g_server.operation_events = xEventGroupCreate();
    if (!g_server.operation_events)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_INIT_FAILED, ESP_ERR_NO_MEM, "Event group creation failed");
        return ESP_ERR_NO_MEM;
    }

    g_server_initialized = true;
    bitmans_bles_fsm_transition(BITMANS_BLES_STATE_READY);

    ESP_LOGI(TAG, "BLE Server initialized successfully");
    return ESP_OK;
}

esp_err_t bitmans_bles_set_callbacks(const bitmans_bles_callbacks_t *callbacks)
{
    if (!g_server_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (callbacks)
    {
        g_server.callbacks = *callbacks;

        // Create server task if not already created
        if (!g_server.server_task_handle)
        {
            BaseType_t result = xTaskCreate(
                bitmans_bles_server_task,
                "bles_server",
                g_server.config.task_stack_size,
                NULL,
                g_server.config.task_priority,
                &g_server.server_task_handle);

            if (result != pdPASS)
            {
                return ESP_ERR_NO_MEM;
            }
        }
    }

    return ESP_OK;
}

esp_err_t bitmans_bles_add_service(const bitmans_bles_service_def_t *service_def)
{
    if (!g_server_initialized || !service_def)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_server.state != BITMANS_BLES_STATE_READY)
    {
        ESP_LOGE(TAG, "Cannot add service in state %s", bitmans_bles_state_to_string(g_server.state));
        return ESP_ERR_INVALID_STATE;
    }

    if (g_server.service_count >= BITMANS_BLES_MAX_SERVICES)
    {
        ESP_LOGE(TAG, "Maximum services (%d) exceeded", BITMANS_BLES_MAX_SERVICES);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Adding service '%s' with app_id %d", service_def->name, service_def->app_id);

    // Allocate service structure
    bitmans_bles_service_t *service = &g_server.services[g_server.service_count];
    service->definition = *service_def;
    service->state = BITMANS_BLES_SERVICE_STATE_DEFINED;
    service->gatts_if = ESP_GATT_IF_NONE;
    service->service_handle = 0;
    service->current_char_index = 0;
    service->current_descr_index = 0;
    service->server = &g_server;

    // Allocate characteristics array
    if (service_def->characteristic_count > 0)
    {
        service->characteristics = malloc(service_def->characteristic_count * sizeof(bitmans_bles_characteristic_t));
        if (!service->characteristics)
        {
            return ESP_ERR_NO_MEM;
        }

        // Initialize characteristics
        for (int i = 0; i < service_def->characteristic_count; i++)
        {
            service->characteristics[i].definition = service_def->characteristics[i];
            service->characteristics[i].handle = 0;
            service->characteristics[i].cccd_handle = 0;
            service->characteristics[i].service = service;
        }
    }

    g_server.service_count++;
    ESP_LOGI(TAG, "Service '%s' added successfully (%d/%d)", service_def->name, g_server.service_count, BITMANS_BLES_MAX_SERVICES);

    return ESP_OK;
}

esp_err_t bitmans_bles_start(void)
{
    if (!g_server_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_server.state != BITMANS_BLES_STATE_READY)
    {
        ESP_LOGE(TAG, "Cannot start in state %s", bitmans_bles_state_to_string(g_server.state));
        return ESP_ERR_INVALID_STATE;
    }

    if (g_server.service_count == 0)
    {
        ESP_LOGE(TAG, "No services defined");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting BLE Server with %d services", g_server.service_count);

    // Allocate services array if not already done
    if (!g_server.services)
    {
        ESP_LOGE(TAG, "Services array not allocated");
        return ESP_ERR_INVALID_STATE;
    }

    // Start the FSM process
    g_server.current_service_index = 0;
    return bitmans_bles_fsm_transition(BITMANS_BLES_STATE_REGISTERING_APPS);
}

esp_err_t bitmans_bles_stop(uint32_t timeout_ms)
{
    if (!g_server_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping BLE Server with timeout %dms", timeout_ms);

    g_server.stop_requested = true;

    // Signal server task to stop
    if (g_server.event_queue)
    {
        bitmans_bles_internal_event_t stop_event = {
            .type = BITMANS_BLES_INTERNAL_EVENT_STOP_REQUESTED};
        xQueueSend(g_server.event_queue, &stop_event, pdMS_TO_TICKS(100));
    }

    // Begin stop sequence if not already in progress
    if (g_server.state != BITMANS_BLES_STATE_ERROR)
    {
        bitmans_bles_begin_stop_sequence();
    }

    // Wait for stop completion
    if (g_server.operation_events)
    {
        EventBits_t bits = xEventGroupWaitBits(
            g_server.operation_events,
            BLES_STOP_COMPLETE_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(timeout_ms));

        if (!(bits & BLES_STOP_COMPLETE_BIT))
        {
            ESP_LOGW(TAG, "Stop timeout after %dms", timeout_ms);
            return ESP_ERR_TIMEOUT;
        }
    }

    ESP_LOGI(TAG, "BLE Server stopped successfully");
    return ESP_OK;
}

esp_err_t bitmans_bles_start_advertising(void)
{
    if (!g_server_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_server.state < BITMANS_BLES_STATE_SETTING_ADV_DATA)
    {
        ESP_LOGE(TAG, "Cannot start advertising in state %s", bitmans_bles_state_to_string(g_server.state));
        return ESP_ERR_INVALID_STATE;
    }

    if (g_server.advertising_enabled)
    {
        ESP_LOGW(TAG, "Advertising already enabled");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting advertising");

    esp_ble_adv_params_t adv_params = {
        .adv_int_min = g_server.config.adv_int_min,
        .adv_int_max = g_server.config.adv_int_max,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

    esp_err_t ret = esp_ble_gap_start_advertising(&adv_params);
    if (ret != ESP_OK)
    {
        bitmans_bles_set_error(BITMANS_BLES_ERROR_ADV_START_FAILED, ret, "Failed to start advertising");
    }

    return ret;
}

esp_err_t bitmans_bles_stop_advertising(void)
{
    if (!g_server_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!g_server.advertising_enabled)
    {
        ESP_LOGW(TAG, "Advertising not enabled");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping advertising");

    esp_err_t ret = esp_ble_gap_stop_advertising();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop advertising: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t bitmans_bles_send_response(uint16_t conn_id, uint32_t trans_id,
                                     esp_gatt_status_t status, uint16_t handle,
                                     const uint8_t *data, uint16_t len)
{
    if (!g_server_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_gatt_rsp_t response = {0};
    response.attr_value.handle = handle;
    response.attr_value.len = len;

    if (data && len > 0 && len <= ESP_GATT_MAX_ATTR_LEN)
    {
        memcpy(response.attr_value.value, data, len);
    }

    // Find the gatts_if for this connection
    esp_gatt_if_t gatts_if = ESP_GATT_IF_NONE;
    for (int i = 0; i < g_server.service_count; i++)
    {
        if (g_server.services[i].gatts_if != ESP_GATT_IF_NONE)
        {
            gatts_if = g_server.services[i].gatts_if;
            break; // Use first available gatts_if
        }
    }

    if (gatts_if == ESP_GATT_IF_NONE)
    {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_ble_gatts_send_response(gatts_if, conn_id, trans_id, status, &response);
}

esp_err_t bitmans_bles_send_notification(bitmans_bles_characteristic_t *characteristic,
                                         const uint8_t *data, uint16_t len)
{
    if (!g_server_initialized || !characteristic || !data)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_server.is_connected)
    {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_ble_gatts_send_indicate(characteristic->service->gatts_if, g_server.conn_id,
                                       characteristic->handle, len, (uint8_t *)data, false);
}

esp_err_t bitmans_bles_send_indication(bitmans_bles_characteristic_t *characteristic,
                                       const uint8_t *data, uint16_t len)
{
    if (!g_server_initialized || !characteristic || !data)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_server.is_connected)
    {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_ble_gatts_send_indicate(characteristic->service->gatts_if, g_server.conn_id,
                                       characteristic->handle, len, (uint8_t *)data, true);
}

bitmans_bles_state_t bitmans_bles_get_state(void)
{
    return g_server_initialized ? g_server.state : BITMANS_BLES_STATE_IDLE;
}

const char *bitmans_bles_get_last_error(bitmans_bles_error_t *error_code, esp_err_t *esp_error)
{
    if (error_code)
    {
        *error_code = g_server.last_error;
    }
    if (esp_error)
    {
        *esp_error = g_server.last_esp_error;
    }
    return bitmans_bles_error_to_string(g_server.last_error);
}

bool bitmans_bles_is_connected(void)
{
    return g_server_initialized ? g_server.is_connected : false;
}

bool bitmans_bles_get_connection_info(uint16_t *conn_id, esp_bd_addr_t *remote_bda)
{
    if (!g_server_initialized || !g_server.is_connected)
    {
        return false;
    }

    if (conn_id)
    {
        *conn_id = g_server.conn_id;
    }
    if (remote_bda)
    {
        memcpy(*remote_bda, g_server.remote_bda, ESP_BD_ADDR_LEN);
    }

    return true;
}

esp_err_t bitmans_bles_clear_error(void)
{
    if (!g_server_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_server.state != BITMANS_BLES_STATE_ERROR)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Clearing error state, attempting recovery");

    g_server.last_error = BITMANS_BLES_ERROR_NONE;
    g_server.last_esp_error = ESP_OK;

    // Attempt to return to a safe state
    return bitmans_bles_fsm_transition(BITMANS_BLES_STATE_READY);
}

bitmans_bles_service_t *bitmans_bles_get_service_by_name(const char *name)
{
    if (!g_server_initialized || !name)
    {
        return NULL;
    }

    for (int i = 0; i < g_server.service_count; i++)
    {
        if (strcmp(g_server.services[i].definition.name, name) == 0)
        {
            return &g_server.services[i];
        }
    }

    return NULL;
}

bitmans_bles_characteristic_t *bitmans_bles_get_characteristic_by_name(
    bitmans_bles_service_t *service, const char *name)
{
    if (!service || !name)
    {
        return NULL;
    }

    for (int i = 0; i < service->definition.characteristic_count; i++)
    {
        if (strcmp(service->characteristics[i].definition.name, name) == 0)
        {
            return &service->characteristics[i];
        }
    }

    return NULL;
}

bitmans_bles_characteristic_t *bitmans_bles_get_characteristic_by_handle(uint16_t handle)
{
    if (!g_server_initialized)
    {
        return NULL;
    }

    return (bitmans_bles_characteristic_t *)bitmans_hash_table_try_get(&g_server.handle_to_char, handle);
}

void bitmans_bles_deinit(void)
{
    if (!g_server_initialized)
    {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing BLE Server");

    // Stop everything
    bitmans_bles_stop(5000);

    // Cleanup hash tables
    bitmans_hash_table_cleanup(&g_server.gatts_if_to_service);
    bitmans_hash_table_cleanup(&g_server.handle_to_char);

    // Cleanup FreeRTOS objects
    if (g_server.event_queue)
    {
        vQueueDelete(g_server.event_queue);
        g_server.event_queue = NULL;
    }

    if (g_server.operation_events)
    {
        vEventGroupDelete(g_server.operation_events);
        g_server.operation_events = NULL;
    }

    if (g_server.server_task_handle)
    {
        // Task should have already exited from stop sequence
        g_server.server_task_handle = NULL;
    }

    // Free services memory
    for (int i = 0; i < g_server.service_count; i++)
    {
        if (g_server.services[i].characteristics)
        {
            free(g_server.services[i].characteristics);
        }
    }

    // Disable BLE stack
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    // Clear global state
    memset(&g_server, 0, sizeof(g_server));
    g_server_initialized = false;

    ESP_LOGI(TAG, "BLE Server deinitialized");
}
