#include "CanvasManager.hpp"
#include "../DebugLog.h"

CanvasManager::CanvasManager() {
    DebugLog::log("[CanvasManager] Constructor");
}

Canvas* CanvasManager::createCanvas(int width, int height, const char* name) {
    DebugLog::log("[CanvasManager] createCanvas %dx%d name='%s'", width, height, name ? name : "unnamed");
    auto canvas = std::make_unique<Canvas>(width, height, name);
    Canvas* ptr = canvas.get();
    m_canvases.push_back(std::move(canvas));
    m_activeCanvas = ptr;
    DebugLog::log("[CanvasManager] Total canvases: %zu", m_canvases.size());
    return ptr;
}

void CanvasManager::closeCanvas(int index) {
    DebugLog::log("[CanvasManager] closeCanvas(%d), canvases=%zu", index, m_canvases.size());
    if (index < 0 || index >= (int)m_canvases.size()) return;
    if (m_canvases.size() <= 1) return;
    bool wasActive = (m_canvases[index].get() == m_activeCanvas);
    m_canvases.erase(m_canvases.begin() + index);
    if (wasActive) {
        if (index >= (int)m_canvases.size())
            index = (int)m_canvases.size() - 1;
        m_activeCanvas = m_canvases[index].get();
    }
}

Canvas* CanvasManager::getCanvas(int index) {
    if (index < 0 || index >= (int)m_canvases.size()) return nullptr;
    return m_canvases[index].get();
}

const Canvas* CanvasManager::getCanvas(int index) const {
    if (index < 0 || index >= (int)m_canvases.size()) return nullptr;
    return m_canvases[index].get();
}

void CanvasManager::setActiveCanvas(int index) {
    DebugLog::log("[CanvasManager] setActiveCanvas(%d)", index);
    Canvas* c = getCanvas(index);
    if (c) m_activeCanvas = c;
}

int CanvasManager::activeCanvasIndex() const {
    for (int i = 0; i < (int)m_canvases.size(); i++) {
        if (m_canvases[i].get() == m_activeCanvas) return i;
    }
    return 0;
}
