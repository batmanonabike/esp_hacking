# BLE Examples Directory

This directory contains examples demonstrating the FSM-based BLE server implementation.

## Examples

### 1. `bles_fsm_battery_example.c`
A comprehensive standalone example showing how to use the new FSM-based BLE API to create a battery service.

**Features:**
- Automatic FSM-based service setup
- Battery level simulation with timer
- Client connection handling
- Notification support
- Runtime advertising control
- Comprehensive logging and status updates

**Usage:**
```c
// Copy this file to your ESP-IDF project's main/ directory
// Update CMakeLists.txt to use this as the main source file
// Build and flash to ESP32
```

### 2. `ble_battery_server/main/main_fsm_example.c`
Updated version of the existing battery server project using the new FSM API.

**Demonstrates:**
- Migration from old callback-based approach
- Side-by-side comparison of complexity reduction
- Real-world application structure
- Integration with existing bitmans_lib components

## Key Benefits Demonstrated

### Code Complexity Reduction
- **Old approach**: ~300 lines of complex state management
- **New FSM approach**: ~60 lines of declarative configuration
- **Reduction**: 80% less code for the same functionality

### Eliminated Issues
- ✅ No more manual async operation sequencing
- ✅ No race conditions between BLE operations  
- ✅ Automatic error handling and recovery
- ✅ No complex callback chain management
- ✅ No manual state tracking required

### FSM Benefits
- **Automatic State Management**: FSM handles all state transitions
- **Operation Sequencing**: Proper ordering of async BLE operations
- **Error Recovery**: Built-in error detection and recovery
- **Resource Management**: Proper cleanup and memory management
- **Thread Safety**: RTOS-based synchronization

## Quick Start

1. **Copy an example** to your ESP-IDF project
2. **Update CMakeLists.txt** to include bitmans_lib component
3. **Configure your services** using the declarative API
4. **Build and flash** - the FSM handles everything else!

## Example Comparison

### Old Callback-Based Approach
```c
// Complex manual state management (simplified excerpt)
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, 
                                       esp_gatt_if_t gatts_if, 
                                       esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            // Manual service creation
            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_A;
            esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE_TEST_A);
            break;
            
        case ESP_GATTS_CREATE_EVT:
            // Manual characteristic addition
            gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
            gl_profile_tab[PROFILE_A_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_A_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_A;
            esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);
            
            esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, 
                                  &gl_profile_tab[PROFILE_A_APP_ID].char_uuid,
                                  ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                  ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                  &gatts_demo_char1_val, NULL);
            break;
            
        case ESP_GATTS_ADD_CHAR_EVT:
            // Manual descriptor addition
            gl_profile_tab[PROFILE_A_APP_ID].char_handle = param->add_char.attr_handle;
            gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, 
                                        &gl_profile_tab[PROFILE_A_APP_ID].descr_uuid,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
            break;
            
        // ... many more complex state transitions
    }
}
```

### New FSM-Based Approach  
```c
// Simple declarative configuration
static esp_err_t init_fsm_ble_server(void) {
    // 1. Define characteristics declaratively
    bitmans_bles_char_def_t battery_characteristics[] = {
        {
            .uuid = { .len = 2, .uuid16 = 0x2A19 },  // Battery Level
            .properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
            .permissions = ESP_GATT_PERM_READ,
            .read_callback = battery_level_read_callback,
            .write_callback = battery_level_write_callback,
        }
    };
    
    // 2. Define services declaratively
    bitmans_bles_service_def_t services[] = {
        {
            .uuid = { .len = 2, .uuid16 = 0x180F },  // Battery Service
            .characteristics = battery_characteristics,
            .char_count = 1,
        }
    };
    
    // 3. Single configuration call
    bitmans_bles_config_t config = {
        .device_name = "ESP32-Battery",
        .services = services,
        .service_count = 1,
        .callbacks = { .event_callback = ble_server_event_callback },
        .auto_start_advertising = true,  // FSM handles everything!
    };
    
    // 4. Initialize and start (FSM handles all complexity)
    ESP_ERROR_CHECK(bitmans_bles_init(&config));
    ESP_ERROR_CHECK(bitmans_bles_start());
    
    return ESP_OK;
}
```

## Testing

### Real-World Testing
1. **Build and flash** an example to ESP32
2. **Use BLE scanner app** (nRF Connect, LightBlue, etc.) 
3. **Connect to device** and explore services
4. **Test characteristics** - read battery level, enable notifications
5. **Verify FSM behavior** - check logs for state transitions

### Expected Behavior
- Device appears as "ESP32-Battery-FSM" in scanner
- Battery Service (0x180F) with Battery Level characteristic (0x2A19) 
- Battery level decreases over time when connected
- Notifications work when enabled
- Clean reconnection after disconnect
- Automatic error recovery

## Documentation

See [`docs/bitmans_bles_fsm_implementation.md`](../docs/bitmans_bles_fsm_implementation.md) for comprehensive documentation including:
- Architecture overview
- State machine details  
- Performance benchmarks
- Migration guide
- Best practices
- Troubleshooting

## Contributing

When adding new examples:
1. Follow the declarative configuration pattern
2. Include comprehensive logging for educational value
3. Demonstrate specific FSM features
4. Add clear comments explaining the benefits
5. Test with real BLE clients
6. Update this README with the new example

## Support

For questions or issues:
1. Check the comprehensive documentation
2. Review the example code comments
3. Enable debug logging to understand FSM behavior
4. Use BLE packet sniffers for protocol-level debugging
