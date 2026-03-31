/**
 * Bridge between C main.c and C++ subsystems.
 *
 * Owns the global instances of MotionSystem, LedMatrix, LayerCompositor,
 * and ConfigManager. Provides extern "C" functions called from main.c tasks.
 */

#include <cstring>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "motion/motion_system.h"
#include "led/led_matrix.h"
#include "led/layer_compositor.h"
#include "config/config_manager.h"
#include "ble_service.h"
#include "ble/ble_protocol.h"
#include "config/pin_config.h"

static const char *TAG = "bridge";

// Global subsystem instances
static MotionSystem g_motion;
static LedMatrix g_matrix;
static LayerCompositor g_compositor;
static ConfigManager g_config;

extern "C" {

void app_init_subsystems(i2c_mux_t *mux) {
    // Load config from NVS (or defaults)
    g_config.init(&g_motion, &g_compositor, &g_matrix);
    g_config.load();

    const system_config_t &cfg = g_config.get_config();

    // Initialize motion system with loaded config
    g_motion.init(mux, cfg);

    // Initialize LED matrix from config
    if (cfg.led_matrix.num_rings > 0) {
        g_matrix.configure(cfg.led_matrix.num_rings, cfg.led_matrix.leds_per_ring);
        g_matrix.init_strip(LED_STRIP_PIN);
    } else {
        // Default: no LEDs configured yet
        ESP_LOGW(TAG, "No LED matrix configured");
    }

    // Initialize layer compositor
    g_compositor.init(MAX_LED_LAYERS);

    // Restore LED layers, motion pattern, PID, and axis limits from loaded config.
    // Must run after compositor and matrix are initialized.
    g_config.apply_config();

    ESP_LOGI(TAG, "Subsystems initialized");
}

void app_motion_update(float dt) {
    g_motion.update(dt);
}

void app_led_render(float dt) {
    if (g_matrix.get_led_count() == 0) return;

    g_compositor.render(g_matrix, dt);
    g_matrix.push();
}

void app_process_ble_command(uint16_t chr_uuid_short, const uint8_t *data, uint16_t len) {
    g_config.process_command(chr_uuid_short, data, len);
}

void app_config_save_pending(void) {
    g_config.save_pending();
}

static void write_f32(uint8_t *&p, float v) {
    memcpy(p, &v, 4);
    p += 4;
}

void app_update_ble_state(void) {
    if (ble_service_get_state() != BLE_STATE_CONNECTED) return;

    const system_config_t &cfg = g_config.get_config();

    // ── FF02: Motion State ──────────────────────────────────
    // [pattern_id: u8]
    // [pattern_params: 8 x f32]
    // [encoder0..3: 4 x f32]
    // [gravity_xyz: 3 x f32]
    // [axis0_limits: 2 x f32] [axis1_limits: 2 x f32]
    // Total: 1 + 32 + 16 + 12 + 16 = 77 bytes
    {
        uint8_t buf[128];
        uint8_t *p = buf;

        *p++ = cfg.motion_pattern.pattern_id;

        // Pattern parameters (so app knows current values)
        for (int i = 0; i < 8; i++) {
            write_f32(p, cfg.motion_pattern.params[i]);
        }

        // Encoder positions
        for (int axis = 0; axis < 2; axis++) {
            for (int half = 0; half < 2; half++) {
                write_f32(p, g_motion.get_position(axis, half));
            }
        }

        // Gravity vector
        vec3_t grav = g_motion.get_gravity();
        write_f32(p, grav.x);
        write_f32(p, grav.y);
        write_f32(p, grav.z);

        // Axis limits
        for (int a = 0; a < MAX_AXES; a++) {
            write_f32(p, cfg.axes[a].limit_min);
            write_f32(p, cfg.axes[a].limit_max);
        }

        ble_set_motion_state(buf, (uint16_t)(p - buf));
    }

    // ── FF04: LED State ─────────────────────────────────────
    // [num_rings: u8] [leds_per_ring: u8 * num_rings]
    // [num_layers: u8]
    // For each layer:
    //   [effect_id: u8] [blend_mode: u8] [enabled: u8]
    //   [flip_x: u8] [flip_y: u8] [mirror_x: u8] [mirror_y: u8]
    //   [params: 8 x f32]
    // Per layer: 7 + 32 = 39 bytes. Worst case: 1 + 20 + 1 + 8*39 = 334 bytes.
    {
        uint8_t buf[400];
        uint8_t *p = buf;

        // Matrix config
        *p++ = cfg.led_matrix.num_rings;
        uint8_t nr = cfg.led_matrix.num_rings;
        if (nr > MAX_LED_RINGS) nr = MAX_LED_RINGS;
        for (int i = 0; i < nr; i++) {
            *p++ = cfg.led_matrix.leds_per_ring[i];
        }

        // Layers
        *p++ = cfg.num_layers;
        for (int i = 0; i < cfg.num_layers && i < MAX_LED_LAYERS; i++) {
            const layer_config_t &lc = cfg.layers[i];

            *p++ = lc.effect_id;
            *p++ = lc.blend_mode;
            *p++ = lc.enabled ? 1 : 0;
            *p++ = lc.flip_x ? 1 : 0;
            *p++ = lc.flip_y ? 1 : 0;
            *p++ = lc.mirror_x ? 1 : 0;
            *p++ = lc.mirror_y ? 1 : 0;

            for (int j = 0; j < 8; j++) {
                write_f32(p, lc.params[j]);
            }
        }

        ble_set_led_state(buf, (uint16_t)(p - buf));
    }

    // ── FF06: System Info ───────────────────────────────────
    // [firmware_version: 3 x u8]
    // [num_servos: u8]
    // Per servo: [axis: u8] [half: u8] [invert: u8] [mux_ch: u8] [kp: f32] [ki: f32] [kd: f32]
    // Per servo: 4 + 12 = 16 bytes. 4 servos = 64 + 4 = 68 bytes
    // [num_imus: u8]
    // Per IMU: [mux_ch: u8] [tap_enabled: u8]
    {
        uint8_t buf[128];
        uint8_t *p = buf;

        // Firmware version
        *p++ = 1; // major
        *p++ = 0; // minor
        *p++ = 0; // patch

        // Servo configs
        *p++ = MAX_SERVOS;
        for (int i = 0; i < MAX_SERVOS; i++) {
            const servo_config_t &sc = cfg.servos[i];
            *p++ = sc.assignment.axis;
            *p++ = sc.assignment.half;
            *p++ = sc.assignment.invert ? 1 : 0;
            *p++ = sc.assignment.mux_channel;
            write_f32(p, sc.pid.kp);
            write_f32(p, sc.pid.ki);
            write_f32(p, sc.pid.kd);
        }

        // IMU configs
        *p++ = MAX_IMU;
        for (int i = 0; i < MAX_IMU; i++) {
            *p++ = cfg.imus[i].mux_channel;
            *p++ = cfg.imus[i].tap_enabled ? 1 : 0;
        }

        ble_set_system_info(buf, (uint16_t)(p - buf));
    }

    // ── FF08: Profile list ──────────────────────────────────
    // [slot_occupied: u8] per slot (4 bytes total, 1 = occupied, 0 = empty)
    {
        uint8_t buf[MAX_PROFILE_SLOTS];
        for (int i = 0; i < MAX_PROFILE_SLOTS; i++) {
            char ns[16];
            snprintf(ns, sizeof(ns), "tail_prof%d", i);
            nvs_handle_t handle;
            if (nvs_open(ns, NVS_READONLY, &handle) == ESP_OK) {
                size_t sz = 0;
                esp_err_t err = nvs_get_blob(handle, "cfg", nullptr, &sz);
                buf[i] = (err == ESP_OK && sz == sizeof(system_config_t)) ? 1 : 0;
                nvs_close(handle);
            } else {
                buf[i] = 0;
            }
        }
        ble_set_profile_list(buf, MAX_PROFILE_SLOTS);
    }
}

void app_notify_tap_events(void) {
    if (ble_service_get_state() != BLE_STATE_CONNECTED) return;
    if (g_motion.check_tap_base()) {
        uint8_t evt = SYS_EVENT_TAP_BASE;
        ble_notify_system_event(&evt, 1);
    }
    if (g_motion.check_tap_tip()) {
        uint8_t evt = SYS_EVENT_TAP_TIP;
        ble_notify_system_event(&evt, 1);
    }
}

} // extern "C"
