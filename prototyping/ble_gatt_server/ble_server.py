import asyncio
import logging
import uuid

from bleak import BleakServer, BleakClient
from bleak.backends.characteristic import GattCharacteristic
from bleak.backends.service import GattService
from bleak.backends.descriptor import GattDescriptor

# --- Define your custom UUIDs here ---
# Replace these with your generated UUIDs
CUSTOM_SERVICE_UUID = "0000AB00-0000-1000-8000-00805F9B34FB"
# For characteristics, you'd typically make them readable, writable, or notify/indicate
CUSTOM_CHAR_UUID_READ = "0000AB01-0000-1000-8000-00805F9B34FB"
CUSTOM_CHAR_UUID_WRITE = "0000AB02-0000-1000-8000-00805F9B34FB"
CUSTOM_CHAR_UUID_NOTIFY = "0000AB03-0000-1000-8000-00805F9B34FB"

# --- Global variable to hold some data for the characteristic ---
# This is a very simple way to store characteristic data;
# in a real app, you might use a class or a more robust data structure.
char_read_value = b"Hello from Server!"
char_notify_value = b"Initial Notify Value"

# --- Characteristic Read/Write/Notify Handlers ---
async def read_request_handler(characteristic: GattCharacteristic, **kwargs) -> bytes:
    global char_read_value
    logging.info(f"Client reading characteristic {characteristic.uuid}. Value: {char_read_value.decode()}")
    return char_read_value

async def write_request_handler(characteristic: GattCharacteristic, value: bytearray, **kwargs):
    global char_read_value # Or a different variable if this char is write-only
    logging.info(f"Client wrote to characteristic {characteristic.uuid}: {value.decode()}")
    # char_read_value = value # Example: update the readable value upon write
    # For a write-only characteristic, you'd process the 'value' here.

# --- Main Server Logic ---
async def run_server():
    logging.basicConfig(level=logging.INFO)
    logging.info("Starting BLE server...")

    # Create a service
    service = GattService(CUSTOM_SERVICE_UUID)

    # Create a readable characteristic
    char_read = GattCharacteristic(
        CUSTOM_CHAR_UUID_READ,
        ["read"], # Properties: read, write, notify, indicate, etc.
        read_callback=read_request_handler,
    )
    service.add_characteristic(char_read)

    # Create a writable characteristic
    char_write = GattCharacteristic(
        CUSTOM_CHAR_UUID_WRITE,
        ["write", "write-without-response"],
        write_callback=write_request_handler,
    )
    service.add_characteristic(char_write)

    # Create a characteristic for notifications
    # For notifications, the server can push data to the client when it changes.
    # The client needs to subscribe to notifications first.
    char_notify = GattCharacteristic(
        CUSTOM_CHAR_UUID_NOTIFY,
        ["notify"],
    )
    # Add a Client Characteristic Configuration Descriptor (CCCD)
    # This is required for notifications/indications.
    cccd = GattDescriptor(
        "00002902-0000-1000-8000-00805f9b34fb", # Standard CCCD UUID
        ["read", "write"] # Client reads/writes this to enable/disable notifications
    )
    char_notify.add_descriptor(cccd)
    service.add_characteristic(char_notify)


    async with BleakServer(service, advertisement_data={"local_name": "MyESP32_DesktopServer", "service_uuids": [CUSTOM_SERVICE_UUID]}) as server:
        logging.info(f"Server started. Advertising '{server.name}' with service UUID {CUSTOM_SERVICE_UUID}")
        logging.info(f"Device Address: {await server.get_bluetooth_address()}") # May not work on all platforms or without admin

        # Keep the server running. In a real app, you might have other tasks.
        # This loop also demonstrates how to send notifications.
        notify_counter = 0
        while True:
            await asyncio.sleep(5) # Wait for 5 seconds
            if server.is_connected:
                # If a client is connected and has subscribed to notifications for char_notify
                # (i.e., written to its CCCD), then we can send a notification.
                # BleakServer doesn't directly expose which clients are subscribed to which characteristic's notifications.
                # You'd typically manage subscription state within your characteristic's logic or CCCD write handler.
                # For simplicity, we'll just try to notify if connected.
                try:
                    global char_notify_value
                    notify_counter += 1
                    new_notify_value = f"Notify update {notify_counter}".encode()
                    char_notify_value = new_notify_value
                    logging.info(f"Sending notification for {char_notify.uuid}: {char_notify_value.decode()}")
                    await server.notify(char_notify, char_notify_value)
                except Exception as e:
                    logging.error(f"Error sending notification: {e}")
            else:
                logging.info("No client connected. Waiting for connection...")


if __name__ == "__main__":
    try:
        asyncio.run(run_server())
    except KeyboardInterrupt:
        logging.info("Server shutting down...")
    except Exception as e:
        logging.error(f"An error occurred: {e}")
