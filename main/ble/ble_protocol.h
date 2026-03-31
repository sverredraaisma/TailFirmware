#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

/**
 * BLE Command Protocol
 *
 * All write characteristics use: [command_id: u8] [payload: variable]
 * All state/notify characteristics return structured data.
 *
 * Service UUID: 0000FF00-0000-1000-8000-00805F9B34FB
 */

// Characteristic short UUIDs (last 2 bytes of 128-bit UUID)
#define BLE_CHR_MOTION_CMD      0xFF01  // Write, Write No Rsp
#define BLE_CHR_MOTION_STATE    0xFF02  // Read, Notify
#define BLE_CHR_LED_CMD         0xFF03  // Write, Write No Rsp
#define BLE_CHR_LED_STATE       0xFF04  // Read, Notify
#define BLE_CHR_FFT_STREAM      0xFF05  // Write No Rsp
#define BLE_CHR_SYSTEM_CFG      0xFF06  // Read, Write
#define BLE_CHR_SYSTEM_STATE    0xFF07  // Read, Notify
#define BLE_CHR_PROFILE         0xFF08  // Read, Write

// ── Motion Commands (FF01) ──────────────────────────────────────
#define MCMD_SELECT_PATTERN     0x01  // [pattern_id: u8]
#define MCMD_SET_PATTERN_PARAM  0x02  // [param_id: u8] [value: f32 LE]
#define MCMD_SET_SERVO_CFG      0x03  // [servo_id: u8] [axis: u8] [half: u8] [invert: u8]
#define MCMD_SET_PID            0x04  // [servo_id: u8] [kp: f32] [ki: f32] [kd: f32]
#define MCMD_CALIBRATE_ZERO     0x05  // (no payload)
#define MCMD_SET_AXIS_LIMITS    0x06  // [axis: u8] [min: f32] [max: f32]
#define MCMD_SET_TAP_ENABLE     0x07  // [imu_id: u8] [enabled: u8]

// Pattern IDs
#define PATTERN_STATIC          0x00
#define PATTERN_WAGGING         0x01
#define PATTERN_LOOSE           0x02

// ── Motion State (FF02) ─────────────────────────────────────────
// Read/Notify payload:
//   [pattern_id: u8]                          1 byte
//   [pattern_params: f32 x 8]                32 bytes
//   [encoder0..3: f32 x 4]                   16 bytes
//   [gravity_x: f32] [gravity_y: f32] [gravity_z: f32]  12 bytes
//   [axis0_limit_min: f32] [axis0_limit_max: f32]         8 bytes
//   [axis1_limit_min: f32] [axis1_limit_max: f32]         8 bytes
#define MOTION_STATE_SIZE (1 + 8*4 + 4*4 + 3*4 + 4*4)  // 77 bytes

// ── LED Commands (FF03) ─────────────────────────────────────────
#define LCMD_SET_LAYER          0x01  // [layer: u8] [effect_id: u8] [blend_mode: u8]
#define LCMD_SET_EFFECT_PARAM   0x02  // [layer: u8] [param_id: u8] [value: f32 LE]
#define LCMD_REMOVE_LAYER       0x03  // [layer: u8]
#define LCMD_SET_TRANSFORM      0x04  // [layer: u8] [flip_x: u8] [flip_y: u8] [mirror_x: u8] [mirror_y: u8]
#define LCMD_UPLOAD_IMAGE_CHUNK 0x05  // [offset: u16 LE] [data: up to MTU-3 bytes]
#define LCMD_FINALIZE_IMAGE     0x06  // [width: u8] [height: u8] [target_layer: u8]
#define LCMD_SET_LAYER_ENABLED  0x07  // [layer: u8] [enabled: u8]

// Effect IDs
#define EFFECT_RAINBOW          0x00
#define EFFECT_STATIC_COLOR     0x01
#define EFFECT_IMAGE            0x02
#define EFFECT_AUDIO_POWER      0x03
#define EFFECT_AUDIO_BAR        0x04
#define EFFECT_AUDIO_FREQ_BARS  0x05

// Blend modes (matches BlendMode enum)
#define BLEND_MULTIPLY          0x00
#define BLEND_ADD               0x01
#define BLEND_SUBTRACT          0x02
#define BLEND_MIN               0x03
#define BLEND_MAX               0x04
#define BLEND_OVERWRITE         0x05

// ── LED State (FF04) ────────────────────────────────────────────
// Read/Notify payload:
//   [num_layers: u8]
//   For each layer: [effect_id: u8] [blend_mode: u8] [enabled: u8]

// ── FFT Stream (FF05) ───────────────────────────────────────────
// Write No Rsp payload:
//   [loudness: u8] [num_bins: u8] [bins: u8 * num_bins]

// ── System Config (FF06) ────────────────────────────────────────
#define SCMD_SET_MATRIX_CFG     0x01  // [num_rings: u8] [leds_per_ring: u8 * num_rings]
#define SCMD_GET_SYSTEM_INFO    0x02  // (no payload, returns info on read)

// ── System State (FF07) ─────────────────────────────────────────
// Notify payload on tap: [event: u8] (0x01 = base tap, 0x02 = tip tap)
#define SYS_EVENT_TAP_BASE      0x01
#define SYS_EVENT_TAP_TIP       0x02

// ── Profile (FF08) ──────────────────────────────────────────────
#define PCMD_SAVE_PROFILE       0x01  // [slot: u8]
#define PCMD_LOAD_PROFILE       0x02  // [slot: u8]
#define PCMD_LIST_PROFILES      0x03  // (no payload, returns list on read)
#define PCMD_DELETE_PROFILE     0x04  // [slot: u8]

#define MAX_PROFILE_SLOTS       4

#endif
