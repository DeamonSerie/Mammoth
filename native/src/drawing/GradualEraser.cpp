#include "GradualEraser.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <algorithm>

GradualEraser::GradualEraser() {}

void GradualEraser::beginStroke(const Layer& layer) {
    m_originalData.assign(layer.data(), layer.data() + layer.dataSize());
    m_origW = layer.width();
    m_origH = layer.height();
}

void GradualEraser::endStroke() {
    m_originalData.clear();
    m_origW = 0;
    m_origH = 0;
}

std::vector<Vec2> GradualEraser::interpolatePoints(const Vec2& from, const Vec2& to) const {
    DebugLog::log("[GradualEraser] interpolatePoints from=(%.1f,%.1f) to=(%.1f,%.1f) size=%.1f spacing=%.2f", from.x, from.y, to.x, to.y, m_size, m_spacing);
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
    stamp(layer, cx, cy, 1.0f);
}

void GradualEraser::stamp(Layer& layer, float cx, float cy, float pressure) const {
    float p = std::clamp(pressure, 0.0f, 1.0f);

    float e = 1.0f - std::pow(1.0f - p, 1.8f);

    float rScale = 0.2f + 0.8f * e;

    const float eraseStartEased = 0.3f;
    const float eraseFullEased = 0.70f;
    float eScale = 0.0f;
    if (e > eraseStartEased) {
        eScale = std::min(1.0f, (e - eraseStartEased) / (eraseFullEased - eraseStartEased));
    }

    int radius = (int)std::ceil(m_size * 0.5f * rScale);
    int centerX = (int)std::round(cx);
    int centerY = (int)std::round(cy);
    int r2 = radius * radius;
    int w = layer.width();
    int h = layer.height();
    uint8_t* pixels = layer.mutableData();
    if (!pixels || w <= 0 || h <= 0 || radius <= 0 || m_opacity <= 0.0f) return;
    if (m_originalData.empty()) return;

    for (int dy = -radius; dy <= radius; dy++) {
        int py = centerY + dy;
        if (py < 0 || py >= h) continue;
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > r2) continue;
            int px = centerX + dx;
            if (px < 0 || px >= w) continue;

            size_t origOff = ((size_t)py * w + px) * 4;
            uint8_t origA = m_originalData[origOff + 3];
            if (origA == 0) continue;

            uint8_t origR = m_originalData[origOff + 0];
            uint8_t origG = m_originalData[origOff + 1];
            uint8_t origB = m_originalData[origOff + 2];

            float t = m_opacity * eScale;
            float lightenFactor = (1.0f - t) * (1.0f - t) * 0.8f;
            float eraseFactor = t;

            float origDa = origA / 255.0f;
            float newA = origDa * (1.0f - eraseFactor);
            float newR = origR + (255.0f - origR) * lightenFactor;
            float newG = origG + (255.0f - origG) * lightenFactor;
            float newB = origB + (255.0f - origB) * lightenFactor;

            size_t dstOff = ((size_t)py * w + px) * 4;
            if (newA < 0.01f) {
                pixels[dstOff] = pixels[dstOff + 1] = pixels[dstOff + 2] = pixels[dstOff + 3] = 0;
            } else {
                pixels[dstOff + 0] = (uint8_t)std::clamp(newR, 0.0f, 255.0f);
                pixels[dstOff + 1] = (uint8_t)std::clamp(newG, 0.0f, 255.0f);
                pixels[dstOff + 2] = (uint8_t)std::clamp(newB, 0.0f, 255.0f);
                pixels[dstOff + 3] = (uint8_t)(newA * 255.0f);
            }
        }
    }
    layer.setDirty();
}

void GradualEraser::stampStroke(Layer& layer, const std::vector<Vec2>& pts,
                                const std::vector<float>& pressures) const {
    if (pts.empty()) return;
    if (pts.size() == 1) {
        float p0 = pressures.empty() ? 1.0f : pressures[0];
        stamp(layer, pts[0].x, pts[0].y, p0);
        return;
    }
    size_t n = pts.size();
    for (size_t i = 1; i < n; i++) {
        Vec2 from = pts[i - 1];
        Vec2 to = pts[i];
        float p0 = (i - 1 < pressures.size()) ? pressures[i - 1] : 1.0f;
        float p1 = (i < pressures.size()) ? pressures[i] : 1.0f;

        std::vector<Vec2> seg = interpolatePoints(from, to);
        int m = (int)seg.size();
        for (int j = 0; j < m; j++) {
            float t = (m <= 1) ? 0.0f : (float)j / (float)(m - 1);
            float pr = p0 + (p1 - p0) * t;
            stamp(layer, seg[j].x, seg[j].y, pr);
        }
    }
}