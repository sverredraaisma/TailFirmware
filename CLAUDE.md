# TailFirmware

Firmware for Raspberry Pi Pico 2W that controls 4 servos via BLE commands, advertising as "Tail controller".

## Build

Requires: Pico SDK 2.2.0, ARM GCC toolchain, CMake, Ninja, Python 3, and a host C++ compiler (MinGW/MSVC) for SDK tools (pioasm, picotool).

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

Output: `build/TailFirmware.uf2` - flash by holding BOOTSEL and dragging to the USB drive.

## Project Structure

- `src/main.c` - Entry point, initializes CYW43, servos, and BLE
- `src/servo.h/c` - PWM servo control for 4 servos on GPIO 2-5
- `src/ble_service.h/c` - BLE GATT server with custom service
- `src/tail_service.gatt` - BTstack GATT database definition (compiled to `tail_service.h` at build time)
- `src/btstack_config.h` - BTstack configuration defines

## Key Details

- Board: `pico2_w` (RP2350 + CYW43439)
- CYW43 arch: `pico_cyw43_arch_none` (BLE-only, no lwIP/WiFi stack)
- BLE stack: BTstack (included in Pico SDK)
- Servos: 50Hz PWM, 500-2500us pulse range, GPIO 2/3/4/5
- Debug output: USB serial (stdio over USB)

## BLE Protocol

Service UUID: `0000FF00-0000-1000-8000-00805F9B34FB`

| Characteristic | UUID | Properties | Format |
|---|---|---|---|
| Servo Command | `0000FF01-...` | Write, Write Without Response | `[servo_id(0-3), angle(0-180)]` |
| Servo State | `0000FF02-...` | Read, Notify | `[angle0, angle1, angle2, angle3]` |

## Conventions

- C11 standard, no C++ in firmware sources
- Pico SDK environment variable `PICO_SDK_PATH` must be set
- GATT header is auto-generated - edit `tail_service.gatt`, not the generated header
- BTstack handle constants follow pattern: `ATT_CHARACTERISTIC_<UUID_WITH_UNDERSCORES>_01_VALUE_HANDLE`
