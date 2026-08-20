#include "Mouse.hpp"
#include "../DebugLog.h"

Mouse::Mouse() {
    DebugLog::log("[Mouse] Constructor");
}

void Mouse::onMove(float x, float y, float dx, float dy) {
    DebugLog::log("[Mouse] onMove pos=(%.1f,%.1f) delta=(%.1f,%.1f)", x, y, dx, dy);
    m_position = {x, y};
    m_delta = {dx, dy};
}

void Mouse::onButton(int button, bool pressed) {
    DebugLog::log("[Mouse] onButton button=%d pressed=%d", button, pressed);
    if (button >= 0 && button < 5)
        m_buttons[button] = pressed;
}

void Mouse::onScroll(float x, float y) {
    DebugLog::log("[Mouse] onScroll delta=(%.1f,%.1f)", x, y);
    m_scrollDelta = {x, y};
}

void Mouse::endFrame() {
    m_delta = {0, 0};
    m_scrollDelta = {0, 0};
}
