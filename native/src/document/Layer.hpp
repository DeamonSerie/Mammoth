#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include "../app/Types.hpp"

class Layer {
public:
    Layer();
    Layer(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }
    bool visible() const { return m_visible; }
    float opacity() const { return m_opacity; }
    uint32_t color() const { return m_color; }
    const char* name() const { return m_name.c_str(); }
    bool isAttributeLayer() const { return m_isAttributeLayer; }
    int attributeSourceIndex() const { return m_attributeSourceIndex; }
    int groupId() const { return m_groupId; }

    void setVisible(bool v) { m_visible = v; setDirty(); }
    void setOpacity(float o) { m_opacity = o; setDirty(); }
    void setColor(uint32_t c) { m_color = c; setDirty(); }
    void setName(const char* n) { m_name = n; }
    void setAttributeLayer(bool isAttr, int sourceIdx = -1);
    void setGroupId(int id) { m_groupId = id; setDirty(); }

    void resize(int w, int h);

    Color getPixel(int x, int y) const;
    void setPixel(int x, int y, const Color& c);
    void blendPixel(int x, int y, const Color& c);

    const uint8_t* data() const { return m_pixels.data(); }
    uint8_t* data() { return m_pixels.data(); }
    size_t dataSize() const { return m_pixels.size(); }
    uint8_t* mutableData() { return m_pixels.data(); }

    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }
    void setFrame(class Frame* frame) { m_frame = frame; }

    void clear();

    static void alphaBlend(uint8_t& dstR, uint8_t& dstG, uint8_t& dstB, uint8_t& dstA,
                           uint8_t srcR, uint8_t srcG, uint8_t srcB, uint8_t srcA);

    void setDirty();

private:
    int m_width = 0;
    int m_height = 0;
    bool m_visible = true;
    float m_opacity = 1.0f;
    uint32_t m_color = 0xFFFFFFFF;
    std::string m_name;
    bool m_isAttributeLayer = false;
    int m_attributeSourceIndex = -1;
    int m_groupId = -1;
    std::vector<uint8_t> m_pixels; // RGBA
    bool m_dirty = false;
    class Frame* m_frame = nullptr;
};
