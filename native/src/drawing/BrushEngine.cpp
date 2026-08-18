#include "BrushEngine.hpp"
#include <cmath>
#include <algorithm>

BrushEngine::BrushEngine() {}

void BrushEngine::applyStamp(Layer& layer, float cx, float cy,
                              const Brush& brush, float pressure)
{
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
            stampEraser(layer, centerX, centerY, radius, pressure);
            break;
    }
}

void BrushEngine::applyStroke(Layer& layer, const std::vector<Vec2>& points,
                               const Brush& brush, float pressure)
{
    for (const auto& pt : points)
        applyStamp(layer, pt.x, pt.y, brush, pressure);
}

void BrushEngine::stampHardRound(Layer& layer, int centerX, int centerY,
                                  int radius, const Brush& brush, float pressure)
{
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

void BrushEngine::stampEraser(Layer& layer, int centerX, int centerY,
                               int radius, float pressure)
{
    (void)pressure;
    int r2 = radius * radius;
    int w = layer.width();
    int h = layer.height();
    uint8_t* pixels = layer.data();
    if (!pixels || w <= 0 || h <= 0) return;

    for (int dy = -radius; dy <= radius; dy++) {
        int py = centerY + dy;
        if (py < 0 || py >= h) continue;
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int px = centerX + dx;
                if (px >= 0 && px < w) {
                    size_t off = (py * w + px) * 4;
                    pixels[off + 0] = 0;
                    pixels[off + 1] = 0;
                    pixels[off + 2] = 0;
                    pixels[off + 3] = 0;
                }
            }
        }
    }
    layer.setDirty();
}
