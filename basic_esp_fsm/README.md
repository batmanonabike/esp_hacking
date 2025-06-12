# Basic ESP32 Finite State Machine Demo

## Overview

This project demonstrates a simple finite state machine (FSM) implementation for ESP32 using function pointers to handle different behaviors based on the current state. The FSM simulates a connection-oriented system with four distinct states and various events that trigger state transitions.

## State Machine Design

### States
- **DISCONNECTED**: Initial state, no active connection
- **CONNECTING**: Attempting to establish a connection
- **CONNECTED**: Active connection established
- **DISCONNECTING**: Gracefully terminating connection

### Events
- **CONNECT_REQUEST**: Request to initiate a connection
- **CONNECTION_SUCCESS**: Connection established successfully
- **CONNECTION_FAILED**: Connection attempt failed
- **DISCONNECT_REQUEST**: Request to terminate connection
- **CONNECTION_LOST**: Unexpected loss of connection
- **TIMEOUT**: Periodic timeout event for housekeeping

### State Transitions

```
DISCONNECTED --[CONNECT_REQUEST]--> CONNECTING
CONNECTING --[CONNECTION_SUCCESS]--> CONNECTED
CONNECTING --[CONNECTION_FAILED/TIMEOUT]--> DISCONNECTED
CONNECTING --[DISCONNECT_REQUEST]--> DISCONNECTING
CONNECTED --[DISCONNECT_REQUEST]--> DISCONNECTING
CONNECTED --[CONNECTION_LOST/CONNECTION_FAILED]--> DISCONNECTED
DISCONNECTING --[CONNECTION_LOST/TIMEOUT]--> DISCONNECTED
```

## Architecture Features

### Function Pointers
Each state has a dedicated handler function accessed via function pointers stored in the FSM context:
- `fsm_state_disconnected_handler()`
- `fsm_state_connecting_handler()`
- `fsm_state_connected_handler()`
- `fsm_state_disconnecting_handler()`

### Callback Pattern
The FSM supports structured callbacks for application-level event handling:
- `on_state_changed`: Notified when state transitions occur
- `on_event_processed`: Notified when events are processed
- `on_connection_data`: Notified when connection data is available

### State Information
The FSM maintains arbitrary state information:
- Connection ID (unique identifier)
- Connection attempt counter
- Connected time in seconds
- Data transfer statistics (bytes sent/received)
- Security status flag

### Event-Driven Architecture
- Uses FreeRTOS event groups for synchronization
- Non-blocking event processing
- Error handling with appropriate ESP-IDF error codes

## Implementation Details

### Core Files
- `fsm.h`: FSM interface definitions and structures
- `fsm.c`: FSM implementation with state handlers
- `main.c`: Demo application showcasing the FSM

### Key Components

#### FSM Context Structure
```c
typedef struct fsm_context_t {
    fsm_state_t currentState;
    fsm_state_func_t stateHandlers[FSM_STATE_MAX];
    fsm_state_info_t stateInfo;
    fsm_callbacks_t callbacks;
    EventGroupHandle_t eventGroup;
    const char *szTag;
} fsm_context_t;
```

#### State Handler Function Pointer
```c
typedef esp_err_t (*fsm_state_func_t)(struct fsm_context_t *pContext, fsm_event_t event);
```

### Error Handling
- All functions return `esp_err_t` for consistent error reporting
- Parameter validation with NULL pointer checks
- Structured logging with appropriate log levels
- Event group bits for synchronization and error signaling

## Demo Application

The demo application creates two tasks:

1. **Event Generator Task**: Sends a predefined sequence of events to demonstrate state transitions
2. **Timeout Generator Task**: Sends periodic timeout events based on current state

### Demo Sequence
1. Request connection → transition to CONNECTING
2. Connection succeeds → transition to CONNECTED
3. Timeout events → simulate data transfer while connected
4. Request disconnection → transition to DISCONNECTING
5. Complete disconnection → return to DISCONNECTED
6. Repeat with connection failure scenario

## Building and Running

### Prerequisites
- ESP-IDF development environment
- ESP32 development board

### Build Commands
```bash
cd basic_esp_fsm
idf.py build
idf.py -p COMx flash monitor
```

### Expected Output
The demo will display:
- State transition logs with detailed information
- Event processing logs with results
- Connection statistics and data transfer simulation
- Callback notifications for state changes and events

## Key Learning Points

1. **Function Pointers**: Demonstrates how to use function pointers for runtime behavior selection
2. **State Machine Design**: Shows proper state transition handling and event validation
3. **Callback Patterns**: Illustrates structured callback implementation with context passing
4. **Event-Driven Programming**: Uses FreeRTOS event groups for synchronization
5. **Error Handling**: Implements consistent ESP-IDF error handling patterns
6. **Memory Management**: Uses proper initialization and cleanup patterns

## Extensions

This basic FSM can be extended for real-world applications:
- BLE connection management
- WiFi state handling
- Protocol state machines
- Hardware peripheral state control
- Power management states

## Compliance with Coding Standards

This project follows the established ESP32 BLE development workspace coding standards:
- Allman-style brackets
- `bat_` function prefixes (where applicable)
- Structured error handling with ESP-IDF patterns
- Event-driven architecture with FreeRTOS primitives
- Proper resource management (RAII pattern)
- Comprehensive logging with appropriate levels
