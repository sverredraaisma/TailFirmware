#pragma once

#include "motion/motion_pattern.h"

class WaggingPattern : public MotionPattern {
public:
    WaggingPattern();

    void update(const MotionInput &input, MotionOutput &output) override;
    void set_param(uint8_t param_id, float value) override;
    float get_param(uint8_t param_id) const override;

private:
    float frequency_;     // Hz
    float x_amplitude_;   // degrees
    float y1_position_;   // degrees
    float y2_position_;   // degrees
    float phase_;         // radians, accumulated
};
