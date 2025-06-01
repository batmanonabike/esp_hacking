# BLE Client Operations on ESP32

This document outlines the typical steps a BLE client needs to perform to scan for and interact with BLE peripherals, using the ESP-IDF framework.

## Client Workflow

1.  **Initialize BLE Stack**:
    *   Initializes the Bluetooth controller hardware.
    *   Enables BLE mode for the controller.
    *   Initializes and enables Bluedroid, which is the ESP-IDF's Bluetooth stack.
    *   Registers callback functions for GAP (Generic Access Profile) and GATTC (GATT Client) events. These callbacks are essential for handling asynchronous BLE operations.
    *   Registers a *GATTC application profile*.[^1]

2.  **Start Scanning**:
    *   Configures the scan parameters, such as:
        *   `scan_type`: E.g., active scan (requests advertising data) or passive scan.
        *   `own_addr_type`: The public or random address type of the scanning device.
        *   `scan_filter_policy`: Determines which advertising packets are processed (e.g., allow all, or use a filter accept list).
        *   `scan_interval`: How often the scan operation is performed.
        *   `scan_window`: How long the scan lasts during each interval.
    *   The actual command to start scanning, `esp_ble_gap_start_scanning()`, is invoked asynchronously. It's called within the `esp_gap_cb` callback when the `ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT` event is received. This event confirms that the scan parameters set by `esp_ble_gap_set_scan_params()` have been applied.

3.  **Process Scan Results**:
    *   The `esp_gap_cb` callback function is where GAP-related events are handled.
    *   When a BLE advertisement or scan response is received from a nearby device, the `ESP_GAP_BLE_SCAN_RESULT_EVT` event is triggered.
    *   Within the handler for this event, you can access information about the discovered device, most importantly its Bluetooth Device Address (BDA) via `scan_result->scan_rst.bda`. Other information like advertising data and RSSI is also available.
    *   **Action**: Implement logic within the `ESP_GAP_BLE_SCAN_RESULT_EVT` case in `esp_gap_cb` to:
        *   Parse advertising data (e.g., device name, service UUIDs).
        *   Decide if the discovered device is the one you want to connect to.

4.  **Connect to a Device (Optional, after scanning and identifying a target)**:
    *   If your scanning logic identifies a target peripheral:
        *   First, stop the ongoing scan by calling `esp_ble_gap_stop_scanning()`.
        *   The `ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT` event in `esp_gap_cb` will confirm that the scan has stopped.
        *   Then, initiate a connection to the peripheral using `esp_ble_gattc_open()`. You'll need to provide the `gattc_if` (obtained from `ESP_GATTC_REG_EVT`) and the target device's BDA.
    *   **Action**: Call `esp_ble_gap_stop_scanning()` then `esp_ble_gattc_open()` when a desired device is found.

5.  **Handle GATT Client Events**:
    *   Once a connection is initiated, the `esp_gattc_cb` callback function handles GATTC (GATT Client) events. Key events include:
        *   `ESP_GATTC_REG_EVT`: Confirms the GATTC application registration and provides the `gattc_if`[^2] needed for other GATTC operations.
        *   `ESP_GATTC_CONNECT_EVT`: Indicates the low-level BLE link is established.
        *   `ESP_GATTC_OPEN_EVT`: Triggered when the GATT connection to the peripheral is successfully opened (or fails). If successful, you can now proceed with service discovery. MTU (Maximum Transmission Unit) negotiation also typically happens here.
        *   `ESP_GATTC_DISCONNECT_EVT`: Indicates the BLE connection has been terminated.
        *   `ESP_GATTC_SEARCH_RES_EVT`: Reports a service found during service discovery (`esp_ble_gattc_search_service`).
        *   `ESP_GATTC_SEARCH_CMPL_EVT`: Indicates that service discovery is complete.
        *   `ESP_GATTC_GET_CHAR_EVT` (if used): Reports a characteristic found.
        *   `ESP_GATTC_READ_CHAR_EVT`: Contains the data read from a characteristic.
        *   `ESP_GATTC_WRITE_CHAR_EVT`: Confirms a write operation.
        *   `ESP_GATTC_NOTIFY_EVT`: Contains data received via a notification from the peripheral.
    *   **Action**: Implement handlers for these events in `esp_gattc_cb` to manage the connection, discover services and characteristics, and perform read/write/notify operations.

## Summary of Functions to Call

To initiate a scan, the primary sequence from your application code would be:

1.  Initialise the BLE Stack.
2.  Start Scanning.

The subsequent discovery, connection, and data exchange processes are event-driven and handled within the `esp_gap_cb` and `esp_gattc_cb` callback functions.

[^1]: A **GATTC (GATT Client) application profile** is a way to register and manage a distinct instance of a GATT client functionality within your application. When you call `esp_ble_gattc_app_register(app_id)`[^3], you are telling the underlying Bluetooth stack (Bluedroid) that a part of your application intends to act as a GATT client.

[^2]: The `gattc_if` (GATT Client Interface) handle is generated by the BLE stack upon successful GATTC application registration. This handle is delivered via the `ESP_GATTC_REG_EVT` and is crucial for most subsequent GATTC function calls to specify which registered client profile is performing the operation and to route events correctly.

[^3]: GATTC `app_id`s are chosen by the developer. For a single client profile, `0` is common. If managing multiple distinct client functionalities (e.g., connecting to different types of peripherals), use unique small integers (0, 1, 2, etc.) for each `app_id` to differentiate them. These IDs are local to your ESP32 application.
