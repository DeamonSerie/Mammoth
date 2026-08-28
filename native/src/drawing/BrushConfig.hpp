#pragma once

// Centralized brush stroke resolution configuration.
// Developers can modify these values to change brush stroke quality/performance tradeoffs.

namespace BrushConfig {

// Base resolution multiplier for brush overlay.
// Higher = sharper brush edges at high zoom, more VRAM.
// 12288 = 12K (current), 8192 = 8K, 4096 = 4K, 2048 = 2K.
// Recommended: power of 2 for GPU texture efficiency.
constexpr int BRUSH_OVERLAY_SIZE = 12288;

// Brush density factor for preview generation.
// Higher = more supersampling in preview, more CPU.
constexpr int BRUSH_DENSITY = 10;

// Custom brush maximum height multiplier.
// How much taller a custom brush segment can be relative to its radius.
constexpr float CUSTOM_MAX_HEIGHT = 2.0f;

// Custom brush minimum dimension clamp.
constexpr float CUSTOM_MIN_DIM = 0.05f;

// Custom brush secondary count (number of radial segments).
constexpr int CUSTOM_SECONDARY_COUNT = 8;

// Preview texture size limits.
constexpr int PREVIEW_MIN_BUF = 32;
constexpr int PREVIEW_MAX_BUF = 512;

// Grid configuration (visible at zoom >= 16x).
constexpr int GRID_ZOOM_THRESHOLD = 16;
constexpr int GRID_ALPHA = 40;
constexpr int GRID_THICKNESS = 2;

// Eraser configuration (gradual erase behavior).
// These control how the eraser responds to pressure.
// See GradualEraser.cpp for the pressure-response curve.
constexpr float ERASER_ERASE_START_EASED = 0.3f;   // Eased pressure where erasing begins (0..1)
constexpr float ERASER_ERASE_FULL_EASED = 0.70f;   // Eased pressure where erasing is full (0..1)
constexpr float ERASER_R_SCALE_MIN = 0.2f;         // Radius scale at zero pressure
constexpr float ERASER_R_SCALE_MAX = 0.8f;         // Radius scale multiplier at full pressure
constexpr float ERASER_LIGHTEN_FACTOR_BASE = 0.8f; // Base lighten factor when not erasing

} // namespace BrushConfig