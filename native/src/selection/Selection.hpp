#pragma once
#include "../app/Types.hpp"
#include <vector>
#include <cstdint>

class Selection {
public:
    Selection();

    bool active() const { return m_active; }
    void setActive(bool a) { m_active = a; }
    void clear();

    int x() const { return m_x; }
    int y() const { return m_y; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    void setRect(int x, int y, int w, int h);

    bool contains(int px, int py) const;

    const std::vector<uint8_t>& mask() const { return m_mask; }

private:
    bool m_active = false;
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;
    std::vector<uint8_t> m_mask;
};
