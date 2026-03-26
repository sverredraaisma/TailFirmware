# Build Environment

## Required Tools

| Tool | Version Used | Purpose |
|------|-------------|---------|
| [Pico SDK](https://github.com/raspberrypi/pico-sdk) | 2.2.0 | Hardware abstraction, build system, libraries |
| [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) | 15.2.1 (arm-none-eabi) | Cross-compiler for RP2350 (Cortex-M33) |
| [CMake](https://cmake.org/) | 4.3.0 | Build system generator |
| [Ninja](https://ninja-build.org/) | 1.13.2 | Fast build executor |
| [Python 3](https://www.python.org/) | 3.14.3 | Required by SDK tools (GATT compiler, etc.) |
| Host C++ compiler (MinGW-w64 GCC) | 15.2.0 | Builds SDK host tools (pioasm, picotool) |

## Environment Variables

- `PICO_SDK_PATH` - Must point to the Pico SDK root directory (e.g., `C:\Users\Sverr\CLionProjects\raspberryPiPicoSDK\pico-sdk`)

## Build Commands

```bash
# Configure (first time or after CMakeLists.txt changes)
mkdir build && cd build
cmake -G Ninja ..

# Build
ninja

# Clean rebuild
# Delete the build/ directory and re-run configure + build
```

## Output Files

| File | Description |
|------|-------------|
| `build/TailFirmware.uf2` | UF2 firmware image for drag-and-drop flashing |
| `build/TailFirmware.elf` | ELF binary for debugger use |
| `build/TailFirmware.bin` | Raw binary image |
| `build/TailFirmware.hex` | Intel HEX format |

## Flashing

1. Hold BOOTSEL button on the Pico 2W
2. Connect USB cable (or press reset while holding BOOTSEL)
3. A USB mass storage drive appears
4. Copy `TailFirmware.uf2` to the drive
5. The board reboots automatically

## SDK Host Tools

The Pico SDK builds two host tools from source during the first build:

- **pioasm** - PIO assembler, compiles `.pio` files to C headers (used by CYW43 SPI driver)
- **picotool** - Pico tool for binary inspection and flashing

These require a **native** (non-ARM) C++ compiler. On Windows, this is satisfied by MinGW-w64 GCC (`g++`) or MSVC (`cl.exe`). The ARM cross-compiler (`arm-none-eabi-gcc`) cannot build these.

## Known Build Notes

- The GATT compiler (`compile_gatt.py`) warns about PyCryptodome not being installed. This is optional - it's only used to calculate the GATT Database Hash. Without it, a random hash is used, which is fine for development.
- picotool is built from source if not installed system-wide. The SDK warns about this but it works fine.
