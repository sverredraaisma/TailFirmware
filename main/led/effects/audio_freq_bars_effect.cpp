#include "led/effects/audio_freq_bars_effect.h"
#include "config/fft_buffer.h"
#include <algorithm>
#include <cmath>

void AudioFreqBarsEffect::render(RGB *buffer, const LedCoord *coords,
                                  uint16_t count, float dt) {
    FftBuffer &fft = FftBuffer::instance();

    int bars = static_cast<int>(std::max(1.0f, std::min(static_cast<float>(MAX_BARS), num_bars_)) + 0.5f);

    // Update bar levels from FFT data
    if (fft.is_fresh()) {
        uint8_t num_bins = fft.get_num_bins();
        for (int b = 0; b < bars; b++) {
            // Map this bar to a range of FFT bins
            int bin_start = (b * num_bins) / bars;
            int bin_end = ((b + 1) * num_bins) / bars;
            if (bin_end <= bin_start) bin_end = bin_start + 1;

            // Find max bin value in this bar's range
            uint8_t max_val = 0;
            for (int k = bin_start; k < bin_end && k < num_bins; k++) {
                uint8_t v = fft.get_bin(static_cast<uint8_t>(k));
                if (v > max_val) max_val = v;
            }

            float fft_level = static_cast<float>(max_val) / 255.0f;
            bar_levels_[b] = std::max(bar_levels_[b], fft_level);
        }
    }

    // Decay all bars
    for (int b = 0; b < bars; b++) {
        bar_levels_[b] -= fade_rate_ * dt;
        if (bar_levels_[b] < 0.0f) bar_levels_[b] = 0.0f;
    }

    int orient = static_cast<int>(orientation_ + 0.5f);

    RGB color;
    color.r = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, red_)));
    color.g = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, green_)));
    color.b = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, blue_)));

    for (uint16_t i = 0; i < count; i++) {
        LedCoord c = transform_coord(coords[i]);

        // Determine which axis selects the bar and which determines height
        float bar_axis, height_axis;
        if (orient == 0) {
            // Horizontal bars (x selects bar, y is height)
            bar_axis = c.x;
            height_axis = c.y;
        } else {
            // Vertical bars (y selects bar, x is height)
            bar_axis = c.y;
            height_axis = c.x;
        }

        // Determine which bar this LED falls into
        int bar_idx = static_cast<int>(bar_axis * bars);
        if (bar_idx >= bars) bar_idx = bars - 1;
        if (bar_idx < 0) bar_idx = 0;

        float level = bar_levels_[bar_idx];

        if (height_axis <= level) {
            buffer[i] = color;
        } else {
            buffer[i] = RGB::black();
        }
    }
}

void AudioFreqBarsEffect::set_param(uint8_t param_id, float value) {
    switch (param_id) {
        case 0: num_bars_ = value; break;
        case 1: red_ = value; break;
        case 2: green_ = value; break;
        case 3: blue_ = value; break;
        case 4: fade_rate_ = value; break;
        case 5: orientation_ = value; break;
        default: break;
    }
}

float AudioFreqBarsEffect::get_param(uint8_t param_id) const {
    switch (param_id) {
        case 0: return num_bars_;
        case 1: return red_;
        case 2: return green_;
        case 3: return blue_;
        case 4: return fade_rate_;
        case 5: return orientation_;
        default: return 0.0f;
    }
}
