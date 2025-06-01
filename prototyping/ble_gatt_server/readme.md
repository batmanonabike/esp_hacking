# Python BLE GATT Server Prototype

This directory contains a Python script (`ble_server.py`) that implements a simple BLE GATT server using the `bleak` library. This server is intended for prototyping and testing BLE client applications, such as an ESP32 client.

## Prerequisites

1.  **Python Installation**:
    *   Ensure you have Python installed on your system (version 3.7 or newer is recommended).
    *   You can download Python from [python.org](https://www.python.org/downloads/).
    *   During installation, make sure to check the box that says "Add Python to PATH" or "Add python.exe to Path".

2.  **pip (Python Package Installer)**:
    *   pip is usually installed automatically with Python.
    *   You can verify its installation by opening a terminal or command prompt and typing:
        ```bash
        pip --version
        ```

## Installation of Dependencies

The server script requires the `bleak` library. Install it using pip:

```bash
pip install bleak
```

Depending on your operating system, you might need to install additional dependencies for `bleak` to function correctly (e.g., specific Bluetooth development libraries on Linux). Refer to the [Bleak documentation](https://bleak.readthedocs.io/en/latest/installation.html) for OS-specific requirements.

## Running the Server

1.  **Navigate to the Directory**:
    Open a terminal or command prompt and navigate to this directory:
    ```bash
    cd path\\to\\esp_hacking_22\\prototyping\\ble_gatt_server
    ```
    (Replace `path\\to` with the actual path to the `esp_hacking_22` directory).

2.  **Run the Script**:
    Execute the Python script:
    ```bash
    python ble_server.py
    ```

3.  **Server Output**:
    *   The server will start and log information to the console, including the advertised name ("MyESP32\_DesktopServer") and the custom service UUID it's using.
    *   It will also attempt to log its Bluetooth device address, though this might not be available on all platforms or without administrator/root privileges.
    *   The server will continuously run, waiting for client connections and periodically attempting to send notifications if a client is connected and subscribed.

    Example output:
    ```
    INFO:root:Starting BLE server...
    INFO:root:Server started. Advertising 'MyESP32_DesktopServer' with service UUID 0000ab00-0000-1000-8000-00805f9b34fb
    INFO:root:Device Address: XX:XX:XX:XX:XX:XX
    INFO:root:No client connected. Waiting for connection...
    ```

4.  **Stopping the Server**:
    Press `Ctrl+C` in the terminal where the server is running to stop it.

## Customization

*   **UUIDs**: The script uses predefined custom UUIDs for the service and characteristics (e.g., `CUSTOM_SERVICE_UUID`). You should replace these with your own unique 128-bit UUIDs if you are developing a specific application. These UUIDs must match what your client application (e.g., the ESP32 client) is expecting.
*   **Characteristic Values and Behavior**: The `read_request_handler`, `write_request_handler`, and the notification logic in the `run_server` loop are basic examples. You can modify these to simulate the data and behavior your actual BLE peripheral will have.
