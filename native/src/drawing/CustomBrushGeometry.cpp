#include "CustomBrushGeometry.hpp"
#include "BrushConfig.hpp"
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

bool StampMask::build(const ResolvedCustomBrush& resolved, float radius) {
    m_segments.clear();
    m_valid = false;
    m_radius = radius;
    m_maxR = 0;
    if (radius <= 0.0f || !resolved.isValid()) return false;

    // Build the ordered segment layout from the resolved configuration.
    float angle = 0.0f;
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) {
        const CombinedCurve& cc = resolved.curves[i];
        Segment s;
        s.startAngle = angle;
        s.span = baseSegmentSpan() * cc.width;
        s.radiusScale = cc.height;
        s.type = cc.type;
        angle += s.span;
        m_segments.push_back(s);
    }
    m_totalSpan = 0.0f;
    for (const auto& s : m_segments) m_totalSpan += s.span;
    if (m_totalSpan <= 0.0f) return false;

    m_maxR = (int)std::ceil(radius * CUSTOM_MAX_HEIGHT);
    m_valid = true;
    return true;
}

bool StampMask::covers(int dx, int dy) const {
    if (!m_valid) return false;

    float d = std::sqrt((float)(dx * dx + dy * dy));
    if (d > m_radius * CUSTOM_MAX_HEIGHT) return false;

    float theta = std::atan2((float)dy, (float)dx);
    if (theta < 0.0f) theta += TWO_PI;
    float wrapped = std::fmod(theta, m_totalSpan);

    // Locate the segment containing this angle.
    const Segment* seg = nullptr;
    float localT = 0.0f;
    float acc = wrapped;
    for (size_t si = 0; si < m_segments.size(); si++) {
        const Segment& s = m_segments[si];
        bool last = (si == m_segments.size() - 1);
        if (acc < s.span || last) {
            seg = &s;
            localT = (s.span > 0.0f) ? std::clamp(acc / s.span, 0.0f, 1.0f) : 0.0f;
            break;
        }
        acc -= s.span;
    }
    if (!seg) return false;

    float rMax = m_radius * seg->radiusScale;
    if (d > rMax) return false;

    float profile = segmentProfile(seg->type, localT);
    if (d > rMax * profile) return false;

    return true;
}

void stamp(Layer& layer, float cx, float cy, float radius,
           const ResolvedCustomBrush& resolved, const Color& color)
{
    StampMask mask;
    if (!mask.build(resolved, radius)) return;

    int centerX = (int)std::round(cx);
    int centerY = (int)std::round(cy);
    Color c = color;
    int maxR = mask.maxRadius();

    for (int dy = -maxR; dy <= maxR; dy++) {
        for (int dx = -maxR; dx <= maxR; dx++) {
            if (!mask.covers(dx, dy)) continue;
            layer.blendPixel(centerX + dx, centerY + dy, c);
        }
    }
}

int coveragePixelCount(const ResolvedCustomBrush& resolved, float radius) {
    StampMask mask;
    if (!mask.build(resolved, radius)) return 0;
    int count = 0;
    int maxR = mask.maxRadius();
    for (int y = -maxR; y <= maxR; y++)
        for (int x = -maxR; x <= maxR; x++)
            if (mask.covers(x, y)) count++;
    return count;
}

} // namespace CustomBrushGeometry
