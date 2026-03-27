# TailFirmware Development Plan

> **Status: All 12 phases implemented and compiling.** The firmware is feature-complete
> and ready for hardware testing and companion app development.

## Table of Contents

1. [Feasibility Analysis](#1-feasibility-analysis)
2. [Architecture Decisions](#2-architecture-decisions)
3. [BLE Protocol Design](#3-ble-protocol-design)
4. [Development Phases](#4-development-phases)
5. [Phase Details](#5-phase-details)

---

## 1. Feasibility Analysis

### ESP32-C3 Hardware Budget

| Resource | Available | Required | Margin | Status |
|----------|-----------|----------|--------|--------|
| LEDC PWM channels | 6 | 4 (servos) | 2 spare | OK |
| RMT TX channels | 2 | 1 (LED strip) | 1 spare | OK |
| I2C controllers | 1 | 1 (with TCA9548A mux) | 0 | OK (mux solves multi-device) |
| Usable RAM | ~276 KB | ~80-120 KB estimated | ~150 KB | OK |
| Flash | 4 MB | ~1 MB firmware + config | ~3 MB | OK |
| GPIO | ~13 usable | 7 (4 servo + 1 LED + 2 I2C) | 6 spare | OK |
| CPU cores | 1 @ 160 MHz | - | Tight but viable | Needs careful task design |
| BLE throughput | ~30-50 KB/s | ~3 KB/s (FFT @ 30fps) | 10x headroom | OK |
| GATT characteristics | 64 max | ~20-30 estimated | OK | OK |

### I2C Bus Timing Budget

All devices connect through a single I2C bus via TCA9548A multiplexer at 400 kHz.

| Operation | Time | Frequency |
|-----------|------|-----------|
| Mux channel switch | ~35 us | Per device read |
| AS5600 angle read (2 bytes) | ~55 us | Per encoder |
| IMU accel+gyro read (12 bytes) | ~100 us | Per IMU |
| **4 encoders + 2 IMUs** | **~630 us total** | **Per control cycle** |
| At 100 Hz control loop | 63 ms budget, 0.63 ms used | **1% I2C bus utilization** |

### RAM Budget Estimate

| Subsystem | Estimated Usage |
|-----------|----------------|
| NimBLE stack + buffers | ~50 KB |
| FreeRTOS tasks (5 tasks, stacks) | ~20 KB |
| LED framebuffers (200 LEDs * 3 bytes * 3 layers) | ~2 KB |
| LED coordinate map (200 LEDs * 8 bytes) | ~1.6 KB |
| Motion system (PID state, pattern state) | ~2 KB |
| FFT buffer (double buffered, 128 bins) | ~0.5 KB |
| BLE GATT service tables + descriptors | ~4 KB |
| NVS cache | ~4 KB |
| General heap, stack, buffers | ~20 KB |
| **Total estimated** | **~105 KB** |
| **Remaining** | **~170 KB** |

### Identified Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Single-core CPU contention | PID loop jitter, LED flicker | Strict FreeRTOS priority scheme; hardware peripherals (LEDC, RMT) are interrupt-free once configured |
| BLE + motion control timing | Missed control deadlines | Motion control at highest task priority; BLE runs in its own task at medium priority |
| Many GATT characteristics | Complex app integration, memory | Use structured command protocol with fewer characteristics carrying serialized data |
| LED rendering at high LED count | Frame drops at >200 LEDs | Limit to 30 fps LED update; render in dedicated low-priority task; skip frames if behind |
| Persistent config complexity | Corruption, boot failures | NVS with schema versioning; validate all values on load; fallback to defaults |

### Verdict: ESP32-C3 is viable

The project fits within the ESP32-C3's resource envelope. The main challenge is single-core CPU scheduling, which is manageable with proper FreeRTOS task priorities and the fact that servo PWM (LEDC) and LED output (RMT) are handled entirely in hardware after setup.

---

## 2. Architecture Decisions

### C++ for Application Logic

**Decision: Use C++ (C++20) for pattern/effect systems, C for hardware drivers.**

**Why:**
- The lighting effect layer system (decorator pattern with blend modes) maps directly to C++ polymorphism (virtual base class, derived effects, composable layers)
- The motion pattern system (extensible patterns with varying parameters) also benefits from inheritance and virtual dispatch
- ESP-IDF fully supports C++ up to C++23 (gnu++2b) with the bundled RISC-V compiler
- Hardware drivers (LEDC, RMT, I2C) can remain as plain C - ESP-IDF's APIs are all C

**Implementation:**
- `.c` files: servo driver, I2C/mux driver, encoder driver, IMU driver, LED strip driver
- `.cpp` files: motion patterns, lighting effects, layer compositor, BLE config manager
- Headers shared between C and C++ use `extern "C"` guards

### FreeRTOS Task Architecture

| Task | Priority | Stack | Rate | Responsibility |
|------|----------|-------|------|---------------|
| `motion_ctrl` | 5 (highest app) | 4 KB | 100 Hz | Read encoders, run PID, update servos |
| `nimble_host` | 4 | 4 KB | Event-driven | All BLE operations |
| `led_render` | 3 | 4 KB | 30-60 Hz | Compute effect layers, push to RMT |
| `config_mgr` | 2 | 3 KB | Event-driven | Process BLE config writes, NVS persistence |
| `led_status` | 1 (lowest) | 2 KB | 5 Hz | Onboard LED blink pattern |

**Inter-task communication:**
- `motion_ctrl` reads shared config (mutex-protected) set by `config_mgr`
- `led_render` reads shared config and FFT data (lock-free ring buffer for FFT)
- `config_mgr` receives commands via FreeRTOS queue from BLE write callbacks
- IMU tap events sent via FreeRTOS event group bits

### Module Dependency Graph

```
main.c (app_main)
  |
  ├── ble_service (NimBLE GATT server)
  |     └── config_manager (serialization, NVS, dispatches to subsystems)
  |
  ├── motion_system
  |     ├── servo_driver (LEDC PWM)
  |     ├── encoder_driver (AS5600 via I2C + mux)
  |     ├── imu_driver (BMI270 via I2C + mux)
  |     ├── pid_controller
  |     ├── axis_controller (2 servos + 2 encoders → 1 axis)
  |     └── motion_patterns/
  |           ├── pattern_base (virtual)
  |           ├── static_pattern
  |           ├── wagging_pattern
  |           └── loose_pattern
  |
  └── led_system
        ├── led_strip_driver (WS2812B via RMT)
        ├── led_matrix (ring config → coordinate mapping)
        ├── layer_compositor (blend modes, layer stack)
        └── effects/
              ├── effect_base (virtual)
              ├── rainbow_effect
              ├── static_color_effect
              ├── image_effect
              ├── audio_power_effect
              ├── audio_bar_effect
              └── audio_freq_bars_effect
```

### File Structure

```
TailFirmware/
├── CMakeLists.txt
├── sdkconfig.defaults
├── build.bat
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                          # app_main, task creation, NVS init
│   │
│   ├── drivers/                        # Hardware drivers (C)
│   │   ├── servo_driver.h / .c         # LEDC PWM for continuous rotation servos
│   │   ├── encoder_driver.h / .c       # AS5600 I2C reads
│   │   ├── imu_driver.h / .c           # BMI270 I2C reads + tap interrupt
│   │   ├── i2c_mux.h / .c             # TCA9548A multiplexer control
│   │   └── led_strip_driver.h / .c     # WS2812B via RMT
│   │
│   ├── motion/                         # Motion system (C++)
│   │   ├── pid_controller.h / .cpp     # PID with configurable gains
│   │   ├── axis_controller.h / .cpp    # 2 servos + 2 encoders = 1 axis
│   │   ├── motion_system.h / .cpp      # 2 axes + IMU + pattern dispatch
│   │   └── patterns/
│   │       ├── motion_pattern.h        # Virtual base class
│   │       ├── static_pattern.h / .cpp
│   │       ├── wagging_pattern.h / .cpp
│   │       └── loose_pattern.h / .cpp
│   │
│   ├── led/                            # LED system (C++)
│   │   ├── color.h                     # RGB/HSV types and conversion
│   │   ├── led_matrix.h / .cpp         # Ring config → coordinate map
│   │   ├── layer_compositor.h / .cpp   # Layer stack with blend modes
│   │   └── effects/
│   │       ├── led_effect.h            # Virtual base class
│   │       ├── rainbow_effect.h / .cpp
│   │       ├── static_color_effect.h / .cpp
│   │       ├── image_effect.h / .cpp
│   │       ├── audio_power_effect.h / .cpp
│   │       ├── audio_bar_effect.h / .cpp
│   │       └── audio_freq_bars_effect.h / .cpp
│   │
│   ├── ble/                            # BLE service (C)
│   │   ├── ble_service.h / .c          # NimBLE GATT server, GAP
│   │   └── ble_protocol.h             # Command IDs, packet formats
│   │
│   └── config/                         # Configuration management (C++)
│       ├── config_manager.h / .cpp     # Central config store, NVS persistence
│       └── config_types.h             # Shared config structs
│
└── docs/
    ├── development-plan.md             # This file
    ├── ble-protocol.md                 # Full BLE characteristic spec
    └── ...
```

---

## 3. BLE Protocol Design

### Service Layout

Rather than one characteristic per config item (which would exceed practical limits), the design uses a structured approach with grouped characteristics.

**Primary Service:** `0000FF00-0000-1000-8000-00805F9B34FB` (same as current)

| Characteristic | UUID | Properties | Description |
|---|---|---|---|
| **Motion Command** | `FF01` | Write, Write No Rsp | Servo commands, pattern select, motion params |
| **Motion State** | `FF02` | Read, Notify | Encoder angles, active pattern, motion status |
| **LED Command** | `FF03` | Write, Write No Rsp | Layer config, effect select, effect params |
| **LED State** | `FF04` | Read, Notify | Active layers, current effect params |
| **FFT Stream** | `FF05` | Write No Rsp | Real-time FFT data (30 fps) |
| **System Config** | `FF06` | Read, Write | LED matrix config, servo config, IMU config |
| **System State** | `FF07` | Read, Notify | IMU data, tap events, system status |
| **Profile** | `FF08` | Read, Write | Save/load/list configuration profiles |

### Command Packet Format

All write characteristics use a common packet structure:

```
[command_id: u8] [payload: variable]
```

This keeps the number of characteristics low while allowing unlimited commands per subsystem. The companion app documentation will specify every command_id and its payload format.

### FFT Stream Format

```
[loudness: u8] [num_bins: u8] [bin_values: u8 * num_bins]
```

- Total size at 64 bins: 66 bytes per frame
- At 30 fps: 1,980 bytes/sec (~2 KB/s), well within BLE capacity
- Uses Write Without Response for minimum latency

---

## 4. Development Phases

### Overview

| Phase | Name | Duration Est. | Dependencies |
|-------|------|---------------|-------------|
| **1** | Core Infrastructure | - | None |
| **2** | Servo Position Control | - | Phase 1 |
| **3** | 2-Axis Motion System | - | Phase 2 |
| **4** | IMU Integration | - | Phase 1 |
| **5** | Motion Patterns | - | Phase 3, 4 |
| **6** | LED Strip & Matrix | - | Phase 1 |
| **7** | Effect Layer System | - | Phase 6 |
| **8** | LED Effects | - | Phase 7 |
| **9** | FFT Audio Streaming | - | Phase 7 |
| **10** | BLE Configuration System | - | All above |
| **11** | Persistence & Profiles | - | Phase 10 |
| **12** | Integration & Polish | - | All above |

### Dependency Graph

```
Phase 1 (Infrastructure)
  ├── Phase 2 (Servo Control)
  │     └── Phase 3 (2-Axis Motion)
  │           └── Phase 5 (Motion Patterns) ← also needs Phase 4
  ├── Phase 4 (IMU)
  │
  ├── Phase 6 (LED Strip)
  │     └── Phase 7 (Layer System)
  │           ├── Phase 8 (LED Effects)
  │           └── Phase 9 (FFT Streaming)
  │
  └── Phase 10 (BLE Config) ← needs all features
        └── Phase 11 (Persistence)
              └── Phase 12 (Integration)
```

---

## 5. Phase Details

### Phase 1: Core Infrastructure

**Goal:** Project skeleton with C++ support, I2C bus, multiplexer, and FreeRTOS task framework.

**Tasks:**
1. Restructure project for C++ support
   - Update `main/CMakeLists.txt` to include `.cpp` sources
   - Set C++ standard to C++20 in CMake
   - Verify C/C++ interop with `extern "C"` headers
2. I2C bus initialization
   - Configure I2C0 at 400 kHz on chosen GPIO pins
   - Write `i2c_mux.h/.c` - TCA9548A driver
     - `i2c_mux_init(i2c_port, sda_pin, scl_pin)`
     - `i2c_mux_select_channel(channel)` - write channel byte to 0x70
     - `i2c_mux_disable_all()`
   - Test with logic analyzer or by scanning for devices on each channel
3. Config types header
   - Define shared structs: `servo_config_t`, `axis_config_t`, `led_matrix_config_t`, `pid_params_t`
   - Keep plain C compatible (no C++ features in shared structs)
4. FreeRTOS task skeleton
   - Create all tasks in `app_main` with proper priorities
   - Tasks initially just log and sleep at their target rate
   - Verify timing with `esp_timer_get_time()` instrumentation
5. Status LED task (carry over from current firmware)

**Deliverables:** Project builds with C++, I2C mux talks to downstream devices, all tasks running at target rates.

**Testing:**
- Scan I2C bus through each mux channel, verify expected device addresses appear
- Measure task execution timing via serial output
- Verify BLE still works (NimBLE task runs alongside others)

---

### Phase 2: Servo Position Control

**Goal:** Closed-loop position control for a single continuous rotation servo using encoder feedback.

**Tasks:**
1. AS5600 encoder driver (`encoder_driver.h/.c`)
   - `encoder_init(mux_channel)`
   - `encoder_read_angle(mux_channel)` → returns 0-4095 (12-bit, 0-360 degrees)
   - Handle angle wraparound (0/4095 boundary) with multi-turn tracking
   - Use raw angle register (0x0C-0x0D) for fastest reads
2. Servo driver update (`servo_driver.h/.c`)
   - Adapt current LEDC servo code for continuous rotation semantics
   - `servo_set_speed(servo_id, speed)` where speed maps to 500-2500us pulse:
     - -1000 to -1: reverse (500-1449us)
     - 0: stop (1450-1550us, centered at 1500us)
     - 1 to 1000: forward (1551-2500us)
   - Deadband calibration per servo (stored in config)
3. PID controller (`pid_controller.h/.cpp`)
   - Template-free, lightweight PID class
   - Configurable Kp, Ki, Kd, output limits, integral windup limit
   - `update(setpoint, measurement, dt)` → returns control output
   - Anti-windup with integral clamping
   - Derivative on measurement (not error) to avoid derivative kick
4. Single servo closed-loop test
   - Wire one servo + one encoder
   - PID reads encoder, outputs to servo speed
   - Test: command position 180°, verify servo moves to 180° and holds
   - Tune PID gains experimentally

**Deliverables:** One servo can be commanded to an angle and holds position via PID.

**Testing:**
- Step response: command 0° → 180°, log encoder angle over time, verify settling
- Disturbance rejection: physically push servo, verify it returns to setpoint
- Wrap handling: command positions that cross the 0°/360° boundary

---

### Phase 3: 2-Axis Motion System

**Goal:** Abstraction layer that maps 2 axes (X, Y) to 4 servos with calibration.

**Tasks:**
1. Axis controller (`axis_controller.h/.cpp`)
   - Each axis has 2 servo-encoder pairs (first half, second half)
   - `set_position(first_half_angle, second_half_angle)`
   - `get_position()` → returns both current angles
   - Applies rotation direction config (invert per servo)
   - Applies offset calibration (zero position)
   - Enforces rotation limits per axis
2. Motion system (`motion_system.h/.cpp`)
   - Owns 2 `AxisController` instances (X axis, Y axis)
   - Owns the 100 Hz control loop (called from `motion_ctrl` task)
   - `calibrate_zero()` - stores current encoder positions as zero offset
   - `set_limits(axis, min_angle, max_angle)`
   - Servo-to-axis assignment configurable (which servo is X first half, etc.)
3. BLE servo config characteristic
   - Add basic write handler for servo configuration commands:
     - Assign servo to axis/half
     - Set spin direction
     - Set PID gains
     - Trigger zero calibration
     - Set axis limits

**Deliverables:** 4 servos move in coordinated 2-axis motion. Calibration and limits work.

**Testing:**
- Move X axis: both X servos respond, Y servos hold
- Calibrate zero at arbitrary position, verify new zero is respected
- Hit rotation limit, verify servo stops
- Change servo assignment via BLE, verify correct servo moves

---

### Phase 4: IMU Integration

**Goal:** Read orientation and detect taps from 2 IMUs.

**Tasks:**
1. IMU driver (`imu_driver.h/.c`)
   - Target chip: BMI270 (recommended for hardware tap detection)
   - Fallback: MPU-6050 with software tap detection
   - `imu_init(mux_channel)` - configure accelerometer range, tap detection
   - `imu_read_accel(mux_channel)` → returns x, y, z in mg
   - `imu_read_gyro(mux_channel)` → returns x, y, z in dps
   - `imu_get_gravity_vector(mux_channel)` → normalized gravity direction
   - BMI270-specific: configure tap detection interrupt, threshold, quiet time
2. Tap detection
   - If BMI270: read tap status register, hardware handles detection
   - If MPU-6050: software detection - threshold on accel spike with debounce
   - Expose as FreeRTOS event group bit (`TAP_BASE_BIT`, `TAP_TIP_BIT`)
   - Configurable enable/disable per IMU via BLE
3. IMU data in motion system
   - `motion_system` reads base IMU gravity vector each control cycle
   - Gravity vector available to motion patterns as input
   - Tap events available to patterns and effects as triggers

**Deliverables:** Gravity direction known, taps detected and distributed to subsystems.

**Testing:**
- Rotate the device, verify gravity vector changes correctly
- Tap base IMU, verify event fires with correct debounce
- Verify tap enable/disable works via BLE command
- Stress test: rapid taps, verify no false triggers or missed events

---

### Phase 5: Motion Patterns

**Goal:** Extensible pattern system with initial 3 patterns.

**Tasks:**
1. Pattern base class (`motion_pattern.h`)
   ```cpp
   class MotionPattern {
   public:
       virtual ~MotionPattern() = default;
       virtual void update(const MotionInput &input, MotionOutput &output, float dt) = 0;
       virtual void set_param(uint8_t param_id, float value) = 0;
       virtual float get_param(uint8_t param_id) const = 0;
   };
   ```
   - `MotionInput`: current positions, gravity vector, tap event, dt
   - `MotionOutput`: target positions for each axis half (4 values)
2. Static pattern
   - Parameters: x1_pos, x2_pos, y1_pos, y2_pos (4 floats)
   - Simply outputs the configured positions
3. Wagging pattern
   - Parameters: frequency, x_amplitude, y1_pos, y2_pos
   - Sine wave on X axis at given frequency and amplitude
   - Y axis holds at configured positions
   - Phase offset between first and second half for natural wave motion
4. Loose pattern
   - Parameters: damping, reactivity
   - Physics simulation: gravity vector from base IMU drives a spring-damper model
   - Tail swings naturally based on body movement
   - More complex: second-half lags behind first-half for chain-like behavior
5. Pattern manager in `motion_system`
   - `set_pattern(pattern_id)` - switches active pattern
   - `set_pattern_param(param_id, value)` - forwards to active pattern
   - Pattern switch is instant (no blending for now)
6. BLE commands for pattern control
   - Select pattern, set parameters

**Deliverables:** 3 working motion patterns switchable via BLE.

**Testing:**
- Static: set positions, verify servos move to exact angles
- Wagging: verify oscillation frequency matches parameter, second half trails
- Loose: rotate device, verify tail swings with gravity
- Switch between patterns, verify smooth transition

---

### Phase 6: LED Strip and Matrix

**Goal:** WS2812B output and configurable ring-to-coordinate mapping.

**Tasks:**
1. LED strip driver (`led_strip_driver.h/.c`)
   - Use ESP-IDF's `led_strip` component (RMT backend)
   - `led_strip_init(gpio, num_leds)`
   - `led_strip_set_pixel(index, r, g, b)`
   - `led_strip_refresh()` → pushes buffer to LEDs via RMT
   - `led_strip_clear()`
2. Color types (`color.h`)
   - `struct RGB { uint8_t r, g, b; }`
   - `struct HSV { uint16_t h; uint8_t s, v; }`
   - `RGB hsv_to_rgb(HSV)`
   - Blend helpers: `rgb_multiply`, `rgb_add`, `rgb_subtract`, `rgb_min`, `rgb_max`
3. LED matrix (`led_matrix.h/.cpp`)
   - Configurable ring layout: `set_ring_config(num_rings, leds_per_ring[])`
   - Builds coordinate map: each LED gets `(x, y)` in [0.0, 1.0] range
     - Rings evenly spaced on Y axis (first ring at y=0, last at y=1)
     - LEDs in each ring evenly spaced on X axis (first at x=0, last at x=1)
   - `get_led_count()` → total LEDs
   - `get_led_coords(index)` → (x, y) pair
   - `set_pixel(index, RGB)` → sets in output buffer
   - `push()` → calls `led_strip_refresh()` with current buffer
4. BLE commands for matrix config
   - Set number of rings, LEDs per ring
   - Read current config

**Deliverables:** LED strip lights up, coordinate system mapped correctly.

**Testing:**
- Light all LEDs white, verify count and wiring
- Light LEDs by coordinate: all x < 0.5 red, all x >= 0.5 blue → verify spatial mapping
- Change ring config via BLE, verify remapping

---

### Phase 7: Effect Layer System

**Goal:** Composable effect layers with blend modes.

**Tasks:**
1. Effect base class (`led_effect.h`)
   ```cpp
   class LedEffect {
   public:
       virtual ~LedEffect() = default;
       virtual void render(RGB *buffer, uint16_t count,
                          const LedMatrix &matrix, float dt) = 0;
       virtual void set_param(uint8_t param_id, float value) = 0;
       virtual float get_param(uint8_t param_id) const = 0;

       bool flip_x = false, flip_y = false;
       bool mirror_x = false, mirror_y = false;
   };
   ```
   - `render()` writes the effect's output into the provided buffer
   - Flip/mirror applied to coordinates before sampling
2. Blend modes (`layer_compositor.h/.cpp`)
   ```cpp
   enum class BlendMode {
       Multiply, Add, Subtract, Min, Max, Overwrite
   };
   ```
   - `blend_pixel(RGB base, RGB overlay, BlendMode mode)` → blended RGB
   - Multiply: `result = base * overlay / 255` per channel
   - Add: `result = min(base + overlay, 255)` per channel
   - Subtract: `result = max(base - overlay, 0)` per channel
   - Min: `result = min(base, overlay)` per channel
   - Max: `result = max(base, overlay)` per channel
   - Overwrite: `result = (overlay == black) ? base : overlay`
3. Layer compositor
   - Stack of layers, each with: effect pointer, blend mode, enabled flag
   - `set_layer(index, effect, blend_mode)`
   - `remove_layer(index)`
   - `render_all(matrix, dt)`:
     1. Start with black buffer
     2. For each enabled layer bottom to top:
        - Render effect into temp buffer
        - Blend temp buffer into output buffer using layer's blend mode
     3. Return final output buffer
   - Maximum ~8 layers (configurable, memory-dependent)
4. LED render task integration
   - `led_render` task runs at 30-60 Hz
   - Calls `compositor.render_all()`, then `matrix.push()`

**Deliverables:** Multiple effects composited with blend modes and rendered to LEDs.

**Testing:**
- Single layer: solid red → all LEDs red
- Two layers: solid red (base) + solid blue (add) → purple
- Multiply mode: white base * half-brightness green overlay → dark green
- Overwrite with partial black mask → only non-black pixels replaced
- Flip/mirror: verify coordinate transforms work correctly

---

### Phase 8: LED Effects

**Goal:** Implement all 6 initial effects.

**Tasks:**
1. **Rainbow effect**
   - Parameters: direction (0=horizontal, 1=vertical, 2=diagonal), speed, scale
   - Uses HSV color space, sweeps hue across coordinate axis
   - `time_offset += speed * dt`; hue at each LED = `(coord * scale + time_offset) % 360`
2. **Static color effect**
   - Parameters: R, G, B
   - Fills entire buffer with the configured color
3. **Image effect**
   - Parameters: orientation (rotation 0/90/180/270)
   - Stores one image in NVS (raw RGB, limited by flash budget, e.g. 32x32 = 3 KB)
   - Image uploaded via BLE (chunked writes to a characteristic)
   - Maps image pixels to LED coordinates using nearest-neighbor sampling
   - Squish-to-fit: image always maps to full [0,1] x [0,1] coordinate space
4. **Audio power effect**
   - Parameters: R, G, B, fade_rate
   - Reads loudness from FFT stream buffer
   - Sets all LEDs to configured color at brightness = loudness
   - Fade: if no new data, brightness decays at `fade_rate` per second
5. **Audio bar effect**
   - Parameters: R, G, B, direction, fade_rate
   - Reads loudness from FFT stream buffer
   - Draws a bar that sweeps from one edge to `loudness` fraction of the axis
   - Direction determines which coordinate axis to sweep
6. **Audio frequency bars effect**
   - Parameters: freq_range_start, freq_range_end, num_bars, R, G, B, fade_rate, orientation
   - Reads frequency bins from FFT stream buffer
   - Groups bins into `num_bars` bands
   - Each band draws a bar proportional to its energy level
   - Bars distributed evenly across one axis, height on the other axis

**Deliverables:** All 6 effects functional and parameterizable via BLE.

**Testing:**
- Each effect individually with various parameter combinations
- Layer combinations: rainbow + audio power (multiply) → rainbow that pulses with music
- Image upload via BLE, verify correct display
- Flip/mirror on each effect

---

### Phase 9: FFT Audio Streaming

**Goal:** Receive real-time FFT data from companion app and make it available to effects.

**Tasks:**
1. FFT data buffer (`fft_buffer.h/.cpp`)
   - Double-buffered: write to back buffer, flip to front on complete frame
   - Lock-free: BLE write callback fills back buffer, render task reads front buffer
   - Stores: loudness (uint8), bin count, bin values (uint8 array)
   - Stale detection: timestamp of last received frame
2. BLE FFT streaming characteristic (FF05)
   - Write Without Response for minimum latency
   - Packet format: `[loudness: u8] [num_bins: u8] [bins: u8 * num_bins]`
   - On receive: copy into back buffer, flip
3. Integration with audio effects
   - Effects call `fft_buffer_get_loudness()`, `fft_buffer_get_bin(index)`
   - If data is stale (>200ms), effects fade to zero or hold last value
4. Integration with motion patterns
   - `MotionInput` includes loudness value
   - Patterns can optionally react to audio (e.g., wag faster with beat)

**Deliverables:** Companion app can stream FFT data, audio effects respond in real-time.

**Testing:**
- Send synthetic FFT data via nRF Connect (manual hex writes)
- Verify audio power effect responds to loudness changes
- Verify frequency bars display correct relative levels
- Latency test: measure time from BLE write to LED change
- Stale data test: stop streaming, verify graceful fade

---

### Phase 10: BLE Configuration System

**Goal:** Complete BLE GATT service with all config characteristics.

**Tasks:**
1. Define all command IDs and payload formats (`ble_protocol.h`)
   - Document every command for companion app developers
   - Group by subsystem (motion, LED, system, profile)
2. Motion commands (FF01):
   - `0x01`: Select pattern (payload: pattern_id)
   - `0x02`: Set pattern parameter (payload: param_id, float value)
   - `0x03`: Set servo config (payload: servo_id, axis, half, direction)
   - `0x04`: Set PID gains (payload: servo_id, Kp, Ki, Kd as floats)
   - `0x05`: Calibrate zero position
   - `0x06`: Set axis limits (payload: axis, min, max)
   - `0x07`: Enable/disable IMU tap (payload: imu_id, enabled)
3. Motion state (FF02, notify):
   - Current encoder angles (4x uint16)
   - Active pattern ID + parameters
   - IMU orientation (gravity vector)
4. LED commands (FF03):
   - `0x01`: Set layer effect (payload: layer_idx, effect_id, blend_mode)
   - `0x02`: Set effect parameter (payload: layer_idx, param_id, value)
   - `0x03`: Remove layer (payload: layer_idx)
   - `0x04`: Set flip/mirror (payload: layer_idx, flip_x, flip_y, mirror_x, mirror_y)
   - `0x05`: Upload image chunk (payload: offset, data)
   - `0x06`: Finalize image upload
5. LED state (FF04, notify):
   - Active layers (count, effect IDs, blend modes)
   - Per-layer parameters
6. System config (FF06):
   - `0x01`: Set LED matrix config (payload: num_rings, leds_per_ring[])
   - `0x02`: Read system info (firmware version, uptime, free heap)
7. System state (FF07, notify):
   - IMU raw data (optional, for debugging)
   - Tap events (notification on tap)
   - Battery level (if applicable)
8. ATT MTU negotiation
   - Request MTU of 256 to fit larger config packets in single writes

**Deliverables:** Every feature controllable via BLE.

**Testing:**
- Walk through every command from nRF Connect
- Verify state notifications reflect changes
- Invalid command handling (wrong size, out of range values)
- Rapid command sequences (stress test)

---

### Phase 11: Persistence and Profiles

**Goal:** Configuration survives power cycles. Save/load profiles.

**Tasks:**
1. NVS schema design
   - Namespace: `tail_cfg` for active config
   - Namespace: `tail_profN` for saved profiles (N = 0-3)
   - Keys: `servo_cfg`, `axis_cfg`, `matrix_cfg`, `layers`, `pattern`, `pid_X` etc.
   - Schema version number for migration on firmware updates
2. Config save/load
   - On every config change: debounce 2 seconds, then write to NVS
   - On boot: load from NVS, validate all values, fallback to defaults on error
   - `config_manager` handles serialization to/from NVS blobs
3. Profile system (FF08 characteristic)
   - `0x01`: Save current config to profile slot (payload: slot 0-3)
   - `0x02`: Load profile (payload: slot 0-3)
   - `0x03`: List profiles (returns: slot occupancy + names)
   - `0x04`: Delete profile (payload: slot)
   - `0x05`: Rename profile (payload: slot, name string)
4. Power-on restore
   - On boot: load last active profile (stored as `active_profile` in NVS)
   - If load fails: use defaults (all servos centered, LED off, no pattern)

**Deliverables:** Unplug and replug → same state. Multiple saveable profiles.

**Testing:**
- Configure LEDs and motion, power cycle, verify restored
- Save 4 profiles, switch between them
- Corrupt NVS intentionally, verify graceful fallback to defaults
- Firmware update: verify old config still loads (schema migration)

---

### Phase 12: Integration and Polish

**Goal:** Everything works together reliably. Ready for companion app development.

**Tasks:**
1. Integration testing
   - Run all systems simultaneously: motion pattern + LED effects + FFT stream + BLE config
   - Profile memory usage: `heap_caps_get_free_size()` under full load
   - Profile CPU usage: `vTaskGetRunTimeStats()` to verify no task starvation
2. Performance optimization
   - If LED rendering is slow: reduce effect complexity or frame rate
   - If motion control jitters: increase task priority or reduce I2C reads
   - If BLE throughput bottlenecks: tune connection parameters
3. Error handling hardening
   - I2C timeouts: retry with backoff, log errors, continue operation
   - BLE disconnects: clean up state, resume advertising
   - NVS full: compact or erase oldest data
   - Stack overflow detection: enable FreeRTOS stack overflow checking
4. BLE protocol documentation
   - Complete spec for every characteristic, command, and notification
   - Example packet sequences for common operations
   - Markdown doc suitable for companion app developers
5. Companion app developer guide
   - Service discovery procedure
   - Recommended connection parameters
   - Command sequence for initial setup
   - Command sequence for streaming FFT data
   - Error handling and reconnection strategy

**Deliverables:** Stable firmware, complete documentation, ready for app development.
