#include "BrushEngine.hpp"
#include "CustomBrushGeometry.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <algorithm>

static constexpr int SUPERSAMPLE = 4;

BrushEngine::BrushEngine() {
    DebugLog::log("[BrushEngine] Constructor");
}

void BrushEngine::applyStamp(Layer& layer, float cx, float cy,
                              const Brush& brush, float pressure)
{
    DebugLog::log("[BrushEngine] applyStamp cx=%.1f cy=%.1f type=%d size=%.1f pressure=%.2f",
                  cx, cy, (int)brush.type(), brush.size(), pressure);
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
        case BrushType::Custom:
            stampCustomShape(layer, centerX, centerY,
                             brush.size() * 0.5f * pressure, brush, pressure);
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
    float baseAlpha = c.af() * brush.opacity() * pressure;
    int N = SUPERSAMPLE;
    float invN = 1.0f / (float)(N * N);

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int inside = 0;
            for (int sy = 0; sy < N; sy++) {
                for (int sx = 0; sx < N; sx++) {
                    float fx = (float)dx + ((float)sx + 0.5f) * invN - 0.5f;
                    float fy = (float)dy + ((float)sy + 0.5f) * invN - 0.5f;
                    if (fx * fx + fy * fy <= (float)(radius * radius))
                        inside++;
                }
            }
            if (inside == 0) continue;
            Color stamp = c;
            stamp.a = (uint8_t)(255.0f * baseAlpha * (float)inside * invN);
            layer.blendPixel(centerX + dx, centerY + dy, stamp);
        }
    }
}

void BrushEngine::stampSoftRound(Layer& layer, int centerX, int centerY,
                                  int radius, const Brush& brush, float pressure)
{
    DebugLog::log("[BrushEngine] stampSoftRound center=(%d,%d) radius=%d hardness=%.2f", centerX, centerY, radius, brush.hardness());
    Color c = brush.color();
    float hard = brush.hardness();
    float baseAlpha = c.af() * brush.opacity() * pressure;
    int N = SUPERSAMPLE;
    float invN = 1.0f / (float)(N * N);

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            float accumAlpha = 0.0f;
            for (int sy = 0; sy < N; sy++) {
                for (int sx = 0; sx < N; sx++) {
                    float fx = (float)dx + ((float)sx + 0.5f) * invN - 0.5f;
                    float fy = (float)dy + ((float)sy + 0.5f) * invN - 0.5f;
                    float dist = std::sqrt(fx * fx + fy * fy);
                    if (dist > (float)radius) continue;
                    float normDist = (dist / (float)radius);
                    float a;
                    if (normDist < hard) {
                        a = 1.0f;
                    } else {
                        a = (1.0f - normDist) / (1.0f - hard);
                    }
                    accumAlpha += a;
                }
            }
            if (accumAlpha < 0.001f) continue;
            Color stamp = c;
            stamp.a = (uint8_t)(255.0f * baseAlpha * std::clamp(accumAlpha * invN, 0.0f, 1.0f));
            layer.blendPixel(centerX + dx, centerY + dy, stamp);
        }
    }
}
void BrushEngine::stampCustomShape(Layer& layer, int centerX, int centerY,
                                    float radius, const Brush& brush, float pressure)
{
    DebugLog::log("[BrushEngine] stampCustomShape center=(%d,%d) radius=%.1f", centerX, centerY, radius);
    Color c = brush.color();
    c.a = (uint8_t)(c.a * brush.opacity() * pressure);
    CustomBrushGeometry::stamp(layer, (float)centerX, (float)centerY,
                               radius, brush.customConfig().resolve(), c);
}

void BrushEngine::stampEraser(Layer& layer, float cx, float cy) {
    DebugLog::log("[BrushEngine] stampEraser cx=%.1f cy=%.1f", cx, cy);
    if (m_eraser) {
        m_eraser->stamp(layer, cx, cy);
    }
}
