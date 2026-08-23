#include "RectSelectTool.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <cstring>
#include <utility>
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

    if (m_floating) {
        // While stamping the marquee is just a guide; no source dimming.
        Color c(255, 200, 60, 230);
        renderer.queueSolidRect(sx, sy, sw, 1, c);
        renderer.queueSolidRect(sx, sy + sh, sw, 1, c);
        renderer.queueSolidRect(sx, sy, 1, sh, c);
        renderer.queueSolidRect(sx + sw, sy, 1, sh, c);
        return;
    }

    renderer.queueSolidRect(sx, sy, sw, sh, Color(128, 128, 128, 100));

    Color c(120, 120, 120, 220);
    renderer.queueSolidRect(sx, sy, sw, 1, c);
    renderer.queueSolidRect(sx, sy + sh, sw, 1, c);
    renderer.queueSolidRect(sx, sy, 1, sh, c);
    renderer.queueSolidRect(sx + sw, sy, 1, sh, c);
}

// ---- Stamping ------------------------------------------------------------------

bool RectSelectTool::beginStamp(const Layer& layer, const Canvas& canvas,
                                const Rect& canvasRect) {
    if (!m_hasSelection) return false;

    Rect cr = getCanvasRect(canvas, canvasRect);
    int minX = std::max(0, (int)std::floor(cr.x));
    int minY = std::max(0, (int)std::floor(cr.y));
    int maxX = std::min(layer.width(), (int)std::ceil(cr.x + cr.w));
    int maxY = std::min(layer.height(), (int)std::ceil(cr.y + cr.h));
    if (minX >= maxX || minY >= maxY) return false;

    m_stampW = maxX - minX;
    m_stampH = maxY - minY;
    m_stampPixels.resize((size_t)m_stampW * m_stampH * 4);

    const uint8_t* src = layer.data();
    for (int y = 0; y < m_stampH; y++) {
        size_t srcRow = ((size_t)(minY + y) * layer.width() + minX) * 4;
        size_t dstRow = (size_t)y * m_stampW * 4;
        std::memcpy(&m_stampPixels[dstRow], &src[srcRow], (size_t)m_stampW * 4);
    }

    m_floatX = (float)minX;
    m_floatY = (float)minY;
    return beginStampFromPixels(std::move(m_stampPixels), m_stampW, m_stampH,
                                (float)minX, (float)minY);
}

bool RectSelectTool::beginStampFromPixels(std::vector<uint8_t> pixels, int w, int h,
                                          float canvasX, float canvasY) {
    if (w <= 0 || h <= 0 || pixels.size() != (size_t)w * h * 4) return false;

    m_stampW = w;
    m_stampH = h;
    m_stampPixels = std::move(pixels);
    m_floatX = canvasX;
    m_floatY = canvasY;
    m_draggingFloat = false;
    m_floating = true;
    DebugLog::log("[RectSelectTool] beginStampFromPixels rect=(%.1f,%.1f,%d,%d)",
                  canvasX, canvasY, w, h);
    return true;
}

void RectSelectTool::startFloatDrag(float sx, float sy, const Canvas& canvas,
                                    const Rect& canvasRect) {
    if (!m_floating) return;
    Vec2 c = canvas.screenToCanvas(sx - canvasRect.x, sy - canvasRect.y,
                                   canvasRect.w, canvasRect.h);
    m_dragGrabCanvas = c;
    m_dragStartFloatX = m_floatX;
    m_dragStartFloatY = m_floatY;
    m_draggingFloat = true;
}

void RectSelectTool::updateFloatDrag(float sx, float sy, const Canvas& canvas,
                                     const Rect& canvasRect) {
    if (!m_floating || !m_draggingFloat) return;
    Vec2 c = canvas.screenToCanvas(sx - canvasRect.x, sy - canvasRect.y,
                                   canvasRect.w, canvasRect.h);
    m_floatX = m_dragStartFloatX + (c.x - m_dragGrabCanvas.x);
    m_floatY = m_dragStartFloatY + (c.y - m_dragGrabCanvas.y);
}

bool RectSelectTool::containsScreenPoint(float sx, float sy, const Canvas& canvas,
                                         const Rect& canvasRect) const {
    if (!m_floating) return false;
    Vec2 c = canvas.screenToCanvas(sx - canvasRect.x, sy - canvasRect.y,
                                   canvasRect.w, canvasRect.h);
    return c.x >= m_floatX && c.x < m_floatX + m_stampW &&
           c.y >= m_floatY && c.y < m_floatY + m_stampH;
}

Rect RectSelectTool::floatScreenRect(const Canvas& canvas, const Rect& canvasRect) const {
    Vec2 p = canvas.canvasToScreen(m_floatX, m_floatY, canvasRect.w, canvasRect.h);
    return Rect{canvasRect.x + p.x, canvasRect.y + p.y,
                m_stampW * canvas.zoom(), m_stampH * canvas.zoom()};
}

bool RectSelectTool::commitFloat(Layer& layer) {
    if (!m_floating) return false;

    int baseX = (int)std::lround(m_floatX);
    int baseY = (int)std::lround(m_floatY);
    for (int y = 0; y < m_stampH; y++) {
        int dy = baseY + y;
        if (dy < 0 || dy >= layer.height()) continue;
        for (int x = 0; x < m_stampW; x++) {
            int dx = baseX + x;
            if (dx < 0 || dx >= layer.width()) continue;
            size_t off = ((size_t)y * m_stampW + x) * 4;
            Color c(m_stampPixels[off], m_stampPixels[off + 1],
                    m_stampPixels[off + 2], m_stampPixels[off + 3]);
            layer.blendPixel(dx, dy, c);
        }
    }
    layer.setDirty();

    m_floating = false;
    m_draggingFloat = false;
    m_stampPixels.clear();
    m_stampW = m_stampH = 0;
    DebugLog::log("[RectSelectTool] commitFloat at (%d,%d)", baseX, baseY);
    return true;
}

void RectSelectTool::cancelFloat() {
    if (!m_floating) return;
    DebugLog::log("[RectSelectTool] cancelFloat");
    m_floating = false;
    m_draggingFloat = false;
    m_stampPixels.clear();
    m_stampW = m_stampH = 0;
}
