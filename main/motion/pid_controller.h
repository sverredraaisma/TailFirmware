#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

class PidController {
public:
    PidController();

    void set_gains(float kp, float ki, float kd);
    void set_output_limits(float min, float max);
    void set_integral_limit(float limit);
    void reset();

    /**
     * Compute PID output.
     * @param setpoint      Desired value
     * @param measurement   Current measured value
     * @param dt            Time step in seconds
     * @return              Control output, clamped to output limits
     */
    float update(float setpoint, float measurement, float dt);

private:
    float kp_, ki_, kd_;
    float output_min_, output_max_;
    float integral_limit_;
    float integral_;
    float prev_measurement_;  // derivative on measurement, not error
    bool first_update_;
};

#endif
