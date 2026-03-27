#include "layer_compositor.h"

void LayerCompositor::init(uint8_t max_layers)
{
    layers_.resize(max_layers);
    for (auto &layer : layers_) {
        layer.effect = nullptr;
        layer.blend_mode = BlendMode::Overwrite;
        layer.enabled = true;
    }
}

void LayerCompositor::set_layer(uint8_t index, std::unique_ptr<LedEffect> effect, BlendMode mode)
{
    if (index >= layers_.size()) {
        return;
    }
    layers_[index].effect = std::move(effect);
    layers_[index].blend_mode = mode;
    layers_[index].enabled = true;
}

void LayerCompositor::remove_layer(uint8_t index)
{
    if (index >= layers_.size()) {
        return;
    }
    layers_[index].effect = nullptr;
    layers_[index].enabled = true;
}

void LayerCompositor::set_layer_enabled(uint8_t index, bool enabled)
{
    if (index >= layers_.size()) {
        return;
    }
    layers_[index].enabled = enabled;
}

void LayerCompositor::render(LedMatrix &matrix, float dt)
{
    uint16_t count = matrix.get_led_count();
    if (count == 0) {
        return;
    }

    temp_buffer_.resize(count);

    // Accumulate composited output in a local buffer
    std::vector<RGB> output(count, RGB::black());

    for (size_t layer_idx = 0; layer_idx < layers_.size(); layer_idx++) {
        const Layer &layer = layers_[layer_idx];
        if (!layer.enabled || !layer.effect) {
            continue;
        }

        // Clear temp buffer to black
        for (uint16_t i = 0; i < count; i++) {
            temp_buffer_[i] = RGB::black();
        }

        // Render effect into temp buffer
        layer.effect->render(temp_buffer_.data(), matrix.get_coords(), count, dt);

        // Blend into output
        for (uint16_t i = 0; i < count; i++) {
            output[i] = blend(output[i], temp_buffer_[i], layer.blend_mode);
        }
    }

    // Write final output to matrix
    for (uint16_t i = 0; i < count; i++) {
        matrix.set_pixel(i, output[i]);
    }
}

RGB LayerCompositor::blend(RGB base, RGB overlay, BlendMode mode)
{
    switch (mode) {
        case BlendMode::Multiply:  return rgb_multiply(base, overlay);
        case BlendMode::Add:       return rgb_add(base, overlay);
        case BlendMode::Subtract:  return rgb_subtract(base, overlay);
        case BlendMode::Min:       return rgb_min(base, overlay);
        case BlendMode::Max:       return rgb_max(base, overlay);
        case BlendMode::Overwrite: return rgb_overwrite(base, overlay);
        default:                   return overlay;
    }
}
