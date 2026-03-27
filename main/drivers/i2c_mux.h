#ifndef I2C_MUX_H
#define I2C_MUX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define I2C_MUX_ADDR        0x70  // TCA9548A default address (A0=A1=A2=GND)
#define I2C_MUX_NUM_CHANNELS 8

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t mux_dev;
    uint8_t active_channel;
} i2c_mux_t;

/**
 * Initialize I2C bus and TCA9548A multiplexer.
 * @param mux       Pointer to mux context
 * @param sda_pin   GPIO for SDA
 * @param scl_pin   GPIO for SCL
 * @param freq_hz   I2C clock frequency (e.g. 400000)
 */
esp_err_t i2c_mux_init(i2c_mux_t *mux, int sda_pin, int scl_pin, uint32_t freq_hz);

/**
 * Select a downstream channel on the multiplexer.
 * @param mux       Pointer to mux context
 * @param channel   Channel 0-7
 */
esp_err_t i2c_mux_select_channel(i2c_mux_t *mux, uint8_t channel);

/** Disable all downstream channels. */
esp_err_t i2c_mux_disable_all(i2c_mux_t *mux);

/**
 * Add a device on a specific mux channel.
 * The caller must select the channel before communicating with this device.
 * @param mux           Pointer to mux context
 * @param dev_addr      7-bit I2C address of the downstream device
 * @param out_handle    Receives the device handle
 */
esp_err_t i2c_mux_add_device(i2c_mux_t *mux, uint8_t dev_addr,
                              i2c_master_dev_handle_t *out_handle);

#ifdef __cplusplus
}
#endif

#endif
