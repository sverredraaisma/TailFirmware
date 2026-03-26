# BLE Protocol

## Device Info

- **Advertised Name:** Tail controller
- **Advertisement Type:** Connectable undirected
- **Advertisement Interval:** 30ms (0x0030)
- **Flags:** LE General Discoverable + BR/EDR Not Supported (0x06)

## GATT Service

**Service UUID:** `0000FF00-0000-1000-8000-00805F9B34FB`

### Servo Command Characteristic

- **UUID:** `0000FF01-0000-1000-8000-00805F9B34FB`
- **Properties:** Write, Write Without Response
- **Value:** 2 bytes

| Byte | Field | Range | Description |
|------|-------|-------|-------------|
| 0 | servo_id | 0-3 | Which servo to control |
| 1 | angle | 0-180 | Target angle in degrees |

Out-of-range servo_id values are silently ignored. Angles above 180 are clamped to 180.

**Write Without Response** is supported for low-latency control (no acknowledgment round-trip).

### Servo State Characteristic

- **UUID:** `0000FF02-0000-1000-8000-00805F9B34FB`
- **Properties:** Read, Notify
- **Value:** 4 bytes

| Byte | Field | Description |
|------|-------|-------------|
| 0 | angle0 | Current angle of servo 0 |
| 1 | angle1 | Current angle of servo 1 |
| 2 | angle2 | Current angle of servo 2 |
| 3 | angle3 | Current angle of servo 3 |

**Notifications:** Enable by writing `0x0100` to the Client Characteristic Configuration (CCC) descriptor. When enabled, a notification is sent after each servo command write.

## Usage Examples

### Set servo 0 to 45 degrees
Write `0x00 0x2D` to characteristic `0000FF01-...`

### Set servo 2 to 180 degrees
Write `0x02 0xB4` to characteristic `0000FF01-...`

### Read all servo positions
Read characteristic `0000FF02-...`, returns e.g. `0x2D 0x5A 0xB4 0x5A` (45, 90, 180, 90 degrees)

## Connection Behavior

- Only one concurrent BLE connection is supported
- On disconnect, advertising resumes automatically
- Servo positions are preserved across BLE disconnects (servos hold their last commanded position)
- All servos initialize to 90 degrees (center) on power-up
