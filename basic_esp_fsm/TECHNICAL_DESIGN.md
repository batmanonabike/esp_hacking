// filepath: c:\source\repos\esp_hacking_47\basic_esp_fsm\TECHNICAL_DESIGN.md

# Technical Design Document - Basic ESP32 FSM

## Overview

This document provides detailed technical information about the finite state machine implementation, focusing on the use of function pointers, callback patterns, and event-driven architecture.

## Core Design Patterns

### 1. Function Pointer State Handlers

The FSM uses an array of function pointers to implement different behaviors for each state:

```c
typedef esp_err_t (*fsm_state_func_t)(struct fsm_context_t *pContext, fsm_event_t event);

typedef struct fsm_context_t {
    fsm_state_t currentState;
    fsm_state_func_t stateHandlers[FSM_STATE_MAX];  // Function pointer array
    // ... other members
} fsm_context_t;
```

**Initialization:**
```c
pContext->stateHandlers[FSM_STATE_DISCONNECTED] = fsm_state_disconnected_handler;
pContext->stateHandlers[FSM_STATE_CONNECTING] = fsm_state_connecting_handler;
pContext->stateHandlers[FSM_STATE_CONNECTED] = fsm_state_connected_handler;
pContext->stateHandlers[FSM_STATE_DISCONNECTING] = fsm_state_disconnecting_handler;
```

**Runtime Dispatch:**
```c
fsm_state_func_t stateHandler = pContext->stateHandlers[pContext->currentState];
esp_err_t result = stateHandler(pContext, event);
```

### 2. Callback Pattern Implementation

The FSM supports structured callbacks for loose coupling between the state machine and application logic:

```c
typedef struct {
    void *pContext;                                    // User context
    void (*on_state_changed)(fsm_context_t *, fsm_state_t, fsm_state_t);
    void (*on_event_processed)(fsm_context_t *, fsm_event_t, esp_err_t);
    void (*on_connection_data)(fsm_context_t *, const char *, size_t);
} fsm_callbacks_t;
```

**Benefits:**
- Separation of concerns between FSM logic and application behavior
- Extensible notification system
- Type-safe callback interfaces
- Context preservation across callback invocations

### 3. Event-Driven Architecture

Uses FreeRTOS event groups for synchronization and coordination:

```c
#define FSM_EVENT_PROCESSED_BIT     BIT0
#define FSM_TRANSITION_COMPLETE_BIT BIT1
#define FSM_ERROR_BIT               BIT2

EventGroupHandle_t eventGroup = xEventGroupCreate();
```

**Synchronization Points:**
- Event processing completion
- State transition completion
- Error condition signaling

## State Handler Implementations

### Common Pattern

Each state handler follows a consistent pattern:

```c
esp_err_t fsm_state_X_handler(fsm_context_t *pContext, fsm_event_t event)
{
    // Parameter validation
    if (pContext == NULL) return ESP_ERR_INVALID_ARG;
    
    // Event handling with switch statement
    switch (event) {
        case VALID_EVENT_1:
            // Handle valid transition
            return fsm_transition_to_state(pContext, NEW_STATE);
            
        case VALID_EVENT_2:
            // Handle state-specific logic
            // Update state information
            return ESP_OK;
            
        case IGNORED_EVENT:
            // Log and ignore invalid events for this state
            ESP_LOGW(pContext->szTag, "Ignoring event in current state");
            return ESP_OK;
            
        default:
            // Error for unexpected events
            ESP_LOGE(pContext->szTag, "Unhandled event");
            return ESP_ERR_INVALID_ARG;
    }
}
```

### State-Specific Behaviors

#### DISCONNECTED State
- **Primary Role**: Entry point and idle state
- **Key Behaviors**:
  - Accepts connection requests
  - Rejects invalid events gracefully
  - Maintains connection attempt statistics
- **Transitions**: Only to CONNECTING on CONNECT_REQUEST

#### CONNECTING State  
- **Primary Role**: Connection establishment
- **Key Behaviors**:
  - Handles connection success/failure
  - Supports timeout-based transitions
  - Can be interrupted by disconnect requests
- **State Information**: Increments connection attempt counter
- **Transitions**: To CONNECTED (success), DISCONNECTED (failure), DISCONNECTING (interrupt)

#### CONNECTED State
- **Primary Role**: Active connection management
- **Key Behaviors**:
  - Processes periodic timeout events for housekeeping
  - Simulates data transfer and statistics
  - Monitors for unexpected disconnections
- **State Information**: Updates connected time, data transfer counters
- **Callbacks**: Triggers data notifications via callbacks
- **Transitions**: To DISCONNECTING (graceful), DISCONNECTED (error)

#### DISCONNECTING State
- **Primary Role**: Graceful connection termination
- **Key Behaviors**:
  - Waits for disconnection completion
  - Logs final connection statistics
  - Handles queued connection requests
- **Transitions**: Always returns to DISCONNECTED

## State Information Management

The FSM maintains rich state information that demonstrates real-world connection management:

```c
typedef struct {
    char szConnectionId[32];        // Unique connection identifier
    uint32_t ulConnectionAttempts;  // Retry counter
    uint32_t ulConnectedTime;       // Session duration
    uint32_t ulDataBytesSent;       // Outbound data counter
    uint32_t ulDataBytesReceived;   // Inbound data counter  
    bool bIsSecure;                 // Security status flag
} fsm_state_info_t;
```

**Usage Examples:**
- Connection retry logic based on attempt counter
- Session timeout based on connected time
- Bandwidth monitoring via data counters
- Security policy enforcement via flags

## Error Handling Strategy

### Multi-Level Error Handling

1. **Parameter Validation**: All public functions validate inputs
2. **State Validation**: Events are validated against current state
3. **Resource Management**: Event groups and memory are properly cleaned up
4. **Error Propagation**: Consistent use of `esp_err_t` return codes

### Error Recovery

```c
// Event processing with error handling
esp_err_t result = stateHandler(pContext, event);
if (result == ESP_OK) {
    xEventGroupSetBits(pContext->eventGroup, FSM_EVENT_PROCESSED_BIT);
} else {
    xEventGroupSetBits(pContext->eventGroup, FSM_ERROR_BIT);
}
```

## Memory Management

### RAII Pattern Implementation

```c
// Initialization
esp_err_t fsm_init(fsm_context_t *pContext, const char *szTag) {
    memset(pContext, 0, sizeof(fsm_context_t));
    pContext->eventGroup = xEventGroupCreate();
    // ... other initialization
    return ESP_OK;
}

// Cleanup
esp_err_t fsm_term(fsm_context_t *pContext) {
    if (pContext->eventGroup != NULL) {
        vEventGroupDelete(pContext->eventGroup);
        pContext->eventGroup = NULL;
    }
    memset(pContext, 0, sizeof(fsm_context_t));
    return ESP_OK;
}
```

## Extensibility Points

### Adding New States

1. Add enum value to `fsm_state_t`
2. Implement handler function following the pattern
3. Register handler in `fsm_init()`
4. Update string conversion functions
5. Document state transitions

### Adding New Events

1. Add enum value to `fsm_event_t` 
2. Update relevant state handlers
3. Update string conversion functions
4. Add to demo sequences if desired

### Extending State Information

1. Add fields to `fsm_state_info_t`
2. Initialize in `fsm_init()`
3. Update in appropriate state handlers
4. Add logging/callback notifications as needed

## Performance Considerations

### Function Pointer Overhead

- **Cost**: Single indirect function call per event
- **Benefit**: Eliminates large switch statements
- **Trade-off**: Minimal performance impact for significant code organization benefits

### Memory Footprint

- **Static**: Function pointer array (4 pointers × 4 bytes = 16 bytes)
- **Dynamic**: Event group handle (~100 bytes)
- **Total Context**: ~200 bytes per FSM instance

### Real-Time Characteristics

- **Deterministic**: Event processing time is bounded
- **Non-blocking**: No indefinite waits in state handlers
- **Interruptible**: Uses FreeRTOS primitives for task coordination

## Testing Strategy

### Unit Testing Approach

1. **State Handler Testing**: Test each handler with all possible events
2. **Transition Testing**: Verify all valid state transitions
3. **Error Path Testing**: Test invalid events and edge cases
4. **Callback Testing**: Verify callback invocation and parameters

### Integration Testing

1. **Task Coordination**: Test multi-task event generation
2. **Timeout Handling**: Test periodic event processing
3. **Resource Management**: Test initialization and cleanup
4. **Error Recovery**: Test error conditions and recovery

## Future Enhancements

### Potential Extensions

1. **Hierarchical States**: Sub-states within main states
2. **History States**: Remember previous state for transitions
3. **Guard Conditions**: Conditional transitions based on context
4. **Action Tables**: Separate actions from transition logic
5. **State Persistence**: Save/restore state across resets

### Real-World Applications

1. **BLE Connection Management**: Replace demo with actual BLE stack integration
2. **WiFi State Machine**: Adapt for WiFi connection lifecycle
3. **Protocol State Machine**: Implement communication protocol states
4. **Device Power States**: Manage sleep/wake transitions
5. **Hardware Control**: Manage peripheral device states

This design provides a solid foundation for complex state management in embedded systems while maintaining clarity, testability, and extensibility.
