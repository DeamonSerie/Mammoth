#include "Canvas.hpp"
#include <algorithm>

Canvas::Canvas() : m_document() {}

Canvas::Canvas(int width, int height, const char* name)
    : m_document(width, height, name)
{}

Vec2 Canvas::screenToCanvas(float sx, float sy, float viewportW, float viewportH) const {
    float canvasScreenW = m_document.width() * m_zoom;
    float canvasScreenH = m_document.height() * m_zoom;
    float offsetX = (viewportW - canvasScreenW) / 2.0f + m_cameraX;
    float offsetY = (viewportH - canvasScreenH) / 2.0f + m_cameraY;
    float cx = (sx - offsetX) / m_zoom;
    float cy = (sy - offsetY) / m_zoom;
    return {cx, cy};
}

Vec2 Canvas::canvasToScreen(float cx, float cy, float viewportW, float viewportH) const {
    float canvasScreenW = m_document.width() * m_zoom;
    float canvasScreenH = m_document.height() * m_zoom;
    float offsetX = (viewportW - canvasScreenW) / 2.0f + m_cameraX;
    float offsetY = (viewportH - canvasScreenH) / 2.0f + m_cameraY;
    float sx = cx * m_zoom + offsetX;
    float sy = cy * m_zoom + offsetY;
    return {sx, sy};
}

void Canvas::update() {
    Frame* frame = m_document.activeFrame();
    if (!frame) return;
    if (frame->isDirty() || m_dirty) {
        frame->compositeToBuffer(m_compositeBuffer, m_compositeW, m_compositeH);
        frame->clearDirty();
        m_dirty = false;
    }
}
