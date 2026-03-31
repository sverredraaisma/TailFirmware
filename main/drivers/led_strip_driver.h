#ifndef LED_STRIP_DRIVER_H
#define LED_STRIP_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>

typedef struct led_strip_ctx *tail_led_strip_t;

esp_err_t led_strip_driver_init(int gpio_num, uint16_t num_leds, tail_led_strip_t *out_handle);
esp_err_t led_strip_driver_set_pixel(tail_led_strip_t strip, uint16_t index,
                                      uint8_t r, uint8_t g, uint8_t b);
esp_err_t led_strip_driver_refresh(tail_led_strip_t strip);
esp_err_t led_strip_driver_clear(tail_led_strip_t strip);
esp_err_t led_strip_driver_deinit(tail_led_strip_t strip);

#ifdef __cplusplus
}
#endif

#endif
