#include "config/config_manager.h"
#include "ble/ble_protocol.h"

// Pattern includes
#include "motion/patterns/static_pattern.h"
#include "motion/patterns/wagging_pattern.h"
#include "motion/patterns/loose_pattern.h"

// Effect includes
#include "led/effects/rainbow_effect.h"
#include "led/effects/static_color_effect.h"
#include "led/effects/image_effect.h"
#include "led/effects/audio_power_effect.h"
#include "led/effects/audio_bar_effect.h"
#include "led/effects/audio_freq_bars_effect.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <cstring>
#include <algorithm>

static const char *TAG = "config_mgr";

// Debounce interval for NVS writes: 2 seconds in microseconds
static constexpr int64_t SAVE_DEBOUNCE_US = 2000000;

static float read_f32(const uint8_t *p)
{
    float v;
    memcpy(&v, p, 4);
    return v;
}

static uint16_t read_u16(const uint8_t *p)
{
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}

// ─── Init ────────────────────────────────────────────────────────────

void ConfigManager::init(MotionSystem *motion, LayerCompositor *compositor, LedMatrix *matrix)
{
    motion_ = motion;
    compositor_ = compositor;
    matrix_ = matrix;
    memset(&config_, 0, sizeof(config_));
    image_upload_len_ = 0;
}

// ─── Command dispatch ────────────────────────────────────────────────

void ConfigManager::process_command(uint16_t chr_uuid_short, const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) {
        return;
    }

    switch (chr_uuid_short) {
        case BLE_CHR_MOTION_CMD:
            handle_motion_cmd(data, len);
            break;
        case BLE_CHR_LED_CMD:
            handle_led_cmd(data, len);
            break;
        case BLE_CHR_FFT_STREAM:
            handle_fft_stream(data, len);
            break;
        case BLE_CHR_SYSTEM_CFG:
            handle_system_cfg(data, len);
            break;
        case BLE_CHR_PROFILE:
            handle_profile_cmd(data, len);
            break;
        default:
            ESP_LOGW(TAG, "Unknown characteristic 0x%04X", chr_uuid_short);
            break;
    }
}

// ─── Motion commands ─────────────────────────────────────────────────

void ConfigManager::handle_motion_cmd(const uint8_t *data, uint16_t len)
{
    uint8_t cmd = data[0];

    switch (cmd) {
        case MCMD_SELECT_PATTERN: {
            if (len < 2) return;
            uint8_t pattern_id = data[1];
            auto pattern = create_pattern(pattern_id);
            if (pattern) {
                motion_->set_pattern(std::move(pattern));
                config_.motion_pattern.pattern_id = pattern_id;
                // Reset pattern params in config
                memset(config_.motion_pattern.params, 0, sizeof(config_.motion_pattern.params));
                mark_dirty();
                ESP_LOGI(TAG, "Pattern selected: %u", pattern_id);
            } else {
                ESP_LOGW(TAG, "Unknown pattern ID: %u", pattern_id);
            }
            break;
        }

        case MCMD_SET_PATTERN_PARAM: {
            if (len < 6) return;
            uint8_t param_id = data[1];
            float value = read_f32(&data[2]);
            motion_->set_pattern_param(param_id, value);
            if (param_id < 8) {
                config_.motion_pattern.params[param_id] = value;
            }
            mark_dirty();
            ESP_LOGI(TAG, "Pattern param %u = %.3f", param_id, value);
            break;
        }

        case MCMD_SET_SERVO_CFG: {
            if (len < 5) return;
            uint8_t servo_id = data[1];
            if (servo_id >= MAX_SERVOS) return;
            config_.servos[servo_id].assignment.axis = data[2];
            config_.servos[servo_id].assignment.half = data[3];
            config_.servos[servo_id].assignment.invert = data[4] != 0;
            mark_dirty();
            ESP_LOGI(TAG, "Servo %u cfg: axis=%u half=%u invert=%u",
                     servo_id, data[2], data[3], data[4]);
            break;
        }

        case MCMD_SET_PID: {
            if (len < 14) return;
            uint8_t servo_id = data[1];
            if (servo_id >= MAX_SERVOS) return;
            float kp = read_f32(&data[2]);
            float ki = read_f32(&data[6]);
            float kd = read_f32(&data[10]);
            motion_->set_pid_gains(servo_id, kp, ki, kd);
            config_.servos[servo_id].pid.kp = kp;
            config_.servos[servo_id].pid.ki = ki;
            config_.servos[servo_id].pid.kd = kd;
            mark_dirty();
            ESP_LOGI(TAG, "Servo %u PID: kp=%.3f ki=%.3f kd=%.3f",
                     servo_id, kp, ki, kd);
            break;
        }

        case MCMD_CALIBRATE_ZERO: {
            motion_->calibrate_zero();
            ESP_LOGI(TAG, "Calibrate zero");
            break;
        }

        case MCMD_SET_AXIS_LIMITS: {
            if (len < 9) return;
            uint8_t axis = data[1];
            if (axis >= MAX_AXES) return;
            float min_deg = read_f32(&data[2]);
            float max_deg = read_f32(&data[6]);
            motion_->set_axis_limits(axis, min_deg, max_deg);
            config_.axes[axis].limit_min = min_deg;
            config_.axes[axis].limit_max = max_deg;
            mark_dirty();
            ESP_LOGI(TAG, "Axis %u limits: [%.1f, %.1f]", axis, min_deg, max_deg);
            break;
        }

        case MCMD_SET_TAP_ENABLE: {
            if (len < 3) return;
            uint8_t imu_id = data[1];
            if (imu_id >= MAX_IMU) return;
            bool enabled = data[2] != 0;
            config_.imus[imu_id].tap_enabled = enabled;
            mark_dirty();
            ESP_LOGI(TAG, "IMU %u tap %s", imu_id, enabled ? "enabled" : "disabled");
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown motion cmd: 0x%02X", cmd);
            break;
    }
}

// ─── LED commands ────────────────────────────────────────────────────

void ConfigManager::handle_led_cmd(const uint8_t *data, uint16_t len)
{
    uint8_t cmd = data[0];

    switch (cmd) {
        case LCMD_SET_LAYER: {
            if (len < 4) return;
            uint8_t layer = data[1];
            if (layer >= MAX_LED_LAYERS) return;
            uint8_t effect_id = data[2];
            uint8_t blend_mode = data[3];

            auto effect = create_effect(effect_id);
            if (effect) {
                BlendMode mode = static_cast<BlendMode>(blend_mode);
                compositor_->set_layer(layer, std::move(effect), mode);
                config_.layers[layer].effect_id = effect_id;
                config_.layers[layer].blend_mode = blend_mode;
                config_.layers[layer].enabled = true;
                // Reset layer params
                memset(config_.layers[layer].params, 0, sizeof(config_.layers[layer].params));
                if (layer >= config_.num_layers) {
                    config_.num_layers = layer + 1;
                }
                mark_dirty();
                ESP_LOGI(TAG, "Layer %u: effect=%u blend=%u", layer, effect_id, blend_mode);
            } else {
                ESP_LOGW(TAG, "Unknown effect ID: %u", effect_id);
            }
            break;
        }

        case LCMD_SET_EFFECT_PARAM: {
            if (len < 7) return;
            uint8_t layer = data[1];
            if (layer >= MAX_LED_LAYERS) return;
            uint8_t param_id = data[2];
            float value = read_f32(&data[3]);

            Layer *l = compositor_->get_layer(layer);
            if (l && l->effect) {
                l->effect->set_param(param_id, value);
            }
            if (param_id < 8) {
                config_.layers[layer].params[param_id] = value;
            }
            mark_dirty();
            ESP_LOGI(TAG, "Layer %u param %u = %.3f", layer, param_id, value);
            break;
        }

        case LCMD_REMOVE_LAYER: {
            if (len < 2) return;
            uint8_t layer = data[1];
            if (layer >= MAX_LED_LAYERS) return;
            compositor_->remove_layer(layer);
            config_.layers[layer].effect_id = 0xFF; // 0xFF = no effect (invalid ID)
            config_.layers[layer].blend_mode = 0;
            config_.layers[layer].enabled = false;
            memset(config_.layers[layer].params, 0, sizeof(config_.layers[layer].params));
            mark_dirty();
            ESP_LOGI(TAG, "Layer %u removed", layer);
            break;
        }

        case LCMD_SET_TRANSFORM: {
            if (len < 6) return;
            uint8_t layer = data[1];
            if (layer >= MAX_LED_LAYERS) return;
            bool flip_x = data[2] != 0;
            bool flip_y = data[3] != 0;
            bool mirror_x = data[4] != 0;
            bool mirror_y = data[5] != 0;

            Layer *l = compositor_->get_layer(layer);
            if (l && l->effect) {
                l->effect->flip_x = flip_x;
                l->effect->flip_y = flip_y;
                l->effect->mirror_x = mirror_x;
                l->effect->mirror_y = mirror_y;
            }
            config_.layers[layer].flip_x = flip_x;
            config_.layers[layer].flip_y = flip_y;
            config_.layers[layer].mirror_x = mirror_x;
            config_.layers[layer].mirror_y = mirror_y;
            mark_dirty();
            ESP_LOGI(TAG, "Layer %u transform: fx=%d fy=%d mx=%d my=%d",
                     layer, flip_x, flip_y, mirror_x, mirror_y);
            break;
        }

        case LCMD_UPLOAD_IMAGE_CHUNK: {
            if (len < 3) return;
            // [cmd: u8] [offset: u16 LE] [data...]
            uint16_t offset = read_u16(&data[1]);
            uint16_t chunk_len = len - 3;
            if (offset + chunk_len > IMAGE_UPLOAD_MAX) {
                ESP_LOGW(TAG, "Image chunk overflow: offset=%u len=%u", offset, chunk_len);
                return;
            }
            memcpy(&image_upload_buf_[offset], &data[3], chunk_len);
            if (offset + chunk_len > image_upload_len_) {
                image_upload_len_ = offset + chunk_len;
            }
            ESP_LOGD(TAG, "Image chunk: offset=%u len=%u total=%u", offset, chunk_len, image_upload_len_);
            break;
        }

        case LCMD_FINALIZE_IMAGE: {
            if (len < 4) return;
            uint8_t width = data[1];
            uint8_t height = data[2];
            uint8_t target_layer = data[3];
            if (target_layer >= MAX_LED_LAYERS) return;

            uint16_t expected_size = (uint16_t)width * height * 3;
            if (expected_size > image_upload_len_ || expected_size == 0) {
                ESP_LOGW(TAG, "Image finalize: size mismatch (expected %u, have %u)",
                         expected_size, image_upload_len_);
                return;
            }

            // Check if the layer already has an ImageEffect; if so, update it in place
            Layer *l = compositor_->get_layer(target_layer);
            ImageEffect *img = nullptr;
            if (l && l->effect && config_.layers[target_layer].effect_id == EFFECT_IMAGE) {
                img = static_cast<ImageEffect *>(l->effect.get());
            }

            if (img) {
                // Update existing image effect
                img->set_image(image_upload_buf_, width, height);
            } else {
                // Create a new image effect
                auto effect = std::make_unique<ImageEffect>();
                effect->set_image(image_upload_buf_, width, height);
                compositor_->set_layer(target_layer, std::move(effect), BlendMode::Overwrite);
                config_.layers[target_layer].effect_id = EFFECT_IMAGE;
                config_.layers[target_layer].blend_mode = BLEND_OVERWRITE;
                config_.layers[target_layer].enabled = true;
                if (target_layer >= config_.num_layers) {
                    config_.num_layers = target_layer + 1;
                }
            }

            image_upload_len_ = 0;
            mark_dirty();
            ESP_LOGI(TAG, "Image finalized: %ux%u -> layer %u", width, height, target_layer);
            break;
        }

        case LCMD_SET_LAYER_ENABLED: {
            if (len < 3) return;
            uint8_t layer = data[1];
            if (layer >= MAX_LED_LAYERS) return;
            bool enabled = data[2] != 0;
            compositor_->set_layer_enabled(layer, enabled);
            config_.layers[layer].enabled = enabled;
            mark_dirty();
            ESP_LOGI(TAG, "Layer %u %s", layer, enabled ? "enabled" : "disabled");
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown LED cmd: 0x%02X", cmd);
            break;
    }
}

// ─── FFT stream ──────────────────────────────────────────────────────

void ConfigManager::handle_fft_stream(const uint8_t *data, uint16_t len)
{
    if (len < 2) return;
    uint8_t loudness = data[0];
    uint8_t num_bins = data[1];
    const uint8_t *bins = (len > 2) ? &data[2] : nullptr;
    FftBuffer::instance().write(loudness, num_bins, bins);
}

// ─── System config ───────────────────────────────────────────────────

void ConfigManager::handle_system_cfg(const uint8_t *data, uint16_t len)
{
    uint8_t cmd = data[0];

    switch (cmd) {
        case SCMD_SET_MATRIX_CFG: {
            if (len < 2) return;
            uint8_t num_rings = data[1];
            if (num_rings > MAX_LED_RINGS) num_rings = MAX_LED_RINGS;
            if (len < 2u + num_rings) return;
            const uint8_t *leds_per_ring = &data[2];
            matrix_->configure(num_rings, leds_per_ring);
            config_.led_matrix.num_rings = num_rings;
            memcpy(config_.led_matrix.leds_per_ring, leds_per_ring, num_rings);
            mark_dirty();
            ESP_LOGI(TAG, "Matrix configured: %u rings", num_rings);
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown system cfg cmd: 0x%02X", cmd);
            break;
    }
}

// ─── Profile commands ────────────────────────────────────────────────

void ConfigManager::handle_profile_cmd(const uint8_t *data, uint16_t len)
{
    uint8_t cmd = data[0];

    switch (cmd) {
        case PCMD_SAVE_PROFILE: {
            if (len < 2) return;
            uint8_t slot = data[1];
            if (slot >= MAX_PROFILE_SLOTS) return;
            char ns[16];
            snprintf(ns, sizeof(ns), "tail_prof%u", slot);
            nvs_save_config(ns, config_);
            ESP_LOGI(TAG, "Profile saved to slot %u", slot);
            break;
        }

        case PCMD_LOAD_PROFILE: {
            if (len < 2) return;
            uint8_t slot = data[1];
            if (slot >= MAX_PROFILE_SLOTS) return;
            char ns[16];
            snprintf(ns, sizeof(ns), "tail_prof%u", slot);
            system_config_t loaded = {};
            if (nvs_load_config(ns, loaded)) {
                config_ = loaded;
                apply_config();
                ESP_LOGI(TAG, "Profile loaded from slot %u", slot);
            } else {
                ESP_LOGW(TAG, "Profile slot %u not found or invalid", slot);
            }
            break;
        }

        case PCMD_DELETE_PROFILE: {
            if (len < 2) return;
            uint8_t slot = data[1];
            if (slot >= MAX_PROFILE_SLOTS) return;
            char ns[16];
            snprintf(ns, sizeof(ns), "tail_prof%u", slot);
            nvs_handle_t handle;
            esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
            if (err == ESP_OK) {
                nvs_erase_all(handle);
                nvs_commit(handle);
                nvs_close(handle);
                ESP_LOGI(TAG, "Profile slot %u deleted", slot);
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown profile cmd: 0x%02X", cmd);
            break;
    }
}

// ─── Pattern factory ─────────────────────────────────────────────────

std::unique_ptr<MotionPattern> ConfigManager::create_pattern(uint8_t pattern_id)
{
    switch (pattern_id) {
        case PATTERN_STATIC:  return std::make_unique<StaticPattern>();
        case PATTERN_WAGGING: return std::make_unique<WaggingPattern>();
        case PATTERN_LOOSE:   return std::make_unique<LoosePattern>();
        default:              return nullptr;
    }
}

// ─── Effect factory ──────────────────────────────────────────────────

std::unique_ptr<LedEffect> ConfigManager::create_effect(uint8_t effect_id)
{
    switch (effect_id) {
        case EFFECT_RAINBOW:         return std::make_unique<RainbowEffect>();
        case EFFECT_STATIC_COLOR:    return std::make_unique<StaticColorEffect>();
        case EFFECT_IMAGE:           return std::make_unique<ImageEffect>();
        case EFFECT_AUDIO_POWER:     return std::make_unique<AudioPowerEffect>();
        case EFFECT_AUDIO_BAR:       return std::make_unique<AudioBarEffect>();
        case EFFECT_AUDIO_FREQ_BARS: return std::make_unique<AudioFreqBarsEffect>();
        default:                     return nullptr;
    }
}

// ─── NVS persistence ─────────────────────────────────────────────────

void ConfigManager::nvs_save_config(const char *ns, const system_config_t &cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open '%s' for write failed: %s", ns, esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(handle, "cfg", &cfg, sizeof(cfg));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS set_blob failed: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Config saved to NVS '%s' (%u bytes)", ns, (unsigned)sizeof(cfg));
        }
    }

    nvs_close(handle);
}

bool ConfigManager::nvs_load_config(const char *ns, system_config_t &cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS open '%s' for read failed: %s", ns, esp_err_to_name(err));
        return false;
    }

    size_t required_size = 0;
    err = nvs_get_blob(handle, "cfg", nullptr, &required_size);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS get_blob size failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    // Validate size matches current struct (reject if different to avoid corruption)
    if (required_size != sizeof(system_config_t)) {
        ESP_LOGW(TAG, "NVS config size mismatch: stored=%u expected=%u",
                 (unsigned)required_size, (unsigned)sizeof(system_config_t));
        nvs_close(handle);
        return false;
    }

    size_t read_size = sizeof(system_config_t);
    err = nvs_get_blob(handle, "cfg", &cfg, &read_size);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS get_blob read failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Config loaded from NVS '%s' (%u bytes)", ns, (unsigned)read_size);
    return true;
}

// ─── Save / Load ─────────────────────────────────────────────────────

void ConfigManager::mark_dirty()
{
    dirty_ = true;
    last_change_time_ = esp_timer_get_time();
}

void ConfigManager::save_pending()
{
    if (!dirty_) return;

    int64_t now = esp_timer_get_time();
    if ((now - last_change_time_) >= SAVE_DEBOUNCE_US) {
        nvs_save_config("tail_cfg", config_);
        dirty_ = false;
    }
}

void ConfigManager::save_now()
{
    nvs_save_config("tail_cfg", config_);
    dirty_ = false;
}

void ConfigManager::load()
{
    if (!nvs_load_config("tail_cfg", config_)) {
        ESP_LOGI(TAG, "No saved config found, using defaults");
        memset(&config_, 0, sizeof(config_));
    }
}

// ─── Apply loaded config to subsystems ───────────────────────────────

void ConfigManager::apply_config()
{
    // Apply LED matrix configuration
    if (config_.led_matrix.num_rings > 0 && matrix_) {
        matrix_->configure(config_.led_matrix.num_rings, config_.led_matrix.leds_per_ring);
    }

    // Apply motion pattern
    if (motion_) {
        auto pattern = create_pattern(config_.motion_pattern.pattern_id);
        if (pattern) {
            // Restore pattern params
            for (int i = 0; i < 8; i++) {
                pattern->set_param(i, config_.motion_pattern.params[i]);
            }
            motion_->set_pattern(std::move(pattern));
        }

        // Apply PID gains
        for (int s = 0; s < MAX_SERVOS; s++) {
            const pid_params_t &pid = config_.servos[s].pid;
            if (pid.kp != 0.0f || pid.ki != 0.0f || pid.kd != 0.0f) {
                motion_->set_pid_gains(s, pid.kp, pid.ki, pid.kd);
            }
        }

        // Apply axis limits
        for (int a = 0; a < MAX_AXES; a++) {
            const axis_config_t &ax = config_.axes[a];
            if (ax.limit_min != 0.0f || ax.limit_max != 0.0f) {
                motion_->set_axis_limits(a, ax.limit_min, ax.limit_max);
            }
        }
    }

    // Apply LED layers
    if (compositor_) {
        for (int i = 0; i < config_.num_layers && i < MAX_LED_LAYERS; i++) {
            const layer_config_t &lc = config_.layers[i];
            auto effect = create_effect(lc.effect_id);
            if (effect) {
                // Restore effect params
                for (int p = 0; p < 8; p++) {
                    effect->set_param(p, lc.params[p]);
                }
                // Restore transforms
                effect->flip_x = lc.flip_x;
                effect->flip_y = lc.flip_y;
                effect->mirror_x = lc.mirror_x;
                effect->mirror_y = lc.mirror_y;

                BlendMode mode = static_cast<BlendMode>(lc.blend_mode);
                compositor_->set_layer(i, std::move(effect), mode);
                compositor_->set_layer_enabled(i, lc.enabled);
            }
        }
    }

    ESP_LOGI(TAG, "Config applied to all subsystems");
}
