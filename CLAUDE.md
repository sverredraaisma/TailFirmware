# TailFirmware

Firmware for ESP32-C3 that controls 4 servos via BLE commands, advertising as "Tail controller".

## Build

Requires: ESP-IDF v5.4+ (installed via Espressif Windows installer at `C:\Espressif`).

```bash
# From ESP-IDF CMD prompt (not Git Bash):
idf.py set-target esp32c3
idf.py build
```

Or from Git Bash:
```bash
cmd.exe /c "C:\Espressif\idf_cmd_init.bat && cd /d C:\Users\Sverr\CLionProjects\TailFirmware && idf.py set-target esp32c3 && idf.py build"
```

Output: `build/TailFirmware.bin` - flash via `idf.py flash monitor`.

## Project Structure

- `main/main.c` - Entry point (app_main), NVS init, LED status task
- `main/servo.h/c` - LEDC PWM servo control for 4 servos on GPIO 3-6
- `main/ble_service.h/c` - NimBLE GATT server with custom service
- `sdkconfig.defaults` - ESP-IDF build configuration defaults

## Key Details

- Target: ESP32-C3 (RISC-V, integrated BLE 5.0)
- BLE stack: NimBLE (included in ESP-IDF)
- PWM: LEDC peripheral, 50Hz, 14-bit resolution, 500-2500us pulse range
- Servo GPIOs: 3, 4, 5, 6 (configurable in servo.h)
- LED GPIO: 8 (onboard LED on most ESP32-C3 dev boards)
- Debug output: USB-Serial-JTAG (built-in USB on ESP32-C3)

## BLE Protocol

Service UUID: `0000FF00-0000-1000-8000-00805F9B34FB`

| Characteristic | UUID | Properties | Format |
|---|---|---|---|
| Servo Command | `0000FF01-...` | Write, Write Without Response | `[servo_id(0-3), angle(0-180)]` |
| Servo State | `0000FF02-...` | Read, Notify | `[angle0, angle1, angle2, angle3]` |

## Conventions

- C11 standard
- ESP-IDF component model (main/ directory)
- NimBLE GATT services defined as static table in ble_service.c
- NVS flash required for BLE bonding persistence
