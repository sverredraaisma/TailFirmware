#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "servo.h"
#include "ble_service.h"

static const char *TAG = "main";

// Onboard LED GPIO (GPIO 8 on most ESP32-C3 dev boards, change if needed)
#define LED_GPIO 8

static void on_servo_command(uint8_t servo_id, uint8_t angle) {
    servo_set_angle(servo_id, angle);
}

static void led_task(void *param) {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    bool led_on = false;

    while (1) {
        switch (ble_service_get_state()) {
            case BLE_STATE_ADVERTISING:
                led_on = !led_on;
                gpio_set_level(LED_GPIO, led_on);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            case BLE_STATE_CONNECTED:
                gpio_set_level(LED_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            default:
                led_on = !led_on;
                gpio_set_level(LED_GPIO, led_on);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
    }
}

void app_main(void) {
    // Initialize NVS (required for BLE bonding storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "TailFirmware: initializing");

    servo_init();
    ble_service_init(on_servo_command);

    xTaskCreate(led_task, "led", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "TailFirmware: BLE advertising as 'Tail controller'");
}
