#include "Clipboard.hpp"
#include "../DebugLog.h"
#include <cstring>

Clipboard::Clipboard() {
    DebugLog::log("[Clipboard] Constructor");
}

void Clipboard::copySelection(const uint8_t* src, int srcW,
                               int sx, int sy, int sw, int sh)
{
    DebugLog::log("[Clipboard] copySelection srcW=%d rect=(%d,%d,%d,%d)", srcW, sx, sy, sw, sh);
    m_object.width = sw;
    m_object.height = sh;
    m_object.pixels.resize(sw * sh * 4);
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            size_t srcOff = ((sy + y) * srcW + (sx + x)) * 4;
            size_t dstOff = (y * sw + x) * 4;
            memcpy(&m_object.pixels[dstOff], &src[srcOff], 4);
        }
    }
    m_object.hasData = true;
}

void Clipboard::clear() {
    DebugLog::log("[Clipboard] clear");
    m_object.hasData = false;
    m_object.pixels.clear();
    m_object.width = 0;
    m_object.height = 0;
}
