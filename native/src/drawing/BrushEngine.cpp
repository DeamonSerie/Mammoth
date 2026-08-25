#include "BrushEngine.hpp"
#include "CustomBrushGeometry.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <algorithm>

BrushEngine::BrushEngine() {
    DebugLog::log("[BrushEngine] Constructor");
}

void BrushEngine::applyStamp(Layer& layer, float cx, float cy,
                              const Brush& brush, float pressure)
{
    DebugLog::log("[BrushEngine] applyStamp cx=%.1f cy=%.1f type=%d size=%.1f pressure=%.2f",
                  cx, cy, (int)brush.type(), brush.size(), pressure);

    int centerX = (int)std::round(cx);
    int centerY = (int)std::round(cy);
    int radius = (int)std::ceil(brush.size() * 0.5f * pressure);
    Color c = brush.color();
    float alpha = c.af() * brush.opacity() * pressure;

    switch (brush.type()) {
        case BrushType::HardRound:
        case BrushType::Pencil:
            stampHardRound(layer, centerX, centerY, radius, c, alpha);
            break;
        case BrushType::SoftRound:
        case BrushType::Airbrush:
            stampSoftRound(layer, centerX, centerY, radius,
                          brush.hardness(), c, alpha);
            break;
        case BrushType::Eraser:
            stampEraser(layer, cx, cy);
            break;
        case BrushType::Custom:
            c.a = (uint8_t)(c.a * brush.opacity() * pressure);
            CustomBrushGeometry::stamp(layer, (float)centerX, (float)centerY,
                                       brush.size() * 0.5f * pressure,
                                       brush.customConfig().resolve(), c);
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

void BrushEngine::stampEraser(Layer& layer, float cx, float cy) {
    DebugLog::log("[BrushEngine] stampEraser cx=%.1f cy=%.1f", cx, cy);
    if (m_eraser) {
        m_eraser->stamp(layer, cx, cy);
    }
}

void BrushEngine::stampHardRound(Layer& layer, int cx, int cy,
                                  int radius, const Color& c, float alpha)
{
    for (int dy = -radius - 1; dy <= radius + 1; dy++) {
        for (int dx = -radius - 1; dx <= radius + 1; dx++) {
            float dist = std::sqrt((float)(dx * dx + dy * dy));
            float a;
            if (dist <= radius - 1.0f) {
                a = 1.0f;
            } else if (dist >= radius + 1.0f) {
                continue;
            } else {
                a = (radius + 1.0f - dist) * 0.5f;
            }
            Color stamp = c;
            stamp.a = (uint8_t)(c.a * alpha * std::clamp(a, 0.0f, 1.0f));
            layer.blendPixel(cx + dx, cy + dy, stamp);
        }
    }
}

void BrushEngine::stampSoftRound(Layer& layer, int cx, int cy,
                                  int radius, float hard, const Color& c,
                                  float alpha)
{
    for (int dy = -radius - 1; dy <= radius + 1; dy++) {
        for (int dx = -radius - 1; dx <= radius + 1; dx++) {
            float dist = std::sqrt((float)(dx * dx + dy * dy));
            if (dist > (float)radius + 1.0f) continue;
            float normDist = (radius > 0) ? dist / (float)radius : 0.0f;
            float a;
            if (normDist <= hard) {
                a = 1.0f;
            } else {
                a = (1.0f - normDist) / (1.0f - hard);
            }
            Color stamp = c;
            stamp.a = (uint8_t)(c.a * alpha * std::clamp(a, 0.0f, 1.0f));
            layer.blendPixel(cx + dx, cy + dy, stamp);
        }
    }
}
