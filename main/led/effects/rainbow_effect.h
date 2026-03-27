#pragma once

#include "led/led_effect.h"

class RainbowEffect : public LedEffect {
public:
    void render(RGB *buffer, const LedCoord *coords,
                uint16_t count, float dt) override;
    void set_param(uint8_t param_id, float value) override;
    float get_param(uint8_t param_id) const override;

private:
    float direction_ = 0.0f; // 0=horizontal, 1=vertical, 2=diagonal
    float speed_ = 60.0f;    // hue shift per second
    float scale_ = 1.0f;     // full hue cycles across coordinate range
    float time_offset_ = 0.0f;
};
