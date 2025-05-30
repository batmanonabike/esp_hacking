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
```
