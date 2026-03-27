#include "motion_system.h"

void MotionSystem::init(i2c_mux_t *mux, const system_config_t &config)
{
    mux_ = mux;

    /*
     * Find servo indices for each axis half by scanning the servo configs.
     * Convention: servos[i].assignment.axis / .half identify which axis/half
     * the servo belongs to.
     */
    uint8_t servo_idx[MAX_AXES][2] = {};  // [axis][half] -> servo index
    const servo_config_t *servo_cfg[MAX_AXES][2] = {};

    for (int i = 0; i < MAX_SERVOS; i++) {
        uint8_t axis = config.servos[i].assignment.axis;
        uint8_t half = config.servos[i].assignment.half;
        if (axis < MAX_AXES && half < 2) {
            servo_idx[axis][half] = static_cast<uint8_t>(i);
            servo_cfg[axis][half] = &config.servos[i];
        }
    }

    /* Configure both axes */
    for (int a = 0; a < MAX_AXES; a++) {
        axes_[a].configure(servo_idx[a][0], servo_idx[a][1],
                           *servo_cfg[a][0], *servo_cfg[a][1]);
        axes_[a].init_encoders(mux);
        axes_[a].set_limits(config.axes[a].limit_min, config.axes[a].limit_max);
    }

    /* Initialize IMUs */
    for (int i = 0; i < MAX_IMU; i++) {
        imu_init(&imus_[i], mux, config.imus[i].mux_channel);
    }
}

void MotionSystem::set_pattern(std::unique_ptr<MotionPattern> pattern)
{
    pattern_ = std::move(pattern);
}

void MotionSystem::set_pattern_param(uint8_t param_id, float value)
{
    if (pattern_) {
        pattern_->set_param(param_id, value);
    }
}

void MotionSystem::calibrate_zero()
{
    for (int a = 0; a < MAX_AXES; a++) {
        axes_[a].calibrate_zero(mux_);
    }
}

void MotionSystem::set_axis_limits(uint8_t axis, float min_deg, float max_deg)
{
    if (axis < MAX_AXES) {
        axes_[axis].set_limits(min_deg, max_deg);
    }
}

void MotionSystem::set_pid_gains(uint8_t servo_id, float kp, float ki, float kd)
{
    /*
     * servo_id 0-3 maps to:
     *   0 -> axis 0, half 0
     *   1 -> axis 0, half 1
     *   2 -> axis 1, half 0
     *   3 -> axis 1, half 1
     */
    if (servo_id < MAX_SERVOS) {
        uint8_t axis = servo_id / 2;
        uint8_t half = servo_id % 2;
        axes_[axis].set_pid_gains(half, kp, ki, kd);
    }
}

void MotionSystem::update(float dt)
{
    /* Read IMU data */
    last_gravity_ = imu_get_gravity_vector(&imus_[0], mux_);

    /* Check tap events */
    if (imu_check_tap(&imus_[0], mux_)) {
        tap_base_ = true;
    }
    if (imu_check_tap(&imus_[1], mux_)) {
        tap_tip_ = true;
    }

    /* Run motion pattern if one is active */
    if (pattern_) {
        MotionInput input = {};
        /* Gather current encoder positions */
        input.encoder_angles[0] = axes_[0].get_position(0);  // X first half
        input.encoder_angles[1] = axes_[0].get_position(1);  // X second half
        input.encoder_angles[2] = axes_[1].get_position(0);  // Y first half
        input.encoder_angles[3] = axes_[1].get_position(1);  // Y second half
        input.gravity = last_gravity_;
        input.tap_base = tap_base_;
        input.tap_tip = tap_tip_;
        input.loudness = loudness_;
        input.dt = dt;

        MotionOutput output = {};
        pattern_->update(input, output);

        /* Map pattern output to axis targets:
         *   output[0] = X first half,  output[1] = X second half
         *   output[2] = Y first half,  output[3] = Y second half
         */
        axes_[0].set_target(output.target_angles[0], output.target_angles[1]);
        axes_[1].set_target(output.target_angles[2], output.target_angles[3]);
    }

    /* Run PID control loops for both axes */
    axes_[0].update(mux_, dt);
    axes_[1].update(mux_, dt);
}

float MotionSystem::get_position(uint8_t axis, uint8_t half) const
{
    if (axis < MAX_AXES) {
        return axes_[axis].get_position(half);
    }
    return 0;
}

bool MotionSystem::check_tap_base()
{
    bool val = tap_base_;
    tap_base_ = false;
    return val;
}

bool MotionSystem::check_tap_tip()
{
    bool val = tap_tip_;
    tap_tip_ = false;
    return val;
}
