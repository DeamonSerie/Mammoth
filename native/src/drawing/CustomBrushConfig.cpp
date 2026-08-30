#include "CustomBrushConfig.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>

bool ResolvedCustomBrush::isValid() const {
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) {
        if ((unsigned)curves[i].type >= (unsigned)curveTypeCount()) return false;
        if (!std::isfinite(curves[i].height) ||
            curves[i].height < CUSTOM_MIN_DIM || curves[i].height > CUSTOM_MAX_HEIGHT) return false;
        if (!std::isfinite(curves[i].width) ||
            curves[i].width < CUSTOM_MIN_DIM || curves[i].width > CUSTOM_MAX_WIDTH) return false;
    }
    return true;
}

bool ResolvedCustomBrush::operator==(const ResolvedCustomBrush& o) const {
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) {
        if (curves[i].type != o.curves[i].type) return false;
        if (curves[i].height != o.curves[i].height) return false;
        if (curves[i].width != o.curves[i].width) return false;
    }
    return true;
}

static float clampRange(float v, float lo, float hi) {
    if (std::isnan(v)) return lo;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool CustomBrushConfig::isAutoSecondary(int slot) {
    // Every third curve in the 8-slot subdivision sequence is automatic.
    return slot == 2 || slot == 5;
}

int CustomBrushConfig::controllingPrimary(int slot) {
    // Each primary piece subdivides into two secondary sections.
    return slot / 2;
}

CurveType CustomBrushConfig::autoSecondary(int slot) const {
    CurveType p = primary[controllingPrimary(slot)];
    // Deterministic derivation reusing the combination table:
    //   Triangle -> Square, Square -> Circle, Circle -> Square.
    return combineCurves(p, p);
}

CurveType CustomBrushConfig::effectiveSecondary(int slot) const {
    if (isAutoSecondary(slot)) return autoSecondary(slot);
    float v = clampRange(secondaryParam[slot], 0.0f, 1.0f);
    int band = (int)(v * 3.0f);
    if (band > 2) band = 2; // v == 1.0 maps into the Triangle band
    return (CurveType)band;
}

bool CustomBrushConfig::setSecondaryCurve(int slot, CurveType t) {
    if (slot < 0 || slot >= CUSTOM_SECONDARY_COUNT) return false;
    if ((unsigned)t >= (unsigned)curveTypeCount()) return false;
    if (isAutoSecondary(slot)) return false; // cannot override system values
    secondaryParam[slot] = ((float)t + 0.5f) / 3.0f; // centre of the band
    return true;
}

void CustomBrushConfig::setSecondaryParam(int slot, float v) {
    if (slot < 0 || slot >= CUSTOM_SECONDARY_COUNT) return;
    secondaryParam[slot] = clampRange(v, 0.0f, 1.0f);
}

void CustomBrushConfig::setHeight(int slot, float v) {
    if (slot < 0 || slot >= CUSTOM_SECONDARY_COUNT) return;
    height[slot] = clampRange(v, CUSTOM_MIN_DIM, CUSTOM_MAX_HEIGHT);
}

void CustomBrushConfig::setWidth(int slot, float v) {
    if (slot < 0 || slot >= CUSTOM_SECONDARY_COUNT) return;
    width[slot] = clampRange(v, CUSTOM_MIN_DIM, CUSTOM_MAX_WIDTH);
}

ResolvedCustomBrush CustomBrushConfig::resolve() const {
    ResolvedCustomBrush out;

    // Stage 1: effective secondary curves (with automatic resolution).
    CurveType eff[CUSTOM_SECONDARY_COUNT];
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) eff[i] = effectiveSecondary(i);

    // Stage 2: each slice combines its two secondary sections with the
    // controlling primary piece (Division 1 determines Division 2), so every
    // primary switch visibly changes the resolved geometry. Both resulting
    // curves of a slice receive the combined type (the table is commutative),
    // while height and width stay independent per curve.
    for (int p = 0; p < CUSTOM_PRIMARY_COUNT; p++) {
        int a = p * 2;
        int b = p * 2 + 1;
        CurveType combined = combineCurves(primary[p],
                                           combineCurves(eff[a], eff[b]));
        for (int k = 0; k < 2; k++) {
            CombinedCurve cc;
            cc.type = combined;
            cc.height = clampRange(height[a + k], CUSTOM_MIN_DIM, CUSTOM_MAX_HEIGHT);
            cc.width = clampRange(width[a + k], CUSTOM_MIN_DIM, CUSTOM_MAX_WIDTH);
            out.curves[a + k] = cc;
        }
    }
    return out;
}

bool CustomBrushConfig::validate() const {
    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) {
        if ((unsigned)primary[i] >= (unsigned)curveTypeCount()) return false;
    }
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) {
        if (!std::isfinite(secondaryParam[i]) ||
            secondaryParam[i] < 0.0f || secondaryParam[i] > 1.0f) return false;
        if (!std::isfinite(height[i]) ||
            height[i] < CUSTOM_MIN_DIM || height[i] > CUSTOM_MAX_HEIGHT) return false;
        if (!std::isfinite(width[i]) ||
            width[i] < CUSTOM_MIN_DIM || width[i] > CUSTOM_MAX_WIDTH) return false;
    }
    return resolve().isValid();
}

std::string CustomBrushConfig::serialize() const {
    std::string s = "CB1";
    char buf[32];
    auto appendInt = [&](int v) {
        snprintf(buf, sizeof(buf), "|%d", v);
        s += buf;
    };
    auto appendFloat = [&](float v) {
        snprintf(buf, sizeof(buf), "|%.9g", (double)v);
        s += buf;
    };
    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) appendInt((int)primary[i]);
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) appendFloat(secondaryParam[i]);
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) appendFloat(height[i]);
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) appendFloat(width[i]);
    return s;
}

CustomBrushConfig CustomBrushConfig::deserialize(const std::string& s) {
    CustomBrushConfig cfg;
    size_t pos = s.find('|');
    size_t tagEnd = (pos == std::string::npos) ? s.size() : pos;
    if (s.substr(0, tagEnd) != "CB1") return cfg;

    auto next = [&]() -> float {
        if (pos == std::string::npos) return -999.0f;
        size_t start = pos + 1;
        pos = s.find('|', start);
        if (start >= s.size()) return -999.0f;
        return strtof(s.c_str() + start, nullptr);
    };

    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) {
        int v = (int)next();
        if (v >= 0 && v < curveTypeCount()) cfg.primary[i] = (CurveType)v;
    }
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) cfg.setSecondaryParam(i, next());
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) cfg.setHeight(i, next());
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) cfg.setWidth(i, next());
    return cfg;
}

bool CustomBrushConfig::operator==(const CustomBrushConfig& o) const {
    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++)
        if (primary[i] != o.primary[i]) return false;
    for (int i = 0; i < CUSTOM_SECONDARY_COUNT; i++) {
        if (secondaryParam[i] != o.secondaryParam[i]) return false;
        if (height[i] != o.height[i]) return false;
        if (width[i] != o.width[i]) return false;
    }
    return true;
}
