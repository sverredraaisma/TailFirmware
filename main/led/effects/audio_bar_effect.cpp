#include "led/effects/audio_bar_effect.h"
#include "config/fft_buffer.h"
#include <algorithm>

void AudioBarEffect::render(RGB *buffer, const LedCoord *coords,
                             uint16_t count, float dt) {
    FftBuffer &fft = FftBuffer::instance();

    if (fft.is_fresh()) {
        float loudness = fft.get_loudness_normalized();
        current_level_ = std::max(current_level_, loudness);
    } else {
        current_level_ -= fade_rate_ * dt;
        if (current_level_ < 0.0f) current_level_ = 0.0f;
    }

    int dir = static_cast<int>(direction_ + 0.5f);

    RGB color;
    color.r = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, red_)));
    color.g = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, green_)));
    color.b = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, blue_)));

    for (uint16_t i = 0; i < count; i++) {
        LedCoord c = transform_coord(coords[i]);

        float axis = (dir == 1) ? c.y : c.x;

        if (axis <= current_level_) {
            buffer[i] = color;
        } else {
            buffer[i] = RGB::black();
        }
    }
}

void AudioBarEffect::set_param(uint8_t param_id, float value) {
    switch (param_id) {
        case 0: red_ = value; break;
        case 1: green_ = value; break;
        case 2: blue_ = value; break;
        case 3: direction_ = value; break;
        case 4: fade_rate_ = value; break;
        default: break;
    }
}

float AudioBarEffect::get_param(uint8_t param_id) const {
    switch (param_id) {
        case 0: return red_;
        case 1: return green_;
        case 2: return blue_;
        case 3: return direction_;
        case 4: return fade_rate_;
        default: return 0.0f;
    }
}
