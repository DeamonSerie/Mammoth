#include "MoveTool.hpp"
#include "../DebugLog.h"
#include <cmath>
#include <cstring>

MoveTool::MoveTool() {}

void MoveTool::clearSelectionMask() {
    m_selMask.clear();
    m_maskOriginX = 0;
    m_maskOriginY = 0;
    m_maskW = 0;
    m_maskH = 0;
}

void MoveTool::clearFloat() {
    m_floatData.clear();
    m_floatW = 0;
    m_floatH = 0;
    m_floatOriginX = 0;
    m_floatOriginY = 0;
    m_hasFloat = false;
    m_prevDrawIntDx = 0;
    m_prevDrawIntDy = 0;
    clearSelectionMask();
}

void MoveTool::begin(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                     float screenX, float screenY, const Rect* selectionCanvasRect)
{
    Vec2 pos = canvas.screenToCanvas(
        screenX - canvasRect.x, screenY - canvasRect.y,
        canvasRect.w, canvasRect.h);

    m_hasSelection = (selectionCanvasRect != nullptr);
    DebugLog::log("[MoveTool] BEGIN hasSelection=%d hasFloat=%d pos=(%.1f,%.1f)", m_hasSelection, m_hasFloat, pos.x, pos.y);
    if (m_hasSelection) {
        m_selectionRect = *selectionCanvasRect;
    } else {
        m_selectionRect = {};
        clearFloat();
    }

    int layerW = layer.width();
    int layerH = layer.height();

    if (m_hasSelection) {
        int scanMinX = std::max(0, (int)std::floor(m_selectionRect.x));
        int scanMinY = std::max(0, (int)std::floor(m_selectionRect.y));
        int scanMaxX = std::min(layerW - 1, (int)std::ceil(m_selectionRect.x + m_selectionRect.w));
        int scanMaxY = std::min(layerH - 1, (int)std::ceil(m_selectionRect.y + m_selectionRect.h));

        const uint8_t* pixels = layer.data();

        if (!m_hasFloat) {
            int contentMinX = layerW, contentMaxX = -1;
            int contentMinY = layerH, contentMaxY = -1;
            for (int y = scanMinY; y <= scanMaxY; y++) {
                for (int x = scanMinX; x <= scanMaxX; x++) {
                    size_t off = (y * layerW + x) * 4;
                    if (pixels[off + 3] > 0) {
                        if (x < contentMinX) contentMinX = x;
                        if (x > contentMaxX) contentMaxX = x;
                        if (y < contentMinY) contentMinY = y;
                        if (y > contentMaxY) contentMaxY = y;
                    }
                }
            }

            if (contentMinX <= contentMaxX && contentMinY <= contentMaxY) {
                m_hasContent = true;
                m_hasFloat = true;
                m_contentMinX = contentMinX;
                m_contentMinY = contentMinY;
                m_contentMaxX = contentMaxX;
                m_contentMaxY = contentMaxY;

                m_floatOriginX = contentMinX;
                m_floatOriginY = contentMinY;
                m_floatW = contentMaxX - contentMinX + 1;
                m_floatH = contentMaxY - contentMinY + 1;
                m_floatData.resize(m_floatW * m_floatH * 4);

                uint8_t* layerData = layer.mutableData();

                // Extract selected pixels into float buffer
                for (int fy = 0; fy < m_floatH; fy++) {
                    int ly = m_floatOriginY + fy;
                    for (int fx = 0; fx < m_floatW; fx++) {
                        int lx = m_floatOriginX + fx;
                        size_t srcOff = (ly * layerW + lx) * 4;
                        size_t dstOff = (fy * m_floatW + fx) * 4;
                        m_floatData[dstOff + 0] = pixels[srcOff + 0];
                        m_floatData[dstOff + 1] = pixels[srcOff + 1];
                        m_floatData[dstOff + 2] = pixels[srcOff + 2];
                        m_floatData[dstOff + 3] = pixels[srcOff + 3];
                    }
                }

                // Clear selected pixels from layer FIRST
                for (int fy = 0; fy < m_floatH; fy++) {
                    int ny = m_floatOriginY + fy;
                    if (ny < 0 || ny >= layerH) continue;
                    for (int fx = 0; fx < m_floatW; fx++) {
                        int nx = m_floatOriginX + fx;
                        if (nx < 0 || nx >= layerW) continue;
                        size_t srcOff = (fy * m_floatW + fx) * 4;
                        if (m_floatData[srcOff + 3] > 0) {
                            size_t dstOff = (ny * layerW + nx) * 4;
                            layerData[dstOff + 0] = 0;
                            layerData[dstOff + 1] = 0;
                            layerData[dstOff + 2] = 0;
                            layerData[dstOff + 3] = 0;
                        }
                    }
                }

                // Save snapshot AFTER clearing - snapshot has hole at original position
                m_savedData.assign(layer.data(), layer.data() + layer.dataSize());

                // Draw float at original position for immediate visibility
                for (int fy = 0; fy < m_floatH; fy++) {
                    int ny = m_floatOriginY + fy;
                    if (ny < 0 || ny >= layerH) continue;
                    for (int fx = 0; fx < m_floatW; fx++) {
                        int nx = m_floatOriginX + fx;
                        if (nx < 0 || nx >= layerW) continue;
                        size_t srcOff = (fy * m_floatW + fx) * 4;
                        if (m_floatData[srcOff + 3] > 0) {
                            size_t dstOff = (ny * layerW + nx) * 4;
                            layerData[dstOff + 0] = m_floatData[srcOff + 0];
                            layerData[dstOff + 1] = m_floatData[srcOff + 1];
                            layerData[dstOff + 2] = m_floatData[srcOff + 2];
                            layerData[dstOff + 3] = m_floatData[srcOff + 3];
                        }
                    }
                }
                layer.setDirty();
            } else {
                m_hasContent = false;
            }
        } else {
            m_hasContent = true;
        }

        m_moving = true;
        m_moveStart = pos;
        m_layerW = layerW;
        m_layerH = layerH;

        float centerX;
        float centerY;
        if (!m_hasFloat) {
            centerX = m_floatOriginX + m_floatW * 0.5f;
            centerY = m_floatOriginY + m_floatH * 0.5f;
            m_prevDrawIntDx = 0;
            m_prevDrawIntDy = 0;
        } else {
            centerX = m_floatOriginX + m_floatW * 0.5f + m_prevDrawIntDx;
            centerY = m_floatOriginY + m_floatH * 0.5f + m_prevDrawIntDy;
        }
        m_grabOffset = {centerX - pos.x, centerY - pos.y};
        m_dragDx = (float)m_prevDrawIntDx;
        m_dragDy = (float)m_prevDrawIntDy;
        DebugLog::log("[MoveTool] BEGIN sel: hasFloat=%d hasContent=%d floatOrigin=(%d,%d) floatSize=(%d,%d) prevDrawInt=(%d,%d) grabOffset=(%.1f,%.1f) dragDx=%.1f",
            m_hasFloat, m_hasContent, m_floatOriginX, m_floatOriginY, m_floatW, m_floatH,
            m_prevDrawIntDx, m_prevDrawIntDy, m_grabOffset.x, m_grabOffset.y, m_dragDx);

    } else {
        int minX = layerW, maxX = -1, minY = layerH, maxY = -1;
        const uint8_t* pixels = layer.data();
        for (int y = 0; y < layerH; y++) {
            for (int x = 0; x < layerW; x++) {
                size_t off = (y * layerW + x) * 4;
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
        m_layerW = layerW;
        m_layerH = layerH;

        if (minX <= maxX && minY <= maxY) {
            m_hasContent = true;
            m_hasFloat = true;

            m_contentMinX = minX;
            m_contentMinY = minY;
            m_contentMaxX = maxX;
            m_contentMaxY = maxY;

            m_floatOriginX = minX;
            m_floatOriginY = minY;
            m_floatW = maxX - minX + 1;
            m_floatH = maxY - minY + 1;
            m_floatData.resize(m_floatW * m_floatH * 4);

            uint8_t* layerData = layer.mutableData();

            for (int fy = 0; fy < m_floatH; fy++) {
                int ly = m_floatOriginY + fy;
                for (int fx = 0; fx < m_floatW; fx++) {
                    int lx = m_floatOriginX + fx;
                    size_t srcOff = (ly * layerW + lx) * 4;
                    size_t dstOff = (fy * m_floatW + fx) * 4;
                    m_floatData[dstOff + 0] = pixels[srcOff + 0];
                    m_floatData[dstOff + 1] = pixels[srcOff + 1];
                    m_floatData[dstOff + 2] = pixels[srcOff + 2];
                    m_floatData[dstOff + 3] = pixels[srcOff + 3];
                }
            }

            for (int fy = 0; fy < m_floatH; fy++) {
                int ny = m_floatOriginY + fy;
                for (int fx = 0; fx < m_floatW; fx++) {
                    int nx = m_floatOriginX + fx;
                    size_t srcOff = (fy * m_floatW + fx) * 4;
                    if (m_floatData[srcOff + 3] > 0) {
                        size_t dstOff = (ny * layerW + nx) * 4;
                        layerData[dstOff + 0] = 0;
                        layerData[dstOff + 1] = 0;
                        layerData[dstOff + 2] = 0;
                        layerData[dstOff + 3] = 0;
                    }
                }
            }

            m_savedData.assign(layer.data(), layer.data() + layer.dataSize());

            for (int fy = 0; fy < m_floatH; fy++) {
                int ny = m_floatOriginY + fy;
                if (ny < 0 || ny >= layerH) continue;
                for (int fx = 0; fx < m_floatW; fx++) {
                    int nx = m_floatOriginX + fx;
                    if (nx < 0 || nx >= layerW) continue;
                    size_t srcOff = (fy * m_floatW + fx) * 4;
                    if (m_floatData[srcOff + 3] > 0) {
                        size_t dstOff = (ny * layerW + nx) * 4;
                        layerData[dstOff + 0] = m_floatData[srcOff + 0];
                        layerData[dstOff + 1] = m_floatData[srcOff + 1];
                        layerData[dstOff + 2] = m_floatData[srcOff + 2];
                        layerData[dstOff + 3] = m_floatData[srcOff + 3];
                    }
                }
            }
            layer.setDirty();

            float centerX = (minX + maxX) * 0.5f;
            float centerY = (minY + maxY) * 0.5f;
            m_grabOffset = {centerX - pos.x, centerY - pos.y};
        } else {
            m_grabOffset = {0, 0};
            m_hasContent = false;
        }
        m_dragDx = 0.0f;
        m_dragDy = 0.0f;
        m_prevIntDx = 0;
        m_prevIntDy = 0;
    }
}

void MoveTool::update(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                      float screenX, float screenY)
{
    if (!m_moving || !m_hasContent) {
        DebugLog::log("[MoveTool] UPDATE SKIP: moving=%d hasContent=%d", m_moving, m_hasContent);
        return;
    }

    Vec2 pos = canvas.screenToCanvas(
        screenX - canvasRect.x, screenY - canvasRect.y,
        canvasRect.w, canvasRect.h);

    float targetCenterX = pos.x + m_grabOffset.x;
    float targetCenterY = pos.y + m_grabOffset.y;

    float currentCenterX;
    float currentCenterY;
    if (m_hasFloat) {
        currentCenterX = m_floatOriginX + m_floatW * 0.5f + m_dragDx;
        currentCenterY = m_floatOriginY + m_floatH * 0.5f + m_dragDy;
    } else {
        currentCenterX = (m_contentMinX + m_contentMaxX) * 0.5f + m_dragDx;
        currentCenterY = (m_contentMinY + m_contentMaxY) * 0.5f + m_dragDy;
    }

    float dx = targetCenterX - currentCenterX;
    float dy = targetCenterY - currentCenterY;

    m_dragDx += dx;
    m_dragDy += dy;

    float dxMin = -m_contentMinX;
    float dxMax = m_layerW - 1 - m_contentMaxX;
    float dyMin = -m_contentMinY;
    float dyMax = m_layerH - 1 - m_contentMaxY;
    if (m_dragDx < dxMin) m_dragDx = dxMin;
    if (m_dragDx > dxMax) m_dragDx = dxMax;
    if (m_dragDy < dyMin) m_dragDy = dyMin;
    if (m_dragDy > dyMax) m_dragDy = dyMax;

    int intDx = (int)std::floor(m_dragDx);
    int intDy = (int)std::floor(m_dragDy);

    uint8_t* layerData = layer.mutableData();

    if (m_hasFloat) {
        std::memcpy(layerData, m_savedData.data(), m_savedData.size());

        for (int fy = 0; fy < m_floatH; fy++) {
            int ny = m_floatOriginY + fy + intDy;
            if (ny < 0 || ny >= m_layerH) continue;
            for (int fx = 0; fx < m_floatW; fx++) {
                int nx = m_floatOriginX + fx + intDx;
                if (nx < 0 || nx >= m_layerW) continue;
                size_t srcOff = (fy * m_floatW + fx) * 4;
                if (m_floatData[srcOff + 3] > 0) {
                    size_t dstOff = (ny * m_layerW + nx) * 4;
                    layerData[dstOff + 0] = m_floatData[srcOff + 0];
                    layerData[dstOff + 1] = m_floatData[srcOff + 1];
                    layerData[dstOff + 2] = m_floatData[srcOff + 2];
                    layerData[dstOff + 3] = m_floatData[srcOff + 3];
                }
            }
        }

        m_prevDrawIntDx = intDx;
        m_prevDrawIntDy = intDy;
    }
    layer.setDirty();
}

void MoveTool::end(Layer& layer) {
    if (!m_moving || !m_hasContent) {
        m_moving = false;
        m_moveStart = {-1, -1};
        m_layerW = 0;
        m_layerH = 0;
        return;
    }

    int intDx = (int)std::floor(m_dragDx);
    int intDy = (int)std::floor(m_dragDy);

    uint8_t* layerData = layer.mutableData();

    if (m_hasFloat) {
        std::memcpy(layerData, m_savedData.data(), m_savedData.size());

        for (int fy = 0; fy < m_floatH; fy++) {
            int ny = m_floatOriginY + fy + intDy;
            if (ny < 0 || ny >= m_layerH) continue;
            for (int fx = 0; fx < m_floatW; fx++) {
                int nx = m_floatOriginX + fx + intDx;
                if (nx < 0 || nx >= m_layerW) continue;
                size_t srcOff = (fy * m_floatW + fx) * 4;
                if (m_floatData[srcOff + 3] > 0) {
                    size_t dstOff = (ny * m_layerW + nx) * 4;
                    layerData[dstOff + 0] = m_floatData[srcOff + 0];
                    layerData[dstOff + 1] = m_floatData[srcOff + 1];
                    layerData[dstOff + 2] = m_floatData[srcOff + 2];
                    layerData[dstOff + 3] = m_floatData[srcOff + 3];
                }
            }
        }

        m_prevDrawIntDx = intDx;
        m_prevDrawIntDy = intDy;
    }
    layer.setDirty();

    m_moving = false;
    m_moveStart = {-1, -1};
    m_layerW = 0;
    m_layerH = 0;
}
