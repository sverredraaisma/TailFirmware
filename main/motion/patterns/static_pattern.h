#pragma once

#include "motion/motion_pattern.h"

class StaticPattern : public MotionPattern {
public:
    StaticPattern();

    void update(const MotionInput &input, MotionOutput &output) override;
    void set_param(uint8_t param_id, float value) override;
    float get_param(uint8_t param_id) const override;

private:
    float positions_[MAX_SERVOS]; // target degrees for each half-axis
};
