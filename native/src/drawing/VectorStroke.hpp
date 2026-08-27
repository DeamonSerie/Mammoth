#pragma once
#include "../app/Types.hpp"
#include "Brush.hpp"
#include <vector>

class Layer;

// Shared coverage-field ribbon rasterizer (pts + per-point radius/opacity).
// Used by both VectorStroke::render and VectorBrushEngine::renderStroke so the
// committed stroke and the pressure-tapered commit path use one implementation.
void renderVectorRibbon(Layer& layer,
                        const std::vector<Vec2>& pts,
                        const std::vector<float>& radii,
                        const std::vector<float>& opacities,
                        const Color& col, float hardness);

struct VectorStroke {
    // Catmull-Rom control points (the simplified pressure stream).
    std::vector<Vec2> controls;
    // Radius and opacity at each control point (the "width function").
    std::vector<float> radii;
    std::vector<float> opacities;
    Color color;
    float hardness = 0.8f;

    // Build from a raw pen stream (positions + pressures) via Douglas-Peucker
    // simplification and the brush's pressure response.
    static VectorStroke fromPenStream(const std::vector<Vec2>& pts,
                                      const std::vector<float>& pressures,
                                      const Brush& b);

    // Build from a traced centerline (output of the raster-to-vector tracer).
    static VectorStroke fromCenterline(const std::vector<Vec2>& centerline,
                                       const std::vector<float>& widths,
                                       const std::vector<float>& opacities,
                                       const Color& c, float hard);

    // --- The "one or more recursive lines of code that stop at a given
    //     point" -----------------------------------------------
    // Recursive Bézier subdivision: a Catmull-Rom segment is converted to a
    // cubic Bézier, then subdivided via de Casteljau at t=0.5.  The recursion
    // stops when the flatness test
    //   |P0-2P1+P2| + |P1-2P2+P3| <= epsilonPx
    // (the alpha/beta criterion) is satisfied — i.e. the segment is flat to
    // within epsilonPx canvas pixels.  Each leaf emits its midpoint with
    // radius/opacity interpolated linearly along the segment.
    void sample(float epsilonPx,
                std::vector<Vec2>& outPts,
                std::vector<float>& outRadii,
                std::vector<float>& outOpa) const;

    // Render the vector stroke into a layer's pixels using a coverage-field
    // ribbon with supersampled antialiasing — identical in character to the
    // live preview so the committed stroke matches what the user drew.
    void render(Layer& layer, float epsilonPx = 0.5f) const;

    // Bounding box (inflated by widest radius).
    Rect bounds() const;

    // Test whether this stroke intersects a rectangle.
    bool intersects(const Rect& r) const;
};
