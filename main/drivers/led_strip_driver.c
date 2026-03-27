#include "led_strip_driver.h"
#include "led_strip.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "led_strip_drv";

struct led_strip_ctx {
    led_strip_handle_t esp_strip;
    uint16_t num_leds;
};

esp_err_t led_strip_driver_init(int gpio_num, uint16_t num_leds, tail_led_strip_t *out_handle) {
    if (out_handle == NULL) return ESP_ERR_INVALID_ARG;

    struct led_strip_ctx *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) return ESP_ERR_NO_MEM;

    ctx->num_leds = num_leds;

    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_num,
        .max_leds = num_leds,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &ctx->esp_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT init failed: %s", esp_err_to_name(err));
        free(ctx);
        return err;
    }

    led_strip_clear(ctx->esp_strip);
    ESP_LOGI(TAG, "LED strip: GPIO %d, %d LEDs", gpio_num, num_leds);
    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t led_strip_driver_set_pixel(tail_led_strip_t strip, uint16_t index,
                                      uint8_t r, uint8_t g, uint8_t b) {
    if (!strip || index >= strip->num_leds) return ESP_ERR_INVALID_ARG;
    return led_strip_set_pixel(strip->esp_strip, index, r, g, b);
}

esp_err_t led_strip_driver_refresh(tail_led_strip_t strip) {
    if (!strip) return ESP_ERR_INVALID_ARG;
    return led_strip_refresh(strip->esp_strip);
}

esp_err_t led_strip_driver_clear(tail_led_strip_t strip) {
    if (!strip) return ESP_ERR_INVALID_ARG;
    return led_strip_clear(strip->esp_strip);
}
