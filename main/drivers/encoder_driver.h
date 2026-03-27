#ifndef ENCODER_DRIVER_H
#define ENCODER_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "i2c_mux.h"

#define AS5600_ADDR 0x36

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t mux_channel;
    int32_t multi_turn_offset; // accumulated full rotations
    uint16_t last_raw_angle;
} encoder_t;

esp_err_t encoder_init(encoder_t *enc, i2c_mux_t *mux, uint8_t mux_channel);
esp_err_t encoder_read_raw(encoder_t *enc, i2c_mux_t *mux, uint16_t *out_raw);
float encoder_read_angle(encoder_t *enc, i2c_mux_t *mux); // multi-turn degrees

#ifdef __cplusplus
}
#endif

#endif
