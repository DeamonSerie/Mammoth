#pragma once
#include "../app/Types.hpp"
#include "../document/Layer.hpp"
#include "CustomBrushConfig.hpp"
#include <vector>

class GradualEraser {
public:
    GradualEraser();

    float size() const { return m_size; }
    float opacity() const { return m_opacity; }
    float spacing() const { return m_spacing; }

    // Shape-erase mode: when enabled, the eraser only touches pixels covered
    // by the configured custom brush shape (scaled to the eraser size).
    bool customShapeEnabled() const { return m_customShape; }
    void setCustomShapeEnabled(bool on) { m_customShape = on; }
    const CustomBrushConfig& customConfig() const { return m_customConfig; }
    void setCustomConfig(const CustomBrushConfig& cfg) { m_customConfig = cfg; }

    void setSize(float s) { m_size = s; }
    void setOpacity(float o) { m_opacity = o; }
    void setSpacing(float s) { m_spacing = s; }

    void beginStroke(const Layer& layer);
    void endStroke();

    std::vector<Vec2> interpolatePoints(const Vec2& from, const Vec2& to) const;
    void stamp(Layer& layer, float cx, float cy) const;

    /// Pressure-aware stamp. `pressure` in [0,1] scales the erase radius and
    /// erase strength so a light pen barely grazes the canvas and a hard press
    /// erases boldly (pencil-eraser feel). Defaults to full pressure.
    void stamp(Layer& layer, float cx, float cy, float pressure) const;

    /// Stamp a whole stroke along a polyline with a parallel pressure list,
    /// interpolating between samples. Used by the app and by tests.
    void stampStroke(Layer& layer, const std::vector<Vec2>& pts,
                     const std::vector<float>& pressures) const;

private:
    float m_size = 12.0f;
    float m_opacity = 1.0f;
    float m_spacing = 0.25f;
    bool m_customShape = false;
    CustomBrushConfig m_customConfig;
    std::vector<uint8_t> m_originalData;
    int m_origW = 0;
    int m_origH = 0;
};
