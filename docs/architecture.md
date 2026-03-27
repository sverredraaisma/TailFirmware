# Architecture

## System Overview

```
  Companion App (Phone)
        |
        | BLE (NimBLE, 8 GATT characteristics)
        |
  ble_service.c ──── cmd_callback ───> ConfigManager
        |                                   |
        |                          ┌────────┼────────┐
        |                          v        v        v
        |                    MotionSystem  LayerCompositor  FftBuffer
        |                     /       \         |
        |              AxisController(x2)   LedEffect(s)
        |               /          \            |
        |         PidController  encoder    LedMatrix
        |              |                        |
        |         servo_driver (LEDC)    led_strip_driver (RMT)
        |              |                        |
        v              v                        v
   [Status LED]   [4 Servos]            [WS2812B Strip]
                       ^
                  [4 AS5600 Encoders + 2 BMI270 IMUs]
                       |
                  TCA9548A I2C Mux
```

## FreeRTOS Tasks

| Task | Priority | Stack | Rate | Responsibility |
|------|----------|-------|------|----------------|
| `motion` | 5 | 4 KB | 100 Hz | Read encoders + IMUs via I2C mux, run PID, set servo speeds, run active motion pattern |
| `nimble_host` | 4 | 4 KB | Event | NimBLE stack - handles all BLE operations |
| `led_rend` | 3 | 4 KB | 30 Hz | Render effect layers via compositor, push pixel buffer to WS2812B strip |
| `config` | 2 | 3 KB | 1 Hz | Debounced NVS save, update BLE state buffers, check tap events |
| `led_stat` | 1 | 2 KB | ~5 Hz | Blink onboard LED (fast=advertising, solid=connected, slow=off) |

## Data Flow

### BLE Write -> Subsystem

1. Phone writes to a GATT characteristic (e.g. FF01)
2. NimBLE calls the access callback in `ble_service.c`
3. Callback copies mbuf data and calls `cmd_callback(chr_uuid_short, data, len)`
4. `cmd_callback` (set during init) points to `app_process_ble_command()` in `app_bridge.cpp`
5. Bridge forwards to `ConfigManager::process_command()`
6. ConfigManager dispatches by characteristic UUID to the appropriate handler
7. Handler parses command_id and payload, calls the target subsystem API
8. Config struct is updated, `mark_dirty()` triggers NVS save after 2s

### Motion Control Loop (100 Hz)

1. `motion_ctrl_task` calls `app_motion_update(dt)` -> `MotionSystem::update(dt)`
2. MotionSystem reads base IMU gravity vector and both IMU tap status
3. Reads all 4 encoder positions via I2C mux (switch channel + read per device, ~630us total)
4. Builds `MotionInput` struct with encoder angles, gravity, taps, loudness, dt
5. Calls active `MotionPattern::update(input, output)` to get target angles
6. Distributes target angles to the 2 `AxisController` instances (indices 0-1 = X, 2-3 = Y)
7. Each AxisController runs PID for both halves: `error = target - current`, PID output -> `servo_set_speed()`
8. LEDC hardware generates PWM continuously without further CPU involvement

### LED Render Loop (30 Hz)

1. `led_render_task` calls `app_led_render(dt)` -> `LayerCompositor::render(matrix, dt)` then `matrix.push()`
2. Compositor iterates layers bottom-to-top
3. For each enabled layer: renders effect into temp buffer, blends into output using layer's blend mode
4. Effects read LED coordinates from the matrix's coordinate map
5. Audio effects read from `FftBuffer::instance()` (lock-free double buffer)
6. Final composited buffer is pushed to the LED strip driver
7. RMT hardware transmits WS2812B protocol without CPU involvement

### FFT Streaming

1. Phone sends FFT frames to FF05 at 30fps using Write Without Response
2. BLE callback dispatches to `ConfigManager::handle_fft_stream()`
3. Data written to `FftBuffer` back buffer, then read index flips (lock-free)
4. Audio effects read from front buffer in the LED render task
5. If no data for 200ms, `FftBuffer::is_fresh()` returns false, effects fade

### Config Persistence

1. Any config change calls `mark_dirty()` with current timestamp
2. `config_task` runs at 1 Hz, calls `save_pending()`
3. If dirty and 2+ seconds since last change: write `system_config_t` as NVS blob to `tail_cfg`
4. On boot: `load()` reads from `tail_cfg`, `apply_config()` recreates all subsystem state
5. Profile save/load: same NVS mechanism but to `tail_prof0`-`tail_prof3` namespaces

## Module Details

### Motion System

**MotionSystem** owns 2 `AxisController` instances and 2 `imu_t` instances. On init, it reads servo config to determine which servo index maps to which axis and half.

**AxisController** manages one axis (2 servo-encoder pairs). Each half has:
- An `encoder_t` for position feedback (AS5600 via I2C mux)
- A `PidController` for closed-loop control
- An invert flag for spin direction
- A zero offset (set during calibration)

**PidController** features:
- Derivative on measurement (not error) to avoid derivative kick on setpoint changes
- Anti-windup with integral clamping
- Configurable output limits (default -1000 to +1000, matching servo speed range)

**Motion Patterns** derive from `MotionPattern`:
- **Static:** Holds fixed positions for all 4 half-axes
- **Wagging:** Sine wave on X axis with configurable frequency and amplitude. Second half trails first by PI/4 phase for natural wave motion
- **Loose:** Spring-damper physics driven by gravity tilt. Second half chains off first half's position for trailing lag. Configurable damping and reactivity

### LED System

**LedMatrix** converts a ring-based physical layout to a [0,1]x[0,1] coordinate system:
- Rings are evenly spaced on Y (first ring y=0, last y=1)
- LEDs within each ring are evenly spaced on X (first x=0, last x=1)
- Coordinate map is rebuilt when ring config changes

**LayerCompositor** implements the decorator pattern:
- Up to 8 layers, rendered bottom-to-top
- Each layer has an effect, blend mode, and enabled flag
- 6 blend modes: Multiply, Add, Subtract, Min, Max, Overwrite

**LedEffect** base class provides:
- `render()` method that fills a pixel buffer given LED coordinates
- `set_param()`/`get_param()` for runtime configuration
- Flip/mirror coordinate transforms applied before sampling

**Effects:**
- **Rainbow:** HSV sweep across configurable axis, animated by speed parameter
- **Static Color:** Solid RGB fill
- **Image:** Nearest-neighbor sampling from an uploaded bitmap (max 32x32, 3KB)
- **Audio Power:** Brightness tracks loudness with configurable fade
- **Audio Bar:** Spatial bar sweeps based on loudness level
- **Audio Freq Bars:** Frequency spectrum display with configurable bar count

### BLE System

**ble_service.c** manages the NimBLE GATT server with 8 characteristics. Key design:
- Write characteristics dispatch through a single `ble_cmd_callback_t` to the config manager
- Read characteristics serve from mutex-protected static buffers updated by `ble_set_*()` functions
- Notify subscriptions tracked per-characteristic, cleared on disconnect
- FFT stream (FF05) uses Write Without Response for minimum latency

### Config System

**ConfigManager** is the central hub:
- Receives BLE commands and dispatches to subsystems
- Maintains the authoritative `system_config_t` struct
- Persists to NVS with 2-second debounce
- Manages 4 profile slots
- Factory methods create pattern/effect instances from IDs
- `apply_config()` reconstructs full subsystem state from a loaded config

## I2C Bus Layout

Single I2C bus at 400 kHz through TCA9548A multiplexer:

| Mux Channel | Device | Address | Purpose |
|-------------|--------|---------|---------|
| 0 | AS5600 | 0x36 | Servo 0 encoder |
| 1 | AS5600 | 0x36 | Servo 1 encoder |
| 2 | AS5600 | 0x36 | Servo 2 encoder |
| 3 | AS5600 | 0x36 | Servo 3 encoder |
| 4 | BMI270 | 0x68 | Base IMU |
| 5 | BMI270 | 0x68 | Tip IMU |

Channel assignments are configurable in `config_types.h` per servo and per IMU.

## Memory Usage

| Region | Usage |
|--------|-------|
| Flash | ~612 KB firmware + NVS partition |
| RAM (heap) | ~105 KB used, ~170 KB free |
| Task stacks | ~17 KB total (5 tasks) |
| LED buffers | ~1.8 KB per 200 LEDs (coords + 2 RGB buffers) |
| NVS | ~800 bytes per config blob, up to 5 copies (active + 4 profiles) |
