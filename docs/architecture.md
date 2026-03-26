# Architecture

## Overview

TailFirmware runs on the ESP32-C3 (RISC-V MCU with integrated BLE 5.0). It exposes a BLE GATT server that accepts servo commands from a connected client and drives 4 PWM outputs to control servos.

```
BLE Client  --(GATT write)-->  ble_service  --(callback)-->  servo module  --(LEDC PWM)-->  Servos
                                    |
                              advertises as
                            "Tail controller"
```

## Modules

### main.c

Initialization sequence:
1. `nvs_flash_init()` - Initialize NVS (required for BLE bonding storage)
2. `servo_init()` - Configure LEDC PWM on GPIO 3-6
3. `ble_service_init(callback)` - Initialize NimBLE stack, GATT services, start host task
4. `xTaskCreate(led_task, ...)` - Start LED status blink task

After initialization, `app_main()` returns. The firmware runs via FreeRTOS tasks:
- **NimBLE host task** - handles all BLE operations (advertising, connections, GATT)
- **LED task** - blinks the onboard LED based on BLE connection state

### servo module (servo.h/c)

Controls 4 servos via the LEDC (LED Control) PWM peripheral.

**Pin assignments:**

| Servo | GPIO | LEDC Channel |
|-------|------|-------------|
| 0     | 3    | 0           |
| 1     | 4    | 1           |
| 2     | 5    | 2           |
| 3     | 6    | 3           |

**PWM configuration:**
- Peripheral: LEDC low-speed mode (ESP32-C3 only supports low-speed)
- Timer: LEDC_TIMER_0, shared by all 4 channels
- Resolution: 14-bit (16384 steps per period)
- Frequency: 50Hz (20ms period)
- Pulse range: 500us (0 degrees) to 2500us (180 degrees)
- All servos initialize to 90 degrees (center)

The angle-to-duty mapping:
```
pulse_us = 500 + (angle * 2000 / 180)
duty = pulse_us * 16384 / 20000
```

### ble_service module (ble_service.h/c)

Manages the BLE GATT server using NimBLE (included in ESP-IDF).

**Initialization:**
1. `nimble_port_init()` - Initialize NimBLE controller and host
2. Configure `ble_hs_cfg` - Set callbacks, security (Just Works + bonding + SC)
3. `ble_svc_gap_device_name_set()` - Set advertised name
4. `gatt_svc_init()` - Register GAP, GATT, and custom servo services
5. `xTaskCreate(nimble_host_task, ...)` - Start NimBLE host task

The NimBLE host task runs `nimble_port_run()`, which blocks and processes all BLE events internally.

**GATT service table** is defined as a static `ble_gatt_svc_def` array:
- Service: `0000FF00-...`
  - Servo Command characteristic (`0000FF01-...`): WRITE | WRITE_NO_RSP
  - Servo State characteristic (`0000FF02-...`): READ | NOTIFY

**Access callbacks:**
- `servo_cmd_access`: Handles write operations, extracts servo_id + angle, calls the servo command callback, sends notification if subscribed
- `servo_state_access`: Handles read operations, returns 4-byte array of current angles

**GAP event handler:**
- `BLE_GAP_EVENT_CONNECT`: Tracks connection handle, updates state
- `BLE_GAP_EVENT_DISCONNECT`: Resets state, restarts advertising
- `BLE_GAP_EVENT_SUBSCRIBE`: Tracks notification subscription for servo state
- `BLE_GAP_EVENT_ADV_COMPLETE`: Restarts advertising

**Advertisement data** is set via NimBLE's `ble_hs_adv_fields` struct:
- Flags: LE General Discoverable + BR/EDR Not Supported
- Complete Local Name: "Tail controller"
- Scan response: 128-bit service UUID

**Connection behavior:**
- On disconnect: restarts advertising automatically
- Notifications: managed by NimBLE via BLE_GAP_EVENT_SUBSCRIBE (no manual CCC handling)
- Bonding keys stored in NVS flash automatically by NimBLE

## Data Flow

### Writing a servo command

1. BLE client writes `[servo_id, angle]` to characteristic `0000FF01-...`
2. NimBLE calls `servo_cmd_access()` with `BLE_GATT_ACCESS_OP_WRITE_CHR`
3. Callback extracts 2 bytes from the mbuf, invokes `servo_cmd_callback`
4. `on_servo_command` (in main.c) calls `servo_set_angle(servo_id, angle)`
5. `servo_set_angle` updates the LEDC duty cycle via `ledc_set_duty` + `ledc_update_duty`
6. If notifications are enabled, builds an mbuf with all 4 angles and calls `ble_gatts_notify_custom`

### Reading servo state

1. BLE client reads characteristic `0000FF02-...`
2. NimBLE calls `servo_state_access()` with `BLE_GATT_ACCESS_OP_READ_CHR`
3. Callback collects all 4 angles via `servo_get_angle()` and appends to response mbuf
