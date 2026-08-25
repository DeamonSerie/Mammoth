#include "Canvas.hpp"
#include "../DebugLog.h"
#include <algorithm>
#include <cmath>

Canvas::Canvas() : m_document() {
    DebugLog::log("[Canvas] Default constructor");
}

Canvas::Canvas(int width, int height, const char* name)
    : m_document(width, height, name)
{
    DebugLog::log("[Canvas] Created %dx%d name='%s'", width, height, name ? name : "unnamed");
}

Vec2 Canvas::screenToCanvas(float sx, float sy, float viewportW, float viewportH) const {
    float canvasScreenW = m_document.width() * m_zoom;
    float canvasScreenH = m_document.height() * m_zoom;
    float offsetX = (viewportW - canvasScreenW) / 2.0f + m_cameraX;
    float offsetY = (viewportH - canvasScreenH) / 2.0f + m_cameraY;
    // Canvas center in viewport space (rotation pivot).
    float ccx = offsetX + canvasScreenW / 2.0f;
    float ccy = offsetY + canvasScreenH / 2.0f;
    // Un-rotate around the canvas center.
    float dx = sx - ccx;
    float dy = sy - ccy;
    float c = std::cos(-m_rotation);
    float s = std::sin(-m_rotation);
    float ux = dx * c - dy * s;
    float uy = dx * s + dy * c;
    float cx = (ux + ccx - offsetX) / m_zoom;
    float cy = (uy + ccy - offsetY) / m_zoom;
    return {cx, cy};
}

Vec2 Canvas::canvasToScreen(float cx, float cy, float viewportW, float viewportH) const {
    float canvasScreenW = m_document.width() * m_zoom;
    float canvasScreenH = m_document.height() * m_zoom;
    float offsetX = (viewportW - canvasScreenW) / 2.0f + m_cameraX;
    float offsetY = (viewportH - canvasScreenH) / 2.0f + m_cameraY;
    // Non-rotated screen position.
    float sx = cx * m_zoom + offsetX;
    float sy = cy * m_zoom + offsetY;
    // Canvas center in viewport space.
    float ccx = offsetX + canvasScreenW / 2.0f;
    float ccy = offsetY + canvasScreenH / 2.0f;
    // Rotate around the canvas center.
    float dx = sx - ccx;
    float dy = sy - ccy;
    float c = std::cos(m_rotation);
    float s = std::sin(m_rotation);
    float rx = dx * c - dy * s + ccx;
    float ry = dx * s + dy * c + ccy;
    return {rx, ry};
}

void Canvas::update() {
    Frame* frame = m_document.activeFrame();
    if (!frame) return;
    if ((const void*)frame != m_compositedFrame || frame->isDirty() || m_dirty) {
        DebugLog::log("[Canvas] update() compositing frame %dx%d", frame->width(), frame->height());
        frame->compositeToBuffer(m_compositeBuffer, m_compositeW, m_compositeH);
        frame->clearDirty();
        m_dirty = false;
        m_compositedFrame = frame;
    }
}
