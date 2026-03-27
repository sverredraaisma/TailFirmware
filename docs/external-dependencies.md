# External Dependencies

All code comes bundled with ESP-IDF except the `led_strip` managed component which is fetched automatically.

## ESP-IDF Components

### NimBLE (`bt`)
BLE 5.0 stack for the ESP32-C3. Provides GAP, GATT server, Security Manager, and NVS-backed bond storage. Configured for peripheral role only via `sdkconfig.defaults`.

### LEDC (`esp_driver_ledc`)
LED Control PWM peripheral. Drives 4 continuous rotation servos at 50 Hz with 14-bit resolution (16384 duty levels). ESP32-C3 supports low-speed mode only.

### I2C Master (`driver`)
I2C master driver using the new ESP-IDF v5.x API (`i2c_master_bus_handle_t`). Communicates with the TCA9548A multiplexer, AS5600 encoders, and BMI270 IMUs at 400 kHz.

### RMT (`esp_driver_rmt`)
Remote Control Transceiver peripheral. Used by the `led_strip` component to generate precise WS2812B timing (800 kHz data rate) entirely in hardware, immune to interrupt jitter.

### NVS Flash (`nvs_flash`)
Non-Volatile Storage. Stores:
- Active configuration (`tail_cfg` namespace)
- Up to 4 saved profiles (`tail_prof0` through `tail_prof3`)
- BLE bonding keys (managed by NimBLE automatically)

### FreeRTOS (`freertos`)
RTOS kernel. The firmware runs 5 tasks:

| Task | Priority | Stack | Rate | Purpose |
|------|----------|-------|------|---------|
| `nimble_host` | 4 | 4 KB | Event-driven | BLE stack |
| `motion` | 5 | 4 KB | 100 Hz | PID control, encoder reads, IMU reads |
| `led_rend` | 3 | 4 KB | 30 Hz | Effect rendering, LED strip output |
| `config` | 2 | 3 KB | 1 Hz | NVS debounced save, BLE state updates |
| `led_stat` | 1 | 2 KB | ~5 Hz | Status LED blink |

## Managed Component

### espressif/led_strip (^2.5)
ESP Component Registry package. Provides WS2812B/SK6812 driver using RMT backend. Declared in `main/idf_component.yml`, fetched on first build.

## External Hardware

### TCA9548A I2C Multiplexer
- Address: 0x70 (A0=A1=A2=GND)
- 8 downstream channels, 400 kHz max
- Channel selection: single byte write, each bit enables a channel
- Driver: `main/drivers/i2c_mux.c`

### AS5600 Magnetic Rotary Encoder (x4)
- Address: 0x36 (fixed, hence the need for multiplexer)
- 12-bit angle (0-4095 = 0-360 degrees)
- Raw angle registers: 0x0C (high) + 0x0D (low)
- Driver: `main/drivers/encoder_driver.c`
- Multi-turn tracking with wrap detection at 0/4095 boundary

### BMI270 IMU (x2)
- Address: 0x68 (SDO=GND)
- 6-axis: accelerometer (+-8g) + gyroscope (+-2000 dps)
- Accelerometer data: registers 0x0C-0x11
- Gyroscope data: registers 0x12-0x17
- Hardware tap detection available (requires proprietary config blob upload - TODO)
- Software tap detection implemented (acceleration spike > 2.5g threshold)
- Driver: `main/drivers/imu_driver.c`

### Continuous Rotation Servos (x4)
- Controlled via 50 Hz PWM pulse width
- 500 us: full reverse, 1500 us: stop, 2500 us: full forward
- Position feedback via paired AS5600 encoder
- PID closed-loop control at 100 Hz

### WS2812B LED Strip
- Configurable ring layout (up to 20 rings, variable LEDs per ring)
- 3.3V data from ESP32-C3 (level shifter recommended for 5V LEDs)
- Single data line on GPIO 7

## C++ Standard Library Usage

The firmware uses C++20 features:
- `std::unique_ptr` for polymorphic effect/pattern ownership
- `std::vector` for LED coordinate maps and pixel buffers
- `std::make_unique` for factory pattern
- `<cmath>` for sin/cos/fmod in patterns and effects
- `<cstring>` / `<algorithm>` for buffer operations

RTTI is disabled (`-fno-rtti`). Virtual dispatch works but `dynamic_cast` does not. Type checking is done via config IDs stored alongside the polymorphic objects.
