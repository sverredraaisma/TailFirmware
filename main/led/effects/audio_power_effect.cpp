#include "led/effects/audio_power_effect.h"
#include "config/fft_buffer.h"
#include <algorithm>

void AudioPowerEffect::render(RGB *buffer, const LedCoord *coords,
                               uint16_t count, float dt) {
    (void)coords;

    FftBuffer &fft = FftBuffer::instance();

    if (fft.is_fresh()) {
        float loudness = fft.get_loudness_normalized();
        current_brightness_ = std::max(current_brightness_, loudness);
    } else {
        current_brightness_ -= fade_rate_ * dt;
        if (current_brightness_ < 0.0f) current_brightness_ = 0.0f;
    }

    RGB color;
    color.r = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, red_)) * current_brightness_);
    color.g = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, green_)) * current_brightness_);
    color.b = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, blue_)) * current_brightness_);

    for (uint16_t i = 0; i < count; i++) {
        buffer[i] = color;
    }
}

void AudioPowerEffect::set_param(uint8_t param_id, float value) {
    switch (param_id) {
        case 0: red_ = value; break;
        case 1: green_ = value; break;
        case 2: blue_ = value; break;
        case 3: fade_rate_ = value; break;
        default: break;
    }
}

float AudioPowerEffect::get_param(uint8_t param_id) const {
    switch (param_id) {
        case 0: return red_;
        case 1: return green_;
        case 2: return blue_;
        case 3: return fade_rate_;
        default: return 0.0f;
    }
}
