# TailFirmware

Firmware for ESP32-C3 that controls a 2-axis animatronic tail with motion patterns, LED effects, IMU sensing, and BLE configuration.

## Build

Requires: ESP-IDF v5.4.1 (installed via Espressif Windows installer at `C:\Espressif`).

```bash
# From ESP-IDF CMD prompt:
idf.py set-target esp32c3
idf.py build
idf.py flash monitor

# From Git Bash (via wrapper):
cmd.exe //c "build.bat set-target esp32c3"
cmd.exe //c "build.bat build"
cmd.exe //c "build.bat flash monitor"
```

Output: `build/TailFirmware.bin` (~612 KB)

## Project Structure

```
main/
  main.c                    Entry point, FreeRTOS task creation, NVS init
  app_bridge.cpp            C/C++ bridge - owns global subsystem instances
  servo.c/h                 LEDC PWM for 4 continuous rotation servos
  ble_service.c/h           NimBLE GATT server with 8 characteristics
  drivers/
    i2c_mux.c/h             TCA9548A I2C multiplexer
    encoder_driver.c/h      AS5600 magnetic rotary encoder (multi-turn)
    imu_driver.c/h          BMI270 IMU (accel, gyro, tap detection)
    led_strip_driver.c/h    WS2812B via RMT peripheral
  motion/
    pid_controller.cpp/h    PID with anti-windup, derivative on measurement
    axis_controller.cpp/h   2 servo-encoder pairs = 1 axis with calibration
    motion_system.cpp/h     2 axes + 2 IMUs + pattern dispatch
    motion_pattern.h        Virtual base class for patterns
    patterns/
      static_pattern.cpp/h
      wagging_pattern.cpp/h
      loose_pattern.cpp/h
  led/
    color.h                 RGB/HSV types, blend helpers
    led_matrix.cpp/h        Ring config -> coordinate mapping
    layer_compositor.cpp/h  Layer stack with 6 blend modes
    led_effect.h            Virtual base class with flip/mirror
    effects/
      rainbow_effect.cpp/h
      static_color_effect.cpp/h
      image_effect.cpp/h
      audio_power_effect.cpp/h
      audio_bar_effect.cpp/h
      audio_freq_bars_effect.cpp/h
  ble/
    ble_protocol.h          All command IDs, characteristic UUIDs
  config/
    config_types.h          Shared C/C++ config structs
    config_manager.cpp/h    BLE command dispatch, NVS persistence, profiles
    fft_buffer.cpp/h        Double-buffered FFT data from BLE stream
```

## Key Details

- Target: ESP32-C3 (RISC-V single-core, 160 MHz, BLE 5.0)
- Language: C for drivers, C++20 for application logic (patterns, effects, config)
- BLE stack: NimBLE (ESP-IDF component)
- Servo PWM: LEDC, 50 Hz, 14-bit, GPIO 3/9/5/6
- LED strip: WS2812B via RMT, GPIO 4
- I2C bus: 400 kHz, GPIO 7 (SDA) / 8 (SCL), via TCA9548A mux
- Status LED: GPIO 10
- Debug: USB-Serial-JTAG

## BLE Service

Service UUID: `0000FF00-0000-1000-8000-00805F9B34FB`

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| Motion Command | `FF01` | Write, Write No Rsp | Pattern select, servo config, PID, calibration |
| Motion State | `FF02` | Read, Notify | Encoder positions, gravity vector |
| LED Command | `FF03` | Write, Write No Rsp | Layer config, effects, image upload |
| LED State | `FF04` | Read, Notify | Active layers and parameters |
| FFT Stream | `FF05` | Write No Rsp | Real-time audio FFT data at 30fps |
| System Config | `FF06` | Read, Write | LED matrix config, system info |
| System State | `FF07` | Read, Notify | Tap events, IMU data |
| Profile | `FF08` | Read, Write | Save/load/delete config profiles |

All write commands use: `[command_id: u8] [payload...]`. See `docs/ble-protocol.md` for full spec.

## Conventions

- C11 for drivers, C++20 for application code
- ESP-IDF component model (`main/` directory)
- `extern "C"` guards on all headers shared between C and C++
- `vec3_t` and all shared types in `config/config_types.h`
- FreeRTOS tasks: nimble_host(event, pri5), motion(100Hz, pri4), LED(30Hz, pri3), config(1Hz, pri2), status LED(pri1)
- NVS namespace `tail_cfg` for active config, `tail_prof0`-`tail_prof3` for profiles
- BLE command IDs defined in `ble/ble_protocol.h`
- Effect/pattern extensibility: derive from `LedEffect`/`MotionPattern`, add factory case in config_manager
