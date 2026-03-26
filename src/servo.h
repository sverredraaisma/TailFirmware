#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

#define NUM_SERVOS 4

// GPIO pins for each servo (PWM-capable pins on Pico 2W header)
#define SERVO_0_PIN 2
#define SERVO_1_PIN 3
#define SERVO_2_PIN 4
#define SERVO_3_PIN 5

// Servo pulse range in microseconds
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500

// Angle limits
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180

void servo_init(void);
void servo_set_angle(uint8_t servo_id, uint8_t angle);
uint8_t servo_get_angle(uint8_t servo_id);

#endif
