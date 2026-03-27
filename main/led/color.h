#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>
#include <algorithm>

struct RGB {
    uint8_t r = 0, g = 0, b = 0;

    static RGB black() { return {0, 0, 0}; }
    static RGB white() { return {255, 255, 255}; }

    bool operator==(const RGB &o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const RGB &o) const { return !(*this == o); }
};

struct HSV {
    uint16_t h = 0; // 0-359
    uint8_t s = 0;  // 0-255
    uint8_t v = 0;  // 0-255
};

inline RGB hsv_to_rgb(HSV hsv) {
    if (hsv.s == 0) return {hsv.v, hsv.v, hsv.v};

    uint8_t region = hsv.h / 60;
    uint8_t remainder = (hsv.h - region * 60) * 255 / 60;

    uint8_t p = (uint16_t)hsv.v * (255 - hsv.s) / 255;
    uint8_t q = (uint16_t)hsv.v * (255 - ((uint16_t)hsv.s * remainder / 255)) / 255;
    uint8_t t = (uint16_t)hsv.v * (255 - ((uint16_t)hsv.s * (255 - remainder) / 255)) / 255;

    switch (region) {
        case 0:  return {hsv.v, t, p};
        case 1:  return {q, hsv.v, p};
        case 2:  return {p, hsv.v, t};
        case 3:  return {p, q, hsv.v};
        case 4:  return {t, p, hsv.v};
        default: return {hsv.v, p, q};
    }
}

// Blend helpers
inline RGB rgb_multiply(RGB base, RGB overlay) {
    return {
        (uint8_t)((uint16_t)base.r * overlay.r / 255),
        (uint8_t)((uint16_t)base.g * overlay.g / 255),
        (uint8_t)((uint16_t)base.b * overlay.b / 255),
    };
}

inline RGB rgb_add(RGB base, RGB overlay) {
    return {
        (uint8_t)std::min(base.r + overlay.r, 255),
        (uint8_t)std::min(base.g + overlay.g, 255),
        (uint8_t)std::min(base.b + overlay.b, 255),
    };
}

inline RGB rgb_subtract(RGB base, RGB overlay) {
    return {
        (uint8_t)std::max(base.r - overlay.r, 0),
        (uint8_t)std::max(base.g - overlay.g, 0),
        (uint8_t)std::max(base.b - overlay.b, 0),
    };
}

inline RGB rgb_min(RGB a, RGB b) {
    return {std::min(a.r, b.r), std::min(a.g, b.g), std::min(a.b, b.b)};
}

inline RGB rgb_max(RGB a, RGB b) {
    return {std::max(a.r, b.r), std::max(a.g, b.g), std::max(a.b, b.b)};
}

inline RGB rgb_overwrite(RGB base, RGB overlay) {
    return (overlay == RGB::black()) ? base : overlay;
}

#endif
