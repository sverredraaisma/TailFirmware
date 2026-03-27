#include "config/fft_buffer.h"
#include "esp_timer.h"
#include <cstring>
#include <algorithm>

static constexpr int64_t STALE_THRESHOLD_US = 200000; // 200ms

void FftBuffer::write(uint8_t loudness, uint8_t num_bins, const uint8_t *bins) {
    uint8_t write_idx = 1 - read_idx_;
    fft_data_t &buf = buffers_[write_idx];

    buf.loudness = loudness;
    buf.num_bins = std::min<uint8_t>(num_bins, MAX_FFT_BINS);
    if (bins && buf.num_bins > 0) {
        memcpy(buf.bins, bins, buf.num_bins);
    }
    buf.timestamp_us = esp_timer_get_time();

    // Flip read index atomically (single byte write is atomic on ESP32-C3)
    read_idx_ = write_idx;
}

uint8_t FftBuffer::get_loudness() const {
    if (!is_fresh()) return 0;
    return buffers_[read_idx_].loudness;
}

float FftBuffer::get_loudness_normalized() const {
    return static_cast<float>(get_loudness()) / 255.0f;
}

uint8_t FftBuffer::get_bin(uint8_t index) const {
    if (!is_fresh()) return 0;
    const fft_data_t &buf = buffers_[read_idx_];
    if (index >= buf.num_bins) return 0;
    return buf.bins[index];
}

uint8_t FftBuffer::get_num_bins() const {
    return buffers_[read_idx_].num_bins;
}

bool FftBuffer::is_fresh() const {
    int64_t now = esp_timer_get_time();
    int64_t ts = buffers_[read_idx_].timestamp_us;
    return (ts != 0) && ((now - ts) < STALE_THRESHOLD_US);
}

FftBuffer &FftBuffer::instance() {
    static FftBuffer inst;
    return inst;
}
