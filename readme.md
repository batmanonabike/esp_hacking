# General Instructions

## Windows Installation

Take from [espressif doc](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/windows-setup.html)

1. Ensure you have the drivers installed (e.g. CH340).  You should see something like: `USB-SERIAL CH340` in device manager when your ESP is connect.
You can the find the COM port from device manager.  Mine was COM11.
2. Install the framework from [here](https://dl.espressif.com/dl/esp-idf/?idf=4.4).  I used `v5.3.1 - Offline Installer`.
3. Use the `ESP-IDF CMD' from the start menu.
4. Copy an example, such as hello_world (`C:\Espressif\frameworks\esp-idf-v5.3.1\examples\get-started\hello_world`) to a working directory.

5. CD to the working directory, set the build target and base configuration.
```bash
cd C:\source\repos\esp_hacking\hello_world
idf.py set-target esp32
idf.py menuconfig
```
Menuconfig allows you to set Wifi network name, password, etc.

6.  Build your project
```bash
idf.py build
```

7. Flash the binaries to the device.
```bash
idf.py -p COM11 flash
```
Replace COM11 with your own port.
This will reboot and start your app.

8.  Monitor its output.
```bash
idf.py -p COM11 monitor
```
This captures output from the device for logging.

## Starting new projects

vscode tip:
1. Use the `ESP-IDF CMD' from the start menu.
2. code .
This ensure that all the environment variables are used.

```
mkdir mycoolproject
cd mycoolproject
idf.py create-project .
idf.py create-project WifiConnect
```

## Other commands
```
idf.py fullclean
idf.py clean
idf.py reconfigure

idf.py size
idf.py size-components
```

## Other notes:

### Opening a workspace, such as `bitman_windows.code-workspace` is useful for allowing sub projects for the ESP-IDF vscode extension.

### If you find that compilations are missing header files and you have added the dependencies...

Ensure that menuconfig is enabling the required feature in the TOP level project (e.g. BLE).  Header files can be hidden otherwise.
```
idf.py menuconfig
idf.py fullclean
idf.py build
```
Note that menuconfig alters `sdkconfig` which should probably be committed to source control.

E.g. Enabling bluetooth in menuconfig made these changes:
```
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_CONTROLLER_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
```

## Project Documentation

- [BLE Introduction](./docs/ble_intro.md)

## Filtering Log Output

NOTE: **This command line isn't currently working in my environment**

To filter log output to see only specific components when using `idf.py monitor`:

The command is `idf.py -p <PORT> monitor --print-filter="TAG:LEVEL"`.

**Example:** To see only logs from the tag `bat_lib:wifi_logging`:

1.  **Try with a specific level first (e.g., `I` for INFO, `V` for VERBOSE):**
    This helps identify if the `*` wildcard is causing issues with the monitor script.
    ```bash
    # Show INFO (and higher) logs for bat_lib:wifi_logging
    idf.py -p COM11 monitor --print-filter="bat_lib:wifi_logging:I"
    ```
    ```bash
    # Show VERBOSE (and higher) logs for bat_lib:wifi_logging
    idf.py -p COM11 monitor --print-filter="bat_lib:wifi_logging:V"
    ```

2.  **If a specific level works, and you need all levels for that tag (equivalent to `*`):**
    Using `V` (VERBOSE) is often equivalent to seeing all messages for that tag. If `*` specifically causes a crash in `idf_monitor.py`, using `V` is a good alternative.
    ```bash
    idf.py -p COM11 monitor --print-filter="bat_lib:wifi_logging:V"
    ```

3.  **To silence other tags and only show yours:**
    ```bash
    # Show only VERBOSE messages from bat_lib:wifi_logging, and ERROR from others
    idf.py -p COM11 monitor --print-filter="*:E,bat_lib:wifi_logging:V"
    ```
    ```bash
    # Show only VERBOSE messages from bat_lib:wifi_logging, and silence all others
    idf.py -p COM11 monitor --print-filter="*:N,bat_lib:wifi_logging:V"
    ```

**If command-line filtering still causes errors in `idf_monitor.py`:**

Consider setting log levels directly in your C code (e.g., in `app_main`):
```c
#include "esp_log.h"

void app_main(void) {
    // Set all other tags to ERROR level
    esp_log_level_set("*", ESP_LOG_ERROR);
    // Set your specific tag to VERBOSE
    esp_log_level_set("bat_lib:wifi_logging", ESP_LOG_VERBOSE);

    // ... rest of your app_main code
}
```
This requires a recompile and flash.

You can also configure log levels via `idf.py menuconfig` under `Component config` -> `Log output`.

Also note that you can reduce binary size wrt logging but *ONLY* via menuconfig/sdkconfig.

`sdkconfig`
```
CONFIG_LOG_DEFAULT_LEVEL_ERROR=y
CONFIG_LOG_DEFAULT_LEVEL=1
```

## JTAG Debugging

Make sure to have first *selected a project in the ESP-IDF extension*! (OpenOCD Server will fail otherwise).
Ensure that `OpenOCD Server` can run.
Once that is running you can `Debug` from the `ESP-IDF: Explorer` *or* from standard vscode menus.
You may need to use the 'ESP-IDF: Add vscode Configuration Folder' for each project before you can debug.

Getting this to work was a bit fiddly, but I suspect it was all down to drivers.

The working driver was installed with `zadig-2.9.exe` as `WinUSB`.
See also:
File:`my_esp_prog.cfg` <-- I'm not sure if this was needed.
File: `settings.json` and `idf.openOcdConfigs` <-- I'm not sure if this was needed either.

```
"idf.openOcdConfigs": [
        "C:/source/repos/esp_hacking_33/my_esp_prog.cfg",
        "${env:IDF_TOOLS_PATH}/tools/openocd-esp32/v0.12.0-esp32-20240318/openocd-esp32/share/openocd/scripts/board/esp32-wrover-kit-1.8v.cfg"
    ],
```

https://medium.com/@manuel.bl/low-cost-esp32-in-circuit-debugging-dbbee39e508b
https://docs.espressif.com/projects/esp-iot-solution/en/latest/hw-reference/ESP-Prog_guide.html

Also note that *monitoring* works over the UART (COM port) not JTAG (which doesn't use a COM port).
To permit both debugging and monitoring, you need two usb cables connected. One to the ESP Prog and one to the ESP32 UART.

## ESP Tool and board identification

https://espressif.github.io/esptool-js/

## Bitmans ESP32 devices

Regarding the chipsets:

- Labels: **A, B** hardware is esp32-devkitC-32, `WROOM-32`, `CH340C`
- Labels: **C, D, E**: `WROOM-32`, `CP2102`
I think I blew up `D` by drunken incorrect battery insertion.
- Label: **F**: `TODO`, no UART, Need ESP Prog + JTAG (no USB)
- Label: **G**: `WROOM-S3-1`, ESP32-S3 (QFN56) (revision v0.2)
Embedded PSRAM 8MB (AP_3v3) [Build issues to address]
- Label: **H**: `TODO`
- Label: **I**: ESP32-S3 Sense, `ESP32-S3` (QFN56) (revision v0.2)
Wi-Fi,BLE,Embedded PSRAM 8MB (AP_3v3)

## Troubleshooting
This is needed for BLE on ESP32-*S3* (for bat_lib)...

MenuConfig:
```
	"Enable BLE 4.2 features"
```
Or (directly in `sdkconfig`):
```
	CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y
```
Then full clean.

## VSCode Intellisense.

You need to provide vscode information *seperate* from the build chain for intellisense to work.

In `.vscode/c_cpp_properties.json`:
```
{
    "configurations": [
        {
            "name": "BitmansESP-IDF",
            "compilerPath": "${env:IDF_TOOLS_PATH}/tools/xtensa-esp-elf/esp-13.2.0_20240530/xtensa-esp-elf/bin/xtensa-esp32-elf-gcc.exe",
```
Also note that there is a *default* compiler path (`C_Cpp.default.compilerPath`) in `.vscode/settings.json`.

### Example
On my windows PC:
```
${env:IDF_TOOLS_PATH}/tools/xtensa-esp-elf/esp-13.2.0_20240530/xtensa-esp-elf/bin/xtensa-esp32-elf-gcc.exe
```
maps to:
```
C:\Espressif\tools\xtensa-esp-elf\esp-13.2.0_20240530\xtensa-esp-elf\bin\xtensa-esp32-elf-gcc.exe
```
And:
```
IDF_TOOLS_PATH=C:\Espressif
IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.3.1
```

You can use the command pallette to select the correct configiuration...
```
C/C++: Select IntelliSense Configuration
```
Then the `name` in `.vscode/c_cpp_properties.json` (e.g. `BitmansESP-IDF`) should appear in the status bar (bottom-right corner of VS Code).
Alternatively you click on that part of the status bar to select the configuration.

### Debug Compiler Path for Intellisense
Also note that there are *debug* versions of the gcc executable...

```
C:\Espressif\tools\xtensa-esp-elf-gdb\14.2_20240403\xtensa-esp-elf-gdb\bin
    xtensa-esp32-elf-gdb.exe
    xtensa-esp32s2-elf-gdb.exe
    xtensa-esp32s3-elf-gdb.exe
```

Though when using an ESP Prog for debugging, I've yet found a purpose for this, debugging seems to work.

### Log from a an active scan against ble_server

```C
esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
```

```
I (5704) bat_lib:ble_client: ESP_GAP_BLE_SCAN_RESULT_EVT
I (5714) bat_lib:ble_client: Comparing advname: BitmansGATTS_0, BitmansGATTS_0
I (5724) ble_client_app: === Using Comprehensive BLE Logging ===
I (5724) bat_lib:ble_client_logging: === COMPREHENSIVE BLE DEVICE SCAN RESULT ===
I (5734) bat_lib:ble_client_logging: Device Address: f4:65:0b:57:5c:3a
I (5744) bat_lib:ble_client_logging: RSSI: -58 dBm
I (5754) bat_lib:ble_client_logging: Address Type: Public
I (5754) bat_lib:ble_client_logging: Device Type: BLE
I (5764) bat_lib:ble_client_logging: Advertising Data Length: 31 bytes
I (5774) bat_lib:ble_client_logging: Scan Response Length: 16 bytes
I (5774) bat_lib:ble_client_logging: Raw Advertising Data:
I (5784) bat_lib:ble_client_logging: 02 01 06 03 19 44 09 11 07 12 56 12 56 78 56 34
I (5794) bat_lib:ble_client_logging: 12 12 34 56 78 9a bc de f0 05 12 06 00 10 00
I (5804) bat_lib:ble_client_logging: Advertised Name: BitmansGATTS_0
I (5804) bat_lib:ble_client_logging: Flags (len 1): 0x06
I (5814) bat_lib:ble_client_logging:   - LE General Discoverable Mode
I (5824) bat_lib:ble_client_logging:   - BR/EDR Not Supported
I (5834) bat_lib:ble_client_logging: Appearance: 0x0944
I (5834) bat_lib:ble_client_logging: === SERVICE UUIDs ===
I (5844) bat_lib:ble_client_logging: Complete 128-bit Service UUIDs (count 1):
I (5854) bat_lib:ble_client_logging:   - f0debc9a-7856-3412-1234-567856125612
I (5854) bat_lib:ble_client_logging: === SCAN RESPONSE DATA ===
I (5864) bat_lib:ble_client_logging: Scan Response Data (len 16):
I (5874) bat_lib:ble_client_logging: 0f 09 42 69 74 6d 61 6e 73 47 41 54 54 53 5f 30
I (5884) bat_lib:ble_client_logging: ============================================
I (5894) ble_client_app: === Debug Analysis for Device #1 ===
I (5894) bat_lib:ble_client_logging: === DEBUG esp_ble_resolve_adv_data FUNCTION ===
I (5904) bat_lib:ble_client_logging: Device Address: f4:65:0b:57:5c:3a
I (5914) bat_lib:ble_client_logging: Advertising Data Length: 31
I (5924) bat_lib:ble_client_logging: Scan Response Length: 16
I (5924) bat_lib:ble_client_logging: Raw advertising data:
I (5934) bat_lib:ble_client_logging: 02 01 06 03 19 44 09 11 07 12 56 12 56 78 56 34
I (5944) bat_lib:ble_client_logging: 12 12 34 56 78 9a bc de f0 05 12 06 00 10 00
I (5954) bat_lib:ble_client_logging: Type 0x01 (FLAGS): ptr=0x3ffce1de, len=1
I (5954) bat_lib:ble_client_logging:   Data:
I (5964) bat_lib:ble_client_logging: 06
I (5964) bat_lib:ble_client_logging: Type 0x02 (16SRV_PART): ptr=0x0, len=0
I (5974) bat_lib:ble_client_logging: Type 0x03 (16SRV_CMPL): ptr=0x0, len=0
I (5984) bat_lib:ble_client_logging: Type 0x04 (32SRV_PART): ptr=0x0, len=0
I (5994) bat_lib:ble_client_logging: Type 0x05 (32SRV_CMPL): ptr=0x0, len=0
I (6004) bat_lib:ble_client_logging: Type 0x06 (128SRV_PART): ptr=0x0, len=0
I (6004) bat_lib:ble_client_logging: Type 0x07 (128SRV_CMPL): ptr=0x3ffce1e5, len=16
I (6014) bat_lib:ble_client_logging:   Data:
I (6024) bat_lib:ble_client_logging: 12 56 12 56 78 56 34 12 12 34 56 78 9a bc de f0
I (6034) bat_lib:ble_client_logging: Type 0x08 (NAME_SHORT): ptr=0x0, len=0
I (6044) bat_lib:ble_client_logging: Type 0x09 (NAME_CMPL): ptr=0x3ffce1fd, len=14
I (6044) bat_lib:ble_client_logging:   Data:
I (6054) bat_lib:ble_client_logging: 42 69 74 6d 61 6e 73 47 41 54 54 53 5f 30
I (6064) bat_lib:ble_client_logging: Type 0x0a (TX_PWR): ptr=0x0, len=0
I (6074) bat_lib:ble_client_logging: Type 0x0d (DEV_CLASS): ptr=0x0, len=0
I (6074) bat_lib:ble_client_logging: Type 0x16 (SERVICE_DATA): ptr=0x0, len=0
I (6084) bat_lib:ble_client_logging: Type 0x19 (APPEARANCE): ptr=0x3ffce1e1, len=2
I (6094) bat_lib:ble_client_logging:   Data:
I (6104) bat_lib:ble_client_logging: 44 09
I (6104) bat_lib:ble_client_logging: Type 0x1a (ADV_INT): ptr=0x0, len=0
I (6114) bat_lib:ble_client_logging: Type 0x20 (32SERVICE_DATA): ptr=0x0, len=0
I (6124) bat_lib:ble_client_logging: Type 0x21 (128SERVICE_DATA): ptr=0x0, len=0
I (6134) bat_lib:ble_client_logging: Type 0xff (MANUFACTURER_SPECIFIC): ptr=0x0, len=0
I (6134) bat_lib:ble_client_logging: ===============================================
I (6144) bat_lib:ble: Looking for UUID: f0debc9a-7856-3412-1234-567856125612
I (6154) bat_lib:ble_client: Resolved advert length: 16
I (6164) bat_lib:ble: Checking UUID: f0debc9a-7856-3412-1234-567856125612
I (6174) bat_lib:ble_client: FOUND UUID
I (6174) bat_lib:ble_client: Found custom service UUID in complete list
I (6184) ble_client_app: Device with custom service UUID found. BDA: f4:65:0b:57:5c:3a
I (6194) bat_lib:ble_client: Stopping BLE scan...
```

### Log from a an active scan against ble_server

```C
esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_PASSIVE,
```

```
I (4643) ble_client_app: === Using Comprehensive BLE Logging ===
I (4653) bat_lib:ble_client_logging: === COMPREHENSIVE BLE DEVICE SCAN RESULT ===
I (4663) bat_lib:ble_client_logging: Device Address: f4:65:0b:57:5c:3a
I (4663) bat_lib:ble_client_logging: RSSI: -50 dBm
I (4673) bat_lib:ble_client_logging: Address Type: Public
I (4683) bat_lib:ble_client_logging: Device Type: BLE
I (4683) bat_lib:ble_client_logging: Advertising Data Length: 31 bytes
I (4693) bat_lib:ble_client_logging: Scan Response Length: 0 bytes
I (4703) bat_lib:ble_client_logging: Raw Advertising Data:
I (4703) bat_lib:ble_client_logging: 02 01 06 03 19 44 09 11 07 12 56 12 56 78 56 34
I (4713) bat_lib:ble_client_logging: 12 12 34 56 78 9a bc de f0 05 12 06 00 10 00
I (4723) bat_lib:ble_client_logging: Advertised Name:
I (4733) bat_lib:ble_client_logging: Flags (len 1): 0x06
I (4733) bat_lib:ble_client_logging:   - LE General Discoverable Mode
I (4743) bat_lib:ble_client_logging:   - BR/EDR Not Supported
I (4753) bat_lib:ble_client_logging: Appearance: 0x0944
I (4753) bat_lib:ble_client_logging: === SERVICE UUIDs ===
I (4763) bat_lib:ble_client_logging: Complete 128-bit Service UUIDs (count 1):
I (4773) bat_lib:ble_client_logging:   - f0debc9a-7856-3412-1234-567856125612
I (4783) bat_lib:ble_client_logging: ============================================
I (4783) ble_client_app: === Debug Analysis ===
I (4793) bat_lib:ble_client_logging: === DEBUG esp_ble_resolve_adv_data FUNCTION ===
I (4803) bat_lib:ble_client_logging: Device Address: f4:65:0b:57:5c:3a
I (4813) bat_lib:ble_client_logging: Advertising Data Length: 31
I (4813) bat_lib:ble_client_logging: Scan Response Length: 0
I (4823) bat_lib:ble_client_logging: Raw advertising data:
I (4833) bat_lib:ble_client_logging: 02 01 06 03 19 44 09 11 07 12 56 12 56 78 56 34
I (4843) bat_lib:ble_client_logging: 12 12 34 56 78 9a bc de f0 05 12 06 00 10 00
I (4843) bat_lib:ble_client_logging: Type 0x01 (FLAGS): ptr=0x3ffcdc5a, len=1
I (4853) bat_lib:ble_client_logging:   Data:
I (4863) bat_lib:ble_client_logging: 06
I (4863) bat_lib:ble_client_logging: Type 0x02 (16SRV_PART): ptr=0x0, len=0
I (4873) bat_lib:ble_client_logging: Type 0x03 (16SRV_CMPL): ptr=0x0, len=0
I (4883) bat_lib:ble_client_logging: Type 0x04 (32SRV_PART): ptr=0x0, len=0
I (4893) bat_lib:ble_client_logging: Type 0x05 (32SRV_CMPL): ptr=0x0, len=0
I (4893) bat_lib:ble_client_logging: Type 0x06 (128SRV_PART): ptr=0x0, len=0
I (4903) bat_lib:ble_client_logging: Type 0x07 (128SRV_CMPL): ptr=0x3ffcdc61, len=16
I (4913) bat_lib:ble_client_logging:   Data:
I (4923) bat_lib:ble_client_logging: 12 56 12 56 78 56 34 12 12 34 56 78 9a bc de f0
I (4933) bat_lib:ble_client_logging: Type 0x08 (NAME_SHORT): ptr=0x0, len=0
I (4933) bat_lib:ble_client_logging: Type 0x09 (NAME_CMPL): ptr=0x0, len=0
I (4943) bat_lib:ble_client_logging: Type 0x0a (TX_PWR): ptr=0x0, len=0
I (4953) bat_lib:ble_client_logging: Type 0x0d (DEV_CLASS): ptr=0x0, len=0
I (4963) bat_lib:ble_client_logging: Type 0x16 (SERVICE_DATA): ptr=0x0, len=0
I (4973) bat_lib:ble_client_logging: Type 0x19 (APPEARANCE): ptr=0x3ffcdc5d, len=2
I (4973) bat_lib:ble_client_logging:   Data:
I (4983) bat_lib:ble_client_logging: 44 09
I (4983) bat_lib:ble_client_logging: Type 0x1a (ADV_INT): ptr=0x0, len=0
I (4993) bat_lib:ble_client_logging: Type 0x20 (32SERVICE_DATA): ptr=0x0, len=0
I (5003) bat_lib:ble_client_logging: Type 0x21 (128SERVICE_DATA): ptr=0x0, len=0
I (5013) bat_lib:ble_client_logging: Type 0xff (MANUFACTURER_SPECIFIC): ptr=0x0, len=0
I (5023) bat_lib:ble_client_logging: ===============================================
I (5033) ble_client_app: Device with custom service UUID found. BDA: f4:65:0b:57:5c:3a
I (5043) bat_lib:ble_client: Stopping BLE scan...
```

Noting that there is NO advert name here because we opted to send the advert name in the `scan response` packet.
Only scans in active mode get the scan response packet.

### Scratch pad

For: `c_cpp_properties.json`

```
SET BAT_ESP32_GCC=%IDF_TOOLS_PATH%\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin\xtensa-esp32-elf-gcc.exe
SET BAT_ESP32_GCC=%IDF_TOOLS_PATH%\tools\xtensa-esp-elf\esp-13.2.0_20240530\xtensa-esp-elf/bin/xtensa-esp32-elf-gcc.exe
```
