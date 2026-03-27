# BLE Protocol Specification

Complete reference for companion app developers. All data is little-endian. All floats are IEEE 754 single-precision (4 bytes, little-endian).

## Connection Setup

- **Device Name:** `Tail controller`
- **Service UUID:** `0000FF00-0000-1000-8000-00805F9B34FB`
- **Pairing:** Just Works (no PIN), bonding enabled, LE Secure Connections
- **Recommended MTU:** Request 256 bytes (default 23 is too small for some commands)
- **Recommended Connection Interval:** 7.5-15 ms for responsive control

## Characteristics Overview

| UUID (short) | Full UUID | Properties | Description |
|---|---|---|---|
| `FF01` | `0000FF01-0000-1000-8000-00805F9B34FB` | Write, Write No Rsp | Motion commands |
| `FF02` | `0000FF02-0000-1000-8000-00805F9B34FB` | Read, Notify | Motion state |
| `FF03` | `0000FF03-0000-1000-8000-00805F9B34FB` | Write, Write No Rsp | LED commands |
| `FF04` | `0000FF04-0000-1000-8000-00805F9B34FB` | Read, Notify | LED state |
| `FF05` | `0000FF05-0000-1000-8000-00805F9B34FB` | Write No Rsp | FFT audio stream |
| `FF06` | `0000FF06-0000-1000-8000-00805F9B34FB` | Read, Write | System config |
| `FF07` | `0000FF07-0000-1000-8000-00805F9B34FB` | Read, Notify | System events |
| `FF08` | `0000FF08-0000-1000-8000-00805F9B34FB` | Read, Write | Profile management |

## General Write Format

All writable characteristics (except FF05) use:

```
[command_id: u8] [payload: variable]
```

Invalid commands or undersized payloads are silently ignored (logged on device serial).

---

## FF01: Motion Command (Write, Write No Rsp)

### 0x01 - Select Pattern

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x01` |
| 1 | u8 | Pattern ID |

**Pattern IDs:**

| ID | Name | Description |
|----|------|-------------|
| `0x00` | Static | Fixed servo positions |
| `0x01` | Wagging | Side-to-side oscillation |
| `0x02` | Loose | Physics-based gravity response |

**Example:** Select wagging: `01 01`

### 0x02 - Set Pattern Parameter

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x02` |
| 1 | u8 | Parameter ID (0-7) |
| 2-5 | f32 | Value (IEEE 754 LE) |

**Total: 6 bytes**

**Pattern parameters:**

#### Static Pattern (0x00)
| Param | Description | Default | Unit |
|-------|-------------|---------|------|
| 0 | X first half position | 0 | degrees |
| 1 | X second half position | 0 | degrees |
| 2 | Y first half position | 0 | degrees |
| 3 | Y second half position | 0 | degrees |

#### Wagging Pattern (0x01)
| Param | Description | Default | Range | Unit |
|-------|-------------|---------|-------|------|
| 0 | Frequency | 1.0 | 0.1-10 | Hz |
| 1 | X amplitude | 45 | 0-180 | degrees |
| 2 | Y first half position | 0 | | degrees |
| 3 | Y second half position | 0 | | degrees |

#### Loose Pattern (0x02)
| Param | Description | Default | Range |
|-------|-------------|---------|-------|
| 0 | Damping | 0.3 | 0.0-1.0 |
| 1 | Reactivity | 3.0 | 0.0-10.0 |

**Example:** Set wagging frequency to 2.0 Hz: `02 00 00 00 00 40` (0x40000000 = 2.0f)

### 0x03 - Set Servo Configuration

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x03` |
| 1 | u8 | Servo ID (0-3) |
| 2 | u8 | Axis (0=X, 1=Y) |
| 3 | u8 | Half (0=first, 1=second) |
| 4 | u8 | Invert (0=normal, 1=reverse direction) |

**Total: 5 bytes**

**Example:** Servo 0 is X-axis first half, normal direction: `03 00 00 00 00`

### 0x04 - Set PID Gains

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x04` |
| 1 | u8 | Servo ID (0-3) |
| 2-5 | f32 | Kp (proportional gain) |
| 6-9 | f32 | Ki (integral gain) |
| 10-13 | f32 | Kd (derivative gain) |

**Total: 14 bytes**

**Default PID values:** Kp=1.0, Ki=0.0, Kd=0.0. Start with Kp only, add Ki/Kd as needed.

### 0x05 - Calibrate Zero Position

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x05` |

**Total: 1 byte.** Stores current encoder positions as the zero reference for all axes.

### 0x06 - Set Axis Limits

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x06` |
| 1 | u8 | Axis (0=X, 1=Y) |
| 2-5 | f32 | Min angle from zero (degrees, typically negative) |
| 6-9 | f32 | Max angle from zero (degrees, typically positive) |

**Total: 10 bytes**

### 0x07 - Enable/Disable IMU Tap Detection

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x07` |
| 1 | u8 | IMU ID (0=base, 1=tip) |
| 2 | u8 | Enabled (0=off, 1=on) |

**Total: 3 bytes**

---

## FF02: Motion State (Read, Notify)

**Payload: 29 bytes.** Updated once per second.

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Active pattern ID |
| 1-4 | f32 | Encoder 0 position (degrees from zero) |
| 5-8 | f32 | Encoder 1 position |
| 9-12 | f32 | Encoder 2 position |
| 13-16 | f32 | Encoder 3 position |
| 17-20 | f32 | Gravity X (from base IMU, in g) |
| 21-24 | f32 | Gravity Y |
| 25-28 | f32 | Gravity Z |

---

## FF03: LED Command (Write, Write No Rsp)

### 0x01 - Set Layer Effect

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x01` |
| 1 | u8 | Layer index (0-7, bottom to top) |
| 2 | u8 | Effect ID |
| 3 | u8 | Blend mode |

**Total: 4 bytes**

**Effect IDs:**

| ID | Name |
|----|------|
| `0x00` | Rainbow |
| `0x01` | Static Color |
| `0x02` | Image |
| `0x03` | Audio Power |
| `0x04` | Audio Bar |
| `0x05` | Audio Freq Bars |

**Blend Modes:**

| ID | Name | Formula |
|----|------|---------|
| `0x00` | Multiply | `base * overlay / 255` per channel |
| `0x01` | Add | `min(base + overlay, 255)` |
| `0x02` | Subtract | `max(base - overlay, 0)` |
| `0x03` | Min | `min(base, overlay)` per channel |
| `0x04` | Max | `max(base, overlay)` per channel |
| `0x05` | Overwrite | `overlay == black ? base : overlay` |

### 0x02 - Set Effect Parameter

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x02` |
| 1 | u8 | Layer index (0-7) |
| 2 | u8 | Parameter ID (0-7) |
| 3-6 | f32 | Value |

**Total: 7 bytes**

**Effect parameters by type:**

#### Rainbow (0x00)
| Param | Description | Default | Range |
|-------|-------------|---------|-------|
| 0 | Direction (0=horizontal, 1=vertical, 2=diagonal) | 0 | 0-2 |
| 1 | Speed (hue shift/sec) | 60 | 0-360 |
| 2 | Scale (hue cycles across range) | 1.0 | 0.1-10 |

#### Static Color (0x01)
| Param | Description | Default | Range |
|-------|-------------|---------|-------|
| 0 | Red | 255 | 0-255 |
| 1 | Green | 255 | 0-255 |
| 2 | Blue | 255 | 0-255 |

#### Image (0x02)
| Param | Description | Default | Range |
|-------|-------------|---------|-------|
| 0 | Orientation (0=0, 1=90, 2=180, 3=270 deg) | 0 | 0-3 |

#### Audio Power (0x03)
| Param | Description | Default | Range |
|-------|-------------|---------|-------|
| 0 | Red | 0 | 0-255 |
| 1 | Green | 255 | 0-255 |
| 2 | Blue | 0 | 0-255 |
| 3 | Fade rate (decay/sec) | 3.0 | 0-10 |

#### Audio Bar (0x04)
| Param | Description | Default | Range |
|-------|-------------|---------|-------|
| 0 | Red | 0 | 0-255 |
| 1 | Green | 0 | 0-255 |
| 2 | Blue | 255 | 0-255 |
| 3 | Direction (0=left-right, 1=bottom-top) | 0 | 0-1 |
| 4 | Fade rate | 3.0 | 0-10 |

#### Audio Freq Bars (0x05)
| Param | Description | Default | Range |
|-------|-------------|---------|-------|
| 0 | Number of bars | 8 | 1-32 |
| 1 | Red | 0 | 0-255 |
| 2 | Green | 255 | 0-255 |
| 3 | Blue | 0 | 0-255 |
| 4 | Fade rate | 5.0 | 0-10 |
| 5 | Orientation (0=horizontal, 1=vertical) | 0 | 0-1 |

### 0x03 - Remove Layer

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x03` |
| 1 | u8 | Layer index |

**Total: 2 bytes**

### 0x04 - Set Layer Transform

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x04` |
| 1 | u8 | Layer index |
| 2 | u8 | Flip X (0/1) |
| 3 | u8 | Flip Y (0/1) |
| 4 | u8 | Mirror X (0/1) |
| 5 | u8 | Mirror Y (0/1) |

**Total: 6 bytes.** Mirror is applied before flip.

### 0x05 - Upload Image Chunk

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x05` |
| 1-2 | u16 LE | Byte offset into image buffer |
| 3+ | u8[] | RGB pixel data |

**Max buffer: 3072 bytes (32x32 RGB).** Upload in chunks that fit your MTU.

### 0x06 - Finalize Image Upload

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x06` |
| 1 | u8 | Width (pixels) |
| 2 | u8 | Height (pixels) |
| 3 | u8 | Target layer index |

**Total: 4 bytes.** Send after all chunks. Creates or updates the Image effect on the target layer.

### 0x07 - Set Layer Enabled

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x07` |
| 1 | u8 | Layer index |
| 2 | u8 | Enabled (0/1) |

**Total: 3 bytes**

---

## FF04: LED State (Read, Notify)

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Number of configured layers |
| 1+ | u8[3] | Per layer: [effect_id, blend_mode, enabled] |

---

## FF05: FFT Audio Stream (Write No Rsp)

**No command_id prefix - raw data for minimum overhead.**

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Loudness (0-255, perceived volume) |
| 1 | u8 | Number of bins (N) |
| 2 to 2+N-1 | u8[] | Bin values, low-to-high frequency |

**Recommended:** 64 bins at 30 fps = 66 bytes/frame, ~2 KB/sec. Use Write Without Response. Data is double-buffered; if no data for 200ms, audio effects fade to silence.

---

## FF06: System Config (Read, Write)

### Write: 0x01 - Set LED Matrix

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | Command: `0x01` |
| 1 | u8 | Number of rings (1-20) |
| 2+ | u8[] | LEDs per ring |

Rings are evenly spaced on Y axis (y=0 to y=1). LEDs per ring are evenly spaced on X axis (x=0 to x=1).

**Example:** 5 rings of 8,10,12,10,8: `01 05 08 0A 0C 0A 08`

---

## FF07: System State (Read, Notify)

Tap event notification payload:

| Offset | Type | Description |
|--------|------|-------------|
| 0 | u8 | `0x01` = base tap, `0x02` = tip tap |

---

## FF08: Profile Management (Read, Write)

| Command | ID | Payload | Description |
|---------|-----|---------|-------------|
| Save | `0x01` | [slot: u8] (0-3) | Save current config to slot |
| Load | `0x02` | [slot: u8] (0-3) | Load config from slot |
| List | `0x03` | (none) | Request profile list (read after) |
| Delete | `0x04` | [slot: u8] (0-3) | Erase a profile slot |

---

## Persistence

- Config changes auto-save to NVS with 2-second debounce
- Power-on restores the last active configuration
- Profile save/load is immediate
- 4 profile slots (0-3) available

---

## Common Sequences

### Initial Setup
1. Connect, negotiate MTU 256
2. Set matrix: `01 05 08 0A 0C 0A 08` to FF06
3. Assign servos: `03 00 00 00 00`, `03 01 00 01 00`, `03 02 01 00 00`, `03 03 01 01 00` to FF01
4. Position tail at neutral, calibrate: `05` to FF01
5. Set limits: `06 00 [min_f32] [max_f32]` and `06 01 [min_f32] [max_f32]` to FF01

### Wagging with Rainbow
1. Select wagging: `01 01` to FF01
2. Set frequency: `02 00 00 00 C0 3F` to FF01 (1.5 Hz)
3. Set amplitude: `02 01 00 00 70 42` to FF01 (60 deg)
4. Set rainbow layer 0: `01 00 00 05` to FF03
5. Save profile: `01 00` to FF08

### Audio Visualization
1. Set audio freq bars on layer 0: `01 00 05 05` to FF03
2. Set 16 bars: `02 00 00 00 00 80 41` to FF03
3. Enable notifications on FF07
4. Stream FFT at 30fps to FF05: `[loudness] [64] [64 bin values]`

### Layer Blending Example
1. Layer 0: static white, overwrite: `01 00 01 05` to FF03
2. Layer 1: audio power green, multiply: `01 01 03 00` to FF03
3. Result: white LEDs that pulse brightness with audio
