#pragma once

#include "pid_controller.h"
#include "drivers/encoder_driver.h"
#include "drivers/i2c_mux.h"
#include "config/config_types.h"

extern "C" {
#include "servo.h"
}

class AxisController {
public:
    /// Configure this axis with two servo-encoder pairs.
    void configure(uint8_t servo_first, uint8_t servo_second,
                   const servo_config_t &cfg_first, const servo_config_t &cfg_second);

    /// Initialize encoders on the I2C bus.
    void init_encoders(i2c_mux_t *mux);

    /// Set the zero offset to current encoder positions.
    void calibrate_zero(i2c_mux_t *mux);

    /// Set rotation limits (degrees from zero).
    void set_limits(float min_deg, float max_deg);

    /// Set PID gains for one half (0 or 1).
    void set_pid_gains(uint8_t half, float kp, float ki, float kd);

    /// Set target positions in degrees from zero. Clamped to limits.
    void set_target(float first_half_deg, float second_half_deg);

    /// Run one control cycle: read encoders, run PID, set servo speeds.
    /// Call at 100Hz. Automatically disables after repeated I2C failures.
    void update(i2c_mux_t *mux, float dt);

    /// Get current position (degrees from zero).
    float get_position(uint8_t half) const;

    /// True if this axis has been disabled due to repeated failures.
    bool is_disabled() const { return disabled_; }

private:
    static constexpr int MAX_CONSECUTIVE_FAILURES = 10;

    struct HalfAxis {
        uint8_t servo_id = 0;
        encoder_t encoder = {};
        PidController pid;
        bool invert = false;
        float zero_offset = 0;
        float target = 0;
        float current = 0;
        int fail_count = 0;
        bool failed = false;  // true = this half's encoder is unreachable
    };

    HalfAxis halves_[2];
    float limit_min_ = -180;
    float limit_max_ = 180;
    bool disabled_ = false;  // true = entire axis disabled (both halves failed)

    float clamp(float val, float lo, float hi) const;
};
