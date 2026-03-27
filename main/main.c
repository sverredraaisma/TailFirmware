#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "config/config_types.h"
#include "drivers/i2c_mux.h"
#include "ble_service.h"

static const char *TAG = "main";

// Hardware pin definitions (compile-time)
#define I2C_SDA_PIN     1
#define I2C_SCL_PIN     2
#define SERVO_0_PIN     3
#define SERVO_1_PIN     4
#define SERVO_2_PIN     5
#define SERVO_3_PIN     6
#define LED_STRIP_PIN   7
#define STATUS_LED_PIN  8

// Event group for inter-task signals
EventGroupHandle_t system_events;
#define EVENT_TAP_BASE  (1 << 0)
#define EVENT_TAP_TIP   (1 << 1)

// Shared state
static i2c_mux_t i2c_mux;
static system_config_t config;

// Forward declarations
static void motion_ctrl_task(void *param);
static void led_render_task(void *param);
static void led_status_task(void *param);

static void on_servo_command(uint8_t servo_id, uint8_t angle) {
    // Legacy simple command - will be replaced by motion system
    (void)servo_id;
    (void)angle;
}

static void led_status_task(void *param) {
    gpio_reset_pin(STATUS_LED_PIN);
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    bool led_on = false;

    while (1) {
        switch (ble_service_get_state()) {
            case BLE_STATE_ADVERTISING:
                led_on = !led_on;
                gpio_set_level(STATUS_LED_PIN, led_on);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            case BLE_STATE_CONNECTED:
                gpio_set_level(STATUS_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            default:
                led_on = !led_on;
                gpio_set_level(STATUS_LED_PIN, led_on);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
    }
}

static void motion_ctrl_task(void *param) {
    const TickType_t period = pdMS_TO_TICKS(10); // 100 Hz
    TickType_t last_wake = xTaskGetTickCount();

    ESP_LOGI(TAG, "Motion control task started (100 Hz)");

    while (1) {
        // Phase 2+: Read encoders, run PID, update servos
        // Phase 4+: Read IMUs, detect taps
        // Phase 5+: Run active motion pattern

        vTaskDelayUntil(&last_wake, period);
    }
}

static void led_render_task(void *param) {
    const TickType_t period = pdMS_TO_TICKS(33); // ~30 Hz
    TickType_t last_wake = xTaskGetTickCount();

    ESP_LOGI(TAG, "LED render task started (30 Hz)");

    while (1) {
        // Phase 7+: Render effect layers, push to strip

        vTaskDelayUntil(&last_wake, period);
    }
}

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "TailFirmware initializing");

    // Create event group
    system_events = xEventGroupCreate();

    // Initialize I2C bus and multiplexer
    ret = i2c_mux_init(&i2c_mux, I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C mux init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "I2C mux initialized");
    }

    // Initialize BLE
    ble_service_init(on_servo_command);

    // Start tasks (highest priority first)
    xTaskCreate(motion_ctrl_task, "motion",  4096, NULL, 5, NULL);
    xTaskCreate(led_render_task,  "led_rend", 4096, NULL, 3, NULL);
    xTaskCreate(led_status_task,  "led_stat", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "All tasks started");
}
