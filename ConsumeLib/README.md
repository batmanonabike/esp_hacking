# ConsumeLib

This is a simple ESP-IDF project that demonstrates how to consume a shared component from another project.

## Project Structure

- `main/`: Contains the main application code
  - `consume_lib_main.c`: The main application source file
  - `CMakeLists.txt`: Component makefile for the main component

- `../components/ProvideLib/`: The shared component this project uses
  - `include/provide_lib.h`: Public API header file
  - `provide_lib.c`: Implementation of the shared component
  - `CMakeLists.txt`: Component makefile
  - `idf_component.yml`: Component metadata

## How Component Sharing Works

This project demonstrates how to use the `EXTRA_COMPONENT_DIRS` CMake variable to include components from outside the project directory. In the root `CMakeLists.txt` file, we set:

```cmake
set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/components" "../components")
```

This tells the ESP-IDF build system to look for components in:
1. The default ESP-IDF components directory
2. The `../components` directory relative to this project

## Building and Running

To build and flash the project:

```bash
# Navigate to the project directory
cd ConsumeLib

# Build the project
idf.py build

# Flash the project to your device
idf.py -p (PORT) flash monitor
```

Replace `(PORT)` with your device's serial port (e.g., COM3 for Windows).

## Expected Output

When running the project, you should see log messages from both the ConsumeLib main application and the ProvideLib shared component:

```
I (0) ConsumeLib: Starting ConsumeLib application
I (10) ProvideLib: Initializing ProvideLib version 1.0.0
I (20) ConsumeLib: ProvideLib version: 1.0.0
I (30) ProvideLib: User message: Hello from ConsumeLib!
I (1030) ProvideLib: User message: Counter value: 0
I (2030) ProvideLib: User message: Counter value: 1
...
```
