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
    m_stack.push_back({false, 0});
}

Frame::Frame(int width, int height)
    : m_width(width), m_height(height)
{
    DebugLog::log("[Frame] Created %dx%d", width, height);
    m_layers.push_back(std::make_unique<Layer>(width, height));
    m_layers[0]->setName("Layer 0");
    m_layers[0]->setFrame(this);
    m_activeLayer = m_layers[0].get();
    m_stack.push_back({false, 0});
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
    m_stack.push_back({false, (int)m_layers.size() - 1});   // new layer on top
    m_activeLayer = ptr;
    setDirty();
    DebugLog::log("[Frame] Added layer '%s', total=%zu", ptr->name(), m_layers.size());
    return ptr;
}

Layer* Frame::insertLayer(int index, const char* name) {
    DebugLog::log("[Frame] insertLayer(%d), current layers=%zu", index, m_layers.size());
    if (index < 0 || index > (int)m_layers.size()) return nullptr;
    auto layer = std::make_unique<Layer>(m_width, m_height);
    if (name && name[0]) {
        layer->setName(name);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Layer %d", (int)m_layers.size());
        layer->setName(buf);
    }
    Layer* ptr = layer.get();
    ptr->setFrame(this);
    m_layers.insert(m_layers.begin() + index, std::move(layer));

    // Everything stored by index at or above the insertion point shifts up
    for (auto& g : m_groups) {
        for (auto& idx : g.layerIndices)
            if (idx >= index) idx++;
    }
    // Stack nodes that reference plain layers shift the same way; group
    // nodes are untouched (they index m_groups, not m_layers).
    for (auto& nd : m_stack) {
        if (!nd.isGroup && nd.index >= index) nd.index++;
    }
    for (auto& l : m_layers) {
        if (l.get() == ptr) continue;
        l->bumpAttributeSourceAtOrAbove(index);
    }

    // Slot the new layer into the stack just below the first plain-layer
    // node that now sits above it (attribute layers are inserted at their
    // holder's old storage slot, so this lands them directly beneath the
    // holder when it owns a node). Attribute layers never paint, so the
    // exact slot only affects panel adjacency.
    auto insertAt = m_stack.end();
    for (auto it = m_stack.begin(); it != m_stack.end(); ++it) {
        if (!it->isGroup && it->index > index) { insertAt = it; break; }
    }
    m_stack.insert(insertAt, {false, index});

    m_activeLayer = ptr;
    setDirty();
    return ptr;
}

void Frame::removeLayer(int index) {
    DebugLog::log("[Frame] removeLayer(%d), current layers=%zu", index, m_layers.size());
    if (index < 0 || index >= (int)m_layers.size()) return;

    // Cascade: attribute layers bound (transitively) to a removed layer are
    // removed along with it - they are meaningless without their holder.
    std::vector<char> doomed(m_layers.size(), 0);
    doomed[index] = 1;
    bool grew = true;
    while (grew) {
        grew = false;
        for (int i = 0; i < (int)m_layers.size(); i++) {
            if (doomed[i]) continue;
            Layer* l = m_layers[i].get();
            int s = l->attributeSourceIndex();
            if (l->isAttributeLayer() && s >= 0 && s < (int)doomed.size() && doomed[s]) {
                doomed[i] = 1;
                grew = true;
                DebugLog::log("[Frame] Cascade-removing attribute layer %d (holder %d deleted)", i, s);
            }
        }
    }

    std::vector<int> removedIndices;   // ascending
    for (int i = 0; i < (int)doomed.size(); i++)
        if (doomed[i]) removedIndices.push_back(i);

    bool removingActive = false;
    for (int idx : removedIndices) {
        if (m_layers[idx].get() == m_activeLayer) { removingActive = true; break; }
    }

    // Erase highest index first so the remaining indices stay valid
    for (int r = (int)removedIndices.size() - 1; r >= 0; r--)
        m_layers.erase(m_layers.begin() + removedIndices[r]);

    if (m_layers.empty()) {
        m_layers.push_back(std::make_unique<Layer>(m_width, m_height));
        m_layers[0]->setName("Layer 0");
        m_layers[0]->setFrame(this);
        m_activeLayer = m_layers[0].get();
        // Every previous layer is gone: rebuild the stack without their
        // nodes, but KEEP the group nodes - deleting the last layer must
        // never dissolve grouping. The fresh layer takes the first vacated
        // slot (bottom of the paint order when none remain).
        std::vector<StackNode> kept;
        int freshSlot = -1;
        for (const StackNode& nd : m_stack) {
            if (nd.isGroup) {
                kept.push_back(nd);
                continue;
            }
            if (freshSlot < 0) freshSlot = (int)kept.size();
        }
        if (freshSlot < 0) freshSlot = (int)kept.size();
        kept.insert(kept.begin() + freshSlot, {false, 0});
        m_stack.swap(kept);
        // Member lists all referenced removed layers.
        for (auto& g : m_groups)
            g.layerIndices.clear();
    } else {
        // Keep pointing at a live layer: prefer the slot the removal started at,
        // falling back to the top layer when the last entry was removed.
        if (removingActive || m_activeLayer == nullptr) {
            int newIdx = std::min(index, (int)m_layers.size() - 1);
            m_activeLayer = m_layers[newIdx].get();
        }
        // Drop removed layers from their group(s), then close the index gaps.
        for (auto& g : m_groups) {
            for (int ri : removedIndices) {
                g.layerIndices.erase(
                    std::remove(g.layerIndices.begin(), g.layerIndices.end(), ri),
                    g.layerIndices.end());
                for (auto& idx : g.layerIndices) {
                    if (idx > ri) idx--;
                }
            }
        }
        // Same bookkeeping for stack nodes: erase the removed layers' own
        // slots (group members have none) and shift the survivors down.
        for (int ri : removedIndices) {
            m_stack.erase(
                std::remove_if(m_stack.begin(), m_stack.end(),
                               [ri](const StackNode& nd) {
                                   return !nd.isGroup && nd.index == ri;
                               }),
                m_stack.end());
            for (auto& nd : m_stack) {
                if (!nd.isGroup && nd.index > ri) nd.index--;
            }
        }
        // Same fix-up for attribute-layer source references. Survivors can
        // never reference a removed slot (the cascade would have doomed them),
        // so this only shifts sources that sat above the removals.
        for (auto& layer : m_layers) {
            for (int ri : removedIndices)
                layer->remapAttributeSourceOnRemove(ri);
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
    // A new group lands on top of the paint stack as one node.
    m_stack.push_back({true, (int)m_groups.size() - 1});
    setDirty();
    DebugLog::log("[Frame] Added group '%s', total=%zu", g.name.c_str(), m_groups.size());
}

void Frame::removeGroup(int groupIndex) {
    if (groupIndex < 0 || groupIndex >= (int)m_groups.size()) return;
    DebugLog::log("[Frame] removeGroup(%d), total=%zu", groupIndex, m_groups.size());

    // Splice the members back into the outer stack at the group's position,
    // keeping their group-internal order so nothing visually jumps.
    auto pos = std::find_if(m_stack.begin(), m_stack.end(),
                            [groupIndex](const StackNode& nd) {
                                return nd.isGroup && nd.index == groupIndex;
                            });
    if (pos != m_stack.end()) {
        std::vector<StackNode> replacement;
        for (int idx : m_groups[groupIndex].layerIndices)
            replacement.push_back({false, idx});
        auto insertAt = m_stack.erase(pos);
        m_stack.insert(insertAt, replacement.begin(), replacement.end());
    }
    m_groups.erase(m_groups.begin() + groupIndex);
    // Groups above the erased one shift down; fix their node indices.
    for (auto& nd : m_stack) {
        if (nd.isGroup && nd.index > groupIndex) nd.index--;
    }
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

void Frame::renameGroup(int index, const char* name) {
    if (index < 0 || index >= (int)m_groups.size() || !name) return;
    m_groups[index].name = name;
    DebugLog::log("[Frame] Renamed group %d to '%s'", index, name);
}

int Frame::groupIdForLayer(int layerIndex) const {
    int gidx = findGroupForLayer(layerIndex);
    if (gidx >= 0) return gidx;
    return -1;
}

void Frame::addLayerToGroup(int layerIndex, int groupIndex) {
    if (layerIndex < 0 || layerIndex >= (int)m_layers.size()) return;
    if (groupIndex < 0 || groupIndex >= (int)m_groups.size()) return;
    auto& idxs = m_groups[groupIndex].layerIndices;
    if (std::find(idxs.begin(), idxs.end(), layerIndex) != idxs.end()) return;
    for (auto& g : m_groups) {
        g.layerIndices.erase(
            std::remove(g.layerIndices.begin(), g.layerIndices.end(), layerIndex),
            g.layerIndices.end());
    }
    // The layer stops owning a top-level stack slot: it now paints inside
    // its group's member run.
    m_stack.erase(
        std::remove_if(m_stack.begin(), m_stack.end(),
                       [layerIndex](const StackNode& nd) {
                           return !nd.isGroup && nd.index == layerIndex;
                       }),
        m_stack.end());
    idxs.push_back(layerIndex);
    setDirty();
    DebugLog::log("[Frame] Added layer %d to group '%s'", layerIndex,
                  m_groups[groupIndex].name.c_str());
}

const Frame::StackNode& Frame::stackNode(int i) const {
    static StackNode none;
    if (i < 0 || i >= (int)m_stack.size()) return none;
    return m_stack[i];
}

int Frame::stackPosForLayer(int layerIndex) const {
    for (int i = 0; i < (int)m_stack.size(); i++) {
        const auto& nd = m_stack[i];
        if (!nd.isGroup && nd.index == layerIndex) return i;
    }
    return -1;
}

int Frame::stackPosForGroup(int groupIndex) const {
    for (int i = 0; i < (int)m_stack.size(); i++) {
        const auto& nd = m_stack[i];
        if (nd.isGroup && nd.index == groupIndex) return i;
    }
    return -1;
}

std::vector<int> Frame::paintOrder() const {
    std::vector<int> order;
    order.reserve(m_layers.size());
    for (const auto& nd : m_stack) {
        if (nd.isGroup) {
            if (nd.index < 0 || nd.index >= (int)m_groups.size()) continue;
            for (int idx : m_groups[nd.index].layerIndices)
                order.push_back(idx);
        } else {
            order.push_back(nd.index);
        }
    }
    return order;
}

bool Frame::moveStackItem(int stackPos, int delta) {
    int target = stackPos + delta;
    if (stackPos < 0 || target < 0 ||
        stackPos >= (int)m_stack.size() || target >= (int)m_stack.size())
        return false;
    std::swap(m_stack[stackPos], m_stack[target]);
    setDirty();
    return true;
}

void Frame::reorderGroupMember(int groupIndex, int memberFrom, int memberTo) {
    if (groupIndex < 0 || groupIndex >= (int)m_groups.size()) return;
    auto& idxs = m_groups[groupIndex].layerIndices;
    if (memberFrom < 0 || memberFrom >= (int)idxs.size()) return;
    if (memberTo < 0 || memberTo >= (int)idxs.size() || memberFrom == memberTo) return;
    int v = idxs[memberFrom];
    idxs.erase(idxs.begin() + memberFrom);
    idxs.insert(idxs.begin() + memberTo, v);
    setDirty();
    DebugLog::log("[Frame] Group '%s' member %d -> pos %d",
                  m_groups[groupIndex].name.c_str(), v, memberTo);
}

void Frame::setLayerVisible(int index, bool v) {
    if (index < 0 || index >= (int)m_layers.size()) return;
    m_layers[index]->setVisible(v);
    setDirty();
    DebugLog::log("[Frame] Set layer %d visible=%d", index, v);
}

void Frame::renameLayer(int index, const char* name) {
    if (index < 0 || index >= (int)m_layers.size() || !name) return;
    m_layers[index]->setName(name);
    DebugLog::log("[Frame] Renamed layer %d to '%s'", index, name);
}

int Frame::attributeChainDepth(int index) const {
    if (index < 0 || index >= (int)m_layers.size()) return 0;
    std::vector<char> seen(m_layers.size(), 0);
    int depth = 0;
    int cur = index;
    seen[cur] = 1;
    const Layer* l = m_layers[cur].get();
    while (l && l->isAttributeLayer()) {
        int s = l->attributeSourceIndex();
        if (s < 0 || s >= (int)m_layers.size() || seen[s]) break;
        seen[s] = 1;
        cur = s;
        l = m_layers[cur].get();
        depth++;
    }
    return depth;
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

    // Attribute layers never draw their own pixels. Instead each visible one
    // modifies its source layer: opacity multipliers stack, and the topmost
    // visible attribute layer providing a tint wins.
    //
    // Both passes walk the PAINT STACK, not raw storage order: groups are
    // single z-slots whose members paint together in group-internal order.
    const int n = (int)m_layers.size();
    const std::vector<int> order = paintOrder();
    std::vector<float> opMul(n, 1.0f);
    std::vector<int> tintFrom(n, -1);
    for (int li : order) {
        if (li < 0 || li >= n) continue;
        const Layer* al = m_layers[li].get();
        if (!al->isAttributeLayer() || !al->visible()) continue;
        int src = al->attributeSourceIndex();
        if (src < 0 || src >= n || src == li) continue;
        opMul[src] *= al->attrOpacity();
        if (al->attrTint() != 0) tintFrom[src] = li;
    }

    for (int li : order) {
        if (li < 0 || li >= n) continue;
        Layer* layer = m_layers[li].get();
        if (!layer->visible()) continue;
        if (layer->isAttributeLayer()) continue;
        float layerOpacity = layer->opacity() * opMul[li];
        const uint8_t* ld = layer->data();

        uint8_t tr = 0, tg = 0, tb = 0, ts = 0;
        bool tinted = false;
        if (tintFrom[li] >= 0) {
            uint32_t t = m_layers[tintFrom[li]]->attrTint();
            tr = (uint8_t)(t & 0xFF);
            tg = (uint8_t)((t >> 8) & 0xFF);
            tb = (uint8_t)((t >> 16) & 0xFF);
            ts = (uint8_t)((t >> 24) & 0xFF);
            tinted = (ts > 0);
        }

        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                size_t off = (y * m_width + x) * 4;
                uint8_t sr = ld[off + 0];
                uint8_t sg = ld[off + 1];
                uint8_t sb = ld[off + 2];
                uint8_t sa = ld[off + 3];

                if (tinted) {
                    // Lerp each channel toward its multiplicative tint
                    sr = (uint8_t)(sr + ((sr * tr / 255) - sr) * ts / 255);
                    sg = (uint8_t)(sg + ((sg * tg / 255) - sg) * ts / 255);
                    sb = (uint8_t)(sb + ((sb * tb / 255) - sb) * ts / 255);
                }

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
