#include "DrawingDocument.hpp"
#include "../DebugLog.h"
#include <algorithm>
#include <cstdio>

DrawingDocument::DrawingDocument() : m_name("Untitled") {
    DebugLog::log("[DrawingDocument] Default constructor");
    m_frames.push_back(std::make_unique<Frame>());
    m_activeFrame = m_frames[0].get();
}

DrawingDocument::DrawingDocument(int width, int height, const char* name)
    : m_width(width), m_height(height), m_name(name ? name : "Untitled")
{
    DebugLog::log("[DrawingDocument] Created %dx%d name='%s'", width, height, m_name.c_str());
    m_frames.push_back(std::make_unique<Frame>(width, height));
    m_activeFrame = m_frames[0].get();
}

void DrawingDocument::resize(int w, int h) {
    DebugLog::log("[DrawingDocument] Resize %dx%d -> %dx%d, frames=%zu", m_width, m_height, w, h, m_frames.size());
    m_width = w;
    m_height = h;
    for (auto& frame : m_frames)
        frame->resize(w, h);
}

Frame* DrawingDocument::addFrame(int index) {
    auto frame = std::make_unique<Frame>(m_width, m_height);
    Frame* ptr = frame.get();
    if (index < 0 || index >= (int)m_frames.size())
        index = (int)m_frames.size();   // append
    else {
        // Frames stored by index at or above the insertion point shift up;
        // group member lists must follow.
        for (auto& g : m_frameGroups) {
            for (auto& idx : g.frameIndices)
                if (idx >= index) idx++;
        }
    }
    m_frames.insert(m_frames.begin() + index, std::move(frame));
    m_activeFrame = ptr;
    DebugLog::log("[DrawingDocument] Added frame at %d, total=%zu", index, m_frames.size());
    return ptr;
}

void DrawingDocument::removeFrame(int index) {
    DebugLog::log("[DrawingDocument] removeFrame(%d), frames=%zu", index, m_frames.size());
    if (index < 0 || index >= (int)m_frames.size()) return;
    if (m_frames.size() <= 1) return;
    m_frames.erase(m_frames.begin() + index);
    // Drop the removed frame from its group, then close the index gaps.
    for (auto& g : m_frameGroups) {
        g.frameIndices.erase(
            std::remove(g.frameIndices.begin(), g.frameIndices.end(), index),
            g.frameIndices.end());
        for (auto& idx : g.frameIndices) {
            if (idx > index) idx--;
        }
    }
    if (index >= (int)m_frames.size())
        index = (int)m_frames.size() - 1;
    m_activeFrame = m_frames[index].get();
}

void DrawingDocument::duplicateFrame(int index) {
    DebugLog::log("[DrawingDocument] duplicateFrame(%d), frames=%zu", index, m_frames.size());
    if (index < 0 || index >= (int)m_frames.size()) return;
    Frame* src = m_frames[index].get();
    std::vector<uint8_t> buf;
    int bufW, bufH;
    src->compositeToBuffer(buf, bufW, bufH);

    auto frame = std::make_unique<Frame>(m_width, m_height);
    if (m_activeFrame) {
        frame->setOpacity(src->opacity());
        frame->setDuration(src->duration());
    }
    frame->clear();

    Layer* dstLayer = frame->getLayer(0);
    if (dstLayer && bufW == m_width && bufH == m_height) {
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                size_t off = (y * m_width + x) * 4;
                dstLayer->setPixel(x, y, Color(buf[off], buf[off+1], buf[off+2], buf[off+3]));
            }
        }
    }

    Frame* ptr = frame.get();
    // duplicateFrame inserts at index+1: shift stored indices above it.
    for (auto& g : m_frameGroups) {
        for (auto& idx : g.frameIndices)
            if (idx > index) idx++;
    }
    m_frames.insert(m_frames.begin() + index + 1, std::move(frame));
    m_activeFrame = ptr;
}

Frame* DrawingDocument::getFrame(int index) {
    if (index < 0 || index >= (int)m_frames.size()) return nullptr;
    return m_frames[index].get();
}

const Frame* DrawingDocument::getFrame(int index) const {
    if (index < 0 || index >= (int)m_frames.size()) return nullptr;
    return m_frames[index].get();
}

void DrawingDocument::setActiveFrame(int index) {
    DebugLog::log("[DrawingDocument] setActiveFrame(%d), frames=%zu", index, m_frames.size());
    Frame* f = getFrame(index);
    if (f) m_activeFrame = f;
}

int DrawingDocument::activeFrameIndex() const {
    for (int i = 0; i < (int)m_frames.size(); i++) {
        if (m_frames[i].get() == m_activeFrame) return i;
    }
    return 0;
}

// ---- Frame groups ------------------------------------------------------------

void DrawingDocument::addFrameGroup(const char* name, uint32_t color) {
    FrameGroup g;
    g.name = name ? name : "Group";
    g.color = color;
    m_frameGroups.push_back(g);
    DebugLog::log("[DrawingDocument] Added frame group '%s', total=%zu",
                  g.name.c_str(), m_frameGroups.size());
}

void DrawingDocument::removeFrameGroup(int groupIndex) {
    if (groupIndex < 0 || groupIndex >= (int)m_frameGroups.size()) return;
    DebugLog::log("[DrawingDocument] removeFrameGroup(%d)", groupIndex);
    m_frameGroups.erase(m_frameGroups.begin() + groupIndex);
}

const DrawingDocument::FrameGroup& DrawingDocument::getFrameGroup(int index) const {
    static FrameGroup empty;
    if (index < 0 || index >= (int)m_frameGroups.size()) return empty;
    return m_frameGroups[index];
}

int DrawingDocument::findGroupForFrame(int frameIndex) const {
    for (int i = 0; i < (int)m_frameGroups.size(); i++) {
        for (int idx : m_frameGroups[i].frameIndices)
            if (idx == frameIndex) return i;
    }
    return -1;
}

void DrawingDocument::addFrameToGroup(int frameIndex, int groupIndex) {
    if (frameIndex < 0 || frameIndex >= (int)m_frames.size()) return;
    if (groupIndex < 0 || groupIndex >= (int)m_frameGroups.size()) return;
    auto& idxs = m_frameGroups[groupIndex].frameIndices;
    if (std::find(idxs.begin(), idxs.end(), frameIndex) != idxs.end()) return;
    for (auto& g : m_frameGroups) {
        g.frameIndices.erase(
            std::remove(g.frameIndices.begin(), g.frameIndices.end(), frameIndex),
            g.frameIndices.end());
    }
    idxs.push_back(frameIndex);
    DebugLog::log("[DrawingDocument] Added frame %d to group '%s'", frameIndex,
                  m_frameGroups[groupIndex].name.c_str());
}

void DrawingDocument::removeFrameFromGroup(int frameIndex) {
    for (auto& g : m_frameGroups) {
        g.frameIndices.erase(
            std::remove(g.frameIndices.begin(), g.frameIndices.end(), frameIndex),
            g.frameIndices.end());
    }
}

void DrawingDocument::setFrameGroupCollapsed(int groupIndex, bool collapsed) {
    if (groupIndex < 0 || groupIndex >= (int)m_frameGroups.size()) return;
    m_frameGroups[groupIndex].collapsed = collapsed;
}

bool DrawingDocument::isFrameGroupCollapsed(int groupIndex) const {
    if (groupIndex < 0 || groupIndex >= (int)m_frameGroups.size()) return false;
    return m_frameGroups[groupIndex].collapsed;
}

void DrawingDocument::renameFrameGroup(int groupIndex, const char* name) {
    if (groupIndex < 0 || groupIndex >= (int)m_frameGroups.size() || !name) return;
    m_frameGroups[groupIndex].name = name;
    DebugLog::log("[DrawingDocument] Renamed frame group %d to '%s'", groupIndex, name);
}

void DrawingDocument::setFrameGroupColor(int groupIndex, uint32_t color) {
    if (groupIndex < 0 || groupIndex >= (int)m_frameGroups.size()) return;
    m_frameGroups[groupIndex].color = color;
}

void DrawingDocument::moveFrame(int fromIndex, int toIndex) {
    int n = (int)m_frames.size();
    if (fromIndex < 0 || fromIndex >= n || toIndex < 0 || toIndex >= n) return;
    if (fromIndex == toIndex) return;
    DebugLog::log("[DrawingDocument] moveFrame(%d -> %d)", fromIndex, toIndex);

    // Erase from source, insert at destination.
    auto frame = std::move(m_frames[fromIndex]);
    m_frames.erase(m_frames.begin() + fromIndex);
    // After erase, everything above fromIndex shifted down by 1.
    // toIndex is expressed in the ORIGINAL array; adjust for the removal.
    int insertAt = (fromIndex < toIndex) ? toIndex - 1 : toIndex;
    m_frames.insert(m_frames.begin() + insertAt, std::move(frame));

    // Update group frameIndices for the combined erase+insert shift.
    for (auto& g : m_frameGroups) {
        for (auto& idx : g.frameIndices) {
            if (idx == fromIndex) {
                // The moved frame lands at insertAt.
                idx = insertAt;
            } else if (fromIndex < toIndex) {
                // Moving right: frames in (fromIndex, insertAt] shift left by 1.
                if (idx > fromIndex && idx <= insertAt) idx--;
            } else {
                // Moving left: frames in [insertAt, fromIndex) shift right by 1.
                if (idx >= insertAt && idx < fromIndex) idx++;
            }
        }
    }
}

void DrawingDocument::moveFrameGroup(int fromIndex, int toIndex) {
    int n = (int)m_frameGroups.size();
    if (fromIndex < 0 || fromIndex >= n || toIndex < 0 || toIndex >= n) return;
    if (fromIndex == toIndex) return;
    DebugLog::log("[DrawingDocument] moveFrameGroup(%d -> %d)", fromIndex, toIndex);
    FrameGroup g = std::move(m_frameGroups[fromIndex]);
    m_frameGroups.erase(m_frameGroups.begin() + fromIndex);
    m_frameGroups.insert(m_frameGroups.begin() + toIndex, std::move(g));
}

void DrawingDocument::clear() {
    DebugLog::log("[DrawingDocument] clear()");
    for (auto& frame : m_frames)
        frame->clear();
}
