#pragma once
#include <vector>
#include <memory>
#include "Canvas.hpp"

class CanvasManager {
public:
    CanvasManager();

    Canvas* createCanvas(int width = 800, int height = 600, const char* name = nullptr);
    void closeCanvas(int index);
    int canvasCount() const { return (int)m_canvases.size(); }
    Canvas* getCanvas(int index);
    const Canvas* getCanvas(int index) const;
    Canvas* activeCanvas() const { return m_activeCanvas; }
    void setActiveCanvas(int index);
    int activeCanvasIndex() const;

private:
    std::vector<std::unique_ptr<Canvas>> m_canvases;
    Canvas* m_activeCanvas = nullptr;
};
