#pragma once
#include "../app/Types.hpp"
#include "../document/Layer.hpp"
#include "../canvas/Canvas.hpp"
#include "../rendering/Renderer.hpp"

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

    void deleteSelected(Layer& layer, Canvas& canvas, const Rect& canvasRect);
    void render(Renderer& renderer) const;

private:
    bool m_selecting = false;
    bool m_hasSelection = false;
    Rect m_selectionRect = {};
};
