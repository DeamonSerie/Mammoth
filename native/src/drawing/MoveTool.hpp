#pragma once
#include "../app/Types.hpp"
#include "../document/Layer.hpp"
#include "../canvas/Canvas.hpp"
#include <vector>

class MoveTool {
public:
    MoveTool();

    bool isMoving() const { return m_moving; }

    void begin(Layer& layer, Canvas& canvas, const Rect& canvasRect,
               float screenX, float screenY);
    void update(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                float screenX, float screenY);
    void end();

private:
    bool m_moving = false;
    Vec2 m_moveStart = {-1, -1};
    std::vector<uint8_t> m_savedData;
    int m_layerW = 0;
    int m_layerH = 0;
};
