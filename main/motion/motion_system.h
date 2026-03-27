#pragma once

#include "axis_controller.h"
#include "motion_pattern.h"
#include "drivers/imu_driver.h"
#include <memory>

class MotionSystem {
public:
    void init(i2c_mux_t *mux, const system_config_t &config);
    void set_pattern(std::unique_ptr<MotionPattern> pattern);
    void set_pattern_param(uint8_t param_id, float value);
    void calibrate_zero();
    void set_axis_limits(uint8_t axis, float min_deg, float max_deg);
    void set_pid_gains(uint8_t servo_id, float kp, float ki, float kd);

    /// Run one control cycle at 100Hz.
    void update(float dt);

    /// Get current position for an axis half.
    float get_position(uint8_t axis, uint8_t half) const;

    /// Get last IMU gravity vector.
    vec3_t get_gravity() const { return last_gravity_; }

    /// Check and clear tap events.
    bool check_tap_base();
    bool check_tap_tip();

private:
    i2c_mux_t *mux_ = nullptr;
    AxisController axes_[2]; // 0=X, 1=Y
    imu_t imus_[2];          // 0=base, 1=tip
    std::unique_ptr<MotionPattern> pattern_;

    vec3_t last_gravity_ = {};
    bool tap_base_ = false;
    bool tap_tip_ = false;
    float loudness_ = 0; // from FFT stream
};
