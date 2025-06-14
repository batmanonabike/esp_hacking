# GitHub Copilot Instructions for ESP32 BLE Development Workspace

## Project Overview

This workspace contains multiple ESP-IDF projects focused on Bluetooth Low Energy (BLE) development for ESP32 microcontrollers. 
The project uses a shared component architecture with the `bat_lib` library for general purpose, `bat_ble_lib` for bluetooth, etc.
These will provide common functionality across multiple applications.

## AI Role
You are a senior developer and your role is to assist in writing, reviewing, and refactoring C code for ESP32 BLE applications.
Always follow the coding standards and architectural principles outlined in this document.
Free free to ask for clarifications on specific coding tasks or architectural decisions.
If you consider a task not clear, or not possible, then ask for more details before proceeding.

## Architecture Principles

### Component-Based Architecture
- **Shared Components**: Common functionality is encapsulated in reusable components (`bat_lib`, `bat_config`)
- **Project Structure**: Each project is self-contained but references shared components via `EXTRA_COMPONENT_DIRS`
- **Separation of Concerns**: Distinct modules for BLE client, BLE server, WiFi, LED control, and logging

### Callback Pattern
- Use structured callback patterns for asynchronous event handling
- Always provide context pointers in callback structures
- Initialize callbacks with no-op functions for optional handlers
- Example pattern:
```c
typedef struct {
    void *pContext;
    void (*on_event)(struct callback_t *, event_param_t *);
} callback_t;

void callback_init(callback_t *pCallbacks, void *pContext) {
    pCallbacks->pContext = pContext;
    if (pCallbacks->on_event == NULL)
        pCallbacks->on_event = no_op_handler;
}
```

### Error Handling Strategy
- **ESP-IDF Error Codes**: Always use `esp_err_t` return types for functions that can fail
- **Error Propagation**: Check and propagate errors using `ESP_ERROR_CHECK()` or custom error handling
- **Logging**: Use structured logging with appropriate log levels (`ESP_LOGI`, `ESP_LOGE`, `ESP_LOGW`, `ESP_LOGD`)
- **Custom Error Handler**: Use `ESP_ERROR_CHECK_RESTART()` for critical failures that should restart the system

### Event-Driven Architecture
- Use FreeRTOS event groups for synchronization between tasks
- Implement state machines using event bits for complex workflows
- Example pattern:
```c
EventGroupHandle_t event_group = xEventGroupCreate();
EventBits_t bits = xEventGroupWaitBits(event_group, TARGET_BIT | ERROR_BIT, 
    pdTRUE, pdFALSE, timeout);
```

## Code Style Guidelines

### Naming Conventions
- **Functions**: Use `bat_` prefix for library functions
  - Examples: `bat_ble_server_init()`, `bat_gatts_start_service()`
- **Types**: Use `_t` suffix for type definitions
  - Examples: `bat_gatts_callbacks_t`, `app_context`
- **Constants**: Use UPPER_CASE with descriptive prefixes
  - Examples: `BAT_APP_ID`, `ERROR_BIT`, `GATTS_READY_TO_START_BIT`
- **Variables**: Use descriptive names with context
  - Pointers: Use `p` prefix (e.g., `pContext`, `pCallbacks`)
  - Arrays/strings: Use descriptive names (e.g., `szAdvName`)

### Function Design
- **Single Responsibility**: Each function should have one clear purpose
- **Parameter Validation**: Check for NULL pointers and invalid arguments
- **Consistent Return Types**: Use `esp_err_t` for operations that can fail
- **Context Passing**: Pass context through structured parameters, not globals

### Memory Management
- **RAII Pattern**: Initialize resources in `_init()` functions, clean up in `_deinit()` functions
- **Dynamic Allocation**: Use `calloc()` for zero-initialized memory
- **Resource Cleanup**: Always implement cleanup callbacks for hash tables and data structures
- **Event Groups**: Create event groups during initialization, clean up on termination

### Documentation Standards
- **Header Comments**: Include file path comments at the top of files
- **Function Documentation**: Document complex functions with purpose, parameters, and return values
- **Inline Comments**: Explain complex logic, state transitions, and ESP-IDF specific behavior
- **Architecture Comments**: Explain BLE state machines, connection flows, and event sequences

## ESP-IDF Specific Guidelines

### BLE Development Patterns
- **Event Handler Structure**: Implement separate handlers for GAP and GATT events
- **State Management**: Use event groups to coordinate between BLE stack events and application logic
- **UUID Handling**: Support both 16-bit standard UUIDs and 128-bit custom UUIDs
- **Service Registration**: Use hash tables to map application IDs to callback structures

### CMake Configuration
- **Component Registration**: Use `idf_component_register()` with explicit SRCS and dependencies
- **Shared Components**: Reference shared components via `EXTRA_COMPONENT_DIRS`
- **Dependencies**: Declare all required components in REQUIRES and PRIV_REQUIRES

### Logging Best Practices
- **Structured Logging**: Use consistent log tags and formatting
- **Log Levels**: 
  - `ESP_LOGI`: Important state changes, successful operations
  - `ESP_LOGE`: Error conditions, failures
  - `ESP_LOGW`: Warnings, unexpected but recoverable conditions
  - `ESP_LOGD`: Debug information, verbose logging
- **Context in Logs**: Include relevant handles, IDs, and state information

### Wrapping ESP-IDF Functions
When creating wrappers for ESP-IDF functions, follow these guidelines:
If asked to create wrappers for ESP-IDF functions, ensure they:
- **Return `esp_err_t`**: Always return ESP-IDF error codes if the original function does.
- **Check Parameters**: Use `assert` for critical checks on input parameters.
- **Log Errors**: Use `ESP_LOGE` for error conditions with context
- **Log Success**: Use `ESP_LOGI` for successful operations
- **Use similar names**: For the wrapper functions, use the same name as the ESP-IDF function with a `bat_` prefix rather than `esp_`.
  - Example: `esp_ble_gatts_register_callback()` becomes `bat_ble_gatts_register_callback()`

### Task and Timing
- **FreeRTOS Delays**: Use `vTaskDelay(pdMS_TO_TICKS(ms))` for delays
- **Non-blocking Operations**: Prefer event-driven patterns over polling
- **Task Priorities**: Document task priorities and their relationships

## BLE-Specific Guidelines

### GATT Server Implementation
- **Service Creation**: Use 128-bit UUIDs for custom services
- **Characteristic Properties**: Set appropriate read/write/notify permissions
- **Event Handling**: Implement all necessary GATTS event handlers
- **Connection Management**: Handle connection/disconnection events properly

### GATT Client Implementation
- **Service Discovery**: Implement proper service and characteristic discovery flows
- **Connection State**: Track connection state through GATTC events
- **Data Handling**: Parse and validate received data appropriately

### Advertising and Scanning
- **Advertisement Data**: Structure advertisement data with service UUIDs and device names
- **Scan Parameters**: Configure appropriate scan intervals and windows
- **Device Filtering**: Implement filtering based on device names or service UUIDs

## Testing and Debugging

### Build Configuration
In general do not build projects unless you are asked to do so.
- **SDK Configuration**: Use `idf.py menuconfig` to enable BLE features
- **Build Commands**: Use `idf.py build` for compilation
- **Flash Commands**: Use `idf.py -p COMx flash monitor` for deployment and monitoring

### Permissions for AI.
- **Directory Creation**: AI can create directories for new projects, components, or source files with asking for permission.

### Debugging Practices
- **Serial Monitoring**: Use `idf.py monitor` for real-time log output
- **State Logging**: Log all important state transitions and events
- **Error Context**: Include error codes and descriptive messages in error logs

## Security Considerations

### BLE Security
- **Pairing**: Implement appropriate pairing and bonding procedures when required
- **Permissions**: Set restrictive GATT permissions for sensitive characteristics
- **Authentication**: Validate client connections and data integrity

### General Security
- **Input Validation**: Validate all external inputs (BLE data, configuration)
- **Buffer Management**: Prevent buffer overflows in string and data handling
- **Resource Limits**: Implement appropriate limits on connections and data transfers

## Performance Guidelines

### Memory Optimization
- **Static Allocation**: Prefer static allocation for predictable memory usage
- **Buffer Reuse**: Reuse buffers where possible to reduce fragmentation
- **Hash Tables**: Use appropriate sizing for hash tables based on expected load

### Power Management
- **Sleep Modes**: Consider deep sleep modes for battery-powered applications
- **Connection Intervals**: Optimize BLE connection intervals for power efficiency
- **Advertising Intervals**: Use appropriate advertising intervals based on discovery requirements

## Integration Patterns

### Multi-Protocol Support
- **WiFi + BLE**: Follow ESP-IDF guidelines for concurrent WiFi and BLE usage
- **Resource Sharing**: Coordinate radio resource usage between protocols
- **Event Coordination**: Use separate event handlers for WiFi and BLE events

### Hardware Integration
- **GPIO Management**: Use consistent GPIO management patterns
- **LED Control**: Implement standardized LED blinking patterns for status indication
- **Peripheral Drivers**: Follow ESP-IDF driver patterns for hardware peripherals

Remember to always refer to the latest ESP-IDF documentation and follow the established patterns in the existing codebase when implementing new features.
