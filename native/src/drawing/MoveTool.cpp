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
    m_savedData.clear();
    m_moveStart = {-1, -1};
    m_layerW = 0;
    m_layerH = 0;
}
