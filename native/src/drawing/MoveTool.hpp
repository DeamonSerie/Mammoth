#pragma once
#include "../app/Types.hpp"
#include "../document/Layer.hpp"
#include "../canvas/Canvas.hpp"
#include <vector>

class MoveTool {
public:
    MoveTool();

    bool isMoving() const { return m_moving; }
    float dragOffsetX() const { return m_dragDx; }
    float dragOffsetY() const { return m_dragDy; }
    const std::vector<uint8_t>& savedData() const { return m_savedData; }
    int savedWidth() const { return m_layerW; }
    int savedHeight() const { return m_layerH; }
    int contentMinX() const { return m_contentMinX; }
    int contentMinY() const { return m_contentMinY; }
    int contentMaxX() const { return m_contentMaxX; }
    int contentMaxY() const { return m_contentMaxY; }

    void begin(Layer& layer, Canvas& canvas, const Rect& canvasRect,
               float screenX, float screenY);
    void update(Layer& layer, Canvas& canvas, const Rect& canvasRect,
                float screenX, float screenY);
    void end(Layer& layer);

private:
    bool m_moving = false;
    Vec2 m_moveStart = {-1, -1};
    Vec2 m_grabOffset = {0, 0};
    std::vector<uint8_t> m_savedData;
    int m_layerW = 0;
    int m_layerH = 0;
    float m_dragDx = 0.0f;
    float m_dragDy = 0.0f;
    int m_contentMinX = 0;
    int m_contentMinY = 0;
    int m_contentMaxX = -1;
    int m_contentMaxY = -1;
    bool m_hasContent = false;
    int m_prevIntDx = 0;
    int m_prevIntDy = 0;
};
