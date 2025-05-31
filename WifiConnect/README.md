# WifiConnect Project

This is a simple ESP-IDF project that demonstrates WifiConnecting using the BitmansLib component.

## Functionality

The project just attempts to connect to wifi based on a timer:
- It will blink in heartbeat mode when connected.

## Building and Running

To build and flash the project to your ESP32 device:
```
idf.py build
idf.py -p [PORT] flash monitor
```

Replace `[PORT]` with your ESP32 serial port (e.g., COM11 on Windows or /dev/ttyUSB0 on Linux).
