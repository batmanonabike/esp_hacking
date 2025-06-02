# BLE Server (GATT Server) Operations on ESP32

This document outlines the steps to implement a BLE GATT server on the ESP32 using the ESP-IDF framework. The server will expose a custom service with a custom service UUID, a custom service name, and a custom characteristic.

## Server Workflow

1. **Initialize BLE Stack**
    * Initialize the Bluetooth controller hardware.
    * Enable BLE mode for the controller.
    * Initialize and enable Bluedroid (ESP-IDF's Bluetooth stack).
    * Register callback functions for GAP (Generic Access Profile) and GATTS (GATT Server) events. These callbacks handle asynchronous BLE operations.
    * Register a *GATTS application profile* using `esp_ble_gatts_app_register(app_id)`.

2. **Define Custom Service and Characteristic**
    * Define a custom 128-bit Service UUID (e.g., `0x12345678-1234-5678-1234-56789abcdef0`).
    * Define a custom Characteristic UUID (e.g., `0xabcdef01-1234-5678-1234-56789abcdef0`).
    * Optionally, define a custom service name for advertising.
    * Set up the attribute table describing the service, characteristic, and their properties (read, write, notify, etc.).

3. **Register and Create the Service**
    * In the GATTS event handler, handle the `ESP_GATTS_REG_EVT` event to create the service using `esp_ble_gatts_create_service()`.
    * Add the characteristic to the service using `esp_ble_gatts_add_char()`.
    * Optionally, add descriptors (e.g., Client Characteristic Configuration Descriptor for notifications).

4. **Start the Service**
    * When the service creation is complete (`ESP_GATTS_CREATE_EVT`), start the service with `esp_ble_gatts_start_service()`.

5. **Advertise the Service**
    * Set up advertising data to include the custom service UUID and service name.
    * Start advertising using `esp_ble_gap_start_advertising()`.
    * Handle advertising events in the GAP event handler.

6. **Handle Client Connections and Requests**
    * In the GATTS event handler, manage events such as:
        * `ESP_GATTS_CONNECT_EVT`: A client has connected.
        * `ESP_GATTS_DISCONNECT_EVT`: A client has disconnected.
        * `ESP_GATTS_READ_EVT`: A client is reading a characteristic.
        * `ESP_GATTS_WRITE_EVT`: A client is writing to a characteristic.
        * `ESP_GATTS_CONF_EVT`: Confirmation of notification/indication sent.
    * Implement logic to respond to read/write requests and send notifications if required.

## Summary of Functions to Call

1. Initialize the BLE stack and register the GATTS application profile.
2. Define and create the custom service and characteristic.
3. Start the service and begin advertising.
4. Handle client connections and GATT requests in the event handler.

## Example Sequence

1. `esp_bt_controller_init()`
2. `esp_bt_controller_enable(ESP_BT_MODE_BLE)`
3. `esp_bluedroid_init()`
4. `esp_bluedroid_enable()`
5. `esp_ble_gatts_register_callback()`
6. `esp_ble_gap_register_callback()`
7. `esp_ble_gatts_app_register(app_id)`
8. In event handler: create service, add characteristic, start service, start advertising

## Notes
* The `app_id` is a developer-chosen integer to identify the GATTS profile.
* Use 128-bit UUIDs for custom services and characteristics.
* The attribute table defines the structure and permissions of your service.
* All operations are event-driven and handled in the registered callbacks.

## Links
* [ESP-IDF BLE GATT Server Example](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/bluetooth.html#ble-gatt-server-demo)
* [ESP-IDF BLE API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gatts.html)
