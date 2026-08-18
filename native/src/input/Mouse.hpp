#pragma once
#include "../app/Types.hpp"

struct PointerEvent {
    float x = 0.0f;
    float y = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    float pressure = 1.0f;
    int button = 0;
    bool pressed = false;
    bool released = false;
};

class Mouse {
public:
    Mouse();

    void onMove(float x, float y, float dx, float dy);
    void onButton(int button, bool pressed);
    void onScroll(float x, float y);

    bool isDown(int button = 0) const { return m_buttons[button]; }
    Vec2 position() const { return m_position; }
    Vec2 delta() const { return m_delta; }
    Vec2 scrollDelta() const { return m_scrollDelta; }

    void endFrame();

private:
    Vec2 m_position;
    Vec2 m_delta;
    Vec2 m_scrollDelta;
    bool m_buttons[5] = {};
};
