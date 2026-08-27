#include "VectorStroke.hpp"
#include "../document/Layer.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <algorithm>

#undef max
#undef min

// ---------------------------------------------------------------------------
// Shared ribbon rasterizer: the stroke is the UNION of its sample discs, so
// per pixel we keep the maximum coverage any disc contributes (plus the
// opacity of the disc that dominates). Features a supersampled soft edge band
// so the committed silhouette matches the live preview's character.
// ---------------------------------------------------------------------------
void renderVectorRibbon(Layer& layer,
                        const std::vector<Vec2>& pts,
                        const std::vector<float>& radii,
                        const std::vector<float>& opacities,
                        const Color& col, float hardness)
{
    if (pts.empty() || pts.size() != radii.size()) return;

    const float maxR = *std::max_element(radii.begin(), radii.end());
    if (maxR < 0.5f) return;

    float minX = pts[0].x, minY = pts[0].y, maxX = pts[0].x, maxY = pts[0].y;
    for (const Vec2& p : pts) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }

    int ox = (int)std::floor(minX - maxR) - 1;
    int oy = (int)std::floor(minY - maxR) - 1;
    int bw = (int)std::ceil(maxX + maxR + 1.0f) - ox;
    int bh = (int)std::ceil(maxY + maxR + 1.0f) - oy;
    if (bw <= 0 || bh <= 0) return;

    const size_t nb = (size_t)bw * (size_t)bh;
    std::vector<float> cov(nb, 0.0f);
    std::vector<float> opa(nb, 1.0f);

    const int SUB = 8;   // supersampling for the silhouette (64 alpha levels)
    const float invN = 1.0f / (float)SUB;

    const size_t n = pts.size();
    for (size_t i = 0; i < n; i++) {
        const Vec2& a = pts[i];
        const Vec2 bpt = (i + 1 < n) ? pts[i + 1] : pts[i];
        Vec2 d = {bpt.x - a.x, bpt.y - a.y};
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        float r0 = radii[i];
        float r1 = (i + 1 < n) ? radii[i + 1] : radii[i];
        float o0 = opacities[i];
        float o1 = (i + 1 < n) ? opacities[i + 1] : opacities[i];
        int steps = std::max(1, (int)std::ceil(len / 0.5f));
        if (n == 1) steps = 1;

        for (int s = 0; s <= steps; s++) {
            float t = (float)s / (float)steps;
            Vec2 p = {a.x + d.x * t, a.y + d.y * t};

            float radius = r0 + (r1 - r0) * t;
            if (radius < 0.5f) continue;
            float strokeAlpha = o0 + (o1 - o0) * t;
            float solidR = radius * hardness;     // hard core (soft brushes shade this->edge)
            float fade = radius - solidR;         // soft-feather band width (0 for hardness 1)
            const float skirt = 1.0f;             // always-on ~1px anti-aliasing at the silhouette
            float iInner = radius - std::max(fade, skirt); // innermost point with full geometric cover

            int x0 = std::max(0, (int)std::floor(p.x - radius - 1.0f) - ox);
            int y0 = std::max(0, (int)std::floor(p.y - radius - 1.0f) - oy);
            int x1 = std::min(bw - 1, (int)std::ceil(p.x + radius + 1.0f) - ox);
            int y1 = std::min(bh - 1, (int)std::ceil(p.y + radius + 1.0f) - oy);

            for (int yy = y0; yy <= y1; yy++) {
                for (int xx = x0; xx <= x1; xx++) {
                    float cx = (float)(xx + ox) + 0.5f;
                    float cy = (float)(yy + oy) + 0.5f;
                    float dxp = cx - p.x;
                    float dyp = cy - p.y;
                    float dist = std::sqrt(dxp * dxp + dyp * dyp);
                    if (dist >= radius + 0.71f) continue; // beyond the outer hairline
                                                        // (max sub-sample offset is ~0.707)

                    // Geometric coverage (always anti-aliased): 4x4 supersample of
                    // the silhouette so hardness=1 gets a smooth ~1px edge too.
                    float geo;
                    if (dist <= iInner) {
                        geo = 1.0f;
                    } else if (dist <= radius) {
                        float sum = 0.0f;
                        for (int sy = 0; sy < SUB; sy++) {
                            float pyy = cy + ((float)sy + 0.5f) * invN - 0.5f;
                            for (int sx = 0; sx < SUB; sx++) {
                                float pxx = cx + ((float)sx + 0.5f) * invN - 0.5f;
                                float ddx = pxx - p.x;
                                float ddy = pyy - p.y;
                                float ds = std::sqrt(ddx * ddx + ddy * ddy);
                                if (ds < radius) sum += 1.0f;
                            }
                        }
                        geo = sum * (1.0f / (float)(SUB * SUB));
                    } else {
                        // Center sits just outside the nominal radius but the pixel
                        // still straddles the silhouette: use the supersample result.
                        float sum = 0.0f;
                        for (int sy = 0; sy < SUB; sy++) {
                            float pyy = cy + ((float)sy + 0.5f) * invN - 0.5f;
                            for (int sx = 0; sx < SUB; sx++) {
                                float pxx = cx + ((float)sx + 0.5f) * invN - 0.5f;
                                float ddx = pxx - p.x;
                                float ddy = pyy - p.y;
                                float ds = std::sqrt(ddx * ddx + ddy * ddy);
                                if (ds < radius) sum += 1.0f;
                            }
                        }
                        geo = sum * (1.0f / (float)(SUB * SUB));
                    }
                    // Soft-brush shading (independent of the geometric edge).
                    float shade = 1.0f;
                    if (fade > 0.0f && dist > solidR) {
                        shade = (dist < radius) ? (1.0f - (dist - solidR) / fade) : 0.0f;
                    }
                    float aa = geo * shade;
                    if (aa <= 0.002f) continue;

                    size_t idx = (size_t)yy * bw + (size_t)xx;
                    if (aa > cov[idx]) {
                        cov[idx] = aa;
                        opa[idx] = strokeAlpha;
                    }
                }
            }
        }
    }

    for (int yy = 0; yy < bh; yy++) {
        for (int xx = 0; xx < bw; xx++) {
            size_t idx = (size_t)yy * bw + (size_t)xx;
            float aa = cov[idx];
            if (aa <= 0.002f) continue;
            Color stamp = col;
            stamp.a = (uint8_t)(col.a * opa[idx] * aa);
            if (stamp.a == 0) continue;
            layer.blendPixel(xx + ox, yy + oy, stamp);
        }
    }
}

// ---------------------------------------------------------------------------
// VectorStroke
// ---------------------------------------------------------------------------
VectorStroke VectorStroke::fromPenStream(const std::vector<Vec2>& pts,
                                         const std::vector<float>& pressures,
                                         const Brush& b)
{
    VectorStroke s;
    if (pts.empty()) return s;
    s.color = b.color();
    s.hardness = b.hardness();
    s.controls = pts;
    s.radii.resize(pts.size());
    s.opacities.resize(pts.size());
    for (size_t i = 0; i < pts.size(); i++) {
        float p = (i < pressures.size()) ? pressures[i] : 1.0f;
        s.radii[i] = b.radiusForPressure(p);
        s.opacities[i] = b.opacityForPressure(p);
    }
    return s;
}

VectorStroke VectorStroke::fromCenterline(const std::vector<Vec2>& centerline,
                                          const std::vector<float>& widths,
                                          const std::vector<float>& opacities,
                                          const Color& c, float hard)
{
    VectorStroke s;
    if (centerline.empty()) return s;
    s.color = c;
    s.hardness = hard;
    s.controls = centerline;
    s.radii.resize(centerline.size());
    s.opacities.resize(centerline.size());
    for (size_t i = 0; i < centerline.size(); i++) {
        s.radii[i] = (i < widths.size()) ? widths[i] * 0.5f : 1.0f;
        s.opacities[i] = (i < opacities.size() && opacities[i] > 0.0f)
            ? opacities[i]
            : 1.0f;
    }
    return s;
}

struct CurveVertex {
    Vec2 b0, b1, b2, b3;
    float r0, r1, o0, o1;
};

static void emitIfDistinct(std::vector<Vec2>& outPts,
                           std::vector<float>& outRadii,
                           std::vector<float>& outOpa,
                           const Vec2& p, float radius, float opa)
{
    if (!outPts.empty()) {
        float dx = outPts.back().x - p.x;
        float dy = outPts.back().y - p.y;
        if (dx * dx + dy * dy < 0.0625f) return; // < 0.25px: dedupe
    }
    outPts.push_back(p);
    outRadii.push_back(radius);
    outOpa.push_back(opa);
}

static void subdivideCatmullSeg(const CurveVertex& cv, size_t depth,
                                float epsPx,
                                float tLo, float tHi,
                                std::vector<Vec2>& outPts,
                                std::vector<float>& outRadii,
                                std::vector<float>& outOpa)
{
    if (depth > 24) {
        // Emit the segment midpoint (de Casteljau at t=0.5).
        float tt = 0.5f * (tLo + tHi);
        Vec2 m = {(cv.b0.x + 3.0f * cv.b1.x + 3.0f * cv.b2.x + cv.b3.x) / 8.0f,
                  (cv.b0.y + 3.0f * cv.b1.y + 3.0f * cv.b2.y + cv.b3.y) / 8.0f};
        emitIfDistinct(outPts, outRadii, outOpa,
                       m, cv.r0 + (cv.r1 - cv.r0) * tt, cv.o0 + (cv.o1 - cv.o0) * tt);
        return;
    }

    // Flatness stop ("stop at a given point"): the Bézier is a convex
    // combination of its control points, so once both middle control points
    // lie within epsilonPx of the chord b0->b3 the entire curve does too.
    // This stops immediately for collinear (straight) control points.
    float cdx = cv.b3.x - cv.b0.x;
    float cdy = cv.b3.y - cv.b0.y;
    float chordLenSq = cdx * cdx + cdy * cdy;
    float maxDev = 0.0f;
    if (chordLenSq > 1e-6f) {
        float inv = 1.0f / std::sqrt(chordLenSq);
        float d1 = std::fabs(cdx * (cv.b1.y - cv.b0.y) - cdy * (cv.b1.x - cv.b0.x)) * inv;
        float d2 = std::fabs(cdx * (cv.b2.y - cv.b0.y) - cdy * (cv.b2.x - cv.b0.x)) * inv;
        maxDev = std::max(d1, d2);
    } else if (chordLenSq == 0.0f && depth >= 4) {
        maxDev = 0.0f; // degenerate point: settle after a few splits
    } else {
        maxDev = 1.0f; // force depth on a zero-length chord until length forms
    }
    if (maxDev <= epsPx) {
        float tt = 0.5f * (tLo + tHi);
        Vec2 m = {(cv.b0.x + 3.0f * cv.b1.x + 3.0f * cv.b2.x + cv.b3.x) / 8.0f,
                  (cv.b0.y + 3.0f * cv.b1.y + 3.0f * cv.b2.y + cv.b3.y) / 8.0f};
        emitIfDistinct(outPts, outRadii, outOpa,
                       m, cv.r0 + (cv.r1 - cv.r0) * tt, cv.o0 + (cv.o1 - cv.o0) * tt);
        return;
    }

    float tMid = 0.5f * (tLo + tHi);

    // Left half (de Casteljau at t=0.5).
    Vec2 l1 = {(cv.b0.x + cv.b1.x) * 0.5f, (cv.b0.y + cv.b1.y) * 0.5f};
    Vec2 l2 = {(cv.b0.x + 2.0f * cv.b1.x + cv.b2.x) * 0.25f,
               (cv.b0.y + 2.0f * cv.b1.y + cv.b2.y) * 0.25f};
    Vec2 l3 = {(cv.b0.x + 3.0f * cv.b1.x + 3.0f * cv.b2.x + cv.b3.x) * 0.125f,
               (cv.b0.y + 3.0f * cv.b1.y + 3.0f * cv.b2.y + cv.b3.y) * 0.125f};
    Vec2 r1 = {(cv.b1.x + 2.0f * cv.b2.x + cv.b3.x) * 0.25f,
               (cv.b1.y + 2.0f * cv.b2.y + cv.b3.y) * 0.25f};
    Vec2 r2 = {(cv.b2.x + cv.b3.x) * 0.5f, (cv.b2.y + cv.b3.y) * 0.5f};

    CurveVertex left = {cv.b0, l1, l2, l3, cv.r0, cv.r1, cv.o0, cv.o1};
    subdivideCatmullSeg(left, depth + 1, epsPx, tLo, tMid,
                        outPts, outRadii, outOpa);

    CurveVertex right = {l3, r1, r2, cv.b3, cv.r0, cv.r1, cv.o0, cv.o1};
    subdivideCatmullSeg(right, depth + 1, epsPx, tMid, tHi,
                        outPts, outRadii, outOpa);
}

void VectorStroke::sample(float epsilonPx,
                          std::vector<Vec2>& outPts,
                          std::vector<float>& outRadii,
                          std::vector<float>& outOpa) const
{
    outPts.clear();
    outRadii.clear();
    outOpa.clear();
    size_t n = controls.size();
    if (n == 0) return;

    if (n == 1) {
        emitIfDistinct(outPts, outRadii, outOpa, controls[0], radii[0], opacities[0]);
        return;
    }

    const float eps = std::max(0.1f, epsilonPx);

    // Anchor the polyline at the stroke's first control point so no leading
    // gap is left behind by the first recursion leaf.
    emitIfDistinct(outPts, outRadii, outOpa, controls[0], radii[0], opacities[0]);

    for (size_t idx = 0; idx + 1 < n; idx++) {
        const Vec2& p0 = (idx > 0) ? controls[idx - 1] : controls[idx];
        const Vec2& p1 = controls[idx];
        const Vec2& p2 = controls[idx + 1];
        const Vec2& p3 = (idx + 2 < n) ? controls[idx + 2] : controls[idx + 1];

        CurveVertex cv;
        cv.b0 = p1;
        cv.b3 = p2;
        cv.b1 = {p1.x + (p2.x - p0.x) / 6.0f, p1.y + (p2.y - p0.y) / 6.0f};
        cv.b2 = {p2.x - (p3.x - p1.x) / 6.0f, p2.y - (p3.y - p1.y) / 6.0f};
        cv.r0 = (idx < radii.size()) ? radii[idx] : 1.0f;
        cv.r1 = (idx + 1 < radii.size()) ? radii[idx + 1] : cv.r0;
        cv.o0 = (idx < opacities.size()) ? opacities[idx] : 1.0f;
        cv.o1 = (idx + 1 < opacities.size()) ? opacities[idx + 1] : cv.o0;

        subdivideCatmullSeg(cv, 0, eps, 0.0f, 1.0f, outPts, outRadii, outOpa);

        // Always carry the segment through its shared control point so a flat
        // (straight) segment still yields a connected polyline end-to-end.
        emitIfDistinct(outPts, outRadii, outOpa, p2,
                       (idx + 1 < radii.size()) ? radii[idx + 1] : cv.r1,
                       (idx + 1 < opacities.size()) ? opacities[idx + 1] : cv.o1);
    }
}

void VectorStroke::render(Layer& layer, float epsilonPx) const
{
    if (controls.size() < 2) {
        if (controls.size() == 1) {
            std::vector<Vec2> pts = {controls[0]};
            std::vector<float> rs = {radii.empty() ? 1.0f : radii[0]};
            std::vector<float> os = {opacities.empty() ? 1.0f : opacities[0]};
            renderVectorRibbon(layer, pts, rs, os, color, hardness);
        }
        return;
    }

    std::vector<Vec2> pts;
    std::vector<float> rs;
    std::vector<float> os;
    sample(epsilonPx, pts, rs, os);
    if (pts.empty()) return;
    renderVectorRibbon(layer, pts, rs, os, color, hardness);
}

Rect VectorStroke::bounds() const
{
    if (controls.empty()) return {0, 0, 0, 0};
    float minX = controls[0].x, minY = controls[0].y, maxX = controls[0].x, maxY = controls[0].y;
    float maxR = 0.0f;
    for (size_t i = 0; i < controls.size(); i++) {
        const Vec2& p = controls[i];
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        maxR = std::max(maxR, (i < radii.size()) ? radii[i] : 0.0f);
    }
    return {minX - maxR - 1.0f, minY - maxR - 1.0f,
            (maxX - minX) + 2.0f * maxR + 2.0f, (maxY - minY) + 2.0f * maxR + 2.0f};
}

bool VectorStroke::intersects(const Rect& r) const
{
    Rect b = bounds();
    if (b.w <= 0 || b.h <= 0) return false;
    return b.x < r.x + r.w && b.x + b.w > r.x &&
           b.y < r.y + r.h && b.y + b.h > r.y;
}