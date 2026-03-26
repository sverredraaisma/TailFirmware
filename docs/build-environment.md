# Build Environment

## Required Tools

| Tool | Version Used | Purpose |
|------|-------------|---------|
| [ESP-IDF](https://github.com/espressif/esp-idf) | v5.4.1 | Espressif IoT Development Framework |
| RISC-V GCC Toolchain | esp-14.2.0 | Cross-compiler for ESP32-C3 (RISC-V) |
| [CMake](https://cmake.org/) | 3.30.2 (bundled with ESP-IDF) | Build system generator |
| [Ninja](https://ninja-build.org/) | 1.12.1 (bundled with ESP-IDF) | Fast build executor |
| Python 3 | 3.11.2 (bundled with ESP-IDF) | Required by ESP-IDF build tools |

All tools are installed via the [ESP-IDF Windows Installer](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/get-started/windows-setup.html) at `C:\Espressif`.

## Environment Setup

ESP-IDF on Windows uses its own Python environment and toolchains. These are managed by the installer and don't require manual PATH setup for normal use.

**ESP-IDF CMD Prompt:** The installer creates a start menu shortcut that opens CMD with everything configured. This is the recommended way to build.

**Git Bash / CLion:** Use the included `build.bat` wrapper which sets up all required environment variables (`IDF_PATH`, `IDF_PYTHON_ENV_PATH`, `ESP_ROM_ELF_DIR`, toolchain paths).

## Build Commands

### From ESP-IDF CMD prompt

```bash
# First time: set target chip
idf.py set-target esp32c3

# Build
idf.py build

# Flash and open serial monitor
idf.py flash monitor

# Just monitor (Ctrl+] to exit)
idf.py monitor
```

### From Git Bash (via build.bat wrapper)

```bash
# Set target
cmd.exe //c "C:\Users\Sverr\CLionProjects\TailFirmware\build.bat set-target esp32c3"

# Build
cmd.exe //c "C:\Users\Sverr\CLionProjects\TailFirmware\build.bat build"

# Flash and monitor
cmd.exe //c "C:\Users\Sverr\CLionProjects\TailFirmware\build.bat flash monitor"
```

## Output Files

| File | Description |
|------|-------------|
| `build/TailFirmware.bin` | Application binary |
| `build/TailFirmware.elf` | ELF binary for debugging |
| `build/bootloader/bootloader.bin` | Second-stage bootloader |
| `build/partition_table/partition-table.bin` | Flash partition table |

## Flashing

The ESP32-C3 has a built-in USB-Serial-JTAG interface. Connect via USB and run:
```bash
idf.py flash monitor
```

If the device isn't detected, hold the BOOT button while pressing RESET to enter download mode.

## Configuration

ESP-IDF uses `sdkconfig` for build configuration. Project defaults are in `sdkconfig.defaults`:
- BLE enabled with NimBLE stack
- Peripheral role only (central/observer/broadcaster disabled)
- LE Secure Connections and Legacy pairing enabled
- USB-Serial-JTAG console for debug output

To modify configuration interactively: `idf.py menuconfig`

## Known Build Notes

- **Git Bash / MSys2:** `idf.py` does not run directly in Git Bash (MSys is unsupported by ESP-IDF). Use `build.bat` or the ESP-IDF CMD prompt.
- **CMake version:** ESP-IDF v5.4.1's `gdbinit.cmake` is incompatible with CMake 4.x. The build.bat forces use of ESP-IDF's bundled CMake 3.30.2.
- **`ESP_ROM_ELF_DIR`:** Must be set for the build to succeed. The build.bat handles this automatically.
