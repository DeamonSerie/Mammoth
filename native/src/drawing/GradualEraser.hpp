#pragma once
#include "../app/Types.hpp"
#include "../document/Layer.hpp"
#include <vector>

class GradualEraser {
public:
    GradualEraser();

    float size() const { return m_size; }
    float opacity() const { return m_opacity; }
    float spacing() const { return m_spacing; }

    void setSize(float s) { m_size = s; }
    void setOpacity(float o) { m_opacity = o; }
    void setSpacing(float s) { m_spacing = s; }

    void beginStroke(const Layer& layer);
    void endStroke();

    std::vector<Vec2> interpolatePoints(const Vec2& from, const Vec2& to) const;
    void stamp(Layer& layer, float cx, float cy) const;

private:
    float m_size = 12.0f;
    float m_opacity = 1.0f;
    float m_spacing = 0.25f;
    std::vector<uint8_t> m_originalData;
    int m_origW = 0;
    int m_origH = 0;
};
