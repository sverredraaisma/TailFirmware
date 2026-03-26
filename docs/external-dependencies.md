# External Dependencies

All external code comes bundled within ESP-IDF. No additional package managers or downloads are needed beyond the ESP-IDF installation.

## ESP-IDF Components Used

### NimBLE (bt component)
Apache NimBLE BLE stack, included in ESP-IDF's `bt` component. Provides the full BLE peripheral stack:
- GAP (Generic Access Profile) for advertising and connections
- GATT server for service/characteristic definitions
- Security Manager for pairing, bonding, and secure connections
- NVS-backed persistent storage for bonding keys (automatic)

NimBLE is configured via `sdkconfig` (menuconfig) rather than a manual config header. Key settings in `sdkconfig.defaults`:
- `CONFIG_BT_NIMBLE_ENABLED=y` - Use NimBLE (not Bluedroid)
- `CONFIG_BT_NIMBLE_ROLE_CENTRAL=n` - Disable unused roles to save flash
- `CONFIG_BT_NIMBLE_SM_SC=y` - Enable LE Secure Connections

### LEDC (esp_driver_ledc)
LED Control PWM peripheral driver. Used to generate 50Hz servo control signals. ESP32-C3 supports only low-speed mode with 6 channels (we use 4). The LEDC handles clock dividers and duty cycle internally given a target frequency and resolution.

### GPIO (esp_driver_gpio)
GPIO driver for the onboard LED control (GPIO 8). Used for status indication blink patterns.

### NVS Flash (nvs_flash)
Non-Volatile Storage library. Stores BLE bonding keys, GATT CCC states, and other persistent data in a dedicated flash partition. Must be initialized before NimBLE starts.

### FreeRTOS (freertos)
Real-time operating system kernel, built into ESP-IDF. The firmware uses two FreeRTOS tasks:
- **NimBLE host task** (priority 5, 4KB stack) - runs `nimble_port_run()` for all BLE processing
- **LED task** (priority 1, 2KB stack) - blinks the status LED based on BLE state

### ESP Logging (log)
ESP-IDF logging framework. Used via `ESP_LOGI`, `ESP_LOGE`, `ESP_LOGW` macros. Output goes to USB-Serial-JTAG console.

## Key Differences from Pico 2W Version

| Aspect | Pico 2W (BTstack) | ESP32-C3 (NimBLE) |
|--------|-------------------|-------------------|
| GATT definition | `.gatt` file compiled by `compile_gatt.py` | C struct table (`ble_gatt_svc_def`) |
| Read/write callbacks | Separate `att_read_callback` and `att_write_callback` | Single `access_cb` per characteristic with `ctxt->op` switch |
| CCC handling | Manual (handle read/write in callbacks) | Automatic (NimBLE manages CCC internally) |
| Notifications | `att_server_request_can_send_now_event` + `ATT_EVENT_CAN_SEND_NOW` | `ble_gatts_notify_custom(conn, handle, mbuf)` directly |
| Bond storage | Manual TLV flash bank init | Automatic via NVS |
| Advertising | Manual byte array construction | `ble_hs_adv_fields` struct with named fields |
| Stack config | `btstack_config.h` with ~15 defines | `sdkconfig` via menuconfig |
| PWM peripheral | Hardware PWM slices with manual clock divider | LEDC with automatic clock management |
| Task model | Bare-metal main loop + interrupt-driven BLE | FreeRTOS tasks |
