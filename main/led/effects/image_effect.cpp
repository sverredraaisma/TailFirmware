#include "led/effects/image_effect.h"
#include <cstring>
#include <algorithm>
#include <cmath>

void ImageEffect::set_image(const uint8_t *rgb_data, uint8_t width, uint8_t height) {
    width_ = width;
    height_ = height;
    size_t size = static_cast<size_t>(width) * height * 3;
    image_data_.resize(size);
    if (rgb_data && size > 0) {
        memcpy(image_data_.data(), rgb_data, size);
    }
}

RGB ImageEffect::sample(float x, float y) const {
    if (width_ == 0 || height_ == 0 || image_data_.empty()) {
        return RGB::black();
    }

    // Map [0,1] to pixel coordinates and clamp
    int px = static_cast<int>(x * (width_ - 1) + 0.5f);
    int py = static_cast<int>(y * (height_ - 1) + 0.5f);
    px = std::max(0, std::min(static_cast<int>(width_ - 1), px));
    py = std::max(0, std::min(static_cast<int>(height_ - 1), py));

    size_t idx = (static_cast<size_t>(py) * width_ + px) * 3;
    RGB result;
    result.r = image_data_[idx];
    result.g = image_data_[idx + 1];
    result.b = image_data_[idx + 2];
    return result;
}

void ImageEffect::render(RGB *buffer, const LedCoord *coords,
                          uint16_t count, float dt) {
    (void)dt;

    int orient = static_cast<int>(orientation_ + 0.5f) & 3; // 0-3

    for (uint16_t i = 0; i < count; i++) {
        LedCoord c = transform_coord(coords[i]);

        // Apply orientation rotation around center (0.5, 0.5)
        float x = c.x;
        float y = c.y;
        switch (orient) {
            case 1: { // 90 degrees CW
                float tmp = x;
                x = 1.0f - y;
                y = tmp;
                break;
            }
            case 2: { // 180 degrees
                x = 1.0f - x;
                y = 1.0f - y;
                break;
            }
            case 3: { // 270 degrees CW
                float tmp = x;
                x = y;
                y = 1.0f - tmp;
                break;
            }
            default: // 0 degrees, no rotation
                break;
        }

        buffer[i] = sample(x, y);
    }
}

void ImageEffect::set_param(uint8_t param_id, float value) {
    switch (param_id) {
        case 0: orientation_ = value; break;
        default: break;
    }
}

float ImageEffect::get_param(uint8_t param_id) const {
    switch (param_id) {
        case 0: return orientation_;
        default: return 0.0f;
    }
}
