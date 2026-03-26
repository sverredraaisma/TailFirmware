#include "servo.h"
#include "driver/ledc.h"

#define SERVO_FREQ_HZ       50
#define SERVO_RESOLUTION     LEDC_TIMER_14_BIT
#define SERVO_DUTY_MAX       ((1 << 14) - 1)
#define SERVO_PERIOD_US      20000

static const int servo_pins[NUM_SERVOS] = {
    SERVO_0_PIN, SERVO_1_PIN, SERVO_2_PIN, SERVO_3_PIN
};

static uint8_t servo_angles[NUM_SERVOS];

static uint32_t angle_to_duty(uint8_t angle) {
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    uint32_t pulse_us = SERVO_MIN_PULSE_US +
        (uint32_t)angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) / SERVO_MAX_ANGLE;
    return pulse_us * (SERVO_DUTY_MAX + 1) / SERVO_PERIOD_US;
}

void servo_init(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    for (int i = 0; i < NUM_SERVOS; i++) {
        ledc_channel_config_t ch_conf = {
            .gpio_num = servo_pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)i,
            .timer_sel = LEDC_TIMER_0,
            .duty = angle_to_duty(90),
            .hpoint = 0,
        };
        ledc_channel_config(&ch_conf);
        servo_angles[i] = 90;
    }
}

void servo_set_angle(uint8_t servo_id, uint8_t angle) {
    if (servo_id >= NUM_SERVOS) return;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    servo_angles[servo_id] = angle;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)servo_id, angle_to_duty(angle));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)servo_id);
}

uint8_t servo_get_angle(uint8_t servo_id) {
    if (servo_id >= NUM_SERVOS) return 0;
    return servo_angles[servo_id];
}
