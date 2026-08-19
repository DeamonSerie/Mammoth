#include "GradualEraser.hpp"
#include "../DebugLog.h"
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
        DebugLog::log("[GradualEraser::stamp] EARLY RETURN pixels=%p w=%d h=%d r=%d op=%.2f", (void*)pixels, w, h, radius, m_opacity);
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
            
            // Sigmoid centered at 80%: 1 / (1 + exp(-14*(x - 0.8)))
            // 100% -> 100% (instant full erase), 50% -> 1.5%, 65% -> 12%, 80% -> 50% (middle)
            float eraseStrength = (m_opacity >= 1.0f) ? 1.0f : 1.0f / (1.0f + std::exp(-14.0f * (m_opacity - 0.8f)));
            float da = dst[3] / 255.0f;
            
            // Reduce alpha toward 0 (erase)
            float newA = da * (1.0f - eraseStrength);
            
            // Blend color toward white (canvas background)
            float invStrength = 1.0f - eraseStrength;
            float newR = dst[0] * invStrength + 255.0f * eraseStrength;
            float newG = dst[1] * invStrength + 255.0f * eraseStrength;
            float newB = dst[2] * invStrength + 255.0f * eraseStrength;
            
            if (newA < 0.01f) {
                dst[0] = dst[1] = dst[2] = dst[3] = 0;
            } else {
                dst[0] = (uint8_t)std::clamp(newR, 0.0f, 255.0f);
                dst[1] = (uint8_t)std::clamp(newG, 0.0f, 255.0f);
                dst[2] = (uint8_t)std::clamp(newB, 0.0f, 255.0f);
                dst[3] = (uint8_t)(newA * 255.0f);
            }
            erased++;
        }
    }
    DebugLog::log("[GradualEraser::stamp] cx=%.1f cy=%.1f r=%d total=%d skipped_zero=%d erased=%d", cx, cy, radius, totalInCircle, skipped, erased);
    layer.setDirty();
    if (centerX >= 0 && centerX < w && centerY >= 0 && centerY < h) {
        size_t off = ((size_t)centerY * w + centerX) * 4;
        DebugLog::log("[GradualEraser::verify] center(%d,%d) rgba=(%d,%d,%d,%d)",
            centerX, centerY, pixels[off], pixels[off+1], pixels[off+2], pixels[off+3]);
    }
}