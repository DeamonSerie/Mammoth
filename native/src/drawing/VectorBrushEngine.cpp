#include "VectorBrushEngine.hpp"
#include "VectorStroke.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <algorithm>

VectorBrushEngine::VectorBrushEngine() {
}

void VectorBrushEngine::renderStrokeSegment(Layer& layer, Vec2 from, Vec2 to,
                                             float radius, float hardness,
                                             const Color& c, float alpha)
{
    renderStrokeSegment(layer, from, to, radius, radius, hardness, c, alpha, alpha);
}

void VectorBrushEngine::renderStrokeSegment(Layer& layer, Vec2 from, Vec2 to,
                                            float rFrom, float rTo, float hardness,
                                            const Color& c, float aFrom, float aTo)
{
    if (rFrom < 0.5f && rTo < 0.5f) return;

    Vec2 d = to - from;
    float len = std::sqrt(d.x * d.x + d.y * d.y);

    // Sample along the segment at ~0.5px steps and stamp a tapered disc at each,
    // so the stroke width (and alpha) varies smoothly between the endpoints.
    int steps = std::max(1, (int)std::ceil(len / 0.5f));
    for (int s = 0; s <= steps; s++) {
        float t = (steps == 0) ? 0.0f : (float)s / (float)steps;
        Vec2 p = from + d * t;
        float radius = std::max(0.0f, rFrom + (rTo - rFrom) * t);
        float strokeAlpha = aFrom + (aTo - aFrom) * t;
        if (radius < 0.5f) continue;

        float pad = radius + 2.0f;
        float minX = p.x - pad;
        float minY = p.y - pad;
        float maxX = p.x + pad;
        float maxY = p.y + pad;

        int x0 = std::max(0, (int)std::floor(minX));
        int y0 = std::max(0, (int)std::floor(minY));
        int x1 = std::min(layer.width() - 1, (int)std::ceil(maxX));
        int y1 = std::min(layer.height() - 1, (int)std::ceil(maxY));

        float solidRadius = radius * hardness;
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;
                float ddx = px - p.x;
                float ddy = py - p.y;
                float dist = std::sqrt(ddx * ddx + ddy * ddy);

                float a;
                if (dist <= solidRadius) {
                    a = 1.0f;
                } else if (dist >= radius) {
                    continue;
                } else {
                    float fadeRange = radius - solidRadius;
                    a = (fadeRange > 0.0f) ? (1.0f - (dist - solidRadius) / fadeRange) : 1.0f;
                }

                Color stamp = c;
                stamp.a = (uint8_t)(c.a * strokeAlpha * std::clamp(a, 0.0f, 1.0f));
                layer.blendPixel(x, y, stamp);
            }
        }
    }
}

void VectorBrushEngine::renderStroke(Layer& layer, const std::vector<Vec2>& pts,
                                     const std::vector<float>& pressures, const Brush& b)
{
    // PRESSURE PIPELINE: each segment's radius/opacity come from the brush's
    // pressure profile (drawing/Pressure.hpp), giving a tapered pencil stroke.
    // The stroke is rasterized through the shared coverage-field ribbon so the
    // committed stroke matches the live preview's soft-edge character.
    if (pts.empty()) return;

    std::vector<Vec2> p = pts;
    std::vector<float> R(pts.size());
    std::vector<float> O(pts.size());
    for (size_t i = 0; i < pts.size(); i++) {
        float pr = (i < pressures.size()) ? pressures[i] : 1.0f;
        R[i] = b.radiusForPressure(pr);
        O[i] = b.opacityForPressure(pr);
    }
    renderVectorRibbon(layer, p, R, O, b.color(), b.hardness());
}

void VectorBrushEngine::simplifyPath(const std::vector<Vec2>& pts,
                                     const std::vector<float>& pressures,
                                     float tolerance,
                                     std::vector<Vec2>& outPts,
                                     std::vector<float>& outPress)
{
    outPts.clear();
    outPress.clear();
    size_t n = pts.size();
    if (n == 0) return;

    std::vector<char> keep(n, 0);

    // Iterative Douglas-Peucker.
    struct Range { int first; int last; };
    std::vector<Range> stack;
    stack.push_back({(int)0, (int)(n - 1)});
    keep[0] = 1;
    keep[n - 1] = 1;

    while (!stack.empty()) {
        Range r = stack.back();
        stack.pop_back();
        int first = r.first, last = r.last;
        if (last <= first + 1) continue;

        Vec2 a = pts[first];
        Vec2 ab = pts[last] - a;
        float lenSq = ab.x * ab.x + ab.y * ab.y;

        float maxD = 0.0f;
        int idx = -1;
        for (int i = first + 1; i < last; i++) {
            Vec2 p = pts[i];
            float t = (lenSq > 0.0f)
                ? std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / lenSq, 0.0f, 1.0f)
                : 0.0f;
            Vec2 proj = {a.x + ab.x * t, a.y + ab.y * t};
            float dx = p.x - proj.x;
            float dy = p.y - proj.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > maxD) {
                maxD = dist;
                idx = i;
            }
        }

        if (maxD > tolerance && idx >= 0) {
            keep[idx] = 1;
            stack.push_back({first, idx});
            stack.push_back({idx, last});
        }
    }

    for (size_t i = 0; i < n; i++) {
        if (keep[i]) {
            outPts.push_back(pts[i]);
            outPress.push_back(i < pressures.size() ? pressures[i] : 1.0f);
        }
    }
}
