#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include "config/pin_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_SERVOS 4

/* Servo pulse range in microseconds */
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500
#define SERVO_STOP_PULSE_US 1500

/* Speed range for continuous rotation servos */
#define SERVO_SPEED_MIN (-1000)
#define SERVO_SPEED_MAX  1000

void servo_init(void);

/**
 * Set continuous rotation servo speed.
 * @param servo_id  Servo index 0-3
 * @param speed     -1000 (full reverse) to +1000 (full forward), 0 = stop
 */
void servo_set_speed(uint8_t servo_id, int16_t speed);

/**
 * Stop a servo (convenience for servo_set_speed(id, 0)).
 */
void servo_stop(uint8_t servo_id);

#ifdef __cplusplus
}
#endif

#endif
