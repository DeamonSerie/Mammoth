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

    int windowWidth() const { return m_framebufferWidth; }
    int windowHeight() const { return m_framebufferHeight; }

    CanvasManager& canvasManager() { return m_canvasManager; }
    Mouse& mouse() { return m_mouse; }

private:
    void handleDrawing();
    void handleEyedropper();

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

    static constexpr size_t MAX_HISTORY = 40;
    std::deque<HistorySnapshot> m_undoStack;
    std::deque<HistorySnapshot> m_redoStack;
};
