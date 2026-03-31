# Build Environment

## Required Tools

| Tool | Version | Purpose |
|------|---------|---------|
| [ESP-IDF](https://github.com/espressif/esp-idf) | v5.4.1 | Espressif IoT Development Framework |
| RISC-V GCC Toolchain | esp-14.2.0 | Cross-compiler for ESP32-C3 (RISC-V) |
| CMake | 3.30.2 (bundled) | Build system generator |
| Ninja | 1.12.1 (bundled) | Fast build executor |
| Python 3 | 3.11.2 (bundled) | ESP-IDF build tools |

All tools installed via the [ESP-IDF Windows Installer](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/get-started/windows-setup.html) at `C:\Espressif`.

## Build Commands

### From ESP-IDF CMD prompt (recommended)

```bash
idf.py set-target esp32c3    # First time only
idf.py build                  # Build firmware
idf.py flash monitor          # Flash and open serial monitor
idf.py menuconfig             # Interactive config editor
```

### From Git Bash (via build.bat wrapper)

```bash
cmd.exe //c "C:\Users\Sverr\CLionProjects\TailFirmware\build.bat set-target esp32c3"
cmd.exe //c "C:\Users\Sverr\CLionProjects\TailFirmware\build.bat build"
cmd.exe //c "C:\Users\Sverr\CLionProjects\TailFirmware\build.bat flash monitor"
```

The `build.bat` wrapper sets `IDF_PATH`, `IDF_PYTHON_ENV_PATH`, `ESP_ROM_ELF_DIR`, and toolchain paths. It's needed because `idf.py` doesn't support Git Bash/MSys directly.

## Output Files

| File | Size | Description |
|------|------|-------------|
| `build/TailFirmware.bin` | ~612 KB | Application binary |
| `build/TailFirmware.elf` | | ELF for debugging |
| `build/bootloader/bootloader.bin` | | Second-stage bootloader |
| `build/partition_table/partition-table.bin` | | Flash partition table |

## Flashing

ESP32-C3 has built-in USB-Serial-JTAG. Connect USB and run `idf.py flash monitor`.

If the device isn't detected, hold BOOT while pressing RESET to enter download mode.

## Configuration

Project defaults are in `sdkconfig.defaults`:
- BLE with NimBLE stack (peripheral role only)
- LE Secure Connections + Legacy pairing
- USB-Serial-JTAG console

The managed component `espressif/led_strip ^2.5` is declared in `main/idf_component.yml` and downloaded automatically on first build.

## Hardware Pin Assignments (compile-time)

| Function | GPIO | Notes |
|----------|------|-------|
| I2C SDA | 7 | 400 kHz, to TCA9548A mux |
| I2C SCL | 8 | |
| Servo 0 | 3 | LEDC channel 0 |
| Servo 1 | 9 | LEDC channel 1 |
| Servo 2 | 5 | LEDC channel 2 |
| Servo 3 | 6 | LEDC channel 3 |
| LED strip | 4 | WS2812B via RMT |
| Status LED | 10 | Onboard LED |

Pin assignments are defined in `main/config/pin_config.h` and can be changed at compile time.

## Known Build Notes

- **Git Bash/MSys2:** `idf.py` rejects MSys. Use `build.bat` wrapper.
- **CMake 4.x:** ESP-IDF v5.4.1's `gdbinit.cmake` is incompatible. `build.bat` forces CMake 3.30.2.
- **`ESP_ROM_ELF_DIR`:** Must be set. `build.bat` handles this.
- **RTTI:** Disabled by default in ESP-IDF. Use `static_cast` instead of `dynamic_cast`. Check config types to determine actual class.
- **led_strip component:** Fetched from ESP Component Registry on first build (requires internet).
