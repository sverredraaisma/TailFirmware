#include "static_pattern.h"

StaticPattern::StaticPattern()
{
    for (int i = 0; i < MAX_SERVOS; i++) {
        positions_[i] = 0.0f;
    }
}

void StaticPattern::update(const MotionInput &input, MotionOutput &output)
{
    (void)input;
    for (int i = 0; i < MAX_SERVOS; i++) {
        output.target_angles[i] = positions_[i];
    }
}

void StaticPattern::set_param(uint8_t param_id, float value)
{
    if (param_id < MAX_SERVOS) {
        positions_[param_id] = value;
    }
}

float StaticPattern::get_param(uint8_t param_id) const
{
    if (param_id < MAX_SERVOS) {
        return positions_[param_id];
    }
    return 0.0f;
}
