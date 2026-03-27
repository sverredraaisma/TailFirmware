#include "led_matrix.h"
#include "esp_log.h"

static const char *TAG = "led_matrix";

LedMatrix::LedMatrix() = default;

void LedMatrix::configure(uint8_t num_rings, const uint8_t *leds_per_ring)
{
    total_leds_ = 0;
    for (uint8_t r = 0; r < num_rings; r++) {
        total_leds_ += leds_per_ring[r];
    }

    coords_.resize(total_leds_);
    buffer_.resize(total_leds_);

    uint16_t global_index = 0;
    for (uint8_t r = 0; r < num_rings; r++) {
        float y = (num_rings == 1) ? 0.5f : r / (float)(num_rings - 1);

        for (uint8_t l = 0; l < leds_per_ring[r]; l++) {
            float x = (leds_per_ring[r] == 1) ? 0.5f : l / (float)(leds_per_ring[r] - 1);
            coords_[global_index] = {x, y};
            global_index++;
        }
    }

    ESP_LOGI(TAG, "Configured %d rings, %d total LEDs", num_rings, total_leds_);
}

void LedMatrix::set_pixel(uint16_t index, RGB color)
{
    if (index < total_leds_) {
        buffer_[index] = color;
    }
}

void LedMatrix::push()
{
    if (strip_ == nullptr) {
        return;
    }

    for (uint16_t i = 0; i < total_leds_; i++) {
        led_strip_driver_set_pixel(strip_, i, buffer_[i].r, buffer_[i].g, buffer_[i].b);
    }
    led_strip_driver_refresh(strip_);
}

void LedMatrix::init_strip(int gpio_num)
{
    esp_err_t err = led_strip_driver_init(gpio_num, total_leds_, &strip_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init LED strip: %s", esp_err_to_name(err));
    }
}
