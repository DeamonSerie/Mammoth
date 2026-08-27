#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>
#include "../app/Types.hpp"
#include "../drawing/VectorStroke.hpp"

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
    float attrOpacity() const { return m_attrOpacity; }
    uint32_t attrTint() const { return m_attrTint; }
    int groupId() const { return m_groupId; }

    void setVisible(bool v) { m_visible = v; setDirty(); }
    void setOpacity(float o) { m_opacity = o; setDirty(); }
    void setColor(uint32_t c) { m_color = c; setDirty(); }
    void setName(const char* n) { m_name = n; }
    void setAttributeLayer(bool isAttr, int sourceIdx = -1);
    // Attribute payload applied to the SOURCE layer at composite time.
    // attrOpacity: multiplier on the source layer's opacity (0..1).
    // attrTint: RGB tint, alpha channel = tint strength (0 = none).
    void setAttrOpacity(float o) { m_attrOpacity = std::clamp(o, 0.0f, 1.0f); setDirty(); }
    void setAttrTint(uint32_t t) { m_attrTint = t; setDirty(); }

    // Index bookkeeping for Frame::reorderLayer / removeLayer / insertLayer
    void remapAttributeSourceOnSwap(int a, int b);
    void remapAttributeSourceOnRemove(int removedIndex);
    void bumpAttributeSourceAtOrAbove(int index) {
        if (m_attributeSourceIndex >= index) m_attributeSourceIndex++;
    }
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

    // --- Vector-stroke layer (the vector brush commit) ----------------------
    // A layer keeps the pixels it actually displays PLUS the list of committed
    // VectorStrokes that produced them. Any edit by another tool that touches
    // pixels must (a) drop the affected strokes from the list (their pixels
    // are already flattened into the layer), then (b) on tool release call
    // revectorizeRegion() so the edited result is converted back into vectors.
    std::vector<VectorStroke>& vectorStrokes() { return m_vectorStrokes; }
    const std::vector<VectorStroke>& vectorStrokes() const { return m_vectorStrokes; }

    // Drop every vector stroke (the pixels keep showing the drawing). Used by
    // any operation that restores pixels wholesale (undo/redo, resize).
    void clearVectorStrokes();

    // Drop strokes intersecting r; returns the union rectangle of the removed
    // strokes' bounds (zero-sized if none).
    Rect dropVectorStrokesIn(const Rect& r);

    // Re-vectorize rectangle r of the layer's current pixels: any strokes
    // intersecting r are first dropped, then r is traced into new strokes
    // (Vectorizer) and appended. Pixels are NOT modified by this call.
    void revectorizeRegion(const Rect& r);

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
    float m_attrOpacity = 1.0f;
    uint32_t m_attrTint = 0;
    int m_groupId = -1;
    std::vector<uint8_t> m_pixels; // RGBA
    std::vector<VectorStroke> m_vectorStrokes;
    bool m_dirty = false;
    class Frame* m_frame = nullptr;
};
