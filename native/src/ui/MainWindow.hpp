#pragma once
#include "../canvas/CanvasManager.hpp"
#include "../drawing/Brush.hpp"
#include "../drawing/GradualEraser.hpp"
#include "../drawing/BrushEngine.hpp"
#include "../drawing/MoveTool.hpp"
#include "../drawing/RectSelectTool.hpp"
#include "../rendering/Renderer.hpp"
#include "../input/Mouse.hpp"
#include <deque>
#include <string>
#include <chrono>
#include <cstdint>

static constexpr float LEFT_SIDEBAR_W = 140.0f;
static constexpr float TOP_TOOLBAR_H = 36.0f;
static constexpr float TIMELINE_H = 80.0f;
static constexpr float LAYER_PANEL_W = 220.0f;

struct TextureCache {
    sg_image image = {};
    sg_sampler sampler = {};
    sg_view view = {};
    int width = 0;
    int height = 0;
    bool valid = false;

    void destroy() {
        if (valid) {
            if (view.id) sg_destroy_view(view);
            if (image.id) sg_destroy_image(image);
            if (sampler.id) sg_destroy_sampler(sampler);
            image = {}; sampler = {}; view = {};
            valid = false; width = 0; height = 0;
        }
    }

    void update(const std::vector<uint8_t>& data, int w, int h) {
        if (w <= 0 || h <= 0 || data.empty()) { destroy(); return; }
        if (valid && width == w && height == h) {
            sg_image_data upd = {};
            upd.mip_levels[0] = sg_range{data.data(), data.size()};
            sg_update_image(image, &upd);
            return;
        }
        destroy();
        sg_sampler_desc sd = {};
        sd.min_filter = SG_FILTER_NEAREST;
        sd.mag_filter = SG_FILTER_NEAREST;
        sd.wrap_u = SG_WRAP_REPEAT;
        sd.wrap_v = SG_WRAP_REPEAT;
        sampler = sg_make_sampler(sd);

        sg_image_desc id = {};
        id.width = w; id.height = h;
        id.pixel_format = SG_PIXELFORMAT_RGBA8;
        id.usage.dynamic_update = true;
        image = sg_make_image(id);

        sg_image_data initData = {};
        initData.mip_levels[0] = sg_range{data.data(), data.size()};
        sg_update_image(image, &initData);

        sg_view_desc vd = {};
        vd.texture.image = image;
        view = sg_make_view(&vd);
        width = w; height = h; valid = true;
    }
};

struct HistorySnapshot {
    int frameIndex = 0;
    int layerIndex = 0;
    std::vector<uint8_t> layerData;
};

class MainWindow {
public:
    MainWindow();
    void init();
    void update(float dt);
    void render();
    void cleanup();

    void onMouseMove(float x, float y, float dx, float dy);
    void onMouseButton(float x, float y, int button, bool pressed);
    void onScroll(float x, float y);
    void onResize(int fbW, int fbH);
    void onKeyDown(int keyCode);
    void saveCurrentFrame();

    void undo();
    void redo();
    void pushUndo();

    // Layer navigation (Ctrl+U = up, Ctrl+B = down)
    void selectLayerAbove();
    void selectLayerBelow();
    // Wrapping scroll (Ctrl+Shift+Q = up, Ctrl+Shift+E = down)
    void scrollLayerUp();
    void scrollLayerDown();

    // Layer groups: Ctrl+G creates one, Ctrl+Shift+B grabs the active layer,
    // then scroll the panel cursor onto a group header and Ctrl+Shift+P drops
    // the grabbed layer into that group.
    void createLayerGroup();
    void grabActiveLayer();
    void placeGrabbedLayer();
    bool grabbedLayerValid() const { return m_grabbedValid; }

    // Tag the active layer with a palette color (Ctrl+Shift+1..9 shortcuts)
    void setLayerTagColor(int paletteIndex);

    // Attribute layers (Ctrl+Shift+A creates one for the active layer,
    // Ctrl+Shift+S cycles its source)
    void createAttributeLayer();
    void cycleAttributeSource();

    // Layer add/remove (Ctrl+= / Ctrl+- shortcuts, same as panel buttons)
    void createLayer();
    void deleteActiveLayer();

    // Inline rename (double-click a layer row or a group header). One edit
    // session runs at a time; commit/cancel/backspace/onChar serve whichever
    // target is active.
    bool layerRenameActive() const { return m_renamingLayerIndex >= 0; }
    bool groupRenameActive() const { return m_renamingGroupIndex >= 0; }
    void startLayerRename(int index);
    void startGroupRename(int index);
    void commitLayerRename();
    void cancelLayerRename();
    void renameBackspace();
    void onChar(uint32_t code);

    int windowWidth() const { return m_framebufferWidth; }
    int windowHeight() const { return m_framebufferHeight; }

    CanvasManager& canvasManager() { return m_canvasManager; }
    Mouse& mouse() { return m_mouse; }

private:
    void handleDrawing();
    void handleEyedropper();

public:
    // Layer panel is a flattened list of rows: group headers and layer rows.
    // isHeader=false -> index is a layer index, true -> a group index.
    // File-local helpers in MainWindow.cpp operate on this type too.
    struct PanelItem { bool isHeader; int index; };

private:
    void buildPanelItems(std::vector<PanelItem>& items);
    // Resolve the keyboard cursor: explicit position if still valid,
    // otherwise follow the active layer (or its group header when collapsed).
    int resolvePanelCursor(const std::vector<PanelItem>& items);
    // Move the cursor by delta rows (-1/+1), clamping or wrapping at the ends.
    void movePanelCursor(int delta, bool wrap);
    // Drop cursor/grab state when the active frame pointer changed.
    void syncPanelFrameState();
    // Shared by Ctrl+Shift+P and header-click drop: moves the grabbed layer
    // (if valid) into group g. Returns false when nothing valid is grabbed.
    bool dropGrabbedIntoGroup(int g);
    // One-line feedback for group operations (drawn at the panel bottom).
    void setStatus(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    bool isInCanvas(float mx, float my) const;
    Rect canvasRect() const;

    void renderLeftSidebar();
    void renderColorPanel();
    void renderTopToolbar();
    void renderLayerPanel();
    void renderTimeline();

    void syncBrushOpacity();
    void syncEraserOpacity();

    // ---- Custom brush section -------------------------------------------
    enum class CustomDrag { None, Secondary, Height, Width };

    static constexpr float CB_Y = 430.0f;

    // Custom brush editor window
    static constexpr float CW_W = 520.0f;
    static constexpr float CW_H = 430.0f;

    void renderCustomBrushSection();
    void renderCustomBrushWindow();
    void updateCustomPreview();
    void applyCustomBrushType();   // syncs m_brush type with enable flag
    bool handleCustomBrushClick(float x, float y);
    bool handleCustomWindowPress(float x, float y);
    void handleCustomBrushDrag(float x);
    void setCustomEnabled(bool enabled);

    // Sidebar layout
    Rect cbToggleRect() const;
    Rect cbEditorButtonRect() const;
    Rect cbPrimarySwitchRect(int i) const;
    Rect cbSecondaryTrackRect(int slot) const;
    Rect cbHeightTrackRect(int slot) const;
    Rect cbWidthTrackRect(int slot) const;
    float cbSectionY(int section) const; // 0=secondary,1=height,2=width grid tops

    // Editor window layout
    Rect cwRect() const;
    Rect cwCloseRect() const;
    Rect cwToggleRect() const;
    Rect cwPrimarySwitchRect(int i) const;
    Rect cwTrackRect(int section, int slot) const; // 0=secondary,1=height,2=width

    CanvasManager m_canvasManager;
    Brush m_brush;
    GradualEraser m_eraser;
    BrushEngine m_brushEngine;
    MoveTool m_moveTool;
    RectSelectTool m_rectSelectTool;
    Renderer m_renderer;
    Mouse m_mouse;

    TextureCache m_canvasTex;
    TextureCache m_checkerTex;

    int m_framebufferWidth = 1280;
    int m_framebufferHeight = 720;

    Vec2 m_lastBrushPos = {-1, -1};
    bool m_drawing = false;

    Tool m_activeTool = Tool::Brush;

    float m_hue = 15.0f;
    float m_sat = 0.8f;
    float m_val = 1.0f;
    bool m_draggingSV = false;
    bool m_draggingHue = false;
    bool m_draggingSize = false;
    bool m_draggingOpacity = false;

    float m_brushSize = 12.0f;
    float m_brushOpacity = 1.0f;

    float m_eraserSize = 12.0f;
    float m_eraserOpacity = 1.0f;

    bool m_customBrushEnabled = false;
    CustomDrag m_customDrag = CustomDrag::None;
    int m_customDragIndex = -1;
    Rect m_customDragTrack{};      // track rect captured at drag start
    bool m_customWindowOpen = false;
    TextureCache m_customPreviewTex;

    int m_currentFrame = 0;
    bool m_playing = false;
    float m_playTimer = 0.0f;
    float m_fps = 12.0f;

    // Inline rename edit state. Exactly one of the two indices is >= 0
    // while a session runs; the buffer and click timing are shared.
    int m_renamingLayerIndex = -1;
    int m_renamingGroupIndex = -1;
    std::string m_renameBuffer;
    float m_uiClock = 0.0f;                        // drives caret blink
    std::chrono::steady_clock::time_point m_lastRowClick{};
    int m_lastClickedLayer = -1;
    int m_lastClickedGroup = -1;

    // Layer-group keyboard flow state. The cursor is a row position in the
    // flattened panel list; it only applies to the frame it was built for.
    const void* m_panelFrame = nullptr;   // frame the cursor/grab belong to
    int m_panelCursor = -1;               // index into buildPanelItems output
    bool m_grabbedValid = false;
    int m_grabbedLayer = -1;

    // Transient status message shown at the bottom of the layer panel.
    std::string m_statusMsg;
    float m_statusTimer = 0.0f;

    static constexpr size_t MAX_HISTORY = 40;
    std::deque<HistorySnapshot> m_undoStack;
    std::deque<HistorySnapshot> m_redoStack;
};
