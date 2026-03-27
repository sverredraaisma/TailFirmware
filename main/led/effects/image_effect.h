#pragma once

#include "led/led_effect.h"
#include <vector>
#include <cstdint>

class ImageEffect : public LedEffect {
public:
    /// Load an image as a flat RGB byte array (r,g,b,r,g,b,...).
    void set_image(const uint8_t *rgb_data, uint8_t width, uint8_t height);

    void render(RGB *buffer, const LedCoord *coords,
                uint16_t count, float dt) override;
    void set_param(uint8_t param_id, float value) override;
    float get_param(uint8_t param_id) const override;

private:
    std::vector<uint8_t> image_data_;
    uint8_t width_ = 0;
    uint8_t height_ = 0;
    float orientation_ = 0.0f; // 0=0deg, 1=90deg, 2=180deg, 3=270deg

    /// Sample the image at normalized coordinates using nearest-neighbor.
    RGB sample(float x, float y) const;
};
