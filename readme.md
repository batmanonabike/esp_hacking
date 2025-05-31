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
```

## Other notes:
If you find that compilations are missing header files and you have added the dependencies...

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
