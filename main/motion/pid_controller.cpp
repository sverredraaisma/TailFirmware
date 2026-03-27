#include "pid_controller.h"

PidController::PidController()
    : kp_(1.0f)
    , ki_(0.0f)
    , kd_(0.0f)
    , output_min_(-1000.0f)
    , output_max_(1000.0f)
    , integral_limit_(500.0f)
    , integral_(0.0f)
    , prev_measurement_(0.0f)
    , first_update_(true)
{
}

void PidController::set_gains(float kp, float ki, float kd)
{
    kp_ = kp;
    ki_ = ki;
    kd_ = kd;
}

void PidController::set_output_limits(float min, float max)
{
    output_min_ = min;
    output_max_ = max;
}

void PidController::set_integral_limit(float limit)
{
    integral_limit_ = limit;
}

void PidController::reset()
{
    integral_ = 0.0f;
    prev_measurement_ = 0.0f;
    first_update_ = true;
}

float PidController::update(float setpoint, float measurement, float dt)
{
    if (dt <= 0.0f) {
        return 0.0f;
    }

    float error = setpoint - measurement;

    /* Proportional term */
    float p_term = kp_ * error;

    /* Integral term with anti-windup */
    integral_ += ki_ * error * dt;
    if (integral_ > integral_limit_) {
        integral_ = integral_limit_;
    } else if (integral_ < -integral_limit_) {
        integral_ = -integral_limit_;
    }
    float i_term = integral_;

    /* Derivative term on measurement (not error) to avoid derivative kick */
    float d_term = 0.0f;
    if (!first_update_) {
        float d_measurement = (measurement - prev_measurement_) / dt;
        d_term = -kd_ * d_measurement;
    }
    prev_measurement_ = measurement;
    first_update_ = false;

    /* Sum and clamp output */
    float output = p_term + i_term + d_term;
    if (output > output_max_) {
        output = output_max_;
    } else if (output < output_min_) {
        output = output_min_;
    }

    return output;
}
