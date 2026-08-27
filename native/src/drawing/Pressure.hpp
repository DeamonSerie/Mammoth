// ============================================================================
//  PRESSURE PIPELINE  —  single source of truth for pen-pressure handling
// ============================================================================
//
//  This file defines how a stylus/finger pressure value (0..1) becomes brush
//  size and opacity. If you want to change how a pen "feels", edit ONLY here
//  (or the Brush::pressureProfile() members). Nothing downstream needs to
//  know the formula.
//
//  Full data flow (each stage is tagged with "// PRESSURE PIPELINE"):
//    1. A pointer/pen/finger event carries pressure (0..1) into Input::Event
//         (translated from the active input backend in app/Application.cpp).
//    2. Application::dispatchInput  ->  MainWindow::onMouseMove / onMouseButton(pressure)
//         (app/Application.cpp, ui/MainWindow.cpp)
//    3. handleDrawing(pressure) stores the per-point pressure of the stroke
//    4. Brush::radiusForPressure / opacityForPressure   <-- THIS FILE, via PressureProfile
//    5. VectorBrushEngine::renderStroke (variable-width stroke)
//       GradualEraser::stamp (pressure-aware erase; see drawing/GradualEraser.cpp)
// ============================================================================

#pragma once
#include <algorithm>
#include <cmath>

struct PressureProfile {
    // === Tweak these to change the feel of a pen =========================
    float sizeResponse   = 0.85f; // 0 = ignore pressure (constant width),
                                  // 1 = pressure fully controls width.
    float opacityResponse = 0.70f; // 0 = constant opacity, 1 = full.
    float ease           = 1.80f; // Ease-out exponent. >1 means an *average*
                                  // hand press already yields a bold line, and
                                  // pressing harder than that adds only a little
                                  // — you never have to mash the screen to get a
                                  // strong line (models average artist hand strength).
    // =====================================================================

    // Raw pressure (0..1) -> eased strength (0..1).
    float eased(float p) const {
        float t = std::clamp(p, 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - t, ease);
    }

    // Pressure -> effective brush radius for a given base (unpressed) radius.
    float radius(float baseRadius, float p) const {
        float e = eased(p);
        return baseRadius * (1.0f - sizeResponse + sizeResponse * e);
    }

    // Pressure -> effective brush opacity for a given base opacity.
    float opacity(float baseOpacity, float p) const {
        float e = eased(p);
        return baseOpacity * (1.0f - opacityResponse + opacityResponse * e);
    }
};
