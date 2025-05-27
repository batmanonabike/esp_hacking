# WifiConnect

This ESP-IDF project connects to a given WiFi SSID, logs when it connects, retries every 5 seconds on failure, and after a successful connection, stays connected for a few seconds, disconnects, and repeats.

## Usage

1. Set your WiFi SSID and password in `main.c` or via `menuconfig`.
Also note, for menuconfig:
```

idf.py fullclean
idf.py menuconfig
Compiler Options → Enable -Werror flag for compiler
```
You need a clean build for this though.
We can also disable in `sdkconfig`
```
CONFIG_COMPILER_WARNINGS_AS_ERRORS is not set
```
^ This seemed to break the build. 

2. Build the project:
   ```
   idf.py build
   ```
3. Flash to your device (replace COMx with your port):
   ```
   idf.py -p COMx flash
   ```
4. Monitor output:
   ```
   idf.py -p COMx monitor
   ```

## Requirements
- ESP-IDF v4.0 or later
- ESP32 or compatible device

## How it works
- Connects to WiFi and logs IP on success
- Retries every 5 seconds if connection fails
- After connecting, stays connected for a few seconds, disconnects, and repeats
