#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float x, y, z;
} vec3_t;

#define MAX_SERVOS      4
#define MAX_AXES        2
#define MAX_IMU         2
#define MAX_LED_RINGS   20
#define MAX_LED_LAYERS  8
#define MAX_FFT_BINS    128

// Servo assignment
typedef struct {
    uint8_t axis;       // 0 = X, 1 = Y
    uint8_t half;       // 0 = first, 1 = second
    bool invert;        // true = reverse spin direction
    uint8_t mux_channel; // I2C mux channel for this servo's encoder
} servo_assignment_t;

typedef struct {
    float kp;
    float ki;
    float kd;
    float output_min;
    float output_max;
    float integral_limit;
} pid_params_t;

typedef struct {
    servo_assignment_t assignment;
    pid_params_t pid;
    int16_t deadband_center; // PWM us center (nominally 1500)
    int16_t deadband_width;  // us of deadband around center (nominally 100)
} servo_config_t;

typedef struct {
    float zero_offset;   // encoder angle at zero position (degrees)
    float limit_min;     // min rotation from zero (degrees, negative = one direction)
    float limit_max;     // max rotation from zero (degrees)
} axis_config_t;

typedef struct {
    uint8_t num_rings;
    uint8_t leds_per_ring[MAX_LED_RINGS];
} led_matrix_config_t;

typedef struct {
    uint8_t effect_id;
    uint8_t blend_mode;  // BlendMode enum value
    bool enabled;
    bool flip_x;
    bool flip_y;
    bool mirror_x;
    bool mirror_y;
    float params[8];     // effect-specific parameters
} layer_config_t;

typedef struct {
    uint8_t pattern_id;
    float params[8];     // pattern-specific parameters
} motion_pattern_config_t;

// FFT data snapshot (written by BLE, read by effects)
typedef struct {
    uint8_t loudness;
    uint8_t num_bins;
    uint8_t bins[MAX_FFT_BINS];
    int64_t timestamp_us; // esp_timer_get_time() when received
} fft_data_t;

// IMU configuration
typedef struct {
    uint8_t mux_channel;
    bool tap_enabled;
} imu_config_t;

// Top-level config
typedef struct {
    servo_config_t servos[MAX_SERVOS];
    axis_config_t axes[MAX_AXES];
    imu_config_t imus[MAX_IMU];
    led_matrix_config_t led_matrix;
    layer_config_t layers[MAX_LED_LAYERS];
    uint8_t num_layers;
    motion_pattern_config_t motion_pattern;
} system_config_t;

#ifdef __cplusplus
}
#endif

#endif
