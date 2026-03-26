#include "ble_service.h"
#include "servo.h"

#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble";

// Same UUIDs as the Pico version (little-endian byte order for NimBLE)
// Service: 0000FF00-0000-1000-8000-00805F9B34FB
static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00);

// Servo Command: 0000FF01-0000-1000-8000-00805F9B34FB
static const ble_uuid128_t servo_cmd_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00);

// Servo State: 0000FF02-0000-1000-8000-00805F9B34FB
static const ble_uuid128_t servo_state_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00);

static uint16_t servo_state_handle;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool notify_enabled = false;
static uint8_t own_addr_type;
static ble_servo_cmd_callback_t servo_cmd_callback = NULL;
static ble_state_t ble_state = BLE_STATE_OFF;

static int gap_event_handler(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

// GATT access callback for servo command characteristic (write)
static int servo_cmd_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (OS_MBUF_PKTLEN(ctxt->om) >= 2 && servo_cmd_callback) {
            uint8_t buf[2];
            os_mbuf_copydata(ctxt->om, 0, 2, buf);
            uint8_t servo_id = buf[0];
            uint8_t angle = buf[1];
            ESP_LOGI(TAG, "Servo %d -> %d deg", servo_id, angle);
            servo_cmd_callback(servo_id, angle);

            // Send notification if enabled
            if (notify_enabled && conn_handle != BLE_HS_CONN_HANDLE_NONE) {
                uint8_t angles[NUM_SERVOS];
                for (int i = 0; i < NUM_SERVOS; i++) {
                    angles[i] = servo_get_angle(i);
                }
                struct os_mbuf *om = ble_hs_mbuf_from_flat(angles, NUM_SERVOS);
                ble_gatts_notify_custom(conn_handle, servo_state_handle, om);
            }
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// GATT access callback for servo state characteristic (read)
static int servo_state_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t angles[NUM_SERVOS];
        for (int i = 0; i < NUM_SERVOS; i++) {
            angles[i] = servo_get_angle(i);
        }
        os_mbuf_append(ctxt->om, angles, NUM_SERVOS);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// GATT service definition table
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &servo_cmd_uuid.u,
                .access_cb = servo_cmd_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &servo_state_uuid.u,
                .access_cb = servo_state_access,
                .val_handle = &servo_state_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 },
        },
    },
    { 0 },
};

static int gatt_svc_init(void) {
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) return rc;

    rc = ble_gatts_add_svcs(gatt_svcs);
    return rc;
}

static void start_advertising(void) {
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed: %d", rc);
        return;
    }

    // Scan response with service UUID
    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.uuids128 = &svc_uuid;
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 0;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_rsp_set_fields failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(60);

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started");
}

static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                conn_handle = event->connect.conn_handle;
                ble_state = BLE_STATE_CONNECTED;
                ESP_LOGI(TAG, "Connected");
            } else {
                ESP_LOGI(TAG, "Connection failed: %d", event->connect.status);
                start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            conn_handle = BLE_HS_CONN_HANDLE_NONE;
            notify_enabled = false;
            ble_state = BLE_STATE_ADVERTISING;
            ESP_LOGI(TAG, "Disconnected");
            start_advertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == servo_state_handle) {
                notify_enabled = event->subscribe.cur_notify;
                ESP_LOGI(TAG, "Notifications %s", notify_enabled ? "enabled" : "disabled");
            }
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            start_advertising();
            break;

        default:
            break;
    }
    return 0;
}

static void on_stack_reset(int reason) {
    ESP_LOGW(TAG, "NimBLE stack reset, reason: %d", reason);
}

static void on_stack_sync(void) {
    ble_hs_id_infer_auto(0, &own_addr_type);
    ble_state = BLE_STATE_ADVERTISING;
    start_advertising();
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    vTaskDelete(NULL);
}

void ble_service_init(ble_servo_cmd_callback_t callback) {
    servo_cmd_callback = callback;

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", ret);
        return;
    }

    // Host stack configuration
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Security: Just Works pairing with bonding + secure connections
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;

    ble_svc_gap_device_name_set("Tail controller");

    gatt_svc_init();

    xTaskCreate(nimble_host_task, "nimble_host", 4096, NULL, 5, NULL);
}

void ble_service_notify_state(const uint8_t *angles, uint8_t count) {
    if (conn_handle != BLE_HS_CONN_HANDLE_NONE && notify_enabled) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(angles, count);
        ble_gatts_notify_custom(conn_handle, servo_state_handle, om);
    }
}

ble_state_t ble_service_get_state(void) {
    return ble_state;
}
