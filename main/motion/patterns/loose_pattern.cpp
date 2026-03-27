#include "loose_pattern.h"
#include <math.h>

LoosePattern::LoosePattern()
    : damping_(0.3f)
    , reactivity_(3.0f)
    , velocity_x_{0, 0}
    , position_x_{0, 0}
    , velocity_y_{0, 0}
    , position_y_{0, 0}
{
}

void LoosePattern::update(const MotionInput &input, MotionOutput &output)
{
    /*
     * Physics-based loose tail: the tail swings opposite to gravity tilt.
     *
     * gravity.x -> X axis deflection (left/right tilt)
     * gravity.y -> Y axis deflection (forward/back tilt)
     *
     * The gravity vector is normalized so components are in [-1, 1].
     * Scale by reactivity to get target deflection in degrees.
     */

    float target_x = -input.gravity.x * reactivity_ * 15.0f;
    float target_y = -input.gravity.y * reactivity_ * 15.0f;

    float dt = input.dt;
    if (dt <= 0.0f || dt > 0.1f) {
        dt = 0.01f; // safety clamp
    }

    /*
     * First half of each axis: spring-damper driven directly by gravity target.
     * Second half: chain physics - targets the first half's position with its own
     * spring-damper, creating a trailing/lagging effect.
     */

    /* X axis - first half */
    {
        float accel = (target_x - position_x_[0]) * SPRING_K
                    - velocity_x_[0] * damping_ * 2.0f * sqrtf(SPRING_K);
        velocity_x_[0] += accel * dt;
        position_x_[0] += velocity_x_[0] * dt;
    }

    /* X axis - second half (chain: targets first half's position) */
    {
        float accel = (position_x_[0] - position_x_[1]) * SPRING_K
                    - velocity_x_[1] * damping_ * 2.0f * sqrtf(SPRING_K);
        velocity_x_[1] += accel * dt;
        position_x_[1] += velocity_x_[1] * dt;
    }

    /* Y axis - first half */
    {
        float accel = (target_y - position_y_[0]) * SPRING_K
                    - velocity_y_[0] * damping_ * 2.0f * sqrtf(SPRING_K);
        velocity_y_[0] += accel * dt;
        position_y_[0] += velocity_y_[0] * dt;
    }

    /* Y axis - second half (chain: targets first half's position) */
    {
        float accel = (position_y_[0] - position_y_[1]) * SPRING_K
                    - velocity_y_[1] * damping_ * 2.0f * sqrtf(SPRING_K);
        velocity_y_[1] += accel * dt;
        position_y_[1] += velocity_y_[1] * dt;
    }

    output.target_angles[0] = position_x_[0];
    output.target_angles[1] = position_x_[1];
    output.target_angles[2] = position_y_[0];
    output.target_angles[3] = position_y_[1];
}

void LoosePattern::set_param(uint8_t param_id, float value)
{
    switch (param_id) {
    case 0:
        damping_ = value;
        if (damping_ < 0.0f) damping_ = 0.0f;
        if (damping_ > 1.0f) damping_ = 1.0f;
        break;
    case 1:
        reactivity_ = value;
        if (reactivity_ < 0.0f) reactivity_ = 0.0f;
        if (reactivity_ > 10.0f) reactivity_ = 10.0f;
        break;
    default:
        break;
    }
}

float LoosePattern::get_param(uint8_t param_id) const
{
    switch (param_id) {
    case 0: return damping_;
    case 1: return reactivity_;
    default: return 0.0f;
    }
}
