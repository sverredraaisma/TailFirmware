#include "servo.h"
#include "driver/ledc.h"

#define SERVO_FREQ_HZ       50
#define SERVO_RESOLUTION     LEDC_TIMER_14_BIT
#define SERVO_DUTY_MAX       ((1 << 14) - 1)  /* 16383 */
#define SERVO_PERIOD_US      20000

static const int servo_pins[NUM_SERVOS] = {
    SERVO_0_PIN, SERVO_1_PIN, SERVO_2_PIN, SERVO_3_PIN
};

static int16_t servo_speeds[NUM_SERVOS];

/**
 * Map speed (-1000 to +1000) to LEDC duty value.
 *
 * speed -1000 -> 500us  pulse -> duty = 500  * 16384 / 20000 = 410
 * speed  0    -> 1500us pulse -> duty = 1500 * 16384 / 20000 = 1229
 * speed +1000 -> 2500us pulse -> duty = 2500 * 16384 / 20000 = 2048
 */
static uint32_t speed_to_duty(int16_t speed)
{
    /* Clamp speed to valid range */
    if (speed < SERVO_SPEED_MIN) speed = SERVO_SPEED_MIN;
    if (speed > SERVO_SPEED_MAX) speed = SERVO_SPEED_MAX;

    /*
     * Map speed to pulse width in microseconds:
     *   speed -1000 -> 500us
     *   speed     0 -> 1500us
     *   speed +1000 -> 2500us
     *
     * pulse_us = 1500 + speed * 1000 / 1000 = 1500 + speed
     */
    uint32_t pulse_us = (uint32_t)(SERVO_STOP_PULSE_US + speed);

    /* Convert pulse width to duty cycle: duty = pulse_us * (DUTY_MAX + 1) / PERIOD_US */
    return pulse_us * (SERVO_DUTY_MAX + 1) / SERVO_PERIOD_US;
}

void servo_init(void)
{
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
            .duty = speed_to_duty(0),  /* Start stopped */
            .hpoint = 0,
        };
        ledc_channel_config(&ch_conf);
        servo_speeds[i] = 0;
    }
}

void servo_set_speed(uint8_t servo_id, int16_t speed)
{
    if (servo_id >= NUM_SERVOS) return;

    if (speed < SERVO_SPEED_MIN) speed = SERVO_SPEED_MIN;
    if (speed > SERVO_SPEED_MAX) speed = SERVO_SPEED_MAX;

    servo_speeds[servo_id] = speed;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)servo_id, speed_to_duty(speed));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)servo_id);
}

void servo_stop(uint8_t servo_id)
{
    servo_set_speed(servo_id, 0);
}
