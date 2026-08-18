#include "Selection.hpp"

Selection::Selection() {}

void Selection::clear() {
    m_active = false;
    m_mask.clear();
    m_x = m_y = m_width = m_height = 0;
}

void Selection::setRect(int x, int y, int w, int h) {
    m_x = x;
    m_y = y;
    m_width = w;
    m_height = h;
    m_mask.resize(w * h, 255);
    m_active = true;
}

bool Selection::contains(int px, int py) const {
    if (!m_active) return true;
    return px >= m_x && px < m_x + m_width && py >= m_y && py < m_y + m_height;
}
