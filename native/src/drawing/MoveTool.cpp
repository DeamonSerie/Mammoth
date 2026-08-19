#include "MoveTool.hpp"
#include <cmath>

MoveTool::MoveTool() {}

void MoveTool::begin(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                     float screenX, float screenY)
{
    Vec2 pos = canvas.screenToCanvas(
        screenX - canvasRect.x, screenY - canvasRect.y,
        canvasRect.w, canvasRect.h);

    m_moving = true;
    m_moveStart = pos;
    m_layerW = layer.width();
    m_layerH = layer.height();
    m_savedData.assign(layer.data(), layer.data() + layer.dataSize());
}

void MoveTool::update(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                      float screenX, float screenY)
{
    if (!m_moving || m_savedData.empty()) return;

    Vec2 pos = canvas.screenToCanvas(
        screenX - canvasRect.x, screenY - canvasRect.y,
        canvasRect.w, canvasRect.h);

    int dx = (int)std::round(pos.x - m_moveStart.x);
    int dy = (int)std::round(pos.y - m_moveStart.y);

    // Clamp movement to keep content within canvas bounds
    // Find bounds of non-transparent content in saved data
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

    if (minX <= maxX && minY <= maxY) {
        // Calculate valid movement range to keep content within canvas
        int dxMin = -minX;
        int dxMax = m_layerW - 1 - (maxX - minX);
        int dyMin = -minY;
        int dyMax = m_layerH - 1 - (maxY - minY);

        if (dx < -minX) dx = -minX;
        if (dx > m_layerW - 1 - maxX) dx = m_layerW - 1 - maxX;
        if (dy < -minY) dy = -minY;
        if (dy > m_layerH - 1 - maxY) dy = m_layerH - 1 - maxY;
    }

    layer.clear();
    for (int y = 0; y < m_layerH; y++) {
        for (int x = 0; x < m_layerW; x++) {
            int nx = x + dx;
            int ny = y + dy;
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
    m_moving = false;
    m_moveStart = {-1, -1};
    m_layerW = 0;
    m_layerH = 0;
    // Preserve m_savedData for next move - don't clear!
}
