#pragma once
#include <vector>
#include <cstdint>
#include "../app/Types.hpp"

struct ClipboardObject {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    bool hasData = false;
};

class Clipboard {
public:
    Clipboard();

    bool hasData() const { return m_object.hasData; }
    const ClipboardObject& object() const { return m_object; }

    void copySelection(const uint8_t* src, int srcW,
                       int sx, int sy, int sw, int sh);
    void clear();

private:
    ClipboardObject m_object;
};
