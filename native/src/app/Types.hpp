#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

enum class Tool {
    Brush,
    Eraser,
    Eyedropper,
    Move,
    RectSelect,
};

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    Color() = default;
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    float rf() const { return r / 255.0f; }
    float gf() const { return g / 255.0f; }
    float bf() const { return b / 255.0f; }
    float af() const { return a / 255.0f; }

    uint32_t pack() const {
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    }

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }

    static Color transparent() { return Color(0, 0, 0, 0); }
    static Color white() { return Color(255, 255, 255); }
    static Color black() { return Color(0, 0, 0); }

    static Color hsvToRgb(float h, float s, float v) {
        h = std::fmod(h, 360.0f);
        if (h < 0) h += 360.0f;
        float c = v * s;
        float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        float r, g, b;
        if (h < 60)       { r = c; g = x; b = 0; }
        else if (h < 120) { r = x; g = c; b = 0; }
        else if (h < 180) { r = 0; g = c; b = x; }
        else if (h < 240) { r = 0; g = x; b = c; }
        else if (h < 300) { r = x; g = 0; b = c; }
        else              { r = c; g = 0; b = x; }
        return {
            (uint8_t)((r + m) * 255.0f),
            (uint8_t)((g + m) * 255.0f),
            (uint8_t)((b + m) * 255.0f),
            255
        };
    }

    static void rgbToHsv(uint8_t rr, uint8_t gg, uint8_t bb, float& h, float& s, float& v) {
        float r = rr / 255.0f, g = gg / 255.0f, b = bb / 255.0f;
        float mx = std::fmax(r, std::fmax(g, b));
        float mn = std::fmin(r, std::fmin(g, b));
        float d = mx - mn;
        v = mx;
        s = (mx > 0.0f) ? d / mx : 0.0f;
        if (d < 0.001f) { h = 0; return; }
        if (mx == r) h = 60.0f * std::fmod((g - b) / d, 6.0f);
        else if (mx == g) h = 60.0f * ((b - r) / d + 2.0f);
        else h = 60.0f * ((r - g) / d + 4.0f);
        if (h < 0) h += 360.0f;
    }
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct IVec2 {
    int x = 0;
    int y = 0;

    IVec2() = default;
    IVec2(int x, int y) : x(x), y(y) {}
};
