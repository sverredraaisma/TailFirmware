#pragma once

#include "config/config_types.h"
#include "motion/motion_system.h"
#include "led/layer_compositor.h"
#include "led/led_matrix.h"
#include "config/fft_buffer.h"

#include <memory>
#include <cstdint>

class MotionPattern;
class LedEffect;

class ConfigManager {
public:
    void init(MotionSystem *motion, LayerCompositor *compositor, LedMatrix *matrix);

    /// Process a BLE command. Called from BLE task via callback.
    /// chr_uuid_short: 0xFF01, 0xFF03, 0xFF05, 0xFF06, 0xFF08
    /// data: full packet including command_id byte
    /// len: packet length
    void process_command(uint16_t chr_uuid_short, const uint8_t *data, uint16_t len);

    /// Save current config to NVS (debounced - call frequently, writes after 2s idle).
    void save_pending();

    /// Force save now.
    void save_now();

    /// Load config from NVS. Call once at startup.
    void load();

    /// Get current system config (read-only).
    const system_config_t &get_config() const { return config_; }

    /// Get mutable config for initialization.
    system_config_t &get_config_mut() { return config_; }

private:
    MotionSystem *motion_ = nullptr;
    LayerCompositor *compositor_ = nullptr;
    LedMatrix *matrix_ = nullptr;
    system_config_t config_ = {};

    int64_t last_change_time_ = 0;
    bool dirty_ = false;

    // Image upload staging buffer (max 32x32 RGB = 3072 bytes)
    static constexpr size_t IMAGE_UPLOAD_MAX = 3072;
    uint8_t image_upload_buf_[IMAGE_UPLOAD_MAX] = {};
    uint16_t image_upload_len_ = 0;

    // Command handlers
    void handle_motion_cmd(const uint8_t *data, uint16_t len);
    void handle_led_cmd(const uint8_t *data, uint16_t len);
    void handle_fft_stream(const uint8_t *data, uint16_t len);
    void handle_system_cfg(const uint8_t *data, uint16_t len);
    void handle_profile_cmd(const uint8_t *data, uint16_t len);

    // Pattern factory
    std::unique_ptr<MotionPattern> create_pattern(uint8_t pattern_id);

    // Effect factory
    std::unique_ptr<LedEffect> create_effect(uint8_t effect_id);

    // NVS helpers
    void nvs_save_config(const char *ns, const system_config_t &cfg);
    bool nvs_load_config(const char *ns, system_config_t &cfg);

    void mark_dirty();

    // Apply loaded config to all subsystems
    void apply_config();
};
