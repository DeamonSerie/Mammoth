#include "DrawingDocument.hpp"
#include "../DebugLog.h"
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
        m_frames.push_back(std::move(frame));
    else
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

void DrawingDocument::clear() {
    DebugLog::log("[DrawingDocument] clear()");
    for (auto& frame : m_frames)
        frame->clear();
}
