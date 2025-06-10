# BLE Server Finite State Machine (bitmans_bles)

## Overview

The `bitmans_bles` library provides a robust, finite state machine-based approach to BLE GATT server development on ESP32. It addresses the key challenges of the original callback-based approach by:

1. **Eliminating race conditions** through proper async operation sequencing
2. **Minimizing global state** usage 
3. **Providing user control** over service definition while handling complexity automatically
4. **Simplifying user code** while maintaining full flexibility

## Key Features

### 🔄 Finite State Machine
- **17 distinct states** covering the entire BLE server lifecycle
- **Automatic state validation** prevents invalid transitions
- **Comprehensive logging** for easy debugging
- **Graceful error handling** with automatic recovery options

### 🚀 Simple User API
```c
// Initialize context
bitmans_bles_init(&ctx, &callbacks, true);

// Define services declaratively  
bitmans_bles_add_service(&ctx, &battery_service);

// Start everything automatically
bitmans_bles_start(&ctx, app_id);
```

### 🛡️ Race Condition Prevention
- **Async operation sequencing**: Waits for each ESP-IDF async call to complete before proceeding
- **Event validation**: Only processes events in expected states
- **Timeout protection**: Won't hang forever on failed operations
- **Clean shutdown**: Proper async teardown sequence

### 🎛️ User Control & Flexibility
- **Declarative service definition**: Define multiple services with multiple characteristics
- **Optional CCCD**: Per-characteristic control over notification descriptors
- **Custom callbacks**: Handle reads, writes, connections, and periodic tasks
- **Manual or automatic**: Choose between auto-progression or manual control

## Architecture

### State Flow
```
IDLE → REGISTERING_APP → APP_REGISTERED → CREATING_SERVICE → 
SERVICE_CREATED → ADDING_CHARACTERISTICS → ADDING_DESCRIPTORS → 
STARTING_SERVICE → SERVICE_STARTED → SETTING_ADV_DATA → 
ADVERTISING → CONNECTED
```

### Shutdown Flow  
```
CONNECTED → STOPPING_ADVERTISING → STOPPING_SERVICE → 
DELETING_SERVICE → UNREGISTERING_APP → STOPPED
```

### Component Structure
```
bitmans_bles_context_t (main context)
├── FSM State Management
├── Service Definitions (declarative)
├── User Callbacks (optional)
├── RTOS Task Management
└── BLE Handle Tracking
```

## Usage Examples

### Basic Battery Service
```c
// Define service structure
bitmans_bles_service_def_t battery_service = {
    .uuid = battery_service_uuid,
    .name = "Battery Service", 
    .is_primary = true,
    .char_count = 1,
    .characteristics = {
        {
            .uuid = battery_level_uuid,
            .properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
            .permissions = ESP_GATT_PERM_READ,
            .add_cccd = true,
            .name = "Battery Level"
        }
    }
};

// User callbacks
bitmans_bles_callbacks_t callbacks = {
    .on_read = handle_read_request,
    .on_write = handle_write_request,
    .on_connection_change = handle_connection,
    .on_periodic = periodic_battery_update,
    .periodic_interval_ms = 5000,
    .user_data = &app_context
};

// Initialize and start
bitmans_bles_init(&ctx, &callbacks, true);
bitmans_bles_add_service(&ctx, &battery_service);
bitmans_bles_start(&ctx, APP_ID);
```

### Multi-Service Example
```c
// Heart Rate Service
bitmans_bles_service_def_t hr_service = {
    .uuid = hr_service_uuid,
    .name = "Heart Rate Service",
    .is_primary = true,
    .char_count = 2,
    .characteristics = {
        {
            .uuid = hr_measurement_uuid,
            .properties = ESP_GATT_CHAR_PROP_BIT_NOTIFY,
            .permissions = 0,
            .add_cccd = true,
            .name = "HR Measurement"
        },
        {
            .uuid = hr_control_uuid,
            .properties = ESP_GATT_CHAR_PROP_BIT_WRITE,
            .permissions = ESP_GATT_PERM_WRITE,
            .add_cccd = false,
            .name = "HR Control Point"
        }
    }
};

// Add multiple services
bitmans_bles_add_service(&ctx, &battery_service);
bitmans_bles_add_service(&ctx, &hr_service);
bitmans_bles_start(&ctx, APP_ID);
```

### Manual Control Mode
```c
// Initialize without auto-progression
bitmans_bles_init(&ctx, &callbacks, false);
bitmans_bles_add_service(&ctx, &service);

// Start registration only
bitmans_bles_start(&ctx, APP_ID);

// Wait for APP_REGISTERED state, then manually proceed
while (bitmans_bles_get_state(&ctx) != BITMANS_BLES_STATE_APP_REGISTERED) {
    vTaskDelay(pdMS_TO_TICKS(10));
}

// Custom logic here before proceeding...
// FSM will handle the rest automatically
```

## Comparison with Original Approach

### Original Callback-Based Issues
```c
// ❌ Race condition prone
static void app_on_gatts_add_char(callbacks *pCb, param *pParam) {
    // Immediately calls async function without waiting
    bitmans_gatts_add_cccd(pCb->service_handle, handle);
}

// ❌ Manual state tracking required
static void app_on_add_char_desc(callbacks *pCb, param *pParam) {
    // How do we know all descriptors are added?
    bitmans_gatts_start_service(pCb->service_handle);  
}

// ❌ Complex error handling
// No centralized way to handle failures
```

### New FSM-Based Solution
```c
// ✅ Automatic async sequencing
case ESP_GATTS_ADD_CHAR_EVT:
    // Store handle, increment counter, check if more needed
    service->characteristics[ctx->current_char_index].handle = param->add_char.attr_handle;
    ctx->current_char_index++;
    bitmans_bles_fsm_transition(ctx, BITMANS_BLES_STATE_ADDING_CHARACTERISTICS);
    // FSM automatically handles next step

// ✅ Centralized state management  
static esp_err_t bitmans_bles_fsm_transition(ctx, new_state) {
    // Validates transitions, handles auto-progression, manages errors
}

// ✅ Clean error recovery
if (operation_failed) {
    ctx->last_error = error_code;
    return bitmans_bles_fsm_transition(ctx, BITMANS_BLES_STATE_ERROR);
}
```

## Advanced Features

### Graceful Stop/Restart
```c
// Stop with timeout protection
esp_err_t result = bitmans_bles_stop(&ctx, 5000);
if (result == ESP_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "Stop timed out - may need ESP32 restart");
    esp_restart();
}

// Restart without rebooting ESP32
bitmans_bles_restart(&ctx);
```

### Notification/Indication Support
```c
// Send notification to all connected clients
bitmans_bles_send_notification(&ctx, 0, char_handle, data, len);

// Send indication with acknowledgment
bitmans_bles_send_indication(&ctx, conn_id, char_handle, data, len);
```

### Handle Lookup
```c
// Find characteristic handle by UUID
uint16_t handle = bitmans_bles_find_char_handle(&ctx, &battery_level_uuid);
if (handle != 0) {
    // Use handle for notifications
}
```

### RTOS Task Integration  
```c
// User callbacks run in separate task context
bitmans_bles_callbacks_t callbacks = {
    .on_periodic = periodic_work,        // Runs every N milliseconds
    .periodic_interval_ms = 1000,        // 1 second interval
    .on_connection_change = handle_conn, // Non-blocking event notification
    .user_data = &app_context
};
```

## Best Practices

### 1. Service Definition
- Define services declaratively before calling `bitmans_bles_start()`
- Use standard Bluetooth UUIDs when possible
- Add CCCD only for characteristics that support notifications/indications

### 2. Error Handling
- Always check return values from bitmans_bles functions
- Use `bitmans_bles_get_last_error()` for detailed error information
- Implement proper timeout handling for stop operations

### 3. Resource Management
- Call `bitmans_bles_deinit()` before application exit
- Use `bitmans_bles_stop()` before reconfiguring services
- Don't store characteristic handles until after service creation completes

### 4. Performance
- Use periodic callbacks for regular work instead of polling
- Keep user callbacks lightweight (they run in BLE event context)
- Use notifications instead of indications when acknowledgment isn't needed

## Debugging

### State Monitoring
```c
// Log current state
ESP_LOGI(TAG, "Current state: %d", bitmans_bles_get_state(&ctx));

// Check if ready for operations
if (bitmans_bles_is_running(&ctx)) {
    // Safe to send notifications
}
```

### Error Diagnosis
```c
esp_err_t last_error = bitmans_bles_get_last_error(&ctx);
if (last_error != ESP_OK) {
    ESP_LOGE(TAG, "BLE Error: %s", esp_err_to_name(last_error));
}
```

### Event Logging
The FSM automatically logs all state transitions and important events at INFO level. Enable ESP_LOG_INFO for the "bitmans_bles" tag to see detailed operation flow.

## Migration from Original API

### 1. Replace Callback Structure
```c
// Old way
bitmans_gatts_callbacks_t gatts_callbacks = {
    .on_reg = app_on_gatts_reg,
    .on_create = app_on_gatts_create,
    // ... many callbacks
};

// New way  
bitmans_bles_callbacks_t callbacks = {
    .on_read = handle_read,
    .on_write = handle_write,
    .user_data = &app_context
};
```

### 2. Replace Manual Service Creation
```c
// Old way - manual async calls
bitmans_gatts_create_service128(gatts_if, &service_uuid);
// Wait for ESP_GATTS_CREATE_EVT...
bitmans_gatts_create_char128(gatts_if, service_handle, &char_uuid, props, perms);
// Wait for ESP_GATTS_ADD_CHAR_EVT...
bitmans_gatts_add_cccd(service_handle, char_handle);
// etc...

// New way - declarative definition
bitmans_bles_service_def_t service = { /* definition */ };
bitmans_bles_add_service(&ctx, &service);
bitmans_bles_start(&ctx, app_id);  // Everything happens automatically
```

### 3. Simplify Read/Write Handling
```c
// Old way - manual response construction
static void app_on_gatts_read(bitmans_gatts_callbacks_t *pCb, esp_ble_gatts_cb_param_t *pParam) {
    if (pParam->read.handle == battery_handle) {
        esp_gatt_rsp_t rsp = {0};
        rsp.attr_value.handle = pParam->read.handle;
        rsp.attr_value.len = 1;
        rsp.attr_value.value[0] = battery_level;
        esp_ble_gatts_send_response(gatts_if, pParam->read.conn_id, pParam->read.trans_id, ESP_GATT_OK, &rsp);
    }
}

// New way - simplified response
static esp_err_t on_read(bitmans_bles_context_t *ctx, uint16_t char_handle, 
                        uint16_t conn_id, uint32_t trans_id, void *user_data) {
    if (char_handle == battery_handle) {
        return bitmans_bles_send_response(ctx, conn_id, trans_id, char_handle, 
                                        &battery_level, sizeof(battery_level));
    }
    return ESP_OK;
}
```

## Conclusion

The `bitmans_bles` FSM-based approach provides a robust, production-ready foundation for BLE GATT server development that:

- **Eliminates common race conditions** through proper async operation sequencing
- **Simplifies user code** while maintaining full control and flexibility  
- **Provides comprehensive error handling** and recovery mechanisms
- **Scales easily** from simple single-service to complex multi-service applications
- **Integrates cleanly** with FreeRTOS task-based architectures

The declarative service definition approach combined with automatic state management makes BLE server development significantly more reliable and maintainable.
