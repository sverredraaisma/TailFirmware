#pragma once

#include "led/led_effect.h"

class AudioBarEffect : public LedEffect {
public:
    void render(RGB *buffer, const LedCoord *coords,
                uint16_t count, float dt) override;
    void set_param(uint8_t param_id, float value) override;
    float get_param(uint8_t param_id) const override;

private:
    float red_ = 0.0f;
    float green_ = 0.0f;
    float blue_ = 255.0f;
    float direction_ = 0.0f; // 0=left-to-right, 1=bottom-to-top
    float fade_rate_ = 3.0f;
    float current_level_ = 0.0f;
};
