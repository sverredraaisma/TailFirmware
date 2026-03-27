#pragma once

#include "config/config_types.h"

class FftBuffer {
public:
    /// Write a new FFT frame (called from BLE task).
    void write(uint8_t loudness, uint8_t num_bins, const uint8_t *bins);

    /// Get current loudness (0-255). Returns 0 if stale (>200ms).
    uint8_t get_loudness() const;

    /// Get normalized loudness (0.0-1.0). Returns 0 if stale.
    float get_loudness_normalized() const;

    /// Get a frequency bin value (0-255). Returns 0 if out of range or stale.
    uint8_t get_bin(uint8_t index) const;

    /// Get number of bins in current frame.
    uint8_t get_num_bins() const;

    /// Check if data is fresh (received within last 200ms).
    bool is_fresh() const;

    /// Singleton accessor.
    static FftBuffer &instance();

private:
    fft_data_t buffers_[2] = {};
    volatile uint8_t read_idx_ = 0;
};
