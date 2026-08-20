#include "BrushEngine.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <algorithm>

BrushEngine::BrushEngine() {
    DebugLog::log("[BrushEngine] Constructor");
}

void BrushEngine::applyStamp(Layer& layer, float cx, float cy,
                              const Brush& brush, float pressure)
{
    DebugLog::log("[BrushEngine] applyStamp cx=%.1f cy=%.1f type=%d size=%.1f pressure=%.2f", cx, cy, (int)brush.type(), brush.size(), pressure);
    int radius = (int)std::ceil(brush.size() * 0.5f * pressure);
    int centerX = (int)std::round(cx);
    int centerY = (int)std::round(cy);

    switch (brush.type()) {
        case BrushType::HardRound:
            stampHardRound(layer, centerX, centerY, radius, brush, pressure);
            break;
        case BrushType::SoftRound:
            stampSoftRound(layer, centerX, centerY, radius, brush, pressure);
            break;
        case BrushType::Pencil:
            stampHardRound(layer, centerX, centerY, radius, brush, pressure);
            break;
        case BrushType::Airbrush:
            stampSoftRound(layer, centerX, centerY, radius, brush, pressure);
            break;
        case BrushType::Eraser:
            stampEraser(layer, cx, cy);
            break;
    }
}

void BrushEngine::applyStroke(Layer& layer, const std::vector<Vec2>& points,
                               const Brush& brush, float pressure)
{
    DebugLog::log("[BrushEngine] applyStroke points=%zu", points.size());
    for (const auto& pt : points)
        applyStamp(layer, pt.x, pt.y, brush, pressure);
}

void BrushEngine::stampHardRound(Layer& layer, int centerX, int centerY,
                                  int radius, const Brush& brush, float pressure)
{
    DebugLog::log("[BrushEngine] stampHardRound center=(%d,%d) radius=%d", centerX, centerY, radius);
    Color c = brush.color();
    c.a = (uint8_t)(c.a * brush.opacity() * pressure);

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                layer.blendPixel(centerX + dx, centerY + dy, c);
            }
        }
    }
}

void BrushEngine::stampSoftRound(Layer& layer, int centerX, int centerY,
                                  int radius, const Brush& brush, float pressure)
{
    DebugLog::log("[BrushEngine] stampSoftRound center=(%d,%d) radius=%d hardness=%.2f", centerX, centerY, radius, brush.hardness());
    Color c = brush.color();
    float hard = brush.hardness();

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            float dist = std::sqrt((float)(dx * dx + dy * dy));
            if (dist > radius) continue;

            float normDist = (radius > 0) ? dist / (float)radius : 0.0f;
            float alpha;
            if (normDist < hard) {
                alpha = 1.0f;
            } else {
                alpha = (1.0f - normDist) / (1.0f - hard);
            }
            alpha *= brush.opacity() * pressure;

            Color stamp = c;
            stamp.a = (uint8_t)(c.a * std::clamp(alpha, 0.0f, 1.0f));
            layer.blendPixel(centerX + dx, centerY + dy, stamp);
        }
    }
}

void BrushEngine::stampEraser(Layer& layer, float cx, float cy) {
    DebugLog::log("[BrushEngine] stampEraser cx=%.1f cy=%.1f", cx, cy);
    if (m_eraser) {
        m_eraser->stamp(layer, cx, cy);
    }
}
