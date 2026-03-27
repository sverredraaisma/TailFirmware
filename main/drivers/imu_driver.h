#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "i2c_mux.h"
#include "config/config_types.h"

typedef struct {
    i2c_master_dev_handle_t dev;
    uint8_t mux_channel;
    bool tap_detected;
} imu_t;

esp_err_t imu_init(imu_t *imu, i2c_mux_t *mux, uint8_t mux_channel);
esp_err_t imu_read_accel(imu_t *imu, i2c_mux_t *mux, vec3_t *out);
esp_err_t imu_read_gyro(imu_t *imu, i2c_mux_t *mux, vec3_t *out);
vec3_t imu_get_gravity_vector(imu_t *imu, i2c_mux_t *mux);
bool imu_check_tap(imu_t *imu, i2c_mux_t *mux);

#ifdef __cplusplus
}
#endif

#endif
