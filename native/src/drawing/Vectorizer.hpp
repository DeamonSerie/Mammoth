#pragma once
#include "../app/Types.hpp"
#include "VectorStroke.hpp"
#include <vector>

// Raster -> vector converter. Takes a rectangle of a layer's already-rasterized
// drawing (alpha-masked) and returns the same content as smooth variable-width
// VectorStrokes, so an edit by another tool can be committed back as vectors
// exactly like a freshly drawn brush stroke.
//
// Pipeline (math per the research in docs/notes):
//   1. Binary mask  : alpha >= A_THRESHOLD inside the region.
//   2. Exact EDT    : Felzenszwalb-Huttenlocher O(n) squared-distance transform
//                     (lower envelope of parabolas, rows then columns).
//   3. Skeleton     : Zhang-Suen thinning to 1px-wide medial axis.
//   4. Graph tracing: endpoints / junctions, branch walk, spur pruning by the
//                     local medial radius.
//   5. Smoothing    : Douglas-Peucker + a corner-aware Chaikin pass.
//   6. Width        : 2 * sqrt(edt) along the centerline (the "width function"),
//                     color sampled at the centerline.
namespace Vectorizer {

// Skeleton pixel alpha threshold (8-bit). Anti-aliased soft edges below this
// are intentionally dropped so the traced silhouette is clean.
constexpr uint8_t kAlphaThreshold = 40;

std::vector<VectorStroke> traceRegion(const Layer& layer, const Rect& region);

} // namespace Vectorizer