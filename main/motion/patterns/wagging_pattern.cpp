#include "wagging_pattern.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

WaggingPattern::WaggingPattern()
    : frequency_(1.0f)
    , x_amplitude_(45.0f)
    , y1_position_(0.0f)
    , y2_position_(0.0f)
    , phase_(0.0f)
{
}

void WaggingPattern::update(const MotionInput &input, MotionOutput &output)
{
    phase_ += input.dt * frequency_ * 2.0f * M_PI;

    /* Keep phase from growing without bound */
    if (phase_ > 2.0f * M_PI * 1000.0f) {
        phase_ = fmodf(phase_, 2.0f * M_PI);
    }

    /* X axis: sinusoidal wag with second half lagging by PI/4 for natural wave */
    output.target_angles[0] = x_amplitude_ * sinf(phase_);
    output.target_angles[1] = x_amplitude_ * sinf(phase_ - M_PI / 4.0f);

    /* Y axis: hold static positions */
    output.target_angles[2] = y1_position_;
    output.target_angles[3] = y2_position_;
}

void WaggingPattern::set_param(uint8_t param_id, float value)
{
    switch (param_id) {
    case 0: frequency_ = value;    break;
    case 1: x_amplitude_ = value;  break;
    case 2: y1_position_ = value;  break;
    case 3: y2_position_ = value;  break;
    default: break;
    }
}

float WaggingPattern::get_param(uint8_t param_id) const
{
    switch (param_id) {
    case 0: return frequency_;
    case 1: return x_amplitude_;
    case 2: return y1_position_;
    case 3: return y2_position_;
    default: return 0.0f;
    }
}
