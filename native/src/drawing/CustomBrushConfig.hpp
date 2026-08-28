#pragma once
#include "CurveTypes.hpp"
#include "BrushConfig.hpp"
#include <cstdint>
#include <string>

// Number of primary pieces (Division 1) and secondary sections (Division 2).
static constexpr int CUSTOM_PRIMARY_COUNT = 4;
static constexpr int CUSTOM_SECONDARY_COUNT = BrushConfig::CUSTOM_SECONDARY_COUNT;

// Height/width ranges for combined curves.
static constexpr float CUSTOM_MIN_DIM = BrushConfig::CUSTOM_MIN_DIM;
static constexpr float CUSTOM_MAX_HEIGHT = BrushConfig::CUSTOM_MAX_HEIGHT;
static constexpr float CUSTOM_MAX_WIDTH = 1.5f;

// A resulting (combined) curve after dependency resolution.
struct CombinedCurve {
    CurveType type = CurveType::Circle;
    float height = 1.0f; // radial scale
    float width = 1.0f;  // angular span scale

    bool operator==(const CombinedCurve& o) const {
        return type == o.type && height == o.height && width == o.width;
    }
    bool operator!=(const CombinedCurve& o) const { return !(*this == o); }
};

// Fully resolved custom brush geometry description, ready for the
// geometry-generation stage. Contains no user-editable state.
struct ResolvedCustomBrush {
    CombinedCurve curves[CUSTOM_SECONDARY_COUNT];

    bool isValid() const;
    bool operator==(const ResolvedCustomBrush& o) const;
    bool operator!=(const ResolvedCustomBrush& o) const { return !(*this == o); }
};

// Authoritative custom-brush configuration model.
//
// Pipeline responsibility: stores user selections, enforces the Division 1 ->
// Division 2 dependency rules and resolves the final combined curves.
// The UI reads/writes this model; geometry generation consumes resolve().
//
// Secondary sliders hold a continuous value in [0,1]. The selected curve type
// is derived by dividing the range into three equal bands:
//   [0, 1/3) -> Circle, [1/3, 2/3) -> Square, [2/3, 1] -> Triangle.
// Automatically determined slots ignore the slider for curve selection.
struct CustomBrushConfig {
    // Division 1: four primary pieces (three-state switches in the UI).
    CurveType primary[CUSTOM_PRIMARY_COUNT] = {
        CurveType::Circle, CurveType::Square,
        CurveType::Triangle, CurveType::Circle
    };

    // Division 2: eight secondary slider values in [0,1].
    float secondaryParam[CUSTOM_SECONDARY_COUNT] = {
        1.0f / 6.0f, 0.5f, 5.0f / 6.0f, 1.0f / 6.0f,
        0.5f, 1.0f / 6.0f, 5.0f / 6.0f, 0.5f
    };

    // Resulting combined-curve dimensions (independent per curve).
    float height[CUSTOM_SECONDARY_COUNT] = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };
    float width[CUSTOM_SECONDARY_COUNT] = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };

    // ---- Dependency rules --------------------------------------------------
    // Every third curve in the subdivision sequence (slots index 2 and 5)
    // is automatically determined by the controlling primary piece.
    static bool isAutoSecondary(int slot);

    // Primary piece that controls a secondary slot (each piece owns 2 slots).
    static int controllingPrimary(int slot);

    // Automatically determined curve for a slot; derived from its primary so
    // that any primary change automatically recalculates it.
    CurveType autoSecondary(int slot) const;

    // Effective curve of a slot (slider-derived value or automatic).
    CurveType effectiveSecondary(int slot) const;

    // Attempt to set a user-controlled secondary curve. Fails (returns false,
    // model unchanged) for automatically determined slots. Sets the slider to
    // the centre of the matching band.
    bool setSecondaryCurve(int slot, CurveType t);

    // Direct slider access; value is clamped into [0,1].
    void setSecondaryParam(int slot, float v);
    void setHeight(int slot, float v);
    void setWidth(int slot, float v);

    // ---- Resolution / validation / serialization ---------------------------
    ResolvedCustomBrush resolve() const;

    // True when all values are within range and dependencies are consistent.
    bool validate() const;

    std::string serialize() const;
    static CustomBrushConfig deserialize(const std::string& s);

    bool operator==(const CustomBrushConfig& o) const;
    bool operator!=(const CustomBrushConfig& o) const { return !(*this == o); }
};
