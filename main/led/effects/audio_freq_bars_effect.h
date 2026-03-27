#pragma once

#include "led/led_effect.h"

class AudioFreqBarsEffect : public LedEffect {
public:
    void render(RGB *buffer, const LedCoord *coords,
                uint16_t count, float dt) override;
    void set_param(uint8_t param_id, float value) override;
    float get_param(uint8_t param_id) const override;

private:
    float num_bars_ = 8.0f;
    float red_ = 0.0f;
    float green_ = 255.0f;
    float blue_ = 0.0f;
    float fade_rate_ = 5.0f;
    float orientation_ = 0.0f; // 0=horizontal bars vertical height, 1=vertical bars horizontal height

    static constexpr int MAX_BARS = 32;
    float bar_levels_[MAX_BARS] = {};
};
