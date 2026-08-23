#pragma once
#include "../app/Types.hpp"
#include "../document/Layer.hpp"
#include "../canvas/Canvas.hpp"
#include "../rendering/Renderer.hpp"
#include <vector>
#include <cstdint>

class RectSelectTool {
public:
    RectSelectTool();

    bool isSelecting() const { return m_selecting; }
    bool hasSelection() const { return m_hasSelection; }
    const Rect& selectionRect() const { return m_selectionRect; }

    void start(float screenX, float screenY);
    void update(float screenX, float screenY);
    void end();
    void clear();

    Rect getCanvasRect(const Canvas& canvas, const Rect& canvasRect) const;

    void deleteSelected(Layer& layer, Canvas& canvas, const Rect& canvasRect);
    void moveSelection(float canvasDx, float canvasDy, const Canvas& canvas, const Rect& canvasRect);
    void render(Renderer& renderer) const;

    // ---- Stamping: paste a floating copy of the selection and drag it away ----
    bool isFloating() const { return m_floating; }
    bool isDraggingFloat() const { return m_draggingFloat; }
    const std::vector<uint8_t>& stampPixels() const { return m_stampPixels; }
    int stampWidth() const { return m_stampW; }
    int stampHeight() const { return m_stampH; }

    // Copies the pixels under the active selection into a floating stamp.
    // The layer is left untouched so the source stays visible underneath.
    bool beginStamp(const Layer& layer, const Canvas& canvas, const Rect& canvasRect);

    // Floats an arbitrary pixel buffer at the given canvas position
    // (used by clipboard paste). Takes a copy so the source can be re-pasted.
    bool beginStampFromPixels(std::vector<uint8_t> pixels, int w, int h,
                              float canvasX, float canvasY);

    void startFloatDrag(float sx, float sy, const Canvas& canvas, const Rect& canvasRect);
    void updateFloatDrag(float sx, float sy, const Canvas& canvas, const Rect& canvasRect);
    void endFloatDrag() { m_draggingFloat = false; }

    bool containsScreenPoint(float sx, float sy, const Canvas& canvas, const Rect& canvasRect) const;
    Rect floatScreenRect(const Canvas& canvas, const Rect& canvasRect) const;

    // Blends the floating copy onto the layer at its current offset.
    bool commitFloat(Layer& layer);
    void cancelFloat();

private:
    bool m_selecting = false;
    bool m_hasSelection = false;
    Rect m_selectionRect = {};

    bool m_floating = false;
    bool m_draggingFloat = false;
    std::vector<uint8_t> m_stampPixels;
    int m_stampW = 0;
    int m_stampH = 0;
    float m_floatX = 0.0f;                 // canvas coords of the floating copy
    float m_floatY = 0.0f;
    Vec2 m_dragGrabCanvas = {};            // grab point (canvas coords)
    float m_dragStartFloatX = 0.0f;
    float m_dragStartFloatY = 0.0f;
};
