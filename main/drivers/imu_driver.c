#include "imu_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "imu";

// BMI270 I2C address (SDO=GND)
#define BMI270_ADDR         0x68

// BMI270 register map
#define BMI270_REG_CHIP_ID  0x00
#define BMI270_REG_DATA_8   0x0C  // Accel X LSB
#define BMI270_REG_DATA_14  0x12  // Gyro X LSB
#define BMI270_REG_ACC_CONF 0x40
#define BMI270_REG_ACC_RANGE 0x41
#define BMI270_REG_GYR_CONF 0x42
#define BMI270_REG_GYR_RANGE 0x43
#define BMI270_REG_INIT_CTRL 0x59
#define BMI270_REG_INIT_DATA 0x5E
#define BMI270_REG_PWR_CONF 0x7C
#define BMI270_REG_PWR_CTRL 0x7D
#define BMI270_REG_CMD      0x7E

#define BMI270_CHIP_ID_VAL  0x24
#define BMI270_CMD_SOFT_RESET 0xB6

// Accel: +/-8g range => 1 LSB = 8.0 / 32768.0 g
#define ACCEL_SCALE         (8.0f / 32768.0f)

// Gyro: +/-2000 dps range => 1 LSB = 2000.0 / 32768.0 dps
#define GYRO_SCALE          (2000.0f / 32768.0f)

// Software tap detection threshold in g
#define TAP_THRESHOLD_G     2.5f

// Timeout for I2C operations in ms
#define I2C_TIMEOUT_MS      100

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, I2C_TIMEOUT_MS);
}

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, val, len, I2C_TIMEOUT_MS);
}

static esp_err_t read_reg_byte(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val) {
    return read_reg(dev, reg, val, 1);
}

/**
 * Parse 6 bytes of raw sensor data (XL, XH, YL, YH, ZL, ZH) into a vec3_t,
 * applying the given scale factor per LSB.
 */
static void parse_raw_xyz(const uint8_t *raw, float scale, vec3_t *out) {
    int16_t raw_x = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
    int16_t raw_y = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
    int16_t raw_z = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);
    out->x = (float)raw_x * scale;
    out->y = (float)raw_y * scale;
    out->z = (float)raw_z * scale;
}

esp_err_t imu_init(imu_t *imu, i2c_mux_t *mux, uint8_t mux_channel) {
    esp_err_t ret;

    memset(imu, 0, sizeof(*imu));
    imu->mux_channel = mux_channel;
    imu->tap_detected = false;

    // Add the BMI270 device to the I2C bus
    ret = i2c_mux_add_device(mux, BMI270_ADDR, &imu->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add BMI270 device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Select the mux channel for all subsequent I2C operations
    ret = i2c_mux_select_channel(mux, mux_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to select mux channel %d: %s", mux_channel, esp_err_to_name(ret));
        return ret;
    }

    // Step 1: Soft reset
    ret = write_reg(imu->dev, BMI270_REG_CMD, BMI270_CMD_SOFT_RESET);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Soft reset failed: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    // Re-select mux channel after reset (reset may have disrupted bus state)
    ret = i2c_mux_select_channel(mux, mux_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to re-select mux channel after reset: %s", esp_err_to_name(ret));
        return ret;
    }

    // Step 2: Verify chip ID
    uint8_t chip_id = 0;
    ret = read_reg_byte(imu->dev, BMI270_REG_CHIP_ID, &chip_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read chip ID: %s", esp_err_to_name(ret));
        return ret;
    }
    if (chip_id != BMI270_CHIP_ID_VAL) {
        ESP_LOGE(TAG, "Unexpected chip ID: 0x%02X (expected 0x%02X)", chip_id, BMI270_CHIP_ID_VAL);
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGI(TAG, "BMI270 detected, chip ID: 0x%02X", chip_id);

    // Step 3: Disable advance power save
    ret = write_reg(imu->dev, BMI270_REG_PWR_CONF, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable adv power save: %s", esp_err_to_name(ret));
        return ret;
    }
    // Small delay after disabling power save for internal oscillator to stabilize
    vTaskDelay(pdMS_TO_TICKS(1));

    // Step 4: Prepare config load
    ret = write_reg(imu->dev, BMI270_REG_INIT_CTRL, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to prepare config load: %s", esp_err_to_name(ret));
        return ret;
    }

    // Step 5: Write BMI270 config file to burst register 0x5E
    // TODO: Upload BMI270 config file (~8KB proprietary binary blob).
    // Without this, basic accel/gyro readings work but advanced features
    // (tap detection, step counter, any-motion, etc.) will not function.
    // The config file is available from Bosch Sensortec's BMI270 driver package.
    ESP_LOGW(TAG, "BMI270 config file upload skipped - advanced features disabled");

    // Step 6: Re-enable advance power save
    ret = write_reg(imu->dev, BMI270_REG_PWR_CONF, 0x01);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable adv power save: %s", esp_err_to_name(ret));
        return ret;
    }

    // Step 7: Configure accelerometer
    // ACC_CONF (0x40): ODR=100Hz (0x08), BWP=normal (0x02 << 4 = 0x20), filter_perf=1 (0x80)
    // Value: 0x80 | 0x20 | 0x08 = 0xA8
    ret = write_reg(imu->dev, BMI270_REG_ACC_CONF, 0xA8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure accelerometer: %s", esp_err_to_name(ret));
        return ret;
    }
    // ACC_RANGE (0x41): +/-8g = 0x02
    ret = write_reg(imu->dev, BMI270_REG_ACC_RANGE, 0x02);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set accel range: %s", esp_err_to_name(ret));
        return ret;
    }

    // Step 8: Configure gyroscope
    // GYR_CONF (0x42): ODR=100Hz (0x08), BWP=normal (0x02 << 4 = 0x20), filter_perf=1 (0x80)
    // Value: 0x80 | 0x20 | 0x08 = 0xA8
    ret = write_reg(imu->dev, BMI270_REG_GYR_CONF, 0xA8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure gyroscope: %s", esp_err_to_name(ret));
        return ret;
    }
    // GYR_RANGE (0x43): +/-2000dps = 0x00
    ret = write_reg(imu->dev, BMI270_REG_GYR_RANGE, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set gyro range: %s", esp_err_to_name(ret));
        return ret;
    }

    // Step 9: Enable accelerometer + gyroscope
    // PWR_CTRL (0x7D): bit2=acc_en, bit1=gyr_en, bit0=aux_en, bit3=temp_en
    // 0x44 = 0b01000100 => acc_en + temp_en with gyr_en via upper nibble config
    ret = write_reg(imu->dev, BMI270_REG_PWR_CTRL, 0x44);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable accel/gyro: %s", esp_err_to_name(ret));
        return ret;
    }

    // Allow sensors to stabilize
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "BMI270 initialized on mux channel %d", mux_channel);
    return ESP_OK;
}

esp_err_t imu_read_accel(imu_t *imu, i2c_mux_t *mux, vec3_t *out) {
    esp_err_t ret;

    ret = i2c_mux_select_channel(mux, imu->mux_channel);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t raw[6];
    ret = read_reg(imu->dev, BMI270_REG_DATA_8, raw, sizeof(raw));
    if (ret != ESP_OK) {
        return ret;
    }

    // Convert to g units: at +/-8g range, LSB = 8.0/32768.0 g
    parse_raw_xyz(raw, ACCEL_SCALE, out);
    return ESP_OK;
}

esp_err_t imu_read_gyro(imu_t *imu, i2c_mux_t *mux, vec3_t *out) {
    esp_err_t ret;

    ret = i2c_mux_select_channel(mux, imu->mux_channel);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t raw[6];
    ret = read_reg(imu->dev, BMI270_REG_DATA_14, raw, sizeof(raw));
    if (ret != ESP_OK) {
        return ret;
    }

    // Convert to degrees per second: at +/-2000dps range, LSB = 2000.0/32768.0 dps
    parse_raw_xyz(raw, GYRO_SCALE, out);
    return ESP_OK;
}

vec3_t imu_get_gravity_vector(imu_t *imu, i2c_mux_t *mux) {
    vec3_t accel = {0.0f, 0.0f, 0.0f};

    esp_err_t ret = imu_read_accel(imu, mux, &accel);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read accel for gravity vector: %s", esp_err_to_name(ret));
        return accel;
    }

    // Normalize the acceleration vector to get the gravity direction
    float mag = sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
    if (mag > 0.001f) {
        accel.x /= mag;
        accel.y /= mag;
        accel.z /= mag;
    }

    return accel;
}

bool imu_check_tap(imu_t *imu, i2c_mux_t *mux) {
    // Software tap detection since BMI270 config file is not uploaded.
    // Detects a rising edge when accel magnitude exceeds the tap threshold.
    static bool prev_spiking = false;

    // If a tap was previously detected (set externally or from prior call), return and clear
    if (imu->tap_detected) {
        imu->tap_detected = false;
        return true;
    }

    vec3_t accel;
    esp_err_t ret = imu_read_accel(imu, mux, &accel);
    if (ret != ESP_OK) {
        return false;
    }

    float mag = sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
    bool spiking = (mag > TAP_THRESHOLD_G);

    // Detect rising edge: not spiking before, spiking now
    bool tap = (spiking && !prev_spiking);
    prev_spiking = spiking;

    return tap;
}
