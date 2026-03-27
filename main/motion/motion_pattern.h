#pragma once

#include "config/config_types.h"

struct MotionInput {
    float encoder_angles[MAX_SERVOS]; // current positions (degrees)
    vec3_t gravity;                    // gravity vector from base IMU
    bool tap_base;
    bool tap_tip;
    float loudness;                    // from FFT stream, 0-1
    float dt;                          // seconds since last update
};

struct MotionOutput {
    float target_angles[MAX_SERVOS]; // desired positions (degrees)
};

class MotionPattern {
public:
    virtual ~MotionPattern() = default;

    /// Compute new target positions based on inputs.
    virtual void update(const MotionInput &input, MotionOutput &output) = 0;

    /// Set a pattern-specific parameter by ID.
    virtual void set_param(uint8_t param_id, float value) = 0;

    /// Get a pattern-specific parameter by ID.
    virtual float get_param(uint8_t param_id) const = 0;
};
