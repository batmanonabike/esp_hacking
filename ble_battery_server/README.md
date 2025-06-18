# Battery Service BLE Server

This project implements a standard Bluetooth Low Energy Battery Service on an ESP32 microcontroller using the ESP-IDF framework.

## Description

The BLE Battery Service server exposes a standard Battery Level characteristic that can be read by BLE client devices. It implements proper notification handling with Client Characteristic Configuration Descriptors (CCCDs) to allow clients to subscribe to battery level changes.

## Features

- Standard Battery Service (UUID: 0x180F)
- Battery Level Characteristic (UUID: 0x2A19)
- Support for read operations and notifications
- Proper CCCD implementation for notification support
- Periodic battery level updates (simulated)
- Visual status indication via LED
- Automatic advertising restart on disconnection

## Hardware Requirements

- ESP32 development board
- LED for status indication (optional)

## Build and Flash

```
idf.py build
idf.py -p [PORT] flash
idf.py -p [PORT] monitor
```

## Expected Output

When running, the device will advertise as "Battery Monitor" and clients can connect to read the battery level or subscribe to notifications for battery level changes.

## Battery Level Simulation

This demo simulates a battery that slowly discharges and then quickly recharges, cycling between 100% and 20%.
