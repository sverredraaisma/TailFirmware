#include "encoder_driver.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "encoder";

#define AS5600_REG_RAW_ANGLE_H 0x0C
#define AS5600_REG_RAW_ANGLE_L 0x0D

esp_err_t encoder_init(encoder_t *enc, i2c_mux_t *mux, uint8_t mux_channel)
{
    if (!enc || !mux || mux_channel >= I2C_MUX_NUM_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(enc, 0, sizeof(encoder_t));
    enc->mux_channel = mux_channel;
    enc->multi_turn_offset = 0;
    enc->last_raw_angle = 0;

    /* Add the AS5600 device on the I2C bus (channel selection is done before each access) */
    esp_err_t ret = i2c_mux_add_device(mux, AS5600_ADDR, &enc->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add AS5600 device on mux channel %d: %s",
                 mux_channel, esp_err_to_name(ret));
        return ret;
    }

    /* Do an initial read to seed last_raw_angle */
    ret = i2c_mux_select_channel(mux, mux_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to select mux channel %d: %s",
                 mux_channel, esp_err_to_name(ret));
        return ret;
    }

    uint8_t reg = AS5600_REG_RAW_ANGLE_H;
    uint8_t data[2] = {0};
    ret = i2c_master_transmit_receive(enc->dev, &reg, 1, data, 2, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed initial read of AS5600 on channel %d: %s",
                 mux_channel, esp_err_to_name(ret));
        return ret;
    }

    enc->last_raw_angle = ((uint16_t)(data[0] & 0x0F) << 8) | data[1];
    ESP_LOGI(TAG, "Encoder on mux channel %d initialized, raw angle: %u",
             mux_channel, enc->last_raw_angle);

    return ESP_OK;
}

esp_err_t encoder_read_raw(encoder_t *enc, i2c_mux_t *mux, uint16_t *out_raw)
{
    if (!enc || !mux || !out_raw) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Select the mux channel for this encoder */
    esp_err_t ret = i2c_mux_select_channel(mux, enc->mux_channel);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Read registers 0x0C (high) and 0x0D (low) in a single transaction */
    uint8_t reg = AS5600_REG_RAW_ANGLE_H;
    uint8_t data[2] = {0};
    ret = i2c_master_transmit_receive(enc->dev, &reg, 1, data, 2, -1);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 12-bit value: high byte bits [3:0] and full low byte */
    uint16_t raw = ((uint16_t)(data[0] & 0x0F) << 8) | data[1];

    /* Multi-turn tracking: detect wraps at 0/4095 boundary */
    int32_t delta = (int32_t)raw - (int32_t)enc->last_raw_angle;
    if (delta > 2048) {
        /* Raw jumped forward by more than half a turn: wrapped backward (e.g. 100 -> 4000) */
        enc->multi_turn_offset--;
    } else if (delta < -2048) {
        /* Raw jumped backward by more than half a turn: wrapped forward (e.g. 4000 -> 100) */
        enc->multi_turn_offset++;
    }

    enc->last_raw_angle = raw;
    *out_raw = raw;
    return ESP_OK;
}

float encoder_read_angle(encoder_t *enc, i2c_mux_t *mux)
{
    uint16_t raw = 0;
    esp_err_t ret = encoder_read_raw(enc, mux, &raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "encoder_read_angle failed: %s", esp_err_to_name(ret));
        return 0.0f;
    }

    /* Continuous angle: full rotations + fractional position */
    float angle = (float)enc->multi_turn_offset * 360.0f
                + (float)raw * 360.0f / 4096.0f;
    return angle;
}
