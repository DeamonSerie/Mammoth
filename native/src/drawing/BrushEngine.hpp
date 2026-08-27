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
    void stampHardRound(Layer& layer, int cx, int cy, int radius,
                        const Color& c, float alpha);
    void stampSoftRound(Layer& layer, int cx, int cy, int radius,
                        float hardness, const Color& c, float alpha);
    void stampEraser(Layer& layer, float cx, float cy, float pressure = 1.0f);

    GradualEraser* m_eraser = nullptr;
};
