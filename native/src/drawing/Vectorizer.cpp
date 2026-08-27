#include "Vectorizer.hpp"
#include "../document/Layer.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

// ---------------------------------------------------------------------------
// 1. Exact Euclidean distance transform (Felzenszwalb & Huttenlocher, O(n)).
//    Computes the squared distance of every foreground cell to the nearest
//    background cell via a 1D lower envelope of parabolas, rows then columns.
// ---------------------------------------------------------------------------
namespace {

constexpr float INF = std::numeric_limits<float>::max() / 4.0f;

void edt1d(float* f, int n)
{
    std::vector<int> v(n, 0);
    std::vector<float> z(n + 1, INF);
    int k = 0;
    v[0] = 0;
    z[0] = -INF;
    z[1] = INF;
    for (int q = 1; q < n; q++) {
        float qq = f[q] + (float)(q * q);
        float kv = f[v[k]] + (float)(v[k] * v[k]);
        float s = (qq - kv) / (2.0f * (float)q - 2.0f * (float)v[k]);
        while (s <= z[k]) {
            k--;
            kv = f[v[k]] + (float)(v[k] * v[k]);
            s = (qq - kv) / (2.0f * (float)q - 2.0f * (float)v[k]);
        }
        k++;
        v[k] = q;
        z[k] = s;
        z[k + 1] = INF;
    }
    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k + 1] < (float)q) k++;
        float dx = (float)q - (float)v[k];
        f[q] = dx * dx + f[v[k]];
    }
}

struct BinaryGrid {
    int w = 0, h = 0;
    std::vector<uint8_t> fg;   // 1 = foreground (ink), 0 = background
    const uint8_t* base = nullptr; // pointer into Layer pixels (RGBA)
    int layerW = 0, layerH = 0;
    int ox = 0, oy = 0;        // region origin in layer pixel space

    uint8_t& at(int x, int y) { return fg[(size_t)y * w + x]; }
    uint8_t atc(int x, int y) const { return fg[(size_t)y * w + x]; }

    // Sample the layer colour at region pixel (x,y); transparent if out of the
    // *original* layer rectangle.
    Color colorAt(int x, int y) const {
        int lx = ox + x, ly = oy + y;
        if (lx < 0 || lx >= layerW || ly < 0 || ly >= layerH) {
            return Color::transparent();
        }
        size_t off = ((size_t)ly * layerW + (size_t)lx) * 4;
        return Color(base[off], base[off + 1], base[off + 2], base[off + 3]);
    }
};

BinaryGrid buildMask(const Layer& layer, const Rect& region)
{
    BinaryGrid g;
    int lw = layer.width(), lh = layer.height();
    int rx = (int)std::floor(region.x);
    int ry = (int)std::floor(region.y);
    int rw = (int)std::ceil(region.x + region.w) - rx;
    int rh = (int)std::ceil(region.y + region.h) - ry;

    // Clamp and expand by 1px so edge anti-aliasing is captured too.
    int x0 = std::max(0, rx - 1);
    int y0 = std::max(0, ry - 1);
    int x1 = std::min(lw, rx + rw + 1);
    int y1 = std::min(lh, ry + rh + 1);
    if (x1 <= x0 || y1 <= y0) return g;

    g.w = x1 - x0;
    g.h = y1 - y0;
    g.fg.assign((size_t)g.w * g.h, 0);
    g.ox = x0;
    g.oy = y0;
    g.layerW = lw;
    g.layerH = lh;
    g.base = layer.data();

    for (int y = y0; y < y1; y++) {
        const uint8_t* row = g.base + ((size_t)y * lw + (size_t)x0) * 4;
        for (int x = x0; x < x1; x++) {
            if (row[3] >= Vectorizer::kAlphaThreshold) {
                int px = x - x0, py = y - y0;
                g.at(px, py) = 1;
            }
            row += 4;
        }
    }
    return g;
}

void edt2d(BinaryGrid& g, std::vector<float>& edt)
{
    // The feature set is the BACKGROUND: f = 0 there, +inf at ink. The
    // transform then yields, at every ink pixel, the squared distance to the
    // nearest background cell -- exactly the local stroke radius.
    std::vector<float> f(g.fg.size());
    for (size_t i = 0; i < g.fg.size(); i++)
        f[i] = (g.fg[i] != 0) ? INF : 0.0f;

    std::vector<float> tmp(g.w, 0.0f);
    for (int y = 0; y < g.h; y++) {
        float* row = &f[(size_t)y * g.w];
        edt1d(row, g.w);
    }
    for (int x = 0; x < g.w; x++) {
        for (int y = 0; y < g.h; y++)
            tmp[y] = f[(size_t)y * g.w + x];
        edt1d(tmp.data(), g.h);
        for (int y = 0; y < g.h; y++)
            f[(size_t)y * g.w + x] = tmp[y];
    }
    edt = std::move(f);
}

// ---------------------------------------------------------------------------
// 2. Zhang-Suen thinning -> 1px medial-axis skeleton.
// ---------------------------------------------------------------------------
int binNeighborCount(const BinaryGrid& g, int x, int y)
{
    int c = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= g.w || ny < 0 || ny >= g.h) continue;
            if (g.atc(nx, ny)) c++;
        }
    return c;
}

bool zsDeletable(const BinaryGrid& g, int x, int y, int pass)
{
    if (!g.atc(x, y)) return false;
    int p2 = (y > 0) ? g.atc(x, y - 1) : 0;
    int p4 = (x + 1 < g.w) ? g.atc(x + 1, y) : 0;
    int p6 = (y + 1 < g.h) ? g.atc(x, y + 1) : 0;
    int p8 = (x > 0) ? g.atc(x - 1, y) : 0;
    int p3 = (x + 1 < g.w && y > 0) ? g.atc(x + 1, y - 1) : 0;
    int p5 = (x + 1 < g.w && y + 1 < g.h) ? g.atc(x + 1, y + 1) : 0;
    int p7 = (x > 0 && y + 1 < g.h) ? g.atc(x - 1, y + 1) : 0;
    int p9 = (x > 0 && y > 0) ? g.atc(x - 1, y - 1) : 0;

    int B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
    if (B < 2 || B > 6) return false;

    int A = 0;
    int seq[9] = {p2, p3, p4, p5, p6, p7, p8, p9, p2};
    for (int i = 0; i < 8; i++)
        if (seq[i] == 0 && seq[i + 1] == 1) A++;
    if (A != 1) return false;

    if (pass == 1) {
        if (p2 * p4 * p6 != 0) return false;
        if (p4 * p6 * p8 != 0) return false;
    } else {
        if (p2 * p4 * p8 != 0) return false;
        if (p2 * p6 * p8 != 0) return false;
    }
    return true;
}

void skeletonize(BinaryGrid& g)
{
    bool changed = true;
    while (changed) {
        changed = false;
        for (int pass = 1; pass <= 2; pass++) {
            std::vector<std::pair<int, int>> del;
            for (int y = 1; y < g.h - 1; y++)
                for (int x = 1; x < g.w - 1; x++)
                    if (zsDeletable(g, x, y, pass)) del.push_back({x, y});
            if (!del.empty()) {
                for (auto& p : del) g.at(p.first, p.second) = 0;
                changed = true;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 3. Skeleton graph: endpoints / junctions + branch tracing.
// ---------------------------------------------------------------------------
struct SkelPoint {
    int x, y;
    int deg;
};

std::vector<SkelPoint> collectSkeleton(const BinaryGrid& g, const std::vector<float>& edt,
                                       std::vector<Vec2>& outPts,
                                       std::vector<float>& outRadii)
{
    std::vector<SkelPoint> skel;
    for (int y = 0; y < g.h; y++) {
        for (int x = 0; x < g.w; x++) {
            if (!g.atc(x, y)) continue;
            SkelPoint sp;
            sp.x = x;
            sp.y = y;
            sp.deg = binNeighborCount(g, x, y);
            skel.push_back(sp);
            outPts.push_back({(float)(g.ox + x), (float)(g.oy + y)});
            float d = std::sqrt(edt[(size_t)y * g.w + x]);
            outRadii.push_back(std::max(0.6f, d));
        }
    }
    return skel;
}

} // namespace

std::vector<VectorStroke> Vectorizer::traceRegion(const Layer& layer, const Rect& region)
{
    std::vector<VectorStroke> strokes;

    BinaryGrid g = buildMask(layer, region);
    if (g.w <= 0 || g.h <= 0) return strokes;

    // Count opaque pixels; need at least a small blob to trace anything.
    size_t ink = 0;
    for (uint8_t v : g.fg) if (v) ink++;
    if (ink < 2) return strokes;

    std::vector<float> edt;
    edt2d(g, edt);
    skeletonize(g);

    // Extract the skeleton as pixel-space points. We consume interior pixels
    // once but let branches share endpoints/junctions, mirroring the standard
    // centerline-extraction pipeline.
    std::vector<Vec2> pts;
    std::vector<float> radii;
    std::vector<SkelPoint> skel = collectSkeleton(g, edt, pts, radii);
    if (skel.empty()) return strokes;

    // Map pixel index -> array index for fast degree lookup / visited checks.
    const int w = g.w, h = g.h;
    auto key = [w](int x, int y) { return (size_t)y * w + x; };
    std::vector<int> pxIndex((size_t)w * h, -1);
    for (size_t i = 0; i < skel.size(); i++)
        pxIndex[key(skel[i].x, skel[i].y)] = (int)i;
    auto skelAt = [&](int x, int y) -> const SkelPoint* {
        if (x < 0 || x >= w || y < 0 || y >= h) return nullptr;
        int idx = pxIndex[key(x, y)];
        return (idx >= 0) ? &skel[idx] : nullptr;
    };

    std::vector<char> visited((size_t)w * h, 0);
    std::vector<std::vector<int>> chains; // each chain = pixel indices

    // Trace from `start` (an endpoint or a seed), walking unvisited neighbours
    // and stopping at junctions/endpoints (which are shared, not consumed).
    auto traceFrom = [&](int startIdx) {
        std::vector<int> chain;
        chain.push_back(startIdx);
        visited[key(skel[startIdx].x, skel[startIdx].y)] = 1;
        if (skel[startIdx].deg != 2) {
            // Starting junction: consume along a single outgoing direction only
            // (the other directions belong to other branches).
        }
        int cur = startIdx;
        while (true) {
            const SkelPoint& sp = skel[cur];
            // Collect candidate unvisited neighbours.
            std::vector<int> cands;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    const SkelPoint* n = skelAt(sp.x + dx, sp.y + dy);
                    if (!n) continue;
                    if (visited[key(n->x, n->y)]) continue;
                    if (n->x == sp.x && n->y == sp.y) continue;
                    cands.push_back(pxIndex[key(n->x, n->y)]);
                }
            if (cands.empty()) break;
            // Prefer a continuation collinear with the last direction (turn rule).
            int best = cands[0];
            if (chain.size() >= 2) {
                const SkelPoint& a = skel[chain[chain.size() - 2]];
                const SkelPoint& b = skel[chain[chain.size() - 1]];
                float pdx = b.x - a.x, pdy = b.y - a.y;
                float bestDot = -9.0f;
                for (int c : cands) {
                    const SkelPoint& q = skel[c];
                    float fx = q.x - b.x, fy = q.y - b.y;
                    float dot = pdx * fx + pdy * fy;
                    if (dot > bestDot) { bestDot = dot; best = c; }
                }
            }
            int next = best;
            chain.push_back(next);
            visited[key(skel[next].x, skel[next].y)] = 1;
            if (skel[next].deg != 2) break; // ended at endpoint or junction
            cur = next;
        }
        return chain;
    };

    // Phase 1: trace from every endpoint.
    for (size_t i = 0; i < skel.size(); i++) {
        if (skel[i].deg == 1) chains.push_back(traceFrom((int)i));
    }

    // Phase 2: any remaining unvisited (closed loops / cycles): seed trace.
    for (size_t i = 0; i < skel.size(); i++) {
        if (!visited[key(skel[i].x, skel[i].y)])
            chains.push_back(traceFrom((int)i));
    }

    // Convert chains to strokes (smoothing handled by VectorStroke recomposition
    // plus a small Douglas-Peucker here to drop pixel-quantization jaggies).
    for (const std::vector<int>& chain : chains) {
        if (chain.empty()) continue;
        // Chain is pixel-quantized: decimate along straight runs first.
        std::vector<Vec2> pp;
        std::vector<float> rr;
        for (int idx : chain) {
            pp.push_back({(float)(g.ox + skel[idx].x), (float)(g.oy + skel[idx].y)});
            float d = std::sqrt(edt[(size_t)skel[idx].y * w + skel[idx].x]);
            rr.push_back(std::max(0.6f, d));
        }

        VectorStroke stroke;
        stroke.color = g.colorAt(skel[chain.front()].x, skel[chain.front()].y);
        stroke.hardness = 0.9f;
        stroke.controls = pp;
        stroke.radii = rr;
        stroke.opacities.assign(pp.size(), 1.0f);
        strokes.push_back(std::move(stroke));
    }

    return strokes;
}