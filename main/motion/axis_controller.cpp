#include "axis_controller.h"
#include "esp_log.h"

static const char *TAG = "axis_ctrl";

void AxisController::configure(uint8_t servo_first, uint8_t servo_second,
                               const servo_config_t &cfg_first,
                               const servo_config_t &cfg_second)
{
    halves_[0].servo_id = servo_first;
    halves_[0].invert = cfg_first.assignment.invert;
    halves_[0].encoder.mux_channel = cfg_first.assignment.mux_channel;
    halves_[0].pid.set_gains(cfg_first.pid.kp, cfg_first.pid.ki, cfg_first.pid.kd);
    halves_[0].pid.set_output_limits(cfg_first.pid.output_min, cfg_first.pid.output_max);
    halves_[0].pid.set_integral_limit(cfg_first.pid.integral_limit);

    halves_[1].servo_id = servo_second;
    halves_[1].invert = cfg_second.assignment.invert;
    halves_[1].encoder.mux_channel = cfg_second.assignment.mux_channel;
    halves_[1].pid.set_gains(cfg_second.pid.kp, cfg_second.pid.ki, cfg_second.pid.kd);
    halves_[1].pid.set_output_limits(cfg_second.pid.output_min, cfg_second.pid.output_max);
    halves_[1].pid.set_integral_limit(cfg_second.pid.integral_limit);

    disabled_ = false;
    for (auto &h : halves_) {
        h.fail_count = 0;
        h.failed = false;
    }
}

void AxisController::init_encoders(i2c_mux_t *mux)
{
    for (int i = 0; i < 2; i++) {
        esp_err_t err = encoder_init(&halves_[i].encoder, mux, halves_[i].encoder.mux_channel);
        if (err != ESP_OK) {
            halves_[i].fail_count = MAX_CONSECUTIVE_FAILURES;
            halves_[i].failed = true;
            ESP_LOGW(TAG, "Encoder %d init failed, disabling half", i);
        }
    }
    if (halves_[0].failed && halves_[1].failed) {
        disabled_ = true;
        ESP_LOGW(TAG, "Both encoders failed, axis disabled");
    }
}

void AxisController::calibrate_zero(i2c_mux_t *mux)
{
    for (int i = 0; i < 2; i++) {
        if (halves_[i].failed) continue;
        halves_[i].zero_offset = encoder_read_angle(&halves_[i].encoder, mux);
        halves_[i].current = 0;
        halves_[i].target = 0;
        halves_[i].pid.reset();
    }
}

void AxisController::set_limits(float min_deg, float max_deg)
{
    limit_min_ = min_deg;
    limit_max_ = max_deg;
}

void AxisController::set_pid_gains(uint8_t half, float kp, float ki, float kd)
{
    if (half < 2) {
        halves_[half].pid.set_gains(kp, ki, kd);
    }
}

void AxisController::set_target(float first_half_deg, float second_half_deg)
{
    halves_[0].target = clamp(first_half_deg, limit_min_, limit_max_);
    halves_[1].target = clamp(second_half_deg, limit_min_, limit_max_);
}

void AxisController::update(i2c_mux_t *mux, float dt)
{
    if (disabled_) return;

    for (int i = 0; i < 2; i++) {
        HalfAxis &h = halves_[i];
        if (h.failed) {
            servo_stop(h.servo_id);
            continue;
        }

        /* Read encoder */
        uint16_t raw = 0;
        esp_err_t err = encoder_read_raw(&h.encoder, mux, &raw);
        if (err != ESP_OK) {
            h.fail_count++;
            if (h.fail_count >= MAX_CONSECUTIVE_FAILURES) {
                h.failed = true;
                servo_stop(h.servo_id);
                ESP_LOGW(TAG, "Encoder %d: %d consecutive failures, disabling", i, h.fail_count);
                if (halves_[0].failed && halves_[1].failed) {
                    disabled_ = true;
                    ESP_LOGW(TAG, "Both encoders failed, axis disabled");
                }
            }
            continue;
        }
        h.fail_count = 0;  // reset on success

        /* Compute position relative to zero */
        float raw_angle = encoder_read_angle(&h.encoder, mux);
        h.current = raw_angle - h.zero_offset;

        /* Run PID controller */
        float output = h.pid.update(h.target, h.current, dt);

        if (h.invert) {
            output = -output;
        }

        int16_t speed = static_cast<int16_t>(clamp(output, SERVO_SPEED_MIN, SERVO_SPEED_MAX));
        servo_set_speed(h.servo_id, speed);
    }
}

float AxisController::get_position(uint8_t half) const
{
    if (half < 2) {
        return halves_[half].current;
    }
    return 0;
}

float AxisController::clamp(float val, float lo, float hi) const
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}
