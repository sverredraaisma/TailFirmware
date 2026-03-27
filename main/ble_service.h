#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    BLE_STATE_OFF,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
} ble_state_t;

// Callback for dispatching received BLE commands to the config manager.
// chr_uuid_short: e.g. 0xFF01 for motion commands
// data: raw payload after the command_id byte is included
// len: total length including command_id
typedef void (*ble_cmd_callback_t)(uint16_t chr_uuid_short, const uint8_t *data, uint16_t len);

void ble_service_init(ble_cmd_callback_t cmd_callback);
ble_state_t ble_service_get_state(void);

// Notify functions for each stateful characteristic
void ble_notify_motion_state(const uint8_t *data, uint16_t len);
void ble_notify_led_state(const uint8_t *data, uint16_t len);
void ble_notify_system_event(const uint8_t *data, uint16_t len);

// Set the read response data for state characteristics.
// These are cached and returned on read requests.
void ble_set_motion_state(const uint8_t *data, uint16_t len);
void ble_set_led_state(const uint8_t *data, uint16_t len);
void ble_set_system_info(const uint8_t *data, uint16_t len);
void ble_set_profile_list(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif
