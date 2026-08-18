#include "Brush.hpp"
#include <cmath>
#include <algorithm>

Brush::Brush() {}

std::vector<Vec2> Brush::interpolatePoints(const Vec2& from, const Vec2& to) const {
    std::vector<Vec2> points;
    float dist = std::sqrt((to.x - from.x) * (to.x - from.x) +
                           (to.y - from.y) * (to.y - from.y));
    float step = std::max(m_size * m_spacing, 1.0f);
    int steps = std::max(1, (int)std::ceil(dist / step));

    for (int i = 0; i <= steps; i++) {
        float t = (steps == 0) ? 0.0f : (float)i / (float)steps;
        points.push_back({from.x + (to.x - from.x) * t,
                          from.y + (to.y - from.y) * t});
    }
    return points;
}
