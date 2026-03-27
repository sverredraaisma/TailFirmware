#pragma once

#include <vector>
#include <memory>
#include "color.h"
#include "led_effect.h"
#include "led_matrix.h"

enum class BlendMode : uint8_t {
    Multiply = 0,
    Add = 1,
    Subtract = 2,
    Min = 3,
    Max = 4,
    Overwrite = 5,
};

struct Layer {
    std::unique_ptr<LedEffect> effect;
    BlendMode blend_mode = BlendMode::Overwrite;
    bool enabled = true;
};

class LayerCompositor {
public:
    /// Set the maximum layer count.
    void init(uint8_t max_layers);

    /// Set an effect at the given layer index. Takes ownership.
    void set_layer(uint8_t index, std::unique_ptr<LedEffect> effect, BlendMode mode);

    /// Remove a layer.
    void remove_layer(uint8_t index);

    /// Enable/disable a layer.
    void set_layer_enabled(uint8_t index, bool enabled);

    /// Get layer count.
    uint8_t get_layer_count() const { return layers_.size(); }

    /// Render all layers into the matrix.
    void render(LedMatrix &matrix, float dt);

private:
    std::vector<Layer> layers_;
    std::vector<RGB> temp_buffer_;

    static RGB blend(RGB base, RGB overlay, BlendMode mode);
};
