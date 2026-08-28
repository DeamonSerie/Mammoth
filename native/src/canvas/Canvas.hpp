#pragma once
#include "../document/DrawingDocument.hpp"
#include "../selection/Selection.hpp"
#include "../clipboard/Clipboard.hpp"
#include "../input/Mouse.hpp"

class Canvas {
public:
    Canvas();
    Canvas(int width, int height, const char* name = nullptr);

    DrawingDocument& document() { return m_document; }
    const DrawingDocument& document() const { return m_document; }

    Selection& selection() { return m_selection; }
    const Selection& selection() const { return m_selection; }

    float zoom() const { return m_zoom; }
    float cameraX() const { return m_cameraX; }
    float cameraY() const { return m_cameraY; }
    float rotation() const { return m_rotation; }

    void setZoom(float z) { m_zoom = z; }
    void setCamera(float x, float y) { m_cameraX = x; m_cameraY = y; }
    void setRotation(float r) { m_rotation = r; }
    void pan(float dx, float dy) { m_cameraX += dx; m_cameraY += dy; }

    Vec2 screenToCanvas(float sx, float sy, float viewportW, float viewportH) const;
    Vec2 canvasToScreen(float cx, float cy, float viewportW, float viewportH) const;

    void update();
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    const std::vector<uint8_t>& compositeBuffer() const { return m_compositeBuffer; }
    int compositeWidth() const { return m_compositeW; }
    int compositeHeight() const { return m_compositeH; }

private:
    DrawingDocument m_document;
    Selection m_selection;
    Clipboard m_clipboard;
    float m_zoom = 1.0f;
    float m_cameraX = 0.0f;
    float m_cameraY = 0.0f;
    float m_rotation = 0.0f;
    bool m_dirty = true;

    std::vector<uint8_t> m_compositeBuffer;
    int m_compositeW = 0;
    int m_compositeH = 0;
    // Zoom the composite buffer was built at. Changing zoom re-composites even
    // when the frame itself is spotless, because the buffer resolution (and the
    // brush strokes' displayed smoothness) depends on it.
    float m_compositedZoom = 1.0f;
    // Which frame the composite buffer was built from. The buffer is shared
    // across the document's frames, so switching frames must force a
    // re-composite even when the new frame is spotless.
    const void* m_compositedFrame = nullptr;
};
