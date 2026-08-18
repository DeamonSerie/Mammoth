#include "Mouse.hpp"

Mouse::Mouse() {}

void Mouse::onMove(float x, float y, float dx, float dy) {
    m_position = {x, y};
    m_delta = {dx, dy};
}

void Mouse::onButton(int button, bool pressed) {
    if (button >= 0 && button < 5)
        m_buttons[button] = pressed;
}

void Mouse::onScroll(float x, float y) {
    m_scrollDelta = {x, y};
}

void Mouse::endFrame() {
    m_delta = {0, 0};
    m_scrollDelta = {0, 0};
}
