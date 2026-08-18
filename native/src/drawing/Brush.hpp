#pragma once
#include "../app/Types.hpp"
#include <vector>

enum class BrushType {
    HardRound,
    SoftRound,
    Pencil,
    Airbrush,
    Eraser
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

    void setType(BrushType t) { m_type = t; }
    void setSize(float s) { m_size = s; }
    void setOpacity(float o) { m_opacity = o; }
    void setHardness(float h) { m_hardness = h; }
    void setColor(const Color& c) { m_color = c; }
    void setSpacing(float s) { m_spacing = s; }

    std::vector<Vec2> interpolatePoints(const Vec2& from, const Vec2& to) const;

private:
    BrushType m_type = BrushType::HardRound;
    float m_size = 8.0f;
    float m_opacity = 1.0f;
    float m_hardness = 0.8f;
    Color m_color;
    float m_spacing = 0.25f;
};
