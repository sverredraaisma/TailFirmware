# External Dependencies

All external code comes bundled within the Pico SDK. No additional package managers or downloads are needed beyond the SDK itself.

## Pico SDK Libraries Used

### pico_stdlib
Standard library for Pico development. Provides GPIO, timing, and stdio functionality.

### hardware_pwm
RP2350 hardware PWM driver. Used to generate 50Hz servo control signals. Provides functions for configuring PWM slices, clock dividers, wrap values, and duty cycles.

### hardware_clocks
Clock management for RP2350. Used to query `clk_sys` frequency at runtime for dynamic PWM divider calculation (avoids hardcoding the 150MHz default).

### hardware_gpio
GPIO configuration. Used for `gpio_set_function()` to assign pins to PWM function.

### pico_btstack_ble
BTstack BLE library integration for the Pico SDK. Provides the full Bluetooth Low Energy stack including:
- L2CAP (Logical Link Control and Adaptation Protocol)
- ATT (Attribute Protocol) server
- SM (Security Manager)
- GATT server framework
- BLE advertisement management

[BTstack](https://github.com/bluekitchen/btstack) is a dual-mode Bluetooth stack by BlueKitchen GmbH, bundled in the Pico SDK at `lib/btstack/`.

### pico_btstack_cyw43
Bridges BTstack to the CYW43439 wireless chip's Bluetooth interface. Provides the HCI transport layer that sends/receives Bluetooth packets over the shared SPI bus to the CYW43439.

Requires these btstack_config.h defines:
- `HCI_OUTGOING_PRE_BUFFER_SIZE >= 4` (CYW43 packet header size)
- `HCI_ACL_CHUNK_SIZE_ALIGNMENT = 4` (alignment requirement)

### pico_cyw43_arch_none
CYW43 architecture layer configured for **BLE-only** (no WiFi/lwIP). This is critical - the other arch variants (`pico_cyw43_arch_threadsafe_background`, `pico_cyw43_arch_lwip_*`) set `CYW43_LWIP=1` which requires the lwIP TCP/IP stack and an `lwipopts.h` header.

`pico_cyw43_arch_none` sets `CYW43_LWIP=0` and still uses the threadsafe_background async context internally, so BLE callbacks work without polling.

**Why not `pico_cyw43_arch_threadsafe_background` directly?**
That library does not explicitly set `CYW43_LWIP`, and the CYW43 driver defaults it to 1 in `cyw43_config.h:137`. This causes `cyw43.h` to `#include "lwip/netif.h"`, which fails without lwIP linked. The `_none` variant is the correct choice for BLE-only projects.

### CYW43439 Driver
Infineon CYW43439 WiFi/Bluetooth combo chip driver, bundled at `lib/cyw43-driver/`. Manages the SPI communication with the wireless chip, firmware loading, and provides the low-level interface for both WiFi and Bluetooth.

## Build-Time Tools

### compile_gatt.py
BTstack's GATT database compiler (`lib/btstack/tool/compile_gatt.py`). Invoked automatically by `pico_btstack_make_gatt_header()` in CMake. Reads `.gatt` files and generates C headers containing:
- `profile_data[]` array (binary GATT database)
- `ATT_CHARACTERISTIC_*_VALUE_HANDLE` defines for each characteristic
- `ATT_CHARACTERISTIC_*_CLIENT_CONFIGURATION_HANDLE` defines for CCC descriptors

### pioasm
PIO (Programmable I/O) assembler. Compiles `.pio` programs used by the CYW43 SPI driver. Built from source as a host tool during the first build.

### picotool
Raspberry Pi Pico utility tool. Used during the build to embed binary info and generate UF2 files. Built from source if not installed system-wide.

## btstack_config.h Reference

BTstack requires a project-level `btstack_config.h` header. Missing defines cause hard `#error` failures. Here are the required defines and why:

| Define | Value | Required By |
|--------|-------|-------------|
| `ENABLE_LE_PERIPHERAL` | (flag) | BTstack BLE peripheral role |
| `HCI_ACL_PAYLOAD_SIZE` | 259 | BTstack HCI layer |
| `MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES` | 0 | BTstack (0 = BLE only, no classic) |
| `MAX_NR_GATT_CLIENTS` | 1 | BTstack GATT |
| `MAX_NR_HCI_CONNECTIONS` | 1 | BTstack HCI |
| `MAX_NR_L2CAP_SERVICES` | 3 | BTstack L2CAP |
| `MAX_NR_L2CAP_CHANNELS` | 3 | BTstack L2CAP |
| `MAX_NR_SM_LOOKUP_ENTRIES` | 3 | BTstack Security Manager |
| `MAX_NR_WHITELIST_ENTRIES` | 1 | BTstack whitelist |
| `NVN_NUM_GATT_SERVER_CCC` | 1 | BTstack GATT CCC descriptors |
| `NVM_NUM_DEVICE_DB_ENTRIES` | 16 | `le_device_db_tlv.c` - paired device storage |
| `NVM_NUM_LINK_KEYS` | 16 | BTstack link key storage |
| `MAX_ATT_DB_SIZE` | 512 | `att_db_util.c` - static ATT DB allocation |
| `HCI_OUTGOING_PRE_BUFFER_SIZE` | 4 | `btstack_hci_transport_cyw43.c` - CYW43 packet header |
| `HCI_ACL_CHUNK_SIZE_ALIGNMENT` | 4 | `btstack_hci_transport_cyw43.c` - CYW43 alignment |
