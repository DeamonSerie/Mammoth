#include "Layer.hpp"
#include "Frame.hpp"
#include "../DebugLog.h"
#include <algorithm>
#include <cstdio>

Layer::Layer() : m_name("Layer 0") {
    DebugLog::log("[Layer] Default constructor");
}

Layer::Layer(int width, int height)
    : m_width(width), m_height(height), m_name("Layer 0")
{
    m_pixels.resize(width * height * 4, 0);
    DebugLog::log("[Layer] Created %dx%d", width, height);
}

void Layer::resize(int w, int h) {
    DebugLog::log("[Layer] Resize %dx%d -> %dx%d", m_width, m_height, w, h);
    std::vector<uint8_t> newPixels(w * h * 4, 0);
    int copyW = std::min(m_width, w);
    int copyH = std::min(m_height, h);
    for (int y = 0; y < copyH; y++) {
        for (int x = 0; x < copyW; x++) {
            size_t srcOff = (y * m_width + x) * 4;
            size_t dstOff = (y * w + x) * 4;
            newPixels[dstOff + 0] = m_pixels[srcOff + 0];
            newPixels[dstOff + 1] = m_pixels[srcOff + 1];
            newPixels[dstOff + 2] = m_pixels[srcOff + 2];
            newPixels[dstOff + 3] = m_pixels[srcOff + 3];
        }
    }
    m_width = w;
    m_height = h;
    m_pixels = std::move(newPixels);
    setDirty();
}

Color Layer::getPixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return Color::transparent();
    size_t off = (y * m_width + x) * 4;
    return Color(m_pixels[off], m_pixels[off + 1], m_pixels[off + 2], m_pixels[off + 3]);
}

void Layer::setPixel(int x, int y, const Color& c) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return;
    size_t off = (y * m_width + x) * 4;
    m_pixels[off + 0] = c.r;
    m_pixels[off + 1] = c.g;
    m_pixels[off + 2] = c.b;
    m_pixels[off + 3] = c.a;
    setDirty();
}

void Layer::blendPixel(int x, int y, const Color& c) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return;
    if (c.a == 0) return;
    size_t off = (y * m_width + x) * 4;
    alphaBlend(m_pixels[off], m_pixels[off + 1], m_pixels[off + 2], m_pixels[off + 3],
               c.r, c.g, c.b, c.a);
    setDirty();
}

void Layer::alphaBlend(uint8_t& dstR, uint8_t& dstG, uint8_t& dstB, uint8_t& dstA,
                        uint8_t srcR, uint8_t srcG, uint8_t srcB, uint8_t srcA)
{
    if (srcA == 0) return;
    if (dstA == 0 || srcA == 255) {
        dstR = srcR;
        dstG = srcG;
        dstB = srcB;
        dstA = srcA;
        return;
    }
    float sa = srcA / 255.0f;
    float da = dstA / 255.0f;
    float outA = sa + da * (1.0f - sa);
    if (outA < 0.001f) {
        dstR = dstG = dstB = dstA = 0;
        return;
    }
    dstR = (uint8_t)((srcR * sa + dstR * da * (1.0f - sa)) / outA);
    dstG = (uint8_t)((srcG * sa + dstG * da * (1.0f - sa)) / outA);
    dstB = (uint8_t)((srcB * sa + dstB * da * (1.0f - sa)) / outA);
    dstA = (uint8_t)(outA * 255.0f);
}

void Layer::clear() {
    DebugLog::log("[Layer] clear()");
    std::fill(m_pixels.begin(), m_pixels.end(), 0);
    setDirty();
}

void Layer::setDirty() {
    m_dirty = true;
    if (m_frame) m_frame->setDirty();
}

void Layer::setAttributeLayer(bool isAttr, int sourceIdx) {
    m_isAttributeLayer = isAttr;
    m_attributeSourceIndex = sourceIdx;
    setDirty();
}

void Layer::remapAttributeSourceOnSwap(int a, int b) {
    if (m_attributeSourceIndex == a) m_attributeSourceIndex = b;
    else if (m_attributeSourceIndex == b) m_attributeSourceIndex = a;
}

void Layer::remapAttributeSourceOnRemove(int removedIndex) {
    if (m_attributeSourceIndex == removedIndex) m_attributeSourceIndex = -1;
    else if (m_attributeSourceIndex > removedIndex) m_attributeSourceIndex--;
}
