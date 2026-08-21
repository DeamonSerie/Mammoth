#include "RectSelectTool.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <algorithm>

RectSelectTool::RectSelectTool() {
    DebugLog::log("[RectSelectTool] Constructor");
}

void RectSelectTool::start(float screenX, float screenY) {
    DebugLog::log("[RectSelectTool] start screen=(%.1f,%.1f)", screenX, screenY);
    m_selecting = true;
    m_hasSelection = false;
    m_selectionRect = Rect{screenX, screenY, 0, 0};
}

void RectSelectTool::update(float screenX, float screenY) {
    if (!m_selecting) return;
    m_selectionRect.w = screenX - m_selectionRect.x;
    m_selectionRect.h = screenY - m_selectionRect.y;
    DebugLog::log("[RectSelectTool] update screen=(%.1f,%.1f) rect=(%.1f,%.1f,%.1f,%.1f)", screenX, screenY, m_selectionRect.x, m_selectionRect.y, m_selectionRect.w, m_selectionRect.h);
}

void RectSelectTool::end() {
    DebugLog::log("[RectSelectTool] end hasSelection=%d", m_hasSelection);
    m_selecting = false;
    m_hasSelection = (std::abs(m_selectionRect.w) > 2.0f &&
                      std::abs(m_selectionRect.h) > 2.0f);
}

void RectSelectTool::clear() {
    DebugLog::log("[RectSelectTool] clear");
    m_selecting = false;
    m_hasSelection = false;
    m_selectionRect = {};
}

Rect RectSelectTool::getCanvasRect(const Canvas& canvas, const Rect& canvasRect) const {
    float sx0 = std::min(m_selectionRect.x, m_selectionRect.x + m_selectionRect.w);
    float sy0 = std::min(m_selectionRect.y, m_selectionRect.y + m_selectionRect.h);
    float sx1 = std::max(m_selectionRect.x, m_selectionRect.x + m_selectionRect.w);
    float sy1 = std::max(m_selectionRect.y, m_selectionRect.y + m_selectionRect.h);

    Vec2 c0 = canvas.screenToCanvas(sx0 - canvasRect.x, sy0 - canvasRect.y,
                                     canvasRect.w, canvasRect.h);
    Vec2 c1 = canvas.screenToCanvas(sx1 - canvasRect.x, sy1 - canvasRect.y,
                                     canvasRect.w, canvasRect.h);

    float rx = std::min(c0.x, c1.x);
    float ry = std::min(c0.y, c1.y);
    float rw = std::abs(c1.x - c0.x);
    float rh = std::abs(c1.y - c0.y);
    return Rect{rx, ry, rw, rh};
}

void RectSelectTool::moveSelection(float canvasDx, float canvasDy,
                                    const Canvas& canvas, const Rect& canvasRect) {
    if (!m_hasSelection) return;
    float screenDx = canvasDx * canvas.zoom();
    float screenDy = canvasDy * canvas.zoom();
    m_selectionRect.x += screenDx;
    m_selectionRect.y += screenDy;
}

void RectSelectTool::deleteSelected(Layer& layer, Canvas& canvas, const Rect& canvasRect) {
    DebugLog::log("[RectSelectTool] deleteSelected hasSelection=%d", m_hasSelection);
    if (!m_hasSelection) return;

    float sx0 = std::min(m_selectionRect.x, m_selectionRect.x + m_selectionRect.w);
    float sy0 = std::min(m_selectionRect.y, m_selectionRect.y + m_selectionRect.h);
    float sx1 = std::max(m_selectionRect.x, m_selectionRect.x + m_selectionRect.w);
    float sy1 = std::max(m_selectionRect.y, m_selectionRect.y + m_selectionRect.h);

    Vec2 c0 = canvas.screenToCanvas(sx0 - canvasRect.x, sy0 - canvasRect.y,
                                     canvasRect.w, canvasRect.h);
    Vec2 c1 = canvas.screenToCanvas(sx1 - canvasRect.x, sy1 - canvasRect.y,
                                     canvasRect.w, canvasRect.h);

    int minX = std::max(0, (int)std::floor(c0.x));
    int minY = std::max(0, (int)std::floor(c0.y));
    int maxX = std::min(layer.width() - 1, (int)std::ceil(c1.x));
    int maxY = std::min(layer.height() - 1, (int)std::ceil(c1.y));

    DebugLog::log("[RectSelectTool] deleteSelected canvas bounds=(%d,%d)-(%d,%d)", minX, minY, maxX, maxY);

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            layer.setPixel(x, y, Color(0, 0, 0, 0));
        }
    }
}

void RectSelectTool::render(Renderer& renderer) const {
    if (!m_selecting && !m_hasSelection) return;

    float sx = std::min(m_selectionRect.x, m_selectionRect.x + m_selectionRect.w);
    float sy = std::min(m_selectionRect.y, m_selectionRect.y + m_selectionRect.h);
    float sw = std::abs(m_selectionRect.w);
    float sh = std::abs(m_selectionRect.h);

    renderer.queueSolidRect(sx, sy, sw, sh, Color(128, 128, 128, 100));

    Color c(120, 120, 120, 220);
    renderer.queueSolidRect(sx, sy, sw, 1, c);
    renderer.queueSolidRect(sx, sy + sh, sw, 1, c);
    renderer.queueSolidRect(sx, sy, 1, sh, c);
    renderer.queueSolidRect(sx + sw, sy, 1, sh, c);
}
