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

    // PRESSURE PIPELINE: clamp the raw event pressure into [0,1] and route it
    // through the eased PressureProfile (Brush::radiusForPressure /
    // opacityForPressure). This makes a *normal* pen press yield a bold,
    // near-full-color stroke while a light touch stays light, and guarantees
    // over-pressure (>1.0) can never enlarge the stroke or wrap its alpha —
    // no accidental screen marking.
    float p = std::clamp(pressure, 0.0f, 1.0f);
    int centerX = (int)std::round(cx);
    int centerY = (int)std::round(cy);
    float radius = brush.radiusForPressure(p);
    int iRadius = (int)std::ceil(radius);
    Color c = brush.color();
    float alpha = brush.opacityForPressure(p) * c.af();

    // At size 1 with full pressure, snap to grid cell and fill entire cell
    if (brush.size() == 1.0f && p >= 1.0f && iRadius <= 1) {
        layer.blendPixel(centerX, centerY, c);
        return;
    }

    // At size 1 with partial pressure, draw single pixel
    if (brush.size() <= 1.0f && iRadius <= 1) {
        layer.blendPixel(centerX, centerY, c);
        return;
    }

    switch (brush.type()) {
        case BrushType::HardRound:
        case BrushType::Pencil:
            stampHardRound(layer, centerX, centerY, iRadius, c, alpha);
            break;
        case BrushType::SoftRound:
        case BrushType::Airbrush:
            stampSoftRound(layer, centerX, centerY, iRadius,
                          brush.hardness(), c, alpha);
            break;
        case BrushType::Eraser:
            stampEraser(layer, cx, cy, p);
            break;
        case BrushType::Custom:
            c.a = (uint8_t)(c.a * brush.opacityForPressure(p));
            CustomBrushGeometry::stamp(layer, (float)centerX, (float)centerY,
                                        brush.radiusForPressure(p),
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

void BrushEngine::stampEraser(Layer& layer, float cx, float cy, float pressure) {
    DebugLog::log("[BrushEngine] stampEraser cx=%.1f cy=%.1f pressure=%.2f", cx, cy, pressure);
    if (m_eraser) {
        m_eraser->stamp(layer, cx, cy, pressure);
    }
}

void BrushEngine::stampHardRound(Layer& layer, int cx, int cy,
                                  int radius, const Color& c, float alpha)
{
    // Classic brush stroke: a solid disc of the picked color with a crisp
    // pixel edge (a pixel is painted when its center falls within `radius`).
    // The 4 pixels sitting exactly on the radius along the axes are skipped so
    // the circle reads round instead of showing bumps at the four edges.
    Color stamp = c;
    stamp.a = (uint8_t)(c.a * alpha);
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > radius * radius) continue;
            if (((dx == radius || dx == -radius) && dy == 0) ||
                ((dy == radius || dy == -radius) && dx == 0)) continue;
            layer.blendPixel(cx + dx, cy + dy, stamp);
        }
    }
}

void BrushEngine::stampSoftRound(Layer& layer, int cx, int cy,
                                  int radius, float hard, const Color& c,
                                  float alpha)
{
    // Classic brush stroke: the disc spans the full radius; inside the
    // hardness boundary it is solid and the edge falls off smoothly toward
    // transparent (soft, anti-aliased edge). The 4 pixels sitting exactly on
    // the radius along the axes are skipped like the hard stamp.
    hard = std::clamp(hard, 0.0f, 1.0f);
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            float dist = std::sqrt((float)(dx * dx + dy * dy));
            if (dist > (float)radius) continue;
            if (((dx == radius || dx == -radius) && dy == 0) ||
                ((dy == radius || dy == -radius) && dx == 0)) continue;
            float normDist = (radius > 0) ? dist / (float)radius : 0.0f;
            float a;
            if (hard >= 1.0f) {
                a = 1.0f;
            } else if (normDist < hard) {
                a = 1.0f;
            } else {
                a = (1.0f - normDist) / (1.0f - hard);
            }
            if (a <= 0.0f) continue;
            Color stamp = c;
            stamp.a = (uint8_t)(c.a * alpha * std::clamp(a, 0.0f, 1.0f));
            layer.blendPixel(cx + dx, cy + dy, stamp);
        }
    }
}
