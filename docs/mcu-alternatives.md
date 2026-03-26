# MCU Alternatives

Comparison of microcontrollers suitable for this project (4+ PWM channels, I2C, BLE, addressable LED support).

## Requirements

- BLE for wireless servo control
- At least 4 PWM channels for servos
- I2C bus
- Ability to drive WS2812B/WS2813B addressable LEDs
- Affordable

## Comparison

| MCU | Dev Board | Price | BLE | PWM | I2C | LED Driver | Logic | Notes |
|-----|-----------|-------|-----|-----|-----|-----------|-------|-------|
| **ESP32-C3** | XIAO ESP32-C3, ESP32-C3-DevKitM | $3-5 | 5.0 | 6ch (LEDC) | 1x | RMT peripheral | 3.3V | Best value. RISC-V single core, RMT is purpose-built for WS2812B timing. WiFi bonus. |
| **ESP32-S3** | XIAO ESP32-S3, ESP32-S3-DevKitC | $5-8 | 5.0 | 8ch (LEDC) | 2x | RMT peripheral | 3.3V | Dual-core, more GPIO, USB-OTG. Overkill but still cheap. |
| **ESP32** (original) | ESP32-DevKitC, NodeMCU-32S | $3-5 | 4.2 | 16ch (LEDC) | 2x | RMT peripheral | 3.3V | Most widely available, huge community. BLE 4.2 is fine for this use case. |
| **nRF52832** | E73 module, Adafruit Feather | $3-10 | 5.0 | 4ch (3 instances) | 2x | SPI/I2S bit-bang | 3.3V | Best BLE power efficiency. Purpose-built for BLE. Smaller community than ESP32. |
| **nRF52840** | XIAO nRF52840, Adafruit Feather | $8-13 | 5.0 | 4ch (4 instances) | 2x | SPI/I2S bit-bang | 3.3V | More flash/RAM, USB, better crypto. Good if you want Zephyr RTOS. |
| **RP2040** (Pico W) | Raspberry Pi Pico W | $6 | 4.2 | 16ch (8 slices) | 2x | PIO | 3.3V | Same SDK as current firmware. PIO is excellent for LED protocols. Minimal code changes needed. |
| **STM32WB55** | Nucleo-WB55RG | $10-15 | 5.2 | 16+ (advanced timers) | 2x | DMA+timer/SPI | 3.3V | Dual-core (M4+M0 for BLE). Most capable, but steeper learning curve. |

## Recommendation

The **ESP32-C3** was selected and the firmware has been ported to it. It offers the best value: cheapest, dedicated RMT hardware for addressable LEDs, BLE 5.0, plenty of PWM, and the largest ecosystem (ESP-IDF + Arduino).

## 3.3V Logic and WS2812B/WS2813B

Every modern BLE-capable MCU uses 3.3V logic. WS2813B LEDs powered at 5V need a HIGH signal of at least 3.5V (0.7 * VDD), so 3.3V is technically out of spec.

**Solutions:**
- **Level shifter** (74HCT125, ~$0.50) - cleanest approach, one chip between MCU and first LED
- **First LED at 3.7V** - power the first LED at a lower voltage so its data output re-levels the signal for the chain
- **In practice** many WS2812B work fine with 3.3V data at 5V power, but it's not guaranteed and may be unreliable at longer wire runs or higher temperatures
