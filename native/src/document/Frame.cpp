#include "Frame.hpp"
#include "../DebugLog.h"
#include <cstdio>

Frame::Frame() {
    DebugLog::log("[Frame] Default constructor");
    m_layers.push_back(std::make_unique<Layer>());
    m_layers[0]->setName("Layer 0");
    m_layers[0]->setFrame(this);
    m_activeLayer = m_layers[0].get();
}

Frame::Frame(int width, int height)
    : m_width(width), m_height(height)
{
    DebugLog::log("[Frame] Created %dx%d", width, height);
    m_layers.push_back(std::make_unique<Layer>(width, height));
    m_layers[0]->setName("Layer 0");
    m_layers[0]->setFrame(this);
    m_activeLayer = m_layers[0].get();
}

void Frame::resize(int w, int h) {
    DebugLog::log("[Frame] Resize %dx%d -> %dx%d, layers=%zu", m_width, m_height, w, h, m_layers.size());
    m_width = w;
    m_height = h;
    for (auto& layer : m_layers)
        layer->resize(w, h);
    setDirty();
}

Layer* Frame::addLayer(const char* name) {
    auto layer = std::make_unique<Layer>(m_width, m_height);
    if (name) layer->setName(name);
    else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Layer %d", (int)m_layers.size());
        layer->setName(buf);
    }
    Layer* ptr = layer.get();
    ptr->setFrame(this);
    m_layers.push_back(std::move(layer));
    m_activeLayer = ptr;
    setDirty();
    DebugLog::log("[Frame] Added layer '%s', total=%zu", ptr->name(), m_layers.size());
    return ptr;
}

void Frame::removeLayer(int index) {
    DebugLog::log("[Frame] removeLayer(%d), current layers=%zu", index, m_layers.size());
    if (index < 0 || index >= (int)m_layers.size()) return;
    m_layers.erase(m_layers.begin() + index);
    if (m_layers.empty()) {
        m_layers.push_back(std::make_unique<Layer>(m_width, m_height));
    }
    if (m_activeLayer == nullptr || index == 0) {
        m_activeLayer = m_layers[0].get();
    }
    setDirty();
}

Layer* Frame::getLayer(int index) {
    if (index < 0 || index >= (int)m_layers.size()) return nullptr;
    return m_layers[index].get();
}

const Layer* Frame::getLayer(int index) const {
    if (index < 0 || index >= (int)m_layers.size()) return nullptr;
    return m_layers[index].get();
}

void Frame::setActiveLayer(int index) {
    DebugLog::log("[Frame] setActiveLayer(%d), layers=%zu", index, m_layers.size());
    Layer* l = getLayer(index);
    if (l) m_activeLayer = l;
}

void Frame::clear() {
    DebugLog::log("[Frame] clear()");
    for (auto& layer : m_layers)
        layer->clear();
    setDirty();
}

void Frame::clearDirty() {
    DebugLog::log("[Frame] clearDirty()");
    m_dirty = false;
    for (auto& layer : m_layers)
        layer->clearDirty();
}

void Frame::compositeToBuffer(std::vector<uint8_t>& out, int& outW, int& outH) const {
    DebugLog::log("[Frame] compositeToBuffer %dx%d, layers=%zu", m_width, m_height, m_layers.size());
    outW = m_width;
    outH = m_height;
    out.assign(m_width * m_height * 4, 0);

    for (auto& layer : m_layers) {
        if (!layer->visible()) continue;
        float layerOpacity = layer->opacity();
        const uint8_t* ld = layer->data();

        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                size_t off = (y * m_width + x) * 4;
                uint8_t sr = ld[off + 0];
                uint8_t sg = ld[off + 1];
                uint8_t sb = ld[off + 2];
                uint8_t sa = ld[off + 3];

                if (layerOpacity < 1.0f) {
                    sa = (uint8_t)(sa * layerOpacity);
                }

                if (sa == 0) continue;

                uint8_t& dr = out[off + 0];
                uint8_t& dg = out[off + 1];
                uint8_t& db = out[off + 2];
                uint8_t& da = out[off + 3];

                Layer::alphaBlend(dr, dg, db, da, sr, sg, sb, sa);
            }
        }
    }

    if (m_opacity < 1.0f) {
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                size_t off = (y * m_width + x) * 4;
                out[off + 3] = (uint8_t)(out[off + 3] * m_opacity);
            }
        }
    }
}
