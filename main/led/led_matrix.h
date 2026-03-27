#pragma once

#include <vector>
#include <cstdint>
#include "led_effect.h"
#include "color.h"
#include "drivers/led_strip_driver.h"

class LedMatrix {
public:
    LedMatrix();

    /// Configure the ring layout. Rebuilds coordinate map.
    void configure(uint8_t num_rings, const uint8_t *leds_per_ring);

    uint16_t get_led_count() const { return total_leds_; }
    const LedCoord &get_coord(uint16_t index) const { return coords_[index]; }
    const LedCoord *get_coords() const { return coords_.data(); }

    /// Set a pixel in the output buffer.
    void set_pixel(uint16_t index, RGB color);

    /// Push the output buffer to the physical LED strip.
    void push();

    /// Initialize the underlying LED strip driver.
    void init_strip(int gpio_num);

private:
    std::vector<LedCoord> coords_;
    std::vector<RGB> buffer_;
    uint16_t total_leds_ = 0;
    tail_led_strip_t strip_ = nullptr;
};
