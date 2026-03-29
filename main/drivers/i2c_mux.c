#include "i2c_mux.h"
#include "esp_log.h"

static const char *TAG = "i2c_mux";

esp_err_t i2c_mux_init(i2c_mux_t *mux, int sda_pin, int scl_pin, uint32_t freq_hz) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &mux->bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t mux_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_MUX_ADDR,
        .scl_speed_hz = freq_hz,
    };
    ret = i2c_master_bus_add_device(mux->bus, &mux_cfg, &mux->mux_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add mux device: %s", esp_err_to_name(ret));
        return ret;
    }

    mux->active_channel = 0xFF;
    return i2c_mux_disable_all(mux);
}

esp_err_t i2c_mux_select_channel(i2c_mux_t *mux, uint8_t channel) {
    if (channel >= I2C_MUX_NUM_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }
    if (channel == mux->active_channel) {
        return ESP_OK;
    }
    uint8_t data = 1 << channel;
    esp_err_t ret = i2c_master_transmit(mux->mux_dev, &data, 1, 10);
    if (ret == ESP_OK) {
        mux->active_channel = channel;
    }
    return ret;
}

esp_err_t i2c_mux_disable_all(i2c_mux_t *mux) {
    uint8_t data = 0x00;
    esp_err_t ret = i2c_master_transmit(mux->mux_dev, &data, 1, 10);
    if (ret == ESP_OK) {
        mux->active_channel = 0xFF;
    }
    return ret;
}

esp_err_t i2c_mux_add_device(i2c_mux_t *mux, uint8_t dev_addr,
                              i2c_master_dev_handle_t *out_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(mux->bus, &dev_cfg, out_handle);
}
