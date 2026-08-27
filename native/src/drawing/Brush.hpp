#pragma once
#include "../app/Types.hpp"
#include "CustomBrushConfig.hpp"
#include "Pressure.hpp"
#include <vector>

enum class BrushType {
    HardRound,
    SoftRound,
    Pencil,
    Airbrush,
    Eraser,
    Custom
};

class Brush {
public:
    Brush();

    BrushType type() const { return m_type; }
    float size() const { return m_size; }
    float opacity() const { return m_opacity; }
    float hardness() const { return m_hardness; }
    Color color() const { return m_color; }
    float spacing() const { return m_spacing; }
    const CustomBrushConfig& customConfig() const { return m_custom; }

    // --- Pressure response (pencil/paper feel) -----------------------------
    // PRESSURE PIPELINE: the actual pressure->size/opacity math lives in
    // PressureProfile (drawing/Pressure.hpp). Use pressureProfile() to read or
    // tune it; the helpers below are thin accessors kept for convenience.
    const PressureProfile& pressureProfile() const { return m_pressure; }
    PressureProfile& pressureProfile() { return m_pressure; }

    float pressureSize() const { return m_pressure.sizeResponse; }
    float pressureOpacity() const { return m_pressure.opacityResponse; }
    float pressureEase() const { return m_pressure.ease; }
    bool pressureEnabled() const { return m_pressureEnabled; }
    void setPressureSize(float v) { m_pressure.sizeResponse = std::clamp(v, 0.0f, 1.0f); }
    void setPressureOpacity(float v) { m_pressure.opacityResponse = std::clamp(v, 0.0f, 1.0f); }
    void setPressureEnabled(bool e) { m_pressureEnabled = e; }
    void setPressureEase(float v) { m_pressure.ease = std::max(1.0f, v); }

    float baseRadius() const { return m_size * 0.5f; }
    float easedPressure(float p) const { return m_pressure.eased(p); }
    float radiusForPressure(float p) const {
        if (!m_pressureEnabled) return baseRadius();
        return m_pressure.radius(baseRadius(), p);
    }
    float opacityForPressure(float p) const {
        if (!m_pressureEnabled) return m_opacity;
        return m_pressure.opacity(m_opacity, p);
    }

    void setType(BrushType t) { m_type = t; }
    void setSize(float s) { m_size = s; }
    void setOpacity(float o) { m_opacity = o; }
    void setHardness(float h) { m_hardness = h; }
    void setColor(const Color& c) { m_color = c; }
    void setSpacing(float s) { m_spacing = s; }
    CustomBrushConfig& customConfig() { return m_custom; }

    std::vector<Vec2> interpolatePoints(const Vec2& from, const Vec2& to) const;

private:
    BrushType m_type = BrushType::HardRound;
    float m_size = 8.0f;
    float m_opacity = 1.0f;
    float m_hardness = 0.8f;
    Color m_color;
    float m_spacing = 0.25f;
    CustomBrushConfig m_custom;

    bool m_pressureEnabled = true;
    PressureProfile m_pressure;
};
