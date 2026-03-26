# Architecture

## Overview

TailFirmware runs on the Raspberry Pi Pico 2W (RP2350 MCU + CYW43439 wireless chip). It exposes a BLE GATT server that accepts servo commands from a connected client and drives 4 PWM outputs to control servos.

```
BLE Client  --(GATT write)-->  ble_service  --(callback)-->  servo module  --(PWM)-->  Servos
                                    |
                              advertises as
                            "Tail controller"
```

## Modules

### main.c

Initialization sequence:
1. `stdio_init_all()` - USB serial for debug
2. `cyw43_arch_init()` - Initialize CYW43439 wireless chip
3. `servo_init()` - Configure PWM on GPIO 2-5
4. `ble_service_init(callback)` - Set up BTstack BLE stack and start advertising
5. `hci_power_control(HCI_POWER_ON)` - Power on Bluetooth
6. Infinite loop with `tight_loop_contents()`

The main loop does nothing because BLE events are handled in the background by the threadsafe_background async context (interrupts/timers).

### servo module (servo.h/c)

Controls 4 servos via hardware PWM.

**Pin assignments:**

| Servo | GPIO | PWM Slice | Channel |
|-------|------|-----------|---------|
| 0     | 2    | 1         | A       |
| 1     | 3    | 1         | B       |
| 2     | 4    | 2         | A       |
| 3     | 5    | 2         | B       |

**PWM configuration:**
- Clock divider: `sys_clk / 1,000,000` (dynamically calculated, 150.0 at default 150MHz RP2350 clock)
- Wrap value: 19999 (20ms period = 50Hz)
- Pulse range: 500 counts (0 degrees) to 2500 counts (180 degrees)
- All servos initialize to 90 degrees (center)

The angle-to-pulse mapping is linear:
```
pulse_us = 500 + (angle * 2000 / 180)
```

### ble_service module (ble_service.h/c)

Manages the BLE GATT server using BTstack.

**Initialization:**
1. `l2cap_init()` - L2CAP protocol layer
2. `sm_init()` - Security Manager (default config, no pairing requirements)
3. `att_server_init()` - ATT server with generated GATT database and read/write callbacks
4. Configure advertisements (30ms interval, connectable undirected)
5. Register HCI event handler and ATT packet handler

**Advertisement data** is manually constructed with:
- Flags: LE General Discoverable + BR/EDR Not Supported (0x06)
- Complete Local Name: "Tail controller"
- 128-bit service UUID list

**GATT callbacks:**
- `att_read_callback`: Returns 4-byte array of current servo angles
- `att_write_callback`: Parses 2-byte command (servo_id + angle), calls servo_cmd_callback, triggers notification if enabled
- `packet_handler`: Handles connect/disconnect events and CAN_SEND_NOW for notifications

**Connection behavior:**
- On disconnect: re-enables advertising automatically
- Notifications: client must enable via CCC descriptor write
- Single connection supported (MAX_NR_HCI_CONNECTIONS = 1)

### tail_service.gatt

BTstack GATT database definition file. Compiled at build time by `compile_gatt.py` into `tail_service.h`, which contains:
- `profile_data[]` - binary GATT database
- Handle defines like `ATT_CHARACTERISTIC_0000FF01_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE`

## Data Flow

### Writing a servo command

1. BLE client writes `[servo_id, angle]` to characteristic `0000FF01-...`
2. BTstack calls `att_write_callback()` in `ble_service.c`
3. Callback invokes `servo_cmd_callback` (set during init, points to `on_servo_command` in main.c)
4. `on_servo_command` calls `servo_set_angle(servo_id, angle)`
5. `servo_set_angle` updates the PWM duty cycle on the corresponding GPIO
6. If notifications are enabled, a state update is queued via `att_server_request_can_send_now_event`

### Reading servo state

1. BLE client reads characteristic `0000FF02-...`
2. BTstack calls `att_read_callback()` in `ble_service.c`
3. Callback collects all 4 angles via `servo_get_angle()` and returns them
