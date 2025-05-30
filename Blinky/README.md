# Blinky Project

This is a simple ESP-IDF project that demonstrates different LED blinking patterns using the BitmansLib component.

## Functionality

The project cycles through different LED blinking modes:
- BLINK_MODE_SLOW: LED blinks slowly (1000ms on, 1000ms off)
- BLINK_MODE_MEDIUM: LED blinks at medium speed (300ms on, 300ms off)
- BLINK_MODE_FAST: LED blinks quickly (100ms on, 100ms off)
- BLINK_MODE_BASIC: Regular LED blinking (500ms on, 500ms off)
- BLINK_MODE_BREATHING: Gradual fade in and out using PWM
- BLINK_MODE_ON: LED remains on
- BLINK_MODE_NONE: LED remains off

## Building and Running

To build and flash the project to your ESP32 device:
```
idf.py build
idf.py -p [PORT] flash monitor
```

Replace `[PORT]` with your ESP32 serial port (e.g., COM11 on Windows or /dev/ttyUSB0 on Linux).
