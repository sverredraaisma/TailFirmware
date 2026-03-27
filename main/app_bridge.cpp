/**
 * Bridge between C main.c and C++ subsystems.
 *
 * Owns the global instances of MotionSystem, LedMatrix, LayerCompositor,
 * and ConfigManager. Provides extern "C" functions called from main.c tasks.
 */

#include <cstring>
#include "esp_log.h"

#include "motion/motion_system.h"
#include "led/led_matrix.h"
#include "led/layer_compositor.h"
#include "config/config_manager.h"
#include "ble_service.h"
#include "ble/ble_protocol.h"

static const char *TAG = "bridge";

// Global subsystem instances
static MotionSystem g_motion;
static LedMatrix g_matrix;
static LayerCompositor g_compositor;
static ConfigManager g_config;

// LED strip GPIO (must match main.c define)
#define LED_STRIP_PIN 7

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

void app_update_ble_state(void) {
    if (ble_service_get_state() != BLE_STATE_CONNECTED) return;

    // Update motion state for BLE reads/notifications
    uint8_t motion_buf[MOTION_STATE_SIZE];
    uint8_t *p = motion_buf;

    *p++ = g_config.get_config().motion_pattern.pattern_id;

    for (int axis = 0; axis < 2; axis++) {
        for (int half = 0; half < 2; half++) {
            float pos = g_motion.get_position(axis, half);
            memcpy(p, &pos, 4);
            p += 4;
        }
    }

    vec3_t grav = g_motion.get_gravity();
    memcpy(p, &grav.x, 4); p += 4;
    memcpy(p, &grav.y, 4); p += 4;
    memcpy(p, &grav.z, 4); p += 4;

    ble_set_motion_state(motion_buf, MOTION_STATE_SIZE);

    // Check tap events and notify
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
