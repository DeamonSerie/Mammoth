#pragma once
#include "Brush.hpp"
#include "../document/Layer.hpp"

class VectorBrushEngine {
public:
    VectorBrushEngine();

    /// Render a single stroke segment (thick line with round caps) directly onto a layer.
    /// This is the core vector rendering primitive: no scalloping, no overlap artifacts.
    /// Constant radius/alpha (back-compat).
    static void renderStrokeSegment(Layer& layer, Vec2 from, Vec2 to,
                                    float radius, float hardness,
                                    const Color& c, float alpha);

    /// Variable-width, pressure-aware segment: radius and alpha taper from the
    /// start endpoint to the end endpoint, giving pencil-like strokes.
    static void renderStrokeSegment(Layer& layer, Vec2 from, Vec2 to,
                                    float rFrom, float rTo, float hardness,
                                    const Color& c, float aFrom, float aTo);

    /// Render a whole stroke from a polyline plus a parallel pressure list,
    /// applying the brush's pressure response. Used by both the live app and tests.
    static void renderStroke(Layer& layer, const std::vector<Vec2>& pts,
                             const std::vector<float>& pressures, const Brush& b);

    /// Douglas-Peucker simplification of a (position, pressure) polyline.
    /// Points whose distance from the simplified line exceeds `tolerance` are
    /// kept; the rest are dropped. `outPts`/`outPress` are filled in order.
    static void simplifyPath(const std::vector<Vec2>& pts,
                             const std::vector<float>& pressures,
                             float tolerance,
                             std::vector<Vec2>& outPts,
                             std::vector<float>& outPress);
};