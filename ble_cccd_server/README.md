# BLE CCCD Server Example

This example demonstrates the use of Client Characteristic Configuration Descriptors (CCCDs) in a BLE GATT server on the ESP32. It shows how to create multiple characteristics with CCCDs and handle notifications and indications.

## What is a CCCD?

A Client Characteristic Configuration Descriptor (CCCD) is a special descriptor with UUID 0x2902 that enables or disables notifications or indications for a characteristic. Clients write to this descriptor to indicate whether they want to receive notifications (0x0001), indications (0x0002), or neither (0x0000).

## Features

- Multiple characteristics with different properties
- Notifications and indications using CCCDs
- Automatic CCCD handling in the BLE stack
- Different data types for each characteristic
- Periodic updates to demonstrate notifications
- Indication support with confirmation

## Hardware Required

- An ESP32 development board
- A computer with Bluetooth capabilities (or a smartphone with a BLE scanner app)

## How to Use

1. Connect your ESP32 development board to your computer
2. Build and flash the example: `idf.py -p [PORT] flash`
3. Monitor the output: `idf.py -p [PORT] monitor`
4. Use a BLE scanner app or tool to connect to "CCCD Demo"
5. Enable notifications/indications on the characteristics
6. Observe the periodic data updates
