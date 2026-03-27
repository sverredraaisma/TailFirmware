#include "led/effects/rainbow_effect.h"
#include <cmath>

void RainbowEffect::render(RGB *buffer, const LedCoord *coords,
                            uint16_t count, float dt) {
    time_offset_ += speed_ * dt / 360.0f; // accumulate in cycles
    // Keep time_offset_ from growing unbounded
    if (time_offset_ > 1000.0f) time_offset_ -= 1000.0f;

    int dir = static_cast<int>(direction_ + 0.5f);

    for (uint16_t i = 0; i < count; i++) {
        LedCoord c = transform_coord(coords[i]);

        float axis;
        switch (dir) {
            case 1:  axis = c.y; break;
            case 2:  axis = (c.x + c.y) * 0.5f; break;
            default: axis = c.x; break;
        }

        float hue = (axis * scale_ + time_offset_) * 360.0f;
        hue = fmodf(hue, 360.0f);
        if (hue < 0.0f) hue += 360.0f;

        HSV hsv;
        hsv.h = static_cast<uint16_t>(hue) % 360;
        hsv.s = 255;
        hsv.v = 255;
        buffer[i] = hsv_to_rgb(hsv);
    }
}

void RainbowEffect::set_param(uint8_t param_id, float value) {
    switch (param_id) {
        case 0: direction_ = value; break;
        case 1: speed_ = value; break;
        case 2: scale_ = value; break;
        default: break;
    }
}

float RainbowEffect::get_param(uint8_t param_id) const {
    switch (param_id) {
        case 0: return direction_;
        case 1: return speed_;
        case 2: return scale_;
        default: return 0.0f;
    }
}
