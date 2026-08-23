#include "Frame.hpp"
#include "../DebugLog.h"
#include <cstdio>
#include <algorithm>

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
    bool removingActive = (m_layers[index].get() == m_activeLayer);
    m_layers.erase(m_layers.begin() + index);
    if (m_layers.empty()) {
        m_layers.push_back(std::make_unique<Layer>(m_width, m_height));
        m_layers[0]->setName("Layer 0");
        m_layers[0]->setFrame(this);
        m_activeLayer = m_layers[0].get();
        // Every previous layer is gone; any stored member lists now point at nothing.
        for (auto& g : m_groups)
            g.layerIndices.clear();
    } else {
        // Keep pointing at a live layer: prefer the slot the removed one occupied,
        // falling back to the top layer when the last entry was removed.
        if (removingActive || m_activeLayer == nullptr) {
            int newIdx = std::min(index, (int)m_layers.size() - 1);
            m_activeLayer = m_layers[newIdx].get();
        }
        // Drop the removed layer from its group(s), then close the index gap.
        for (auto& g : m_groups) {
            g.layerIndices.erase(
                std::remove(g.layerIndices.begin(), g.layerIndices.end(), index),
                g.layerIndices.end());
            for (auto& idx : g.layerIndices) {
                if (idx > index) idx--;
            }
        }
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

// Group management implementations

void Frame::addGroup(const char* name, uint32_t color) {
    Group g;
    g.name = name ? name : "Group";
    g.color = color;
    g.layerIndices.clear();
    m_groups.push_back(g);
    setDirty();
    DebugLog::log("[Frame] Added group '%s', total=%zu", g.name.c_str(), m_groups.size());
}

void Frame::removeGroup(int groupIndex) {
    if (groupIndex < 0 || groupIndex >= (int)m_groups.size()) return;
    DebugLog::log("[Frame] removeGroup(%d), total=%zu", groupIndex, m_groups.size());
    m_groups.erase(m_groups.begin() + groupIndex);
    setDirty();
}

const Frame::Group& Frame::getGroup(int index) const {
    static Group empty;
    if (index < 0 || index >= (int)m_groups.size()) return empty;
    return m_groups[index];
}

int Frame::findGroupForLayer(int layerIndex) const {
    for (int i = 0; i < (int)m_groups.size(); i++) {
        for (int idx : m_groups[i].layerIndices) {
            if (idx == layerIndex) return i;
        }
    }
    return -1;
}

void Frame::setGroupCollapsed(int groupIndex, bool collapsed) {
    if (groupIndex < 0 || groupIndex >= (int)m_groups.size()) return;
    m_groups[groupIndex].collapsed = collapsed;
    setDirty();
}

bool Frame::isGroupCollapsed(int groupIndex) const {
    if (groupIndex < 0 || groupIndex >= (int)m_groups.size()) return false;
    return m_groups[groupIndex].collapsed;
}

int Frame::groupIdForLayer(int layerIndex) const {
    int gidx = findGroupForLayer(layerIndex);
    if (gidx >= 0) return gidx;
    return -1;
}

void Frame::reorderLayer(int from, int to) {
    if (from == to || from < 0 || to < 0 || from >= (int)m_layers.size() || to >= (int)m_layers.size()) return;
    
    std::swap(m_layers[from], m_layers[to]);

    // m_activeLayer tracks the Layer object itself, which survives the swap
    // unchanged - no pointer fix-up needed (the old swap-based "fix-up" here
    // re-pointed the active layer at the WRONG object).
    
    // Update group layer indices
    for (auto& g : m_groups) {
        for (auto& idx : g.layerIndices) {
            if (idx == from) idx = to;
            else if (idx == to) idx = from;
        }
    }
    
    setDirty();
    DebugLog::log("[Frame] Reordered layer %d -> %d", from, to);
}

void Frame::setLayerVisible(int index, bool v) {
    if (index < 0 || index >= (int)m_layers.size()) return;
    m_layers[index]->setVisible(v);
    setDirty();
    DebugLog::log("[Frame] Set layer %d visible=%d", index, v);
}

void Frame::hideGroup(int groupId, bool hide) {
    if (groupId < 0 || groupId >= (int)m_groups.size()) return;
    // Toggle visibility for all layers in the group
    // We need to find the actual group by its logical ID
    // groupId here refers to the group index
    for (int idx : m_groups[groupId].layerIndices) {
        m_layers[idx]->setVisible(!hide);  // if hiding, set to false; if showing, set to true
    }
    setDirty();
    DebugLog::log("[Frame] Hide group %d, hide=%d", groupId, hide);
}

void Frame::setActiveLayerPreserveOrder(int index) {
    if (index < 0 || index >= (int)m_layers.size()) return;
    m_activeLayer = m_layers[index].get();
    // Don't change the Z-order, just the active layer
    setDirty();
    DebugLog::log("[Frame] setActiveLayerPreserveOrder(%d)", index);
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
