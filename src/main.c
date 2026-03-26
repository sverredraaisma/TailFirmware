#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

#include "servo.h"
#include "ble_service.h"

#define LED_TICK_MS 100

static btstack_timer_source_t led_timer;
static bool led_state = false;
static uint32_t led_counter = 0;

static void led_blink_handler(btstack_timer_source_t *ts) {
    led_counter++;

    switch (ble_service_get_state()) {
        case BLE_STATE_ADVERTISING:
            // Fast blink: toggle every 200ms
            if (led_counter % 2 == 0) {
                led_state = !led_state;
            }
            break;
        case BLE_STATE_CONNECTED:
            // Solid on
            led_state = true;
            break;
        default:
            // Slow blink: toggle every 1000ms
            if (led_counter % 10 == 0) {
                led_state = !led_state;
            }
            break;
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
    btstack_run_loop_set_timer(ts, LED_TICK_MS);
    btstack_run_loop_add_timer(ts);
}

static void on_servo_command(uint8_t servo_id, uint8_t angle) {
    servo_set_angle(servo_id, angle);
}

int main(void) {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("CYW43 init failed\n");
        return -1;
    }

    printf("TailFirmware: initializing\n");

    servo_init();
    ble_service_init(on_servo_command);

    led_timer.process = &led_blink_handler;
    btstack_run_loop_set_timer(&led_timer, LED_TICK_MS);
    btstack_run_loop_add_timer(&led_timer);

    hci_power_control(HCI_POWER_ON);

    printf("TailFirmware: BLE advertising as 'Tail controller'\n");

    while (true) {
        tight_loop_contents();
    }
}
