#include "MoveTool.hpp"
#include "../DebugLog.h"
#include <cmath>

MoveTool::MoveTool() {
    DebugLog::log("[MoveTool] Constructor");
}

void MoveTool::begin(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                     float screenX, float screenY)
{
    DebugLog::log("[MoveTool] begin screen=(%.1f,%.1f)", screenX, screenY);
    Vec2 pos = canvas.screenToCanvas(
        screenX - canvasRect.x, screenY - canvasRect.y,
        canvasRect.w, canvasRect.h);

    // Find content bounds to compute grab offset (cursor relative to content center)
    int minX = layer.width(), maxX = -1, minY = layer.height(), maxY = -1;
    for (int y = 0; y < layer.height(); y++) {
        for (int x = 0; x < layer.width(); x++) {
            Color c = layer.getPixel(x, y);
            if (c.a > 0) {
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

    // Compute grab offset: cursor position minus content center
    if (minX <= maxX && minY <= maxY) {
        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        m_grabOffset = {pos.x - centerX, pos.y - centerY};
    } else {
        m_grabOffset = {0, 0};
    }
    DebugLog::log("[MoveTool] begin complete grabOffset=(%.1f,%.1f) contentBounds=(%d,%d)-(%d,%d)", m_grabOffset.x, m_grabOffset.y, minX, minY, maxX, maxY);
}

void MoveTool::update(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                      float screenX, float screenY)
{
    if (!m_moving || m_savedData.empty()) return;

    Vec2 pos = canvas.screenToCanvas(
        screenX - canvasRect.x, screenY - canvasRect.y,
        canvasRect.w, canvasRect.h);

    DebugLog::log("[MoveTool] update screen=(%.1f,%.1f) canvasPos=(%.1f,%.1f)", screenX, screenY, pos.x, pos.y);

    // Total delta from drag start, adjusted by grab offset
    // Target content center = cursor pos - grab offset
    float targetCenterX = pos.x - m_grabOffset.x;
    float targetCenterY = pos.y - m_grabOffset.y;

    // Find current content center from saved data
    int minX = m_layerW, maxX = -1, minY = m_layerH, maxY = -1;
    for (int y = 0; y < m_layerH; y++) {
        for (int x = 0; x < m_layerW; x++) {
            size_t off = (y * m_layerW + x) * 4;
            if (m_savedData[off + 3] > 0) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    float dx = 0, dy = 0;
    if (minX <= maxX && minY <= maxY) {
        float currentCenterX = (minX + maxX) * 0.5f;
        float currentCenterY = (minY + maxY) * 0.5f;
        dx = targetCenterX - currentCenterX;
        dy = targetCenterY - currentCenterY;

        // Clamp movement to keep content within canvas bounds
        float dxMin = -minX;
        float dxMax = m_layerW - 1 - maxX;
        float dyMin = -minY;
        float dyMax = m_layerH - 1 - maxY;

        if (dx < dxMin) dx = dxMin;
        if (dx > dxMax) dx = dxMax;
        if (dy < dyMin) dy = dyMin;
        if (dy > dyMax) dy = dyMax;
    }
    DebugLog::log("[MoveTool] update dx=%.1f dy=%.1f", dx, dy);

    layer.clear();
    for (int y = 0; y < m_layerH; y++) {
        for (int x = 0; x < m_layerW; x++) {
            int nx = (int)std::round(x + dx);
            int ny = (int)std::round(y + dy);
            if (nx >= 0 && nx < m_layerW && ny >= 0 && ny < m_layerH) {
                size_t srcOff = (y * m_layerW + x) * 4;
                Color c(m_savedData[srcOff + 0], m_savedData[srcOff + 1],
                        m_savedData[srcOff + 2], m_savedData[srcOff + 3]);
                if (c.a > 0) layer.setPixel(nx, ny, c);
            }
        }
    }
    layer.setDirty();
}

void MoveTool::end() {
    DebugLog::log("[MoveTool] end");
    // Commit the final position so next drag starts from here
    m_moving = false;
    m_moveStart = {-1, -1};
    m_layerW = 0;
    m_layerH = 0;
    // Preserve m_savedData for next move - don't clear!
}