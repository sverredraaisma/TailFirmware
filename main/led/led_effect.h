#pragma once

#include <stdint.h>
#include "color.h"

struct LedCoord {
    float x; // 0.0 - 1.0
    float y; // 0.0 - 1.0
};

class LedEffect {
public:
    virtual ~LedEffect() = default;

    /// Render this effect into the buffer.
    /// @param buffer   Output pixel buffer (caller-allocated, count entries)
    /// @param coords   Coordinate of each LED in [0,1] x [0,1] space
    /// @param count    Number of LEDs
    /// @param dt       Seconds since last frame
    virtual void render(RGB *buffer, const LedCoord *coords,
                        uint16_t count, float dt) = 0;

    /// Set an effect-specific parameter by ID.
    virtual void set_param(uint8_t param_id, float value) = 0;

    /// Get an effect-specific parameter by ID.
    virtual float get_param(uint8_t param_id) const = 0;

    // Coordinate transforms (applied before sampling)
    bool flip_x = false;
    bool flip_y = false;
    bool mirror_x = false;
    bool mirror_y = false;

protected:
    /// Apply flip/mirror transforms to a coordinate.
    LedCoord transform_coord(LedCoord c) const {
        if (mirror_x) c.x = (c.x <= 0.5f) ? c.x * 2.0f : (1.0f - c.x) * 2.0f;
        if (mirror_y) c.y = (c.y <= 0.5f) ? c.y * 2.0f : (1.0f - c.y) * 2.0f;
        if (flip_x) c.x = 1.0f - c.x;
        if (flip_y) c.y = 1.0f - c.y;
        return c;
    }
};
