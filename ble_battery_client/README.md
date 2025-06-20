# BLE Battery Client

This project implements a BLE client that connects to a BLE Battery Service server and monitors the battery level.

## Features

- Scans for BLE devices advertising the Battery Service (UUID: 0x180F)
- Connects to the first Battery Service device found
- Reads the Battery Level characteristic (UUID: 0x2A19)
- Subscribes to Battery Level notifications if supported
- Falls back to periodic polling if notifications aren't supported
- Visual feedback via LED patterns
  - Fast blinking: Scanning for devices
  - Breathing pattern: Connected to server
  - Basic blink: Error state

## Hardware Requirements

- ESP32 development board
- LED connected to GPIO pin (configurable)

## Usage

1. Flash the application to your ESP32
2. The device will automatically scan for nearby BLE devices with Battery Service
3. Upon finding a compatible device, it will connect and start monitoring battery level
4. Battery level readings are displayed in the serial monitor

## Serial Monitor Output

Connect to the serial port to see diagnostic messages and battery level readings:

```
I (3752) ble_battery_client: Starting scan for BLE devices
I (4954) ble_battery_client: Found potential battery server: Battery Monitor (index 0)
I (4954) ble_battery_client: Auto-selecting device Battery Monitor (index 0)
I (5710) ble_battery_client: Connecting to battery server: Battery Monitor
I (7048) ble_battery_client: Connected to Battery Monitor
I (7048) ble_battery_client: Searching for Battery Service (UUID: 0x180F)
I (7274) ble_battery_client: Searching for Battery Level characteristic (UUID: 0x2A19)
I (7471) ble_battery_client: Reading initial battery level
I (7644) ble_battery_client: Battery level: 97%
I (7644) ble_battery_client: Enabling battery level notifications
I (12790) ble_battery_client: Battery level notification: 96%
```

## Project Structure

- `main/main.c`: Main application code
- Uses the `bat_gattc_simple` library for simplified BLE client interactions

## Building and Flashing

```bash
# Build the project
idf.py build

# Flash to the ESP32
idf.py -p PORT flash monitor
```

Replace PORT with the serial port of your ESP32 (e.g., COM3 on Windows).

## Dependencies

This project depends on the following components:
- `bat_lib`: Common utility functions
- `bat_ble_lib`: BLE utility functions
- `bat_gattc_simple`: Simplified BLE GATT client API
