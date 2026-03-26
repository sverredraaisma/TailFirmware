#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdint.h>

typedef enum {
    BLE_STATE_OFF,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
} ble_state_t;

// Callback type for servo commands received over BLE
typedef void (*ble_servo_cmd_callback_t)(uint8_t servo_id, uint8_t angle);

void ble_service_init(ble_servo_cmd_callback_t callback);
void ble_service_notify_state(const uint8_t *angles, uint8_t count);
ble_state_t ble_service_get_state(void);

#endif
