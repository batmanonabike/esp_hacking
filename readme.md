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

**Example:** To see only logs from the tag `bitmans_lib:wifi_logging`:

1.  **Try with a specific level first (e.g., `I` for INFO, `V` for VERBOSE):**
    This helps identify if the `*` wildcard is causing issues with the monitor script.
    ```bash
    # Show INFO (and higher) logs for bitmans_lib:wifi_logging
    idf.py -p COM11 monitor --print-filter="bitmans_lib:wifi_logging:I"
    ```
    ```bash
    # Show VERBOSE (and higher) logs for bitmans_lib:wifi_logging
    idf.py -p COM11 monitor --print-filter="bitmans_lib:wifi_logging:V"
    ```

2.  **If a specific level works, and you need all levels for that tag (equivalent to `*`):**
    Using `V` (VERBOSE) is often equivalent to seeing all messages for that tag. If `*` specifically causes a crash in `idf_monitor.py`, using `V` is a good alternative.
    ```bash
    idf.py -p COM11 monitor --print-filter="bitmans_lib:wifi_logging:V"
    ```

3.  **To silence other tags and only show yours:**
    ```bash
    # Show only VERBOSE messages from bitmans_lib:wifi_logging, and ERROR from others
    idf.py -p COM11 monitor --print-filter="*:E,bitmans_lib:wifi_logging:V"
    ```
    ```bash
    # Show only VERBOSE messages from bitmans_lib:wifi_logging, and silence all others
    idf.py -p COM11 monitor --print-filter="*:N,bitmans_lib:wifi_logging:V"
    ```

**If command-line filtering still causes errors in `idf_monitor.py`:**

Consider setting log levels directly in your C code (e.g., in `app_main`):
```c
#include "esp_log.h"

void app_main(void) {
    // Set all other tags to ERROR level
    esp_log_level_set("*", ESP_LOG_ERROR);
    // Set your specific tag to VERBOSE
    esp_log_level_set("bitmans_lib:wifi_logging", ESP_LOG_VERBOSE);

    // ... rest of your app_main code
}
```
This requires a recompile and flash.

You can also configure log levels via `idf.py menuconfig` under `Component config` -> `Log output`.

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

## Bitmans ESP32s...

A, B: esp32-devkitC-32, WROOM-32, CH340C  
C, D, E: WROOM-32, CP2102  
F: Need ESP Prog + JTAG (no USB)  
G: WROOM-S3-1, ESP32-S3 (QFN56) (revision v0.2), Embedded PSRAM 8MB (AP_3v3) [Build issues to address]  
H: TODO   
I: ESP32S3 Sense, ESP32-S3 (QFN56) (revision v0.2), Wi-Fi,BLE,Embedded PSRAM 8MB (AP_3v3)

## Troubleshooting
This is needed for BLE on S3 (for bitmans_lib)...

MenuConfig:
```
	"Enable BLE 4.2 features"
```
Or (directly in `sdkconfig`):
```
	CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y
```
Then full clean.
