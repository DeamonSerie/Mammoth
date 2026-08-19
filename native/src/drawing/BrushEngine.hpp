#pragma once
#include "Brush.hpp"
#include "GradualEraser.hpp"
#include "../document/Layer.hpp"
#include <vector>

class BrushEngine {
public:
    BrushEngine();

    void setEraser(GradualEraser* eraser) { m_eraser = eraser; }

    void applyStamp(Layer& layer, float cx, float cy,
                    const Brush& brush, float pressure = 1.0f);
    void applyStroke(Layer& layer, const std::vector<Vec2>& points,
                     const Brush& brush, float pressure = 1.0f);

private:
    void stampHardRound(Layer& layer, int centerX, int centerY,
                        int radius, const Brush& brush, float pressure);
    void stampSoftRound(Layer& layer, int centerX, int centerY,
                        int radius, const Brush& brush, float pressure);
    void stampEraser(Layer& layer, float cx, float cy);

    GradualEraser* m_eraser = nullptr;
};
