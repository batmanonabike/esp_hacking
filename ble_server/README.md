# ble_server

This project demonstrates a BLE GATT server using the Bitmans library. The main logic is implemented in `bitmans_ble_server.c` within the shared `bitmans_lib` component.

## Structure
- `main/main.c`: Application entry point, initializes and runs the BLE server.
- `main/CMakeLists.txt`: Component registration for the app.
- `CMakeLists.txt`: Project configuration.

## Usage
Build and flash as a standard ESP-IDF project.
