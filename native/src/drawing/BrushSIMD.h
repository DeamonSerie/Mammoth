
#pragma once
#include <x86intrin.h>
#include <cmath>
#include <algorithm>
#include "Brush.hpp"
#include "../document/Layer.hpp"

// ----------------------------------------------------------------------------
// Alignment macro for SSE
// ----------------------------------------------------------------------------
#define ALIGN16 __attribute__((aligned(16)))

// ----------------------------------------------------------------------------
// SIMD Color Structure
// ----------------------------------------------------------------------------
struct ALIGN16 SIMDColor {
    float r, g, b, a;
    
    SIMDColor() : r(0.0f), g(0.0f), b(0.0f), a(0.0f) {}
    SIMDColor(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
    
    // Convert to uint8_t with clamping
    uint8_t toByte(int channel) const {
        float val = (channel == 0) ? r : (channel == 1) ? g : (channel == 2) ? b : a;
        return (uint8_t)(std::clamp(val, 0.0f, 1.0f) * 255.0f + 0.5f);
    }
};

// ----------------------------------------------------------------------------
// Brush Overlay Clear (SSE2 - 4 pixels at a time)
// ----------------------------------------------------------------------------
inline void brushOverlayClearSSE(uint8_t* overlay, int width, int height) {
    const int pixels = width * height;
    const __m128 zero = _mm_setzero_ps();
    
    // Process 4 pixels at a time (16 bytes = 4 RGBA pixels = 1 × __m128)
    int i = 0;
    for (; i + 3 < pixels; i += 4) {
        __m128* overlayPixels = (__m128*)(overlay + i * 4);
        overlayPixels[0] = zero;
    }
    // Handle remaining pixels scalarly
    for (; i < pixels; ++i) {
        overlay[i * 4 + 3] = 0;  // alpha = 0 (transparent)
    }
}

// ----------------------------------------------------------------------------
// Brush Overlay Premultiply Alpha
// ----------------------------------------------------------------------------
inline void brushOverlayPremultiplyAlphaSSE(uint8_t* overlay, int width, int height) {
    const int pixels = width * height;
    for (int i = 0; i < pixels; ++i) {
        uint8_t a = overlay[i * 4 + 3];
        if (a > 0) {
            overlay[i * 4]     = (uint8_t)((overlay[i * 4]     * a + 127) / 255);
            overlay[i * 4 + 1] = (uint8_t)((overlay[i * 4 + 1] * a + 127) / 255);
            overlay[i * 4 + 2] = (uint8_t)((overlay[i * 4 + 2] * a + 127) / 255);
        }
    }
}

// ----------------------------------------------------------------------------
// Brush Overlay Blend (Alpha Blending)
// ----------------------------------------------------------------------------
inline void brushOverlayBlendSSE(
    uint8_t* dest, const uint8_t* overlay, int width, int height
) {
    const int pixels = width * height;
    for (int i = 0; i < pixels; ++i) {
        uint8_t da = dest[i * 4 + 3];
        uint8_t sa = overlay[i * 4 + 3];
        if (sa == 0) continue;
        if (da == 0) {
            dest[i * 4]     = overlay[i * 4];
            dest[i * 4 + 1] = overlay[i * 4 + 1];
            dest[i * 4 + 2] = overlay[i * 4 + 2];
            dest[i * 4 + 3] = sa;
            continue;
        }
        // Standard alpha compositing: out = src + dst * (1 - srcA)
        float srcA = sa / 255.0f;
        float dstA = da / 255.0f;
        float outA = srcA + dstA * (1.0f - srcA);
        if (outA < 0.001f) continue;
        dest[i * 4]     = (uint8_t)((overlay[i * 4]     * srcA + dest[i * 4]     * dstA * (1.0f - srcA)) / outA + 0.5f);
        dest[i * 4 + 1] = (uint8_t)((overlay[i * 4 + 1] * srcA + dest[i * 4 + 1] * dstA * (1.0f - srcA)) / outA + 0.5f);
        dest[i * 4 + 2] = (uint8_t)((overlay[i * 4 + 2] * srcA + dest[i * 4 + 2] * dstA * (1.0f - srcA)) / outA + 0.5f);
        dest[i * 4 + 3] = (uint8_t)(outA * 255.0f + 0.5f);
    }
}

// ----------------------------------------------------------------------------
// Fast Square Root (SSE2 - rsqrt + Newton-Raphson)
// ----------------------------------------------------------------------------
inline float fastSqrtSSE(float x) {
    if (x <= 0.0f) return 0.0f;
    __m128 xps = _mm_set_ss(x);
    __m128 r = _mm_rsqrt_ps(xps);  // 1/sqrt(x)
    // Newton-Raphson iteration for better accuracy: r = r * (1.5 - 0.5 * x * r * r)
    __m128 half = _mm_set1_ps(0.5f);
    __m128 threeHalf = _mm_set1_ps(1.5f);
    r = _mm_mul_ps(r, _mm_sub_ps(threeHalf, _mm_mul_ps(half, _mm_mul_ps(xps, _mm_mul_ps(r, r)))));
    // Result is 1/sqrt(x) * x = sqrt(x)
    return _mm_cvtss_f32(_mm_mul_ps(xps, r));
}

// ----------------------------------------------------------------------------
// Brush Distance Calculation (SSE2)
// ----------------------------------------------------------------------------
inline float brushDistanceSSE(float px, float py, float cx, float cy) {
    float dx = px - cx;
    float dy = py - cy;
    float distSq = dx * dx + dy * dy;
    return fastSqrtSSE(distSq);
}

// ----------------------------------------------------------------------------
// Main Brush Rendering Loop (SSE2 Optimized)
// ----------------------------------------------------------------------------
// Render a brush stamp using SSE2
void brushRenderStampSSE(
    Layer& destLayer,
    int centerX, int centerY,
    int radius,
    const Brush& brush,
    float overlayScale
) {
    // Get brush color and alpha
    Color c = brush.color();
    float brushAlpha = brush.opacity();
    
    // Calculate effective radius
    int r = radius;
    
    // Process the brush bounding box
    // Use SSE for distance calculation where possible
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // Calculate distance from center
            float dxF = static_cast<float>(dx);
            float dyF = static_cast<float>(dy);
            float distSq = dxF * dxF + dyF * dyF;
            
            if (distSq <= static_cast<float>(radius * radius - 2 * radius + 1)) {
                // Full inside - solid pixel
                float a = 1.0f;
                Color stamp = c;
                stamp.a = (uint8_t)(c.a * brushAlpha);
                destLayer.blendPixel(centerX + dx, centerY + dy, stamp);
            } else if (distSq <= static_cast<float>(radius * radius)) {
                // Alpha falloff region
                float dist = fastSqrtSSE(distSq);
                float a = std::max(0.0f, radius - dist);
                Color stamp = c;
                stamp.a = (uint8_t)(c.a * brushAlpha * std::clamp(a, 0.0f, 1.0f));
                destLayer.blendPixel(centerX + dx, centerY + dy, stamp);
            }
            // Outside radius - skip
        }
    }
}