#include "GradualEraser.hpp"
#include <cmath>
#include <algorithm>

GradualEraser::GradualEraser() {}

std::vector<Vec2> GradualEraser::interpolatePoints(const Vec2& from, const Vec2& to) const {
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

void GradualEraser::stamp(Layer& layer, float cx, float cy) const {
    int radius = (int)std::ceil(m_size * 0.5f);
    int centerX = (int)std::round(cx);
    int centerY = (int)std::round(cy);
    int r2 = radius * radius;
    int w = layer.width();
    int h = layer.height();
    uint8_t* pixels = layer.data();
    if (!pixels || w <= 0 || h <= 0 || radius <= 0 || m_opacity <= 0.0f) {
        return;
    }

    int erased = 0;
    int skipped = 0;
    int totalInCircle = 0;
    for (int dy = -radius; dy <= radius; dy++) {
        int py = centerY + dy;
        if (py < 0 || py >= h) continue;
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > r2) continue;
            totalInCircle++;
            int px = centerX + dx;
            if (px < 0 || px >= w) continue;
            size_t off = ((size_t)py * w + px) * 4;
            uint8_t* dst = pixels + off;
            if (dst[3] == 0) { skipped++; continue; }
            float da = dst[3] / 255.0f;
            float newA = da * (1.0f - m_opacity);
            if (newA < 0.01f) {
                dst[0] = dst[1] = dst[2] = dst[3] = 0;
            } else {
                dst[3] = (uint8_t)(newA * 255.0f);
            }
            erased++;
        }
    }
    layer.setDirty();
}
