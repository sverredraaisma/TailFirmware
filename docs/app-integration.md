# Companion App Integration Guide

## Quick Start

1. Power on the ESP32-C3 - the LED blinks fast when advertising
2. Scan for BLE device named **"Tail controller"**
3. Connect and negotiate MTU 256
4. Configure the LED matrix (FF06) to match your hardware
5. Configure servo assignments (FF01) and calibrate zero
6. Select a motion pattern and LED effects
7. Save to a profile slot (FF08)

## Device Discovery

| Field | Value |
|-------|-------|
| Name | `Tail controller` |
| Service UUID | `0000FF00-0000-1000-8000-00805F9B34FB` |

Filter by service UUID in scan response for reliable discovery. The device supports one connection at a time.

**LED status:**
- Fast blink (200ms) = advertising, ready to connect
- Solid on = connected
- Slow blink (1s) = BLE stack not ready

## Connection Setup

1. **Connect** to the device
2. **Negotiate MTU:** Request 256 bytes. Some commands (PID tuning, image upload) exceed the default 23-byte MTU.
3. **Discover services:** Find service `0000FF00-...` with 8 characteristics
4. **Enable notifications** on FF02 (motion state), FF04 (LED state), FF07 (system events) as needed
5. **Pairing:** The device uses Just Works pairing with bonding. The first connection triggers pairing automatically. Subsequent connections reuse the bond.

## LED Matrix Configuration

Before any LED effects work, you must tell the device about the physical LED layout.

The tail has rings of WS2812B LEDs. Each ring can have a different number of LEDs. Rings are evenly spaced along the tail (Y axis), and LEDs within each ring are evenly spaced around the ring (X axis).

**Write to FF06:**
```
01              command: set matrix config
05              number of rings
08 0A 0C 0A 08  LEDs per ring: 8, 10, 12, 10, 8
```

This creates a coordinate system where every LED has an (x, y) position in [0,1] x [0,1]. All effects operate in this coordinate space.

## Servo Setup

### 1. Assign Servos to Axes

The tail has 2 axes (X=left-right, Y=up-down), each with 2 halves (first half near base, second half near tip). Map each physical servo to its role.

**Write to FF01 for each servo:**
```
03 00 00 00 00    Servo 0 -> X axis, first half, normal direction
03 01 00 01 00    Servo 1 -> X axis, second half, normal direction
03 02 01 00 00    Servo 2 -> Y axis, first half, normal direction
03 03 01 01 01    Servo 3 -> Y axis, second half, inverted
```

### 2. PID Tuning

Each servo has its own PID controller. Start with proportional gain only:

**Write to FF01:**
```
04 00 00 00 40 40 00 00 00 00 00 00 00 00    Servo 0: Kp=3.0, Ki=0, Kd=0
```

Increase Kp until the servo responds quickly but doesn't oscillate. Add Kd (0.1-1.0) to dampen oscillation. Add Ki (0.01-0.1) only if there's steady-state error.

### 3. Calibrate Zero

Position the tail in its desired neutral position, then:

**Write to FF01:** `05`

All current encoder positions are stored as the zero reference.

### 4. Set Axis Limits

Prevent the tail from over-rotating:

**Write to FF01:**
```
06 00 00 00 B4 C2 00 00 B4 42    X axis: -90 to +90 degrees
06 01 00 00 34 C2 00 00 34 42    Y axis: -45 to +45 degrees
```

## Motion Patterns

### Static Pattern (ID: 0x00)

Holds fixed positions. Good for posing.

| Param | Description | Write Example (FF01) |
|-------|-------------|---------------------|
| 0 | X first half (deg) | `02 00 [float]` |
| 1 | X second half (deg) | `02 01 [float]` |
| 2 | Y first half (deg) | `02 02 [float]` |
| 3 | Y second half (deg) | `02 03 [float]` |

**Select:** `01 00`

### Wagging Pattern (ID: 0x01)

Side-to-side oscillation. The second half trails the first by a quarter-cycle for a natural wave.

| Param | Description | Default | Write Example (FF01) |
|-------|-------------|---------|---------------------|
| 0 | Frequency (Hz) | 1.0 | `02 00 00 00 80 3F` (1.0) |
| 1 | Amplitude (deg) | 45 | `02 01 00 00 34 42` (45.0) |
| 2 | Y first half pos | 0 | `02 02 [float]` |
| 3 | Y second half pos | 0 | `02 03 [float]` |

**Select:** `01 01`

### Loose Pattern (ID: 0x02)

Physics-based: the tail reacts to gravity/movement via the base IMU. Feels like a real tail.

| Param | Description | Default | Write Example (FF01) |
|-------|-------------|---------|---------------------|
| 0 | Damping (0-1) | 0.3 | `02 00 9A 99 99 3E` (0.3) |
| 1 | Reactivity (0-10) | 3.0 | `02 01 00 00 40 40` (3.0) |

**Select:** `01 02`

## LED Effects

### Setting Up an Effect

1. **Create a layer:** Write `01 [layer] [effect_id] [blend_mode]` to FF03
2. **Set parameters:** Write `02 [layer] [param_id] [float_value]` to FF03
3. **Optionally set transform:** Write `04 [layer] [flip_x] [flip_y] [mirror_x] [mirror_y]` to FF03

### Rainbow (ID: 0x00)

Animated hue gradient. Parameters: direction (0=H, 1=V, 2=diagonal), speed (deg/sec), scale.

**Example - vertical rainbow, medium speed:**
```
Write FF03: 01 00 00 05          Layer 0, rainbow, overwrite
Write FF03: 02 00 00 00 00 80 3F  Param 0 (direction) = 1.0 (vertical)
Write FF03: 02 00 01 00 00 F0 42  Param 1 (speed) = 120
Write FF03: 02 00 02 00 00 80 3F  Param 2 (scale) = 1.0
```

### Static Color (ID: 0x01)

Solid color fill. Parameters: R, G, B (0-255 as float).

**Example - solid purple:**
```
Write FF03: 01 00 01 05           Layer 0, static color, overwrite
Write FF03: 02 00 00 00 00 80 43  Param 0 (R) = 255
Write FF03: 02 00 01 00 00 00 00  Param 1 (G) = 0
Write FF03: 02 00 02 00 00 80 43  Param 2 (B) = 255
```

### Image (ID: 0x02)

Displays an uploaded bitmap. Max 32x32 pixels (3072 bytes RGB).

**Upload sequence:**
1. Send image data in chunks to FF03: `05 [offset_u16] [rgb_data...]`
2. Finalize: `06 [width] [height] [target_layer]`

### Audio Power (ID: 0x03)

Brightness pulses with audio loudness. Requires FFT streaming on FF05.

Parameters: R, G, B, fade_rate.

### Audio Bar (ID: 0x04)

A bar sweeps across the matrix proportional to volume. Parameters: R, G, B, direction, fade_rate.

### Audio Freq Bars (ID: 0x05)

Spectrum analyzer visualization. Parameters: num_bars, R, G, B, fade_rate, orientation.

## Layer Blending

Layers render bottom (0) to top (7). Each layer's output is blended with the accumulated result using its blend mode.

**Practical combinations:**

| Bottom Layer | Top Layer | Blend | Result |
|---|---|---|---|
| Static white | Audio Power (green) | Multiply | White LEDs pulsing brightness with music |
| Rainbow | Static Color (half-bright) | Multiply | Dimmed rainbow |
| Rainbow | Audio Bar (white) | Max | Rainbow with a white audio bar overlay |
| Image | Rainbow | Add | Image with rainbow tint added |

## FFT Audio Streaming

The companion app performs FFT on microphone/audio input and streams results to the device.

### Frame Format (write to FF05, no command_id)

```
[loudness: u8] [num_bins: u8] [bin0: u8] [bin1: u8] ... [binN-1: u8]
```

- **loudness:** 0-255 perceived volume (app determines scaling)
- **num_bins:** typically 64
- **bins:** energy per frequency band, 0-255, low-to-high frequency
- **Rate:** 30 fps recommended
- **Transport:** Write Without Response for minimum latency

### Freshness

If the device receives no FFT frames for 200ms, audio effects fade to silence. Resume streaming to reactivate.

## IMU Tap Detection

Two IMUs detect taps: one at the base, one at the tip. Enable/disable per IMU:

**Write to FF01:**
```
07 00 01    Enable base IMU tap
07 01 01    Enable tip IMU tap
```

Subscribe to FF07 notifications. On tap, you receive:
- `01` = base tap
- `02` = tip tap

Tap events are also available to motion patterns and LED effects as triggers.

## Profiles

Save and restore complete configurations (motion pattern, LED layers, servo config, matrix layout).

| Action | Write to FF08 |
|--------|---------------|
| Save to slot 0 | `01 00` |
| Load from slot 0 | `02 00` |
| Delete slot 2 | `04 02` |

4 slots available (0-3). Active config auto-saves to NVS with 2-second debounce. On power-up, the last active config is restored.

## Testing with nRF Connect

1. **Scan** and connect to "Tail controller"
2. Expand service `0000FF00-...`
3. **Configure matrix:** tap write on FF06, hex: `01 03 08 08 08` (3 rings of 8)
4. **Set rainbow:** tap write on FF03, hex: `01 00 00 05`
5. **Enable wagging:** tap write on FF01, hex: `01 01`
6. **Read state:** tap read on FF02 to see encoder positions
7. **Enable notifications:** tap the triple-arrow on FF07 for tap events

## Platform Code Examples

### Android (Kotlin) - Set Wagging Pattern

```kotlin
val MOTION_CMD = UUID.fromString("0000FF01-0000-1000-8000-00805F9B34FB")

fun selectWagging(gatt: BluetoothGatt) {
    val char = gatt.getService(SERVICE_UUID).getCharacteristic(MOTION_CMD)
    char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
    char.value = byteArrayOf(0x01, 0x01)  // cmd=select_pattern, id=wagging
    gatt.writeCharacteristic(char)
}

fun setFrequency(gatt: BluetoothGatt, hz: Float) {
    val buf = ByteBuffer.allocate(6).order(ByteOrder.LITTLE_ENDIAN)
    buf.put(0x02)  // cmd=set_pattern_param
    buf.put(0x00)  // param 0 = frequency
    buf.putFloat(hz)
    val char = gatt.getService(SERVICE_UUID).getCharacteristic(MOTION_CMD)
    char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
    char.value = buf.array()
    gatt.writeCharacteristic(char)
}
```

### iOS (Swift) - Stream FFT

```swift
let FFT_STREAM = CBUUID(string: "0000FF05-0000-1000-8000-00805F9B34FB")

func sendFFTFrame(_ peripheral: CBPeripheral, loudness: UInt8, bins: [UInt8]) {
    guard let char = fftCharacteristic else { return }
    var data = Data([loudness, UInt8(bins.count)])
    data.append(contentsOf: bins)
    peripheral.writeValue(data, for: char, type: .withoutResponse)
}

// Call at 30fps from your audio processing callback
```

### Web Bluetooth - Configure Layer

```javascript
const LED_CMD = '0000ff03-0000-1000-8000-00805f9b34fb';

async function setRainbowLayer(cmdChar) {
    // Set rainbow on layer 0 with overwrite blend
    await cmdChar.writeValueWithoutResponse(
        new Uint8Array([0x01, 0x00, 0x00, 0x05])
    );
    // Set speed to 120
    const buf = new ArrayBuffer(7);
    const view = new DataView(buf);
    view.setUint8(0, 0x02);  // cmd
    view.setUint8(1, 0x00);  // layer 0
    view.setUint8(2, 0x01);  // param 1 (speed)
    view.setFloat32(3, 120.0, true);  // little-endian
    await cmdChar.writeValueWithoutResponse(new Uint8Array(buf));
}
```

## Error Handling

- Invalid commands are silently ignored (logged on device serial for debugging)
- Undersized payloads are dropped
- Out-of-range servo/layer/axis IDs are bounds-checked and rejected
- If BLE disconnects, the device resumes advertising. All state is preserved.
- On reconnection, read FF02/FF04 to resync state, or just resend your config
