#pragma once
#include "CustomBrushConfig.hpp"
#include "../document/Layer.hpp"
#include <vector>

// Geometry-generation stage of the custom brush pipeline.
//
// Converts a resolved custom brush configuration into pixel coverage for a
// single stamp. The stamp is composed of eight segments arranged radially
// around the center; each resulting combined curve controls one segment:
//   - type   -> radial silhouette profile across the segment
//               (Triangle: linear spike, Square: flat plateau,
//                Circle: rounded dome)
//   - width  -> angular span scale (base span is 360 / 8 degrees)
//   - height -> radial extent scale
namespace CustomBrushGeometry {

// Angular span (radians) of one segment before width scaling.
float baseSegmentSpan();

// Radial coverage fraction [0,1] at normalized position t in [0,1] within a
// segment of the given curve type. Deterministic and unit-testable.
float segmentProfile(CurveType type, float t);

// Reusable per-stamp geometry. Build once per radius, then test pixel
// coverage with covers() — shared by the brush stamp and the shape eraser.
class StampMask {
public:
    StampMask() = default;

    // Resolves `resolved` geometry for a stamp of the given base radius.
    // Returns false when the configuration is unusable.
    bool build(const ResolvedCustomBrush& resolved, float radius);

    // True when the pixel at offset (dx,dy) from the stamp center is covered
    // by the shape (i.e. would be painted/erased by a stamp centered here).
    bool covers(int dx, int dy) const;

    bool valid() const { return m_valid; }
    float radius() const { return m_radius; }
    int maxRadius() const { return m_maxR; }

private:
    struct Segment {
        float startAngle;
        float span;
        float radiusScale;
        CurveType type;
    };
    std::vector<Segment> m_segments;
    float m_totalSpan = 0.0f;
    float m_radius = 0.0f;
    int m_maxR = 0;
    bool m_valid = false;
};

// Stamps the resolved custom brush at (cx, cy) with overall radius `radius`
// into `layer`, blending with the given color. Respects brush opacity.
void stamp(Layer& layer, float cx, float cy, float radius,
           const ResolvedCustomBrush& resolved, const Color& color);

// Number of covered pixels for a mask of the given radius (for tests).
int coveragePixelCount(const ResolvedCustomBrush& resolved, float radius);

} // namespace CustomBrushGeometry
