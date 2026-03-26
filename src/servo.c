#include "servo.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#define PWM_WRAP 19999  // 20ms period at 1MHz effective clock

static const uint8_t servo_pins[NUM_SERVOS] = {
    SERVO_0_PIN, SERVO_1_PIN, SERVO_2_PIN, SERVO_3_PIN
};

static uint8_t servo_angles[NUM_SERVOS];

static uint16_t angle_to_pulse(uint8_t angle) {
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    return SERVO_MIN_PULSE_US +
           (uint16_t)((uint32_t)angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) / SERVO_MAX_ANGLE);
}

void servo_init(void) {
    float clk_div = (float)clock_get_hz(clk_sys) / 1000000.0f;

    for (int i = 0; i < NUM_SERVOS; i++) {
        gpio_set_function(servo_pins[i], GPIO_FUNC_PWM);

        uint slice = pwm_gpio_to_slice_num(servo_pins[i]);
        uint channel = pwm_gpio_to_channel(servo_pins[i]);

        pwm_set_clkdiv(slice, clk_div);
        pwm_set_wrap(slice, PWM_WRAP);
        pwm_set_chan_level(slice, channel, angle_to_pulse(90));
        pwm_set_enabled(slice, true);

        servo_angles[i] = 90;
    }
}

void servo_set_angle(uint8_t servo_id, uint8_t angle) {
    if (servo_id >= NUM_SERVOS) return;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    servo_angles[servo_id] = angle;

    uint pin = servo_pins[servo_id];
    uint slice = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    pwm_set_chan_level(slice, channel, angle_to_pulse(angle));
}

uint8_t servo_get_angle(uint8_t servo_id) {
    if (servo_id >= NUM_SERVOS) return 0;
    return servo_angles[servo_id];
}
