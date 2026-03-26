# App Integration Guide

## Quick Start

1. Power on the Pico 2W - the LED blinks fast when advertising
2. Open your BLE app and scan for devices
3. Connect to **"Tail controller"**
4. Find service `0000FF00-0000-1000-8000-00805F9B34FB`
5. Write `[servo_id, angle]` to characteristic `0000FF01-...` to move a servo

## Device Discovery

The device advertises as a connectable BLE peripheral:

| Field | Value |
|-------|-------|
| Name | `Tail controller` |
| Service UUID | `0000FF00-0000-1000-8000-00805F9B34FB` |

**Scan filter:** Filter by name `Tail controller` or by the service UUID in the scan response.

**LED status:**
- Fast blink = advertising (ready to connect)
- Solid on = connected
- Slow blink = not ready

## Connection

Connect using a standard BLE GATT connection. No pairing is required to read/write characteristics. If your OS prompts to pair, it will use "Just Works" (no PIN) and the bond is saved for faster reconnects.

Only one connection at a time is supported. While a client is connected, the device stops advertising.

## GATT Service Layout

After connecting, discover services. The relevant service:

**Service:** `0000FF00-0000-1000-8000-00805F9B34FB`

### Characteristic: Servo Command (write)

| Property | Value |
|----------|-------|
| UUID | `0000FF01-0000-1000-8000-00805F9B34FB` |
| Properties | Write, Write Without Response |
| Value | 2 bytes |

**Payload format:**

| Byte | Name | Type | Range | Description |
|------|------|------|-------|-------------|
| 0 | servo_id | uint8 | 0-3 | Servo index |
| 1 | angle | uint8 | 0-180 | Angle in degrees |

**Examples (hex):**

| Action | Bytes |
|--------|-------|
| Servo 0 to 0 degrees | `00 00` |
| Servo 0 to 90 degrees | `00 5A` |
| Servo 0 to 180 degrees | `00 B4` |
| Servo 1 to 45 degrees | `01 2D` |
| Servo 2 to 135 degrees | `02 87` |
| Servo 3 to 90 degrees | `03 5A` |

**Notes:**
- Use **Write Without Response** for low-latency control (recommended for real-time movement)
- Use **Write** (with response) when you need confirmation the command was received
- Invalid servo IDs (>3) are silently ignored
- Angles above 180 are clamped to 180

### Characteristic: Servo State (read/notify)

| Property | Value |
|----------|-------|
| UUID | `0000FF02-0000-1000-8000-00805F9B34FB` |
| Properties | Read, Notify |
| Value | 4 bytes |

**Payload format:**

| Byte | Name | Type | Description |
|------|------|------|-------------|
| 0 | angle_0 | uint8 | Current angle of servo 0 |
| 1 | angle_1 | uint8 | Current angle of servo 1 |
| 2 | angle_2 | uint8 | Current angle of servo 2 |
| 3 | angle_3 | uint8 | Current angle of servo 3 |

**Reading:** Read this characteristic at any time to get all servo positions.

**Notifications:** Write `01 00` to the CCC descriptor (handle follows the characteristic value) to enable notifications. The device sends a notification after each servo command write, so your app receives the updated state automatically.

## Platform Code Examples

### Android (Kotlin)

```kotlin
// UUIDs
val SERVICE_UUID = UUID.fromString("0000FF00-0000-1000-8000-00805F9B34FB")
val SERVO_CMD_UUID = UUID.fromString("0000FF01-0000-1000-8000-00805F9B34FB")
val SERVO_STATE_UUID = UUID.fromString("0000FF02-0000-1000-8000-00805F9B34FB")

// Write servo command (after connecting and discovering services)
fun setServo(gatt: BluetoothGatt, servoId: Int, angle: Int) {
    val service = gatt.getService(SERVICE_UUID)
    val char = service.getCharacteristic(SERVO_CMD_UUID)
    char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
    char.value = byteArrayOf(servoId.toByte(), angle.toByte())
    gatt.writeCharacteristic(char)
}

// Read servo state
fun readState(gatt: BluetoothGatt) {
    val service = gatt.getService(SERVICE_UUID)
    val char = service.getCharacteristic(SERVO_STATE_UUID)
    gatt.readCharacteristic(char)
}

// Enable notifications for state changes
fun enableNotifications(gatt: BluetoothGatt) {
    val service = gatt.getService(SERVICE_UUID)
    val char = service.getCharacteristic(SERVO_STATE_UUID)
    gatt.setCharacteristicNotification(char, true)
    val descriptor = char.getDescriptor(
        UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")  // CCC descriptor
    )
    descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
    gatt.writeDescriptor(descriptor)
}
```

### iOS (Swift / CoreBluetooth)

```swift
import CoreBluetooth

let serviceUUID = CBUUID(string: "0000FF00-0000-1000-8000-00805F9B34FB")
let servoCmdUUID = CBUUID(string: "0000FF01-0000-1000-8000-00805F9B34FB")
let servoStateUUID = CBUUID(string: "0000FF02-0000-1000-8000-00805F9B34FB")

// Scan filtering by service UUID
centralManager.scanForPeripherals(withServices: [serviceUUID])

// Write servo command (in didDiscoverCharacteristicsFor)
func setServo(_ peripheral: CBPeripheral, servoId: UInt8, angle: UInt8) {
    guard let char = servoCommandCharacteristic else { return }
    let data = Data([servoId, angle])
    peripheral.writeValue(data, for: char, type: .withoutResponse)
}

// Read state
func readState(_ peripheral: CBPeripheral) {
    guard let char = servoStateCharacteristic else { return }
    peripheral.readValue(for: char)
}

// Enable notifications
func enableNotifications(_ peripheral: CBPeripheral) {
    guard let char = servoStateCharacteristic else { return }
    peripheral.setNotifyValue(true, for: char)
}

// Handle notification
func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
    guard let data = characteristic.value, data.count == 4 else { return }
    let angles = [UInt8](data)  // [servo0, servo1, servo2, servo3]
}
```

### Web Bluetooth (JavaScript)

```javascript
const SERVICE_UUID = '0000ff00-0000-1000-8000-00805f9b34fb';
const SERVO_CMD_UUID = '0000ff01-0000-1000-8000-00805f9b34fb';
const SERVO_STATE_UUID = '0000ff02-0000-1000-8000-00805f9b34fb';

async function connect() {
    const device = await navigator.bluetooth.requestDevice({
        filters: [{ name: 'Tail controller' }],
        optionalServices: [SERVICE_UUID]
    });
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    const cmdChar = await service.getCharacteristic(SERVO_CMD_UUID);
    const stateChar = await service.getCharacteristic(SERVO_STATE_UUID);
    return { cmdChar, stateChar };
}

async function setServo(cmdChar, servoId, angle) {
    const data = new Uint8Array([servoId, angle]);
    await cmdChar.writeValueWithoutResponse(data);
}

async function enableNotifications(stateChar, callback) {
    stateChar.addEventListener('characteristicvaluechanged', (event) => {
        const angles = new Uint8Array(event.target.value.buffer);
        callback(angles);  // [servo0, servo1, servo2, servo3]
    });
    await stateChar.startNotifications();
}
```

## Testing with nRF Connect

1. Install **nRF Connect** (free, available on iOS and Android)
2. Open the app and tap **Scan**
3. Find **"Tail controller"** and tap **Connect**
4. Expand the service `0000FF00-...`
5. Tap the **up arrow** on `0000FF01-...` to write
6. Select **Byte Array**, enter `00 5A`, tap **Send** - servo 0 moves to 90 degrees
7. Tap the **down arrow** on `0000FF02-...` to read current angles
8. Tap the **triple-down arrow** to enable notifications

## Timing and Latency

- **Write Without Response** latency: ~7.5ms (one BLE connection interval)
- **Write With Response** latency: ~15ms (request + acknowledgment)
- Default BLE connection interval is negotiated by the phone (typically 7.5-30ms)
- The servo PWM updates immediately on command receipt - no firmware-side batching or delay
- For smooth animation, send commands at 20-50ms intervals using Write Without Response
