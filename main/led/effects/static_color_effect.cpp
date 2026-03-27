#include "led/effects/static_color_effect.h"
#include <algorithm>

void StaticColorEffect::render(RGB *buffer, const LedCoord *coords,
                                uint16_t count, float dt) {
    (void)coords;
    (void)dt;

    RGB color;
    color.r = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, red_)));
    color.g = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, green_)));
    color.b = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, blue_)));

    for (uint16_t i = 0; i < count; i++) {
        buffer[i] = color;
    }
}

void StaticColorEffect::set_param(uint8_t param_id, float value) {
    switch (param_id) {
        case 0: red_ = value; break;
        case 1: green_ = value; break;
        case 2: blue_ = value; break;
        default: break;
    }
}

float StaticColorEffect::get_param(uint8_t param_id) const {
    switch (param_id) {
        case 0: return red_;
        case 1: return green_;
        case 2: return blue_;
        default: return 0.0f;
    }
}
