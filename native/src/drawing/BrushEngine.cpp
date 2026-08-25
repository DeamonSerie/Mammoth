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

    switch (brush.type()) {
        case BrushType::HardRound:
        case BrushType::SoftRound:
        case BrushType::Pencil:
        case BrushType::Airbrush:
            stampHighRes(layer, cx, cy, brush, pressure);
            break;
        case BrushType::Eraser:
            stampEraser(layer, cx, cy);
            break;
        case BrushType::Custom:
            stampCustomShape(layer, (int)std::round(cx), (int)std::round(cy),
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

void BrushEngine::stampHighRes(Layer& layer, float cx, float cy,
                                const Brush& brush, float pressure)
{
    static constexpr int DENSITY = 10;
    int srcRadius = (int)std::ceil(brush.size() * 0.5f * pressure);
    int bufSize = srcRadius * 2 * DENSITY + DENSITY * 2;
    int bufCenter = bufSize / 2;

    Layer hiRes(bufSize, bufSize);
    Brush scaledBrush = brush;
    scaledBrush.setSize(brush.size() * DENSITY);

    int hiRadius = (int)std::ceil(scaledBrush.size() * 0.5f * pressure);
    switch (brush.type()) {
        case BrushType::HardRound:
        case BrushType::Pencil:
            stampHardRound(hiRes, bufCenter, bufCenter, hiRadius, scaledBrush, pressure);
            break;
        case BrushType::SoftRound:
        case BrushType::Airbrush:
            stampSoftRound(hiRes, bufCenter, bufCenter, hiRadius, scaledBrush, pressure);
            break;
        default:
            return;
    }

    int dstCenterX = (int)std::round(cx);
    int dstCenterY = (int)std::round(cy);
    downsampleAndBlend(layer, hiRes, dstCenterX, dstCenterY, srcRadius, DENSITY);
}

void BrushEngine::downsampleAndBlend(Layer& target, const Layer& src,
                                      int dstX, int dstY, int dstRadius, int density)
{
    int srcSize = dstRadius * 2 * density + density * 2;
    float invArea = 1.0f / (float)(density * density);

    for (int dy = -dstRadius; dy <= dstRadius; dy++) {
        for (int dx = -dstRadius; dx <= dstRadius; dx++) {
            float r = 0, g = 0, b = 0, a = 0;
            int sxBase = (dx + dstRadius) * density;
            int syBase = (dy + dstRadius) * density;

            for (int sy = 0; sy < density; sy++) {
                for (int sx = 0; sx < density; sx++) {
                    int px = sxBase + sx;
                    int py = syBase + sy;
                    if (px < 0 || px >= srcSize || py < 0 || py >= srcSize) continue;
                    const uint8_t* p = src.data() + ((py * srcSize + px) * 4);
                    float sa = p[3] / 255.0f;
                    r += p[0] * sa;
                    g += p[1] * sa;
                    b += p[2] * sa;
                    a += sa;
                }
            }

            if (a < 0.001f) continue;
            r /= a; g /= a; b /= a;

            float dstA = a * invArea;
            if (dstA > 1.0f) dstA = 1.0f;
            Color out((uint8_t)(r + 0.5f), (uint8_t)(g + 0.5f), (uint8_t)(b + 0.5f),
                      (uint8_t)(dstA * 255.0f + 0.5f));
            target.blendPixel(dstX + dx, dstY + dy, out);
        }
    }
}
