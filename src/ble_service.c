#include "ble_service.h"
#include "servo.h"

#include "btstack.h"
#include "ble/le_device_db_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_flash_bank.h"
#include "tail_service.h"  // generated from tail_service.gatt

#define APP_AD_FLAGS 0x06  // LE General Discoverable, BR/EDR not supported

// Primary adv data: flags + name = 20 bytes (max 31)
static const uint8_t adv_data[] = {
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // Complete local name: "Tail controller"
    0x10, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'T', 'a', 'i', 'l', ' ', 'c', 'o', 'n', 't', 'r', 'o', 'l', 'l', 'e', 'r',
};

// Scan response: 128-bit service UUID = 18 bytes (max 31)
static const uint8_t scan_resp_data[] = {
    0x11, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
};

static btstack_packet_callback_registration_t hci_event_callback_registration;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static bool notifications_enabled = false;
static ble_servo_cmd_callback_t servo_cmd_callback = NULL;
static ble_state_t ble_state = BLE_STATE_OFF;
static btstack_tlv_flash_bank_t tlv_context;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                ble_state = BLE_STATE_ADVERTISING;
                printf("[BLE] Stack ready, advertising\n");
            } else {
                ble_state = BLE_STATE_OFF;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = HCI_CON_HANDLE_INVALID;
            notifications_enabled = false;
            ble_state = BLE_STATE_ADVERTISING;
            gap_advertisements_enable(1);
            printf("[BLE] Disconnected\n");
            break;

        case ATT_EVENT_CONNECTED:
            con_handle = att_event_connected_get_handle(packet);
            ble_state = BLE_STATE_CONNECTED;
            printf("[BLE] Connected\n");
            break;

        case ATT_EVENT_CAN_SEND_NOW: {
            uint8_t angles[NUM_SERVOS];
            for (int i = 0; i < NUM_SERVOS; i++) {
                angles[i] = servo_get_angle(i);
            }
            att_server_notify(con_handle,
                ATT_CHARACTERISTIC_0000FF02_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE,
                angles, NUM_SERVOS);
            break;
        }

        default:
            break;
    }
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle,
                                   uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    (void)connection_handle;

    if (att_handle == ATT_CHARACTERISTIC_0000FF02_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE) {
        uint8_t angles[NUM_SERVOS];
        for (int i = 0; i < NUM_SERVOS; i++) {
            angles[i] = servo_get_angle(i);
        }
        return att_read_callback_handle_blob(angles, NUM_SERVOS, offset, buffer, buffer_size);
    }

    if (att_handle == ATT_CHARACTERISTIC_0000FF02_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) {
        return att_read_callback_handle_little_endian_16(notifications_enabled ? GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION : 0,
                                                         offset, buffer, buffer_size);
    }

    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle,
                               uint16_t transaction_mode, uint16_t offset,
                               uint8_t *buffer, uint16_t buffer_size) {
    (void)connection_handle;
    (void)transaction_mode;
    (void)offset;

    if (att_handle == ATT_CHARACTERISTIC_0000FF01_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE) {
        if (buffer_size >= 2 && servo_cmd_callback) {
            uint8_t servo_id = buffer[0];
            uint8_t angle = buffer[1];
            printf("[BLE] Servo %d -> %d deg\n", servo_id, angle);
            servo_cmd_callback(servo_id, angle);

            // Notify connected client of new state if enabled
            if (notifications_enabled) {
                att_server_request_can_send_now_event(con_handle);
            }
        }
        return 0;
    }

    if (att_handle == ATT_CHARACTERISTIC_0000FF02_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) {
        notifications_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
        printf("[BLE] Notifications %s\n", notifications_enabled ? "enabled" : "disabled");
        return 0;
    }

    return 0;
}

void ble_service_init(ble_servo_cmd_callback_t callback) {
    servo_cmd_callback = callback;

    l2cap_init();

    // Set up persistent bond storage in flash
    const hal_flash_bank_t *flash_bank = pico_flash_bank_instance();
    const btstack_tlv_t *tlv = btstack_tlv_flash_bank_init_instance(&tlv_context, flash_bank, flash_bank);
    btstack_tlv_set_instance(tlv, &tlv_context);
    le_device_db_tlv_configure(tlv, &tlv_context);

    // Security Manager: "Just Works" pairing with bonding + secure connections
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    att_server_init(profile_data, att_read_callback, att_write_callback);

    // Advertisement parameters: 30ms interval, connectable undirected
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(0x0030, 0x0030, 0, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(sizeof(adv_data), (uint8_t *)adv_data);
    gap_scan_response_set_data(sizeof(scan_resp_data), (uint8_t *)scan_resp_data);
    gap_advertisements_enable(1);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    att_server_register_packet_handler(packet_handler);
}

void ble_service_notify_state(const uint8_t *angles, uint8_t count) {
    (void)angles;
    (void)count;
    if (con_handle != HCI_CON_HANDLE_INVALID && notifications_enabled) {
        att_server_request_can_send_now_event(con_handle);
    }
}

ble_state_t ble_service_get_state(void) {
    return ble_state;
}
