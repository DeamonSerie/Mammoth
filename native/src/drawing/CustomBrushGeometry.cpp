#include "CustomBrushGeometry.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

namespace CustomBrushGeometry {

static constexpr float TWO_PI = 6.28318530717958647692f;

float baseSegmentSpan() {
    return TWO_PI / (float)CUSTOM_SECONDARY_COUNT;
}

float segmentProfile(CurveType type, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float u = 2.0f * t - 1.0f; // [-1, 1] across the segment
    switch (type) {
        case CurveType::Triangle:
            // Linear spike: triangular silhouette.
            return 1.0f - std::fabs(u);
        case CurveType::Square: {
            // Flat plateau with steep sides: square edge silhouette.
            const float edge = 0.35f;
            return std::clamp(std::min(t, 1.0f - t) / edge, 0.0f, 1.0f);
        }
        case CurveType::Circle:
        default:
            // Rounded dome: circular arc silhouette.
            return std::sqrt(std::max(0.0f, 1.0f - u * u));
    }
}

namespace {

struct Segment {
    float startAngle;
    float span;
    float radiusScale;
    CurveType type;
};

// Builds the ordered segment layout from a resolved configuration.
void buildSegments(const ResolvedCustomBrush& resolved,
                   std::vector<Segment>& out) {
    out.clear();
    float angle = 0.0f;
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) {
        const CombinedCurve& cc = resolved.curves[i];
        Segment s;
        s.startAngle = angle;
        s.span = baseSegmentSpan() * cc.width;
        s.radiusScale = cc.height;
        s.type = cc.type;
        angle += s.span;
        out.push_back(s);
    }
}

} // namespace

void stamp(Layer& layer, float cx, float cy, float radius,
           const ResolvedCustomBrush& resolved, const Color& color)
{
    if (radius <= 0.0f || !resolved.isValid()) return;

    std::vector<Segment> segments;
    buildSegments(resolved, segments);

    // Total angular coverage may exceed or fall short of 2*PI when widths are
    // scaled; wrap angles against the actual total.
    float totalSpan = 0.0f;
    for (const auto& s : segments) totalSpan += s.span;
    if (totalSpan <= 0.0f) return;

    int maxR = (int)std::ceil(radius * CUSTOM_MAX_HEIGHT);
    Color c = color;

    for (int dy = -maxR; dy <= maxR; dy++) {
        for (int dx = -maxR; dx <= maxR; dx++) {
            float d = std::sqrt((float)(dx * dx + dy * dy));
            if (d > radius * CUSTOM_MAX_HEIGHT) continue;

            float theta = std::atan2((float)dy, (float)dx);
            if (theta < 0.0f) theta += TWO_PI;
            float wrapped = std::fmod(theta, totalSpan);

            // Locate the segment containing this angle.
            const Segment* seg = nullptr;
            float localT = 0.0f;
            float acc = wrapped;
            for (size_t si = 0; si < segments.size(); si++) {
                const Segment& s = segments[si];
                bool last = (si == segments.size() - 1);
                if (acc < s.span || last) {
                    seg = &s;
                    localT = (s.span > 0.0f) ? std::clamp(acc / s.span, 0.0f, 1.0f) : 0.0f;
                    break;
                }
                acc -= s.span;
            }
            if (!seg) continue;

            float rMax = radius * seg->radiusScale;
            if (d > rMax) continue;

            float profile = segmentProfile(seg->type, localT);
            if (d > rMax * profile) continue;

            layer.blendPixel((int)std::round(cx) + dx, (int)std::round(cy) + dy, c);
        }
    }
}

int coveragePixelCount(const ResolvedCustomBrush& resolved, float radius) {
    int pad = (int)std::ceil(radius * CUSTOM_MAX_HEIGHT) + 2;
    int size = pad * 2 + 1;
    Layer layer(size, size);
    stamp(layer, (float)pad, (float)pad, radius, resolved, Color::white());
    int count = 0;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            if (layer.getPixel(x, y).a > 0) count++;
    return count;
}

} // namespace CustomBrushGeometry
