#include "ble_service.h"
#include "ble/ble_protocol.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble";

// ── Max buffer size for cached state data ────────────────────────
#define STATE_BUF_MAX 256

// ── UUID definitions (little-endian byte order for NimBLE) ───────
// Service: 0000FF00-0000-1000-8000-00805F9B34FB
static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00);

// FF01: Motion Command
static const ble_uuid128_t motion_cmd_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00);

// FF02: Motion State
static const ble_uuid128_t motion_state_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x02, 0xFF, 0x00, 0x00);

// FF03: LED Command
static const ble_uuid128_t led_cmd_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00);

// FF04: LED State
static const ble_uuid128_t led_state_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x04, 0xFF, 0x00, 0x00);

// FF05: FFT Stream
static const ble_uuid128_t fft_stream_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x05, 0xFF, 0x00, 0x00);

// FF06: System Config
static const ble_uuid128_t sys_cfg_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x06, 0xFF, 0x00, 0x00);

// FF07: System State
static const ble_uuid128_t sys_state_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x07, 0xFF, 0x00, 0x00);

// FF08: Profile
static const ble_uuid128_t profile_uuid =
    BLE_UUID128_INIT(0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x08, 0xFF, 0x00, 0x00);

// ── Value handles (filled by NimBLE at registration time) ────────
static uint16_t motion_cmd_handle;
static uint16_t motion_state_handle;
static uint16_t led_cmd_handle;
static uint16_t led_state_handle;
static uint16_t fft_stream_handle;
static uint16_t sys_cfg_handle;
static uint16_t sys_state_handle;
static uint16_t profile_handle;

// ── Connection and state tracking ────────────────────────────────
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint8_t own_addr_type;
static ble_cmd_callback_t cmd_callback = NULL;
static ble_state_t ble_state = BLE_STATE_OFF;

// ── Notification subscription flags ──────────────────────────────
static bool notify_motion_state = false;
static bool notify_led_state = false;
static bool notify_sys_state = false;

// ── Cached read buffers (mutex-protected) ────────────────────────
static SemaphoreHandle_t state_mutex;

static uint8_t motion_state_buf[STATE_BUF_MAX];
static uint16_t motion_state_len = 0;

static uint8_t led_state_buf[STATE_BUF_MAX];
static uint16_t led_state_len = 0;

static uint8_t sys_info_buf[STATE_BUF_MAX];
static uint16_t sys_info_len = 0;

static uint8_t profile_list_buf[STATE_BUF_MAX];
static uint16_t profile_list_len = 0;

// ── Forward declarations ─────────────────────────────────────────
static int gap_event_handler(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

// ── Helper: dispatch a write to the command callback ─────────────
static void dispatch_write(uint16_t chr_uuid_short,
                           struct ble_gatt_access_ctxt *ctxt) {
    if (!cmd_callback) return;

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len == 0) return;

    uint8_t buf[256];
    uint16_t copy_len = len < sizeof(buf) ? len : sizeof(buf);
    os_mbuf_copydata(ctxt->om, 0, copy_len, buf);

    cmd_callback(chr_uuid_short, buf, copy_len);
}

// ── Helper: return a cached buffer on read ───────────────────────
static int read_cached(struct ble_gatt_access_ctxt *ctxt,
                       const uint8_t *buf, uint16_t len) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    if (len > 0) {
        os_mbuf_append(ctxt->om, buf, len);
    }
    xSemaphoreGive(state_mutex);
    return 0;
}

// ── GATT access: FF01 Motion Command (write only) ───────────────
static int motion_cmd_access(uint16_t conn_h, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        dispatch_write(BLE_CHR_MOTION_CMD, ctxt);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ── GATT access: FF02 Motion State (read + notify) ──────────────
static int motion_state_access(uint16_t conn_h, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return read_cached(ctxt, motion_state_buf, motion_state_len);
}

// ── GATT access: FF03 LED Command (write only) ──────────────────
static int led_cmd_access(uint16_t conn_h, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        dispatch_write(BLE_CHR_LED_CMD, ctxt);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ── GATT access: FF04 LED State (read + notify) ─────────────────
static int led_state_access(uint16_t conn_h, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return read_cached(ctxt, led_state_buf, led_state_len);
}

// ── GATT access: FF05 FFT Stream (write no rsp only) ────────────
static int fft_stream_access(uint16_t conn_h, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        dispatch_write(BLE_CHR_FFT_STREAM, ctxt);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ── GATT access: FF06 System Config (read + write) ──────────────
static int sys_cfg_access(uint16_t conn_h, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return read_cached(ctxt, sys_info_buf, sys_info_len);
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        dispatch_write(BLE_CHR_SYSTEM_CFG, ctxt);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ── GATT access: FF07 System State (read + notify) ──────────────
static int sys_state_access(uint16_t conn_h, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // System state notify-only data has no persistent read buffer;
    // return empty on read (events are transient).
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ── GATT access: FF08 Profile (read + write) ────────────────────
static int profile_access(uint16_t conn_h, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return read_cached(ctxt, profile_list_buf, profile_list_len);
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        dispatch_write(BLE_CHR_PROFILE, ctxt);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ── GATT service definition table ────────────────────────────────
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            // FF01: Motion Command - Write, Write No Rsp
            {
                .uuid = &motion_cmd_uuid.u,
                .access_cb = motion_cmd_access,
                .val_handle = &motion_cmd_handle,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            // FF02: Motion State - Read, Notify
            {
                .uuid = &motion_state_uuid.u,
                .access_cb = motion_state_access,
                .val_handle = &motion_state_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            // FF03: LED Command - Write, Write No Rsp
            {
                .uuid = &led_cmd_uuid.u,
                .access_cb = led_cmd_access,
                .val_handle = &led_cmd_handle,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            // FF04: LED State - Read, Notify
            {
                .uuid = &led_state_uuid.u,
                .access_cb = led_state_access,
                .val_handle = &led_state_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            // FF05: FFT Stream - Write No Rsp
            {
                .uuid = &fft_stream_uuid.u,
                .access_cb = fft_stream_access,
                .val_handle = &fft_stream_handle,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            // FF06: System Config - Read, Write
            {
                .uuid = &sys_cfg_uuid.u,
                .access_cb = sys_cfg_access,
                .val_handle = &sys_cfg_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            // FF07: System State - Read, Notify
            {
                .uuid = &sys_state_uuid.u,
                .access_cb = sys_state_access,
                .val_handle = &sys_state_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            // FF08: Profile - Read, Write
            {
                .uuid = &profile_uuid.u,
                .access_cb = profile_access,
                .val_handle = &profile_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            { 0 },
        },
    },
    { 0 },
};

// ── GATT service init ────────────────────────────────────────────
static int gatt_svc_init(void) {
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) return rc;

    rc = ble_gatts_add_svcs(gatt_svcs);
    return rc;
}

// ── Advertising ──────────────────────────────────────────────────
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

// ── GAP event handler ────────────────────────────────────────────
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                conn_handle = event->connect.conn_handle;
                ble_state = BLE_STATE_CONNECTED;
                ESP_LOGI(TAG, "Connected (handle=%d)", conn_handle);
            } else {
                ESP_LOGI(TAG, "Connection failed: %d", event->connect.status);
                start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            conn_handle = BLE_HS_CONN_HANDLE_NONE;
            notify_motion_state = false;
            notify_led_state = false;
            notify_sys_state = false;
            ble_state = BLE_STATE_ADVERTISING;
            ESP_LOGI(TAG, "Disconnected");
            start_advertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE: {
            uint16_t attr = event->subscribe.attr_handle;
            bool cur = event->subscribe.cur_notify;

            if (attr == motion_state_handle) {
                notify_motion_state = cur;
                ESP_LOGI(TAG, "Motion state notify %s", cur ? "on" : "off");
            } else if (attr == led_state_handle) {
                notify_led_state = cur;
                ESP_LOGI(TAG, "LED state notify %s", cur ? "on" : "off");
            } else if (attr == sys_state_handle) {
                notify_sys_state = cur;
                ESP_LOGI(TAG, "System state notify %s", cur ? "on" : "off");
            }
            break;
        }

        case BLE_GAP_EVENT_ADV_COMPLETE:
            start_advertising();
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated: %d", event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

// ── NimBLE host callbacks ────────────────────────────────────────
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

// ── Public API ───────────────────────────────────────────────────

void ble_service_init(ble_cmd_callback_t callback) {
    cmd_callback = callback;

    state_mutex = xSemaphoreCreateMutex();
    assert(state_mutex != NULL);

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

ble_state_t ble_service_get_state(void) {
    return ble_state;
}

// ── Notify helpers ───────────────────────────────────────────────

static void send_notify(uint16_t handle, bool subscribed,
                        const uint8_t *data, uint16_t len) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE || !subscribed) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om) {
        ble_gatts_notify_custom(conn_handle, handle, om);
    }
}

void ble_notify_motion_state(const uint8_t *data, uint16_t len) {
    send_notify(motion_state_handle, notify_motion_state, data, len);
}

void ble_notify_led_state(const uint8_t *data, uint16_t len) {
    send_notify(led_state_handle, notify_led_state, data, len);
}

void ble_notify_system_event(const uint8_t *data, uint16_t len) {
    send_notify(sys_state_handle, notify_sys_state, data, len);
}

// ── State buffer setters (called from config_manager task) ───────

void ble_set_motion_state(const uint8_t *data, uint16_t len) {
    if (len > STATE_BUF_MAX) len = STATE_BUF_MAX;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    memcpy(motion_state_buf, data, len);
    motion_state_len = len;
    xSemaphoreGive(state_mutex);
}

void ble_set_led_state(const uint8_t *data, uint16_t len) {
    if (len > STATE_BUF_MAX) len = STATE_BUF_MAX;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    memcpy(led_state_buf, data, len);
    led_state_len = len;
    xSemaphoreGive(state_mutex);
}

void ble_set_system_info(const uint8_t *data, uint16_t len) {
    if (len > STATE_BUF_MAX) len = STATE_BUF_MAX;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    memcpy(sys_info_buf, data, len);
    sys_info_len = len;
    xSemaphoreGive(state_mutex);
}

void ble_set_profile_list(const uint8_t *data, uint16_t len) {
    if (len > STATE_BUF_MAX) len = STATE_BUF_MAX;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    memcpy(profile_list_buf, data, len);
    profile_list_len = len;
    xSemaphoreGive(state_mutex);
}
