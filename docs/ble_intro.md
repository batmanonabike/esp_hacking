# Bluetooth Low Energy (BLE) Introduction

Bluetooth Low Energy (BLE) is a wireless communication protocol designed for low power consumption, making it suitable for IoT devices like smart sensors and switches. It operates differently from Bluetooth Classic.

## Key Concepts for Programmers

### Layered Architecture
BLE has a layered architecture:
1.  **Application Layer**: Where your application logic resides.
2.  **Host Layer**: Implements core BLE protocols (L2CAP, GATT/ATT, SMP, GAP) and provides APIs to the application.
3.  **Controller Layer**: Consists of the Physical Layer (PHY) and Link Layer (LL), interacting directly with the hardware.
    *   The Host and Controller can be on the same chip (VHCI) or separate (HCI via UART, USB, SDIO).

### Generic Access Profile (GAP)
GAP defines how BLE devices discover, connect, and manage connections.
*   **Roles & States**:
    *   **Idle**: Standby state.
    *   **Device Discovery**:
        *   **Advertiser**: Broadcasts presence, indicating if connectable.
        *   **Scanner**: Listens for advertisements.
        *   **Initiator**: A scanner that intends to connect to an advertiser.
    *   **Connection**:
        *   **Peripheral (Slave)**: The advertiser after a connection is established.
        *   **Central (Master)**: The initiator after a connection is established.
*   **Connection Process**:
    1.  Advertiser sends advertising packets.
    2.  Scanner detects advertiser.
    3.  If connectable and desired, scanner (now initiator) sends a Connection Request.
    4.  If advertiser accepts (e.g., not using a Filter Accept List or initiator is on the list), connection is established.
*   **Network Topologies**:
    *   **Connected Topology**: Devices connected, each playing one role.
    *   **Multi-role Topology**: At least one device plays both central and peripheral roles.
    *   **Broadcast Topology (Connectionless)**:
        *   **Broadcaster**: Sends data, does not accept connections.
        *   **Observer**: Receives advertising data, does not initiate connections.

### Generic Attribute Profile (GATT) / Attribute Protocol (ATT)
GATT and ATT define how data is structured and exchanged over a BLE connection using a server/client model.
*   **Attribute Protocol (ATT)**:
    *   Data is stored as **attributes** on a GATT Server.
    *   A GATT Client accesses these attributes.
    *   Attribute structure:
        *   **Handle**: Unique ID (index in an attribute table).
        *   **Type**: UUID (16-bit for SIG-defined, 128-bit common for vendor-defined).
        *   **Value**: The actual data.
        *   **Permissions**: Read, write, notify, indicate permissions.
*   **Generic Attribute Profile (GATT)**:
    *   Builds on ATT, defining:
        *   **Characteristic**: A piece of user data. Composed of:
            *   Characteristic Declaration Attribute.
            *   Characteristic Value Attribute (holds the actual data).
            *   Optional Characteristic Descriptor Attributes (e.g., CCCD for enabling notifications/indications).
        *   **Service**: A collection of related characteristics. Defined by a Service Declaration Attribute. Services can include other services.
        *   **Profile**: A predefined collection of services for a specific use case (e.g., Heart Rate Profile).
    *   **GATT Server**: Device that stores and manages characteristics.
    *   **GATT Client**: Device that accesses characteristics on a GATT Server.

## Links
*   [ESP-IDF BLE Introduction](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-introduction.html)
*   [Bluetooth SIG Assigned Numbers (for UUIDs)](https://www.bluetooth.com/specifications/assigned-numbers/)
*   [ESP-IDF Device Discovery](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-device-discovery.html)
*   [ESP-IDF Connection](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-connection.html)
*   [ESP-IDF Data Exchange](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ble/get-started/ble-data-exchange.html)
