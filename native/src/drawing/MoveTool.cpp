#include "MoveTool.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <cstring>

MoveTool::MoveTool() {}

void MoveTool::begin(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                     float screenX, float screenY)
{
    Vec2 pos = canvas.screenToCanvas(
        screenX - canvasRect.x, screenY - canvasRect.y,
        canvasRect.w, canvasRect.h);

    // Find content bounds by scanning raw pixel data
    int minX = layer.width(), maxX = -1, minY = layer.height(), maxY = -1;
    const uint8_t* pixels = layer.data();
    int lw = layer.width();
    int lh = layer.height();
    for (int y = 0; y < lh; y++) {
        for (int x = 0; x < lw; x++) {
            size_t off = (y * lw + x) * 4;
            if (pixels[off + 3] > 0) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    m_moving = true;
    m_moveStart = pos;
    m_layerW = layer.width();
    m_layerH = layer.height();
    m_savedData.assign(layer.data(), layer.data() + layer.dataSize());

    if (minX <= maxX && minY <= maxY) {
        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        m_grabOffset = {centerX - pos.x, centerY - pos.y};
        m_contentMinX = minX;
        m_contentMinY = minY;
        m_contentMaxX = maxX;
        m_contentMaxY = maxY;
        m_hasContent = true;
    } else {
        m_grabOffset = {0, 0};
        m_hasContent = false;
    }
    m_dragDx = 0.0f;
    m_dragDy = 0.0f;
    m_prevIntDx = 0;
    m_prevIntDy = 0;
}

void MoveTool::update(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                      float screenX, float screenY)
{
    if (!m_moving || m_savedData.empty() || !m_hasContent) return;

    Vec2 pos = canvas.screenToCanvas(
        screenX - canvasRect.x, screenY - canvasRect.y,
        canvasRect.w, canvasRect.h);

    // Target content center = cursor pos + grab offset
    float targetCenterX = pos.x + m_grabOffset.x;
    float targetCenterY = pos.y + m_grabOffset.y;

    float currentCenterX = (m_contentMinX + m_contentMaxX) * 0.5f;
    float currentCenterY = (m_contentMinY + m_contentMaxY) * 0.5f;

    float dx = targetCenterX - currentCenterX;
    float dy = targetCenterY - currentCenterY;

    // Clamp to canvas bounds
    float dxMin = -m_contentMinX;
    float dxMax = m_layerW - 1 - m_contentMaxX;
    float dyMin = -m_contentMinY;
    float dyMax = m_layerH - 1 - m_contentMaxY;

    if (dx < dxMin) dx = dxMin;
    if (dx > dxMax) dx = dxMax;
    if (dy < dyMin) dy = dyMin;
    if (dy > dyMax) dy = dyMax;

    m_dragDx = dx;
    m_dragDy = dy;

    // Only iterate content bounds, not full layer
    int intDx = (int)std::floor(dx);
    int intDy = (int)std::floor(dy);

    // Clear region covering BOTH previous and new content positions
    int clearMinX = std::min(m_contentMinX + m_prevIntDx, m_contentMinX + intDx) - 1;
    int clearMaxX = std::max(m_contentMaxX + m_prevIntDx, m_contentMaxX + intDx) + 1;
    int clearMinY = std::min(m_contentMinY + m_prevIntDy, m_contentMinY + intDy) - 1;
    int clearMaxY = std::max(m_contentMaxY + m_prevIntDy, m_contentMaxY + intDy) + 1;
    if (clearMinX < 0) clearMinX = 0;
    if (clearMaxX >= m_layerW) clearMaxX = m_layerW - 1;
    if (clearMinY < 0) clearMinY = 0;
    if (clearMaxY >= m_layerH) clearMaxY = m_layerH - 1;

    m_prevIntDx = intDx;
    m_prevIntDy = intDy;

    uint8_t* layerData = layer.mutableData();
    for (int y = clearMinY; y <= clearMaxY; y++) {
        size_t off = (y * m_layerW + clearMinX) * 4;
        size_t bytes = (clearMaxX - clearMinX + 1) * 4;
        std::memset(layerData + off, 0, bytes);
    }

    // Draw only content pixels
    for (int y = m_contentMinY; y <= m_contentMaxY; y++) {
        int ny = y + intDy;
        if (ny < 0 || ny >= m_layerH) continue;
        size_t srcRowOff = y * m_layerW;
        size_t dstRowOff = ny * m_layerW;

        for (int x = m_contentMinX; x <= m_contentMaxX; x++) {
            int nx = x + intDx;
            if (nx < 0 || nx >= m_layerW) continue;

            size_t srcOff = (srcRowOff + x) * 4;
            uint8_t a = m_savedData[srcOff + 3];
            if (a > 0) {
                size_t dstOff = (dstRowOff + nx) * 4;
                uint8_t* dst = layer.mutableData();
                dst[dstOff + 0] = m_savedData[srcOff + 0];
                dst[dstOff + 1] = m_savedData[srcOff + 1];
                dst[dstOff + 2] = m_savedData[srcOff + 2];
                dst[dstOff + 3] = a;
            }
        }
    }
    layer.setDirty();
}

void MoveTool::end(Layer& layer) {
    if (!m_moving || m_savedData.empty() || !m_hasContent) {
        m_moving = false;
        m_moveStart = {-1, -1};
        m_layerW = 0;
        m_layerH = 0;
        return;
    }

    int intDx = (int)std::floor(m_dragDx);
    int intDy = (int)std::floor(m_dragDy);

    // Clear region covering both previous and final content positions
    int clearMinX = std::min(m_contentMinX + m_prevIntDx, m_contentMinX + intDx) - 1;
    int clearMaxX = std::max(m_contentMaxX + m_prevIntDx, m_contentMaxX + intDx) + 1;
    int clearMinY = std::min(m_contentMinY + m_prevIntDy, m_contentMinY + intDy) - 1;
    int clearMaxY = std::max(m_contentMaxY + m_prevIntDy, m_contentMaxY + intDy) + 1;
    if (clearMinX < 0) clearMinX = 0;
    if (clearMaxX >= m_layerW) clearMaxX = m_layerW - 1;
    if (clearMinY < 0) clearMinY = 0;
    if (clearMaxY >= m_layerH) clearMaxY = m_layerH - 1;

    uint8_t* layerData = layer.mutableData();
    for (int y = clearMinY; y <= clearMaxY; y++) {
        size_t off = (y * m_layerW + clearMinX) * 4;
        size_t bytes = (clearMaxX - clearMinX + 1) * 4;
        std::memset(layerData + off, 0, bytes);
    }

    // Draw content pixels
    for (int y = m_contentMinY; y <= m_contentMaxY; y++) {
        int ny = y + intDy;
        if (ny < 0 || ny >= m_layerH) continue;
        size_t srcRowOff = y * m_layerW;
        size_t dstRowOff = ny * m_layerW;

        for (int x = m_contentMinX; x <= m_contentMaxX; x++) {
            int nx = x + intDx;
            if (nx < 0 || nx >= m_layerW) continue;

            size_t srcOff = (srcRowOff + x) * 4;
            uint8_t a = m_savedData[srcOff + 3];
            if (a > 0) {
                size_t dstOff = (dstRowOff + nx) * 4;
                uint8_t* dst = layer.mutableData();
                dst[dstOff + 0] = m_savedData[srcOff + 0];
                dst[dstOff + 1] = m_savedData[srcOff + 1];
                dst[dstOff + 2] = m_savedData[srcOff + 2];
                dst[dstOff + 3] = a;
            }
        }
    }
    layer.setDirty();

    m_moving = false;
    m_moveStart = {-1, -1};
    m_layerW = 0;
    m_layerH = 0;
}