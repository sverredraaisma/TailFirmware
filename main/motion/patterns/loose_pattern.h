#pragma once

#include "motion/motion_pattern.h"

class LoosePattern : public MotionPattern {
public:
    LoosePattern();

    void update(const MotionInput &input, MotionOutput &output) override;
    void set_param(uint8_t param_id, float value) override;
    float get_param(uint8_t param_id) const override;

private:
    float damping_;      // 0-1
    float reactivity_;   // 0-10

    /* Spring-damper state for each axis, two halves each */
    float velocity_x_[2];
    float position_x_[2];
    float velocity_y_[2];
    float position_y_[2];

    static constexpr float SPRING_K = 20.0f; // spring stiffness
};
