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
    m_prevOffset.x = 0;
    m_prevOffset.y = 0;
    m_layerW = layer.width();
    m_layerH = layer.height();
    if (m_savedData.empty()) m_savedData.assign(layer.data(), layer.data() + layer.dataSize());
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

    // Clear previous position (using previous offset)
    for (int y = 0; y < m_layerH; y++) {
        for (int x = 0; x < m_layerW; x++) {
            int srcX = x - m_prevOffset.x;
            int srcY = y - m_prevOffset.y;
            if (srcX >= 0 && srcX < m_layerW && srcY >= 0 && srcY < m_layerH) {
                size_t srcOff = (srcY * m_layerW + srcX) * 4;
                if (m_savedData[srcOff + 3] > 0) {
                    layer.setPixel(x, y, Color::transparent());
                }
            }
        }
    }

    // Draw at new position
    for (int y = 0; y < m_layerH; y++) {
        for (int x = 0; x < m_layerW; x++) {
            int srcX = x - dx;
            int srcY = y - dy;
            if (srcX >= 0 && srcX < m_layerW && srcY >= 0 && srcY < m_layerH) {
                size_t srcOff = (srcY * m_layerW + srcX) * 4;
                Color c(m_savedData[srcOff + 0], m_savedData[srcOff + 1],
                        m_savedData[srcOff + 2], m_savedData[srcOff + 3]);
                if (c.a > 0) layer.setPixel(x, y, c);
            }
        }
    }

    m_prevOffset.x = dx;
    m_prevOffset.y = dy;
    layer.setDirty();
}

void MoveTool::end(Layer& layer, Frame* frame) {
    m_moving = false;
    m_moveStart.x = -1;
    m_moveStart.y = -1;
    m_prevOffset.x = 0;
    m_prevOffset.y = 0;
    m_layerW = 0;
    m_layerH = 0;
    if (frame) {
        layer.setDirty();
        frame->setDirty();
    }
}
