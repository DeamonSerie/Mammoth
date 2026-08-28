#include "MainWindow.hpp"
#include "../drawing/CustomBrushGeometry.hpp"
#include "../DebugLog.h"
#include "Font.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>

static constexpr float SV_X = 10.0f;
static constexpr float SV_Y = 175.0f;
static constexpr float SV_SIZE = 120.0f;
static constexpr float HUE_X = 10.0f;
static constexpr float HUE_Y = 302.0f;
static constexpr float HUE_W = 120.0f;
static constexpr float HUE_H = 14.0f;
static constexpr int SV_GRID = 12;
static constexpr int HUE_STEPS = 36;

// ---- Rect helpers -------------------------------------------------------
static Rect unionRects(const Rect& a, const Rect& b) {
    if (a.w <= 0.0f || a.h <= 0.0f) return b;
    if (b.w <= 0.0f || b.h <= 0.0f) return a;
    float x0 = std::min(a.x, b.x);
    float y0 = std::min(a.y, b.y);
    float x1 = std::max(a.x + a.w, b.x + b.w);
    float y1 = std::max(a.y + a.h, b.y + b.h);
    return Rect{x0, y0, x1 - x0, y1 - y0};
}

// Tag palette for layer colors, shared by the layer-panel swatch click and
// the Ctrl+Shift+1..9 shortcuts (index 0..8 in this order).
static const uint32_t kLayerTagPalette[] = {
    Color(255, 255, 255).pack(),   // 1: white (no tag)
    Color(235, 64, 52).pack(),     // 2: red
    Color(245, 166, 35).pack(),    // 3: orange
    Color(255, 214, 10).pack(),    // 4: yellow
    Color(46, 204, 113).pack(),    // 5: green
    Color(26, 188, 156).pack(),    // 6: teal
    Color(52, 152, 219).pack(),    // 7: blue
    Color(155, 89, 182).pack(),    // 8: purple
    Color(233, 30, 140).pack(),    // 9: pink
};
static constexpr int kLayerTagPaletteCount =
    (int)(sizeof(kLayerTagPalette) / sizeof(kLayerTagPalette[0]));

// ---- Inline layer rename helpers --------------------------------------------
// Edit box drawn over the row: x=panelX+38, w=LAYER_PANEL_W-56, text inset 4.
static constexpr float RENAME_TEXT_SCALE = 0.85f;
static constexpr size_t RENAME_MAX_CHARS =
    (size_t)((LAYER_PANEL_W - 64.0f) / (FONT_CHAR_W * RENAME_TEXT_SCALE));

static std::string trimName(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

// Attribute layers are drawn (and hit-tested) shifted right so they read as
// children of their source layer.
static constexpr float ATTR_ROW_INDENT = 16.0f;

// Layers placed inside a group are indented under that group's header.
static constexpr float GROUP_ROW_INDENT = 16.0f;

// ---- Custom brush section layout -------------------------------------------
// Two-column grids of 8 cells each (column-major: Group A = slots 0..3 in the
// left column, Group B = slots 4..7 in the right column).
static constexpr float CB_CELL_H = 15.0f;
static constexpr float CB_COL_W = 60.0f;
static constexpr float CB_TRACK_W = 40.0f;
static constexpr float CB_TRACK_H = 8.0f;

// ---- Custom brush editor window layout --------------------------------------
static constexpr float CW_TITLE_H = 26.0f;
static constexpr float CW_PAD = 16.0f;
static constexpr float CW_RIGHT_X = 200.0f;   // right column offset from window left
static constexpr float CW_PREVIEW_S = 152.0f;
static constexpr float CW_PRIM_W = 66.0f;
static constexpr float CW_PRIM_H = 20.0f;
static constexpr float CW_TRACK_W = 110.0f;
static constexpr float CW_TRACK_H = 9.0f;
static constexpr float CW_ROW_H = 17.0f;
static constexpr float CW_GRID_STRIDE = 90.0f;

MainWindow::MainWindow() {
    DebugLog::log("[MainWindow] Constructor");
}

void MainWindow::init() {
    DebugLog::log("[MainWindow] init()");
    m_renderer.init();
    m_brush.setColor(Color::hsvToRgb(m_hue, m_sat, m_val));
    m_brush.setSize(m_brushSize);
    syncBrushOpacity();
    m_eraser.setSize(m_eraserSize);
    syncEraserOpacity();
    m_brushEngine.setEraser(&m_eraser);

    if (m_canvasManager.canvasCount() == 0) {
        Canvas* c = m_canvasManager.createCanvas(512, 512, "Canvas 1");
        if (c) {
            c->setZoom(1.0f);
            c->setCamera(0.0f, 0.0f);
        }
    }

    DebugLog::log("[MainWindow] Initialized with MakoRender 2D pipeline");
}

void MainWindow::cleanup() {
    DebugLog::log("[MainWindow] cleanup()");
    m_canvasTex.destroy();
    m_checkerTex.destroy();
    m_customPreviewTex.destroy();
    m_brushPreviewTex.destroy();
    if (m_brushPreviewSampler.id) sg_destroy_sampler(m_brushPreviewSampler);
    m_renderer.shutdown();
}

Rect MainWindow::canvasRect() const {
    float fbW = (float)sapp_width();
    float fbH = (float)sapp_height();
    if (fbW <= 0.0f) fbW = (float)m_framebufferWidth;
    if (fbH <= 0.0f) fbH = (float)m_framebufferHeight;

    return Rect{
        LEFT_SIDEBAR_W, TOP_TOOLBAR_H,
        fbW - LEFT_SIDEBAR_W - LAYER_PANEL_W,
        fbH - TOP_TOOLBAR_H - TIMELINE_H
    };
}

bool MainWindow::isInCanvas(float x, float y) const {
    Rect cr = canvasRect();
    return x >= cr.x && x <= cr.x + cr.w && y >= cr.y && y <= cr.y + cr.h;
}

void MainWindow::syncBrushOpacity() {
    m_brush.setOpacity(m_brushOpacity);
}

void MainWindow::syncEraserOpacity() {
    m_eraser.setOpacity(m_eraserOpacity);
}

void MainWindow::applyCustomBrushType() {
    if (!m_customBrushEnabled) {
        m_brush.setType(BrushType::HardRound);
    } else {
        m_brush.setType(BrushType::Custom);
    }
}

void MainWindow::setCustomEnabled(bool enabled) {
    m_customBrushEnabled = enabled;
    if (m_activeTool == Tool::Brush) applyCustomBrushType();
}

// ---- Custom brush section layout helpers -----------------------------------

Rect MainWindow::cbToggleRect() const {
    return Rect{96.0f, CB_Y - 2.0f, 34.0f, 14.0f};
}

Rect MainWindow::cbPrimarySwitchRect(int i) const {
    return Rect{10.0f + i * 30.0f, CB_Y + 30.0f, 27.0f, 13.0f};
}

float MainWindow::cbSectionY(int section) const {
    float gridTop = CB_Y + 59.0f;
    return gridTop + section * (4.0f * CB_CELL_H + 14.0f);
}

Rect MainWindow::cbEditorButtonRect() const {
    return Rect{10.0f, cbSectionY(2) + 4.0f * CB_CELL_H + 5.0f,
                LEFT_SIDEBAR_W - 20.0f, 14.0f};
}

static Rect cbGridTrack(float gridTop, int slot) {
    int col = slot / 4;
    int row = slot % 4;
    return Rect{10.0f + col * CB_COL_W + 17.0f, gridTop + row * CB_CELL_H + 3.0f,
                CB_TRACK_W, CB_TRACK_H};
}

Rect MainWindow::cbSecondaryTrackRect(int slot) const {
    return cbGridTrack(cbSectionY(0), slot);
}

Rect MainWindow::cbHeightTrackRect(int slot) const {
    return cbGridTrack(cbSectionY(1), slot);
}

Rect MainWindow::cbWidthTrackRect(int slot) const {
    return cbGridTrack(cbSectionY(2), slot);
}

static bool hit(const Rect& r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

bool MainWindow::handleCustomBrushClick(float x, float y) {
    CustomBrushConfig& cfg = m_brush.customConfig();

    if (hit(cbToggleRect(), x, y)) {
        setCustomEnabled(!m_customBrushEnabled);
        return true;
    }
    if (!m_customBrushEnabled) return false;

    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) {
        if (hit(cbPrimarySwitchRect(i), x, y)) {
            // Cycle Circle -> Square -> Triangle -> Circle.
            int next = ((int)cfg.primary[i] + 1) % curveTypeCount();
            cfg.primary[i] = (CurveType)next; // dependents re-resolve lazily
            return true;
        }
    }

    if (hit(cbEditorButtonRect(), x, y)) {
        m_customWindowOpen = true;
        return true;
    }

    struct Kind { CustomDrag kind; Rect (MainWindow::*rect)(int) const; };
    static const Kind kinds[] = {
        { CustomDrag::Secondary, &MainWindow::cbSecondaryTrackRect },
        { CustomDrag::Height,    &MainWindow::cbHeightTrackRect },
        { CustomDrag::Width,     &MainWindow::cbWidthTrackRect },
    };
    for (const auto& k : kinds) {
        for (int s = 0; s < CUSTOM_SECONDARY_COUNT; s++) {
            // Automatically determined secondary curves cannot be overridden.
            if (k.kind == CustomDrag::Secondary &&
                CustomBrushConfig::isAutoSecondary(s)) continue;
            Rect tr = (this->*k.rect)(s);
            Rect wide{tr.x - 3.0f, tr.y - 3.0f, tr.w + 6.0f, tr.h + 6.0f};
            if (hit(wide, x, y)) {
                m_customDrag = k.kind;
                m_customDragIndex = s;
                m_customDragTrack = tr;
                handleCustomBrushDrag(x);
                return true;
            }
        }
    }
    return false;
}

void MainWindow::handleCustomBrushDrag(float x) {
    if (m_customDrag == CustomDrag::None || m_customDragIndex < 0) return;
    CustomBrushConfig& cfg = m_brush.customConfig();
    int s = m_customDragIndex;
    const Rect& tr = m_customDragTrack;

    switch (m_customDrag) {
        case CustomDrag::Secondary:
            cfg.setSecondaryParam(s, std::clamp((x - tr.x) / tr.w, 0.0f, 1.0f));
            break;
        case CustomDrag::Height:
            cfg.setHeight(s, CUSTOM_MIN_DIM +
                std::clamp((x - tr.x) / tr.w, 0.0f, 1.0f) *
                (CUSTOM_MAX_HEIGHT - CUSTOM_MIN_DIM));
            break;
        case CustomDrag::Width:
            cfg.setWidth(s, CUSTOM_MIN_DIM +
                std::clamp((x - tr.x) / tr.w, 0.0f, 1.0f) *
                (CUSTOM_MAX_WIDTH - CUSTOM_MIN_DIM));
            break;
        default:
            break;
    }
}

void MainWindow::renderCustomBrushSection() {
    if (m_activeTool != Tool::Brush) return;

    Color textC(210, 210, 210, 255);
    Color dimC(130, 130, 140, 255);
    Color trackBg(24, 24, 28, 255);
    Color accent(60, 110, 180, 255);
    Color autoC(168, 156, 88, 255);

    // Separator + header
    m_renderer.queueSolidRect(0, CB_Y - 12.0f, LEFT_SIDEBAR_W - 1, 1, Color(46, 46, 52, 255));
    m_renderer.drawText("Custom Brush", 10.0f, CB_Y, 0.9f,
                        m_customBrushEnabled ? Color::white() : dimC);

    // Enable toggle
    Rect tr = cbToggleRect();
    Color tbg = m_customBrushEnabled ? Color(60, 110, 180, 255) : Color(45, 45, 52, 255);
    m_renderer.queueSolidRect(tr.x, tr.y, tr.w, tr.h, tbg);
    m_renderer.drawText(m_customBrushEnabled ? "ON" : "OFF",
                        tr.x + 8.0f, tr.y + 3.0f, 0.85f, Color::white());

    const CustomBrushConfig& cfg = m_brush.customConfig();
    if (!m_customBrushEnabled) return;

    // Primary curves (three-state switches)
    m_renderer.drawText("Primary Curves", 10.0f, CB_Y + 18.0f, 0.7f, dimC);
    static const char* shortNames[] = {"Cir", "Squ", "Tri"};
    static const Color stateColors[3] = {
        Color(60, 110, 190, 255),   // Circle
        Color(190, 125, 60, 255),   // Square
        Color(70, 160, 90, 255),    // Triangle
    };
    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) {
        Rect r = cbPrimarySwitchRect(i);
        CurveType t = cfg.primary[i];
        m_renderer.queueSolidRect(r.x, r.y, r.w, r.h, stateColors[(int)t]);
        m_renderer.queueSolidRect(r.x, r.y, r.w, 1, Color(20, 20, 24, 255));
        m_renderer.drawText(shortNames[(int)t], r.x + 5.0f, r.y + 3.0f, 0.65f, Color::white());
    }

    // Grid sections: secondary sliders, height, width
    const char* headers[3] = {"Secondary Curves", "Curve Height", "Curve Width"};
    for (int sec = 0; sec < 3; sec++) {
        float gy = cbSectionY(sec);
        bool heightSec = (sec == 1);
        bool widthSec = (sec == 2);
        m_renderer.drawText(headers[sec], 10.0f, gy - 11.0f, 0.7f, dimC);

        for (int s = 0; s < CUSTOM_SECONDARY_COUNT; s++) {
            Rect r = cbGridTrack(gy, s);
            char label[8];
            snprintf(label, sizeof(label), "%c%d",
                     sec == 0 ? 'C' : (sec == 1 ? 'H' : 'W'), s + 1);
            m_renderer.drawText(label, r.x - 17.0f, r.y - 1.0f, 0.55f, textC);

            float frac;
            bool locked = false;
            if (heightSec || widthSec) {
                float lo = CUSTOM_MIN_DIM;
                float hi = heightSec ? CUSTOM_MAX_HEIGHT : CUSTOM_MAX_WIDTH;
                float v = heightSec ? cfg.height[s] : cfg.width[s];
                frac = std::clamp((v - lo) / (hi - lo), 0.0f, 1.0f);
            } else {
                locked = CustomBrushConfig::isAutoSecondary(s);
                frac = std::clamp(cfg.secondaryParam[s], 0.0f, 1.0f);
            }

            m_renderer.queueSolidRect(r.x, r.y, r.w, r.h, trackBg);
            Color fill = locked ? Color(95, 90, 60, 255) : accent;
            m_renderer.queueSolidRect(r.x, r.y, r.w * frac, r.h, fill);
            float knobX = r.x + r.w * frac;
            m_renderer.queueSolidRect(knobX - 2.0f, r.y - 2.0f, 4.0f, r.h + 4.0f,
                                      locked ? autoC : Color::white());

            if (!locked && !heightSec && !widthSec) {
                // Band ticks separating Circle / Square / Triangle ranges.
                m_renderer.queueSolidRect(r.x + r.w / 3.0f, r.y, 1, r.h, Color(50, 50, 58, 255));
                m_renderer.queueSolidRect(r.x + 2.0f * r.w / 3.0f, r.y, 1, r.h, Color(50, 50, 58, 255));
            }

            // Badge marking automatically determined values.
            if (locked) {
                m_renderer.queueSolidRect(r.x + r.w + 3.0f, r.y, 9.0f, 8.0f, autoC);
                m_renderer.drawText("A", r.x + r.w + 6.0f, r.y, 0.55f, Color::black());
            }
        }
    }

    // Open the full editor window
    Rect er = cbEditorButtonRect();
    m_renderer.queueSolidRect(er.x, er.y, er.w, er.h, Color(45, 45, 52, 255));
    m_renderer.queueSolidRect(er.x, er.y, er.w, 1, Color(60, 60, 70, 255));
    m_renderer.drawText("Open Editor...", er.x + 8.0f, er.y + 3.0f, 0.7f,
                        m_customWindowOpen ? accent : textC);
}

// ---- Custom brush editor window ----------------------------------------------

Rect MainWindow::cwRect() const {
    float fbW = (float)sapp_width();
    float fbH = (float)sapp_height();
    if (fbW <= 0.0f) fbW = (float)m_framebufferWidth;
    if (fbH <= 0.0f) fbH = (float)m_framebufferHeight;

    Rect cr = canvasRect();
    float x = cr.x + std::max(4.0f, (cr.w - CW_W) * 0.5f);
    float y = std::max(6.0f, (fbH - TIMELINE_H - CW_H) * 0.5f);
    return Rect{x, y, CW_W, CW_H};
}

static float cwContentTop(const Rect& win) {
    return win.y + CW_TITLE_H + 12.0f;
}

Rect MainWindow::cwCloseRect() const {
    Rect r = cwRect();
    return Rect{r.x + r.w - 30.0f, r.y + 4.0f, 24.0f, 18.0f};
}

Rect MainWindow::cwToggleRect() const {
    Rect r = cwRect();
    return Rect{r.x + CW_PAD, cwContentTop(r) + CW_PREVIEW_S + 14.0f + 14.0f, 76.0f, 22.0f};
}

Rect MainWindow::cwPrimarySwitchRect(int i) const {
    Rect r = cwRect();
    float ct = cwContentTop(r);
    return Rect{r.x + CW_RIGHT_X + (float)i * (CW_PRIM_W + 6.0f), ct + 13.0f,
                CW_PRIM_W, CW_PRIM_H};
}

Rect MainWindow::cwTrackRect(int section, int slot) const {
    Rect r = cwRect();
    float ct = cwContentTop(r);
    int col = slot / 4;
    int row = slot % 4;
    float headerY = ct + 39.0f + (float)section * CW_GRID_STRIDE;
    float x = r.x + CW_RIGHT_X + (float)col * 150.0f + 20.0f;
    float y = headerY + 12.0f + (float)row * CW_ROW_H;
    return Rect{x, y, CW_TRACK_W, CW_TRACK_H};
}

void MainWindow::updateCustomPreview() {
    const int P = (int)CW_PREVIEW_S;
    Layer pv(P, P);
    Color c = m_brush.color();
    c.a = 255; // preview shows the shape, not opacity
    CustomBrushGeometry::stamp(pv, P * 0.5f, P * 0.5f, P * 0.30f,
                               m_brush.customConfig().resolve(), c);
    std::vector<uint8_t> d(pv.data(), pv.data() + pv.dataSize());
    m_customPreviewTex.update(d, P, P);
}

void MainWindow::updateBrushPreview() {
    bool isEraser = (m_activeTool == Tool::Eraser);
    float size = isEraser ? m_eraser.size() : m_brush.size();
    float radius = size * 0.5f;
    float extent = radius;
    if (!isEraser && m_brush.type() == BrushType::Custom) extent = radius * CUSTOM_MAX_HEIGHT;
    // Buffer at canvas pixel resolution (1:1) for accurate preview — matches what Frame composite shows
    const int MIN_BUF = 32;
    const int MAX_BUF = 512;
    int buf = (int)std::ceil(extent * 2.0f) + 4;
    buf = std::clamp(buf, MIN_BUF, MAX_BUF);
    float cx = buf * 0.5f;
    float cy = buf * 0.5f;

    Layer pv(buf, buf);
    if (isEraser) {
        // Eraser preview: hard circle with same radius logic as GradualEraser at pressure 1.0 (rScale=1)
        int rad = (int)std::ceil(radius);
        Color c(180, 180, 180, 110);
        for (int dy = -rad - 1; dy <= rad + 1; dy++) {
            for (int dx = -rad - 1; dx <= rad + 1; dx++) {
                float dist = std::sqrt((float)(dx*dx + dy*dy));
                float a = 0.0f;
                if (dist <= rad - 0.5f) a = 1.0f;
                else if (dist < rad + 0.5f) a = (rad + 0.5f - dist);
                else continue;
                Color p = c; p.a = (uint8_t)(c.a * a * m_eraser.opacity());
                pv.blendPixel((int)std::round(cx) + dx, (int)std::round(cy) + dy, p);
            }
        }
        // Inner crosshair for eraser center
        pv.blendPixel((int)std::round(cx), (int)std::round(cy), Color(255, 255, 255, 80));
    } else {
        BrushEngine eng;
        eng.setEraser(&m_eraser);
        eng.applyStamp(pv, cx, cy, m_brush, 1.0f);
    }
    std::vector<uint8_t> d(pv.data(), pv.data() + pv.dataSize());
    m_brushPreviewTex.update(d, buf, buf);

    if (!m_brushPreviewSampler.id) {
        sg_sampler_desc sd = {};
        sd.min_filter = SG_FILTER_NEAREST;
        sd.mag_filter = SG_FILTER_NEAREST;
        sd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        sd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        m_brushPreviewSampler = sg_make_sampler(sd);
    } else {
        // Ensure sampler stays NEAREST for crisp pixel preview
        sg_sampler_desc sd = {};
        sd.min_filter = SG_FILTER_NEAREST;
        sd.mag_filter = SG_FILTER_NEAREST;
        sd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        sd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        sg_destroy_sampler(m_brushPreviewSampler);
        m_brushPreviewSampler = sg_make_sampler(sd);
    }
}

void MainWindow::renderCustomBrushWindow() {
    if (!m_customWindowOpen) return;

    Color textC(210, 210, 210, 255);
    Color dimC(130, 130, 140, 255);
    Color trackBg(24, 24, 28, 255);
    Color accent(60, 110, 180, 255);
    Color autoC(168, 156, 88, 255);

    Rect r = cwRect();

    // Frame + title bar
    m_renderer.queueSolidRect(r.x - 1.0f, r.y - 1.0f, r.w + 2.0f, r.h + 2.0f,
                              Color(15, 15, 18, 255));
    m_renderer.queueSolidRect(r.x, r.y, r.w, r.h, Color(36, 36, 40, 255));
    m_renderer.queueSolidRect(r.x, r.y, r.w, CW_TITLE_H, Color(28, 28, 32, 255));
    m_renderer.drawText("Custom Brush Editor", r.x + 12.0f, r.y + 7.0f, 0.95f,
                        Color::white());

    Rect cr = cwCloseRect();
    m_renderer.queueSolidRect(cr.x, cr.y, cr.w, cr.h, Color(70, 44, 48, 255));
    m_renderer.drawText("X", cr.x + 8.0f, cr.y + 4.0f, 0.8f, textC);

    float ct = cwContentTop(r);

    // ---- Left column: live preview + enable toggle -------------------------
    float lx = r.x + CW_PAD;
    m_renderer.drawText("Preview", lx, ct, 0.75f, dimC);
    m_renderer.queueSolidRect(lx - 1.0f, ct + 13.0f, CW_PREVIEW_S + 2.0f,
                              CW_PREVIEW_S + 2.0f, Color(20, 20, 24, 255));

    updateCustomPreview();
    if (m_customPreviewTex.valid) {
        m_renderer.queueQuad(lx, ct + 14.0f, CW_PREVIEW_S, CW_PREVIEW_S,
                             m_customPreviewTex.image, m_customPreviewTex.view,
                             m_customPreviewTex.sampler);
    }

    bool enabled = m_customBrushEnabled;
    Rect tg = cwToggleRect();
    m_renderer.queueSolidRect(tg.x, tg.y, tg.w, tg.h,
                              enabled ? accent : Color(45, 45, 52, 255));
    m_renderer.drawText(enabled ? "Enabled" : "Disabled",
                        tg.x + 10.0f, tg.y + 6.0f, 0.85f, Color::white());

    m_renderer.drawText("Slots marked A are", lx, tg.y + 34.0f, 0.65f, dimC);
    m_renderer.drawText("auto-determined by", lx, tg.y + 44.0f, 0.65f, dimC);
    m_renderer.drawText("their primary piece.", lx, tg.y + 54.0f, 0.65f, dimC);

    // ---- Right column: controls --------------------------------------------
    static const char* shortNames[] = {"Circle", "Square", "Tri"};
    static const Color stateColors[3] = {
        Color(60, 110, 190, 255),   // Circle
        Color(190, 125, 60, 255),   // Square
        Color(70, 160, 90, 255),    // Triangle
    };
    const CustomBrushConfig& cfg = m_brush.customConfig();

    m_renderer.drawText("Primary Curves", r.x + CW_RIGHT_X, ct, 0.75f, dimC);
    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) {
        Rect pr = cwPrimarySwitchRect(i);
        CurveType t = cfg.primary[i];
        m_renderer.queueSolidRect(pr.x, pr.y, pr.w, pr.h, stateColors[(int)t]);
        m_renderer.queueSolidRect(pr.x, pr.y, pr.w, 1, Color(20, 20, 24, 255));
        char label[16];
        snprintf(label, sizeof(label), "%d %s", i + 1, shortNames[(int)t]);
        m_renderer.drawText(label, pr.x + 7.0f, pr.y + 6.0f, 0.7f, Color::white());
    }

    static const char* headers[3] = {"Secondary Curves", "Curve Height", "Curve Width"};
    for (int sec = 0; sec < 3; sec++) {
        float headerY = ct + 39.0f + (float)sec * CW_GRID_STRIDE;
        m_renderer.drawText(headers[sec], r.x + CW_RIGHT_X, headerY, 0.75f, dimC);

        for (int s = 0; s < CUSTOM_SECONDARY_COUNT; s++) {
            Rect trk = cwTrackRect(sec, s);
            char label[8];
            snprintf(label, sizeof(label), "%c%d",
                     sec == 0 ? 'C' : (sec == 1 ? 'H' : 'W'), s + 1);
            m_renderer.drawText(label, trk.x - 18.0f, trk.y, 0.55f, textC);

            float frac;
            bool locked = false;
            if (sec == 0) {
                locked = CustomBrushConfig::isAutoSecondary(s);
                frac = std::clamp(cfg.secondaryParam[s], 0.0f, 1.0f);
            } else {
                float lo = CUSTOM_MIN_DIM;
                float hi = (sec == 1) ? CUSTOM_MAX_HEIGHT : CUSTOM_MAX_WIDTH;
                float v = (sec == 1) ? cfg.height[s] : cfg.width[s];
                frac = std::clamp((v - lo) / (hi - lo), 0.0f, 1.0f);
            }

            m_renderer.queueSolidRect(trk.x, trk.y, trk.w, trk.h, trackBg);
            Color fill = locked ? Color(95, 90, 60, 255) : accent;
            m_renderer.queueSolidRect(trk.x, trk.y, trk.w * frac, trk.h, fill);
            float knobX = trk.x + trk.w * frac;
            m_renderer.queueSolidRect(knobX - 2.0f, trk.y - 2.0f, 4.0f, trk.h + 4.0f,
                                      locked ? autoC : Color::white());

            if (!locked && sec == 0) {
                // Band ticks separating Circle / Square / Triangle ranges.
                m_renderer.queueSolidRect(trk.x + trk.w / 3.0f, trk.y, 1, trk.h,
                                          Color(50, 50, 58, 255));
                m_renderer.queueSolidRect(trk.x + 2.0f * trk.w / 3.0f, trk.y, 1, trk.h,
                                          Color(50, 50, 58, 255));
            }
            if (locked) {
                m_renderer.queueSolidRect(trk.x + trk.w + 4.0f, trk.y, 9.0f, 9.0f, autoC);
                m_renderer.drawText("A", trk.x + trk.w + 7.0f, trk.y, 0.55f, Color::black());
            }
        }
    }

    // Flush chrome below the preview texture quad.
    float viewW = (float)sapp_width();
    float viewH = (float)sapp_height();
    if (viewW <= 0.0f) viewW = (float)m_framebufferWidth;
    if (viewH <= 0.0f) viewH = (float)m_framebufferHeight;
    m_renderer.flushSolid(viewW, viewH);
    m_renderer.flushText(viewW, viewH);
    m_renderer.flushQuads(viewW, viewH);
}

bool MainWindow::handleCustomWindowPress(float x, float y) {
    Rect r = cwRect();
    if (!hit(r, x, y)) return false;

    CustomBrushConfig& cfg = m_brush.customConfig();

    if (hit(cwCloseRect(), x, y)) {
        m_customWindowOpen = false;
        return true;
    }
    if (hit(cwToggleRect(), x, y)) {
        setCustomEnabled(!m_customBrushEnabled);
        return true;
    }
    for (int i = 0; i < CUSTOM_PRIMARY_COUNT; i++) {
        Rect pr = cwPrimarySwitchRect(i);
        if (hit(pr, x, y)) {
            int next = ((int)cfg.primary[i] + 1) % curveTypeCount();
            cfg.primary[i] = (CurveType)next;
            return true;
        }
    }
    struct Kind { CustomDrag kind; int section; };
    static const Kind kinds[] = {
        { CustomDrag::Secondary, 0 },
        { CustomDrag::Height,    1 },
        { CustomDrag::Width,     2 },
    };
    for (const auto& k : kinds) {
        for (int s = 0; s < CUSTOM_SECONDARY_COUNT; s++) {
            if (k.kind == CustomDrag::Secondary &&
                CustomBrushConfig::isAutoSecondary(s)) continue;
            Rect trk = cwTrackRect(k.section, s);
            Rect wide{trk.x - 3.0f, trk.y - 3.0f, trk.w + 6.0f, trk.h + 6.0f};
            if (hit(wide, x, y)) {
                m_customDrag = k.kind;
                m_customDragIndex = s;
                m_customDragTrack = trk;
                handleCustomBrushDrag(x);
                return true;
            }
        }
    }
    // Consume clicks anywhere inside the window so painting never starts
    // behind it.
    return true;
}

void MainWindow::pushUndo() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    Layer* l = f->activeLayer();
    if (!l) return;

    HistorySnapshot snap;
    snap.frameIndex = c->document().activeFrameIndex();
    snap.layerIndex = 0;
    for (int i = 0; i < f->layerCount(); i++) {
        if (f->getLayer(i) == l) { snap.layerIndex = i; break; }
    }
    snap.layerData.assign(l->data(), l->data() + l->dataSize());

    m_undoStack.push_back(snap);
    if (m_undoStack.size() > MAX_HISTORY) {
        m_undoStack.pop_front();
    }
    m_redoStack.clear();
}

void MainWindow::undo() {
    if (m_undoStack.empty()) return;
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;

    HistorySnapshot snap = m_undoStack.back();
    m_undoStack.pop_back();

    c->document().setActiveFrame(snap.frameIndex);
    m_currentFrame = snap.frameIndex;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    f->setActiveLayer(snap.layerIndex);
    Layer* l = f->activeLayer();
    if (!l) return;

    // Save current for redo
    HistorySnapshot redoSnap;
    redoSnap.frameIndex = snap.frameIndex;
    redoSnap.layerIndex = snap.layerIndex;
    redoSnap.layerData.assign(l->data(), l->data() + l->dataSize());
    m_redoStack.push_back(redoSnap);
    if (m_redoStack.size() > MAX_HISTORY) m_redoStack.pop_front();

    // Restore snapshot
    if (snap.layerData.size() == (size_t)l->dataSize()) {
        std::memcpy(l->data(), snap.layerData.data(), l->dataSize());
        l->setDirty();
        f->setDirty();
    }
}

void MainWindow::redo() {
    if (m_redoStack.empty()) return;
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;

    HistorySnapshot snap = m_redoStack.back();
    m_redoStack.pop_back();

    c->document().setActiveFrame(snap.frameIndex);
    m_currentFrame = snap.frameIndex;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    f->setActiveLayer(snap.layerIndex);
    Layer* l = f->activeLayer();
    if (!l) return;

    // Save current for undo
    HistorySnapshot undoSnap;
    undoSnap.frameIndex = snap.frameIndex;
    undoSnap.layerIndex = snap.layerIndex;
    undoSnap.layerData.assign(l->data(), l->data() + l->dataSize());
    m_undoStack.push_back(undoSnap);
    if (m_undoStack.size() > MAX_HISTORY) m_undoStack.pop_front();

    // Restore snapshot
    if (snap.layerData.size() == (size_t)l->dataSize()) {
        std::memcpy(l->data(), snap.layerData.data(), l->dataSize());
        l->setDirty();
        f->setDirty();
    }
}

// ---- Layer groups & panel cursor ---------------------------------------------

void MainWindow::setStatus(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    m_statusMsg = buf;
    m_statusTimer = 4.0f;
    DebugLog::log("[MainWindow] status: %s", buf);
}

// Where a panel row's item lives for reordering purposes: a group header
// or ungrouped layer owns a paint-stack slot; a member lives inside its
// group's internal ordering.
namespace {
struct PanelRowCtx {
    bool header = false;
    int groupIdx = -1;
    int memberPos = -1;
    int stackPos = -1;
};

PanelRowCtx panelRowContext(Frame* f, const std::vector<MainWindow::PanelItem>& items, int row) {
    PanelRowCtx c;
    if (!f || row < 0 || row >= (int)items.size()) return c;
    const MainWindow::PanelItem& it = items[row];
    if (it.isHeader) {
        c.header = true;
        c.groupIdx = it.index;
        c.stackPos = f->stackPosForGroup(it.index);
        return c;
    }
    c.groupIdx = f->findGroupForLayer(it.index);
    if (c.groupIdx >= 0) {
        const auto& mem = f->getGroup(c.groupIdx).layerIndices;
        for (int k = 0; k < (int)mem.size(); k++) {
            if (mem[k] == it.index) { c.memberPos = k; break; }
        }
    } else {
        c.stackPos = f->stackPosForLayer(it.index);
    }
    return c;
}

// Move a row one slot toward the panel top (delta +1: the panel shows the
// stack reversed, so up = higher Z) or toward the bottom (-1). Members
// reorder inside their group only; headers and ungrouped layers move
// through the outer stack. Returns false when blocked at an edge.
bool movePanelRow(Frame* f, const std::vector<MainWindow::PanelItem>& items, int row, int delta) {
    if (!f) return false;
    PanelRowCtx c = panelRowContext(f, items, row);
    if (c.header || c.memberPos < 0)
        return f->moveStackItem(c.stackPos, delta);
    const auto& mem = f->getGroup(c.groupIdx).layerIndices;
    int target = c.memberPos + delta;
    if (target < 0 || target >= (int)mem.size()) return false;
    f->reorderGroupMember(c.groupIdx, c.memberPos, target);
    return true;
}

// ---- Timeline geometry (render and click handling must stay in sync) --------
// Button row: Play [10..64], +Frame [72..140], -Frame [148..216],
// Dup [224..274], +Group [282..346]; the speed widget is right-aligned.
static constexpr float TL_STRIP_Y = 36.0f;   // strip top, relative to tlY
static constexpr float TL_TILE = 36.0f;      // frame tile edge
static constexpr float TL_GAP = 6.0f;

// Group-chip metrics. Right-edge control cluster (offsets from chipW):
// count -77 | swatch [-47..-35] | +/- toggle [-31..-17] | delete-x [-15..-2].
// TL_CHIP_BASE_W sizes everything except the name text so short default names
// still clear the count. Render and click handling both go through
// tlGroupChipWidth so the two cannot drift apart.
static constexpr float TL_CHIP_BASE_W = 74.0f;
static constexpr float TL_CHIP_MAX_W = 188.0f;
static inline float tlGroupChipWidth(size_t nameLen) {
    return std::min(TL_CHIP_MAX_W,
                    TL_CHIP_BASE_W + (float)nameLen * (float)FONT_CHAR_W * 0.85f);
}

struct TimelineItem {
    bool isHeader;   // true -> frame-group index, false -> frame index
    int index;
};

// Flatten the timeline into display chips: one section per frame group
// (creation order), then the ungrouped frames. Everything stays in frame
// index order inside each section - playback order never depends on this
// layout, and nothing here can reorder frames.
void buildTimelineItems(DrawingDocument& doc, std::vector<TimelineItem>& out) {
    out.clear();
    for (int g = 0; g < doc.frameGroupCount(); g++) {
        out.push_back({true, g});
        if (doc.isFrameGroupCollapsed(g)) continue;
        for (int f = 0; f < doc.frameCount(); f++)
            if (doc.findGroupForFrame(f) == g) out.push_back({false, f});
    }
    for (int f = 0; f < doc.frameCount(); f++)
        if (doc.findGroupForFrame(f) < 0) out.push_back({false, f});
}
}   // namespace

// The panel cursor / grabbed-layer state belongs to one frame; switching
// frames (timeline click or playback) invalidates it instead of letting
// stale layer indices leak across documents.
void MainWindow::syncPanelFrameState() {
    Canvas* c = m_canvasManager.activeCanvas();
    Frame* f = c ? c->document().activeFrame() : nullptr;
    if ((const void*)f != m_panelFrame) {
        m_panelFrame = f;
        m_panelCursor = -1;
        m_grabbedValid = false;
        m_grabbedLayer = -1;
        m_layerDnd.cancel();
        m_tlDnd.cancel();
        // An open edit session belongs to the old frame; stale indices
        // could rename a same-numbered row in the new one.
        if (anyRenameActive())
            cancelLayerRename();
    }
}

// Flatten the panel into display rows mirroring the true paint stack,
// topmost item first (display order == paint order reversed). A group is
// one slot in the outer stack: it renders as a header row followed by its
// members in group-internal order unless collapsed. Ungrouped layers sit
// wherever the stack puts them - groups are no longer sectioned at the top.
void MainWindow::buildPanelItems(std::vector<PanelItem>& items) {
    items.clear();
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    for (int s = f->stackCount() - 1; s >= 0; s--) {
        const Frame::StackNode& nd = f->stackNode(s);
        if (!nd.isGroup) {
            items.push_back({false, nd.index});
            continue;
        }
        items.push_back({true, nd.index});
        if (f->isGroupCollapsed(nd.index)) continue;
        const Frame::Group& grp = f->getGroup(nd.index);
        for (int m = (int)grp.layerIndices.size() - 1; m >= 0; m--)
            items.push_back({false, grp.layerIndices[m]});
    }
}

// Same walk as buildPanelItems with drag-and-drop extras: each row carries
// its group context and on-screen geometry so LayerDragDrop can plan drops.
// Geometry must mirror the press handler / renderLayerPanel: rows start at
// y=68, 24px tall, 28px stride.
void MainWindow::buildDndRows(std::vector<DndRow>& rows) {
    rows.clear();
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    float y = 68.0f;
    for (int s = f->stackCount() - 1; s >= 0; s--) {
        const Frame::StackNode& nd = f->stackNode(s);
        if (!nd.isGroup) {
            DndRow r;
            r.isHeader = false; r.index = nd.index; r.groupIdx = -1;
            r.memberPos = -1; r.stackPos = s; r.runCount = 0;
            r.y = y; r.h = 24.0f;
            rows.push_back(r);
            y += 28.0f;
            continue;
        }
        const Frame::Group& grp = f->getGroup(nd.index);
        int n = (int)grp.layerIndices.size();
        {
            DndRow r;
            r.isHeader = true; r.index = nd.index; r.groupIdx = nd.index;
            r.memberPos = -1; r.stackPos = s; r.runCount = n;
            r.y = y; r.h = 24.0f;
            rows.push_back(r);
            y += 28.0f;
        }
        if (f->isGroupCollapsed(nd.index)) continue;
        for (int m = n - 1; m >= 0; m--) {
            DndRow r;
            r.isHeader = false; r.index = grp.layerIndices[m];
            r.groupIdx = nd.index; r.memberPos = m; r.stackPos = s;
            r.runCount = n;
            r.y = y; r.h = 24.0f;
            rows.push_back(r);
            y += 28.0f;
        }
    }
}

int MainWindow::resolvePanelCursor(const std::vector<MainWindow::PanelItem>& items) {    if (m_panelCursor >= 0 && m_panelCursor < (int)items.size())
        return m_panelCursor;
    m_panelCursor = -1;
    if (items.empty()) return -1;
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return -1;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer()) return 0;
    int act = -1;
    for (int i = 0; i < f->layerCount(); i++)
        if (f->getLayer(i) == f->activeLayer()) { act = i; break; }
    if (act >= 0) {
        for (int i = 0; i < (int)items.size(); i++) {
            if (!items[i].isHeader && items[i].index == act) {
                m_panelCursor = i;
                return i;
            }
        }
        // Active layer sits inside a collapsed group: land on its header.
        int g = f->findGroupForLayer(act);
        for (int i = 0; i < (int)items.size(); i++) {
            if (items[i].isHeader && items[i].index == g) {
                m_panelCursor = i;
                return i;
            }
        }
    }
    return 0;
}

// delta -1 moves toward the top row of the panel (= higher Z-order),
// +1 toward the bottom; wrap scrolls around the ends.
void MainWindow::movePanelCursor(int delta, bool wrap) {
    syncPanelFrameState();
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    std::vector<PanelItem> items;
    buildPanelItems(items);
    int n = (int)items.size();
    if (n == 0) return;
    int cur = resolvePanelCursor(items);
    if (cur < 0) cur = 0;
    int next = wrap ? (((cur + delta) % n) + n) % n
                    : std::clamp(cur + delta, 0, n - 1);
    m_panelCursor = next;
    if (!items[next].isHeader)
        f->setActiveLayer(items[next].index);
}

void MainWindow::createLayerGroup() {
    syncPanelFrameState();
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "Group %d", f->groupCount());
    uint32_t color = kLayerTagPalette[f->groupCount() % kLayerTagPaletteCount];
    f->addGroup(buf, color);

    // Grouping the active layer right away: "select layer, Ctrl+G" is the
    // natural flow and matches how every other art app behaves.
    int g = f->groupCount() - 1;
    int act = -1;
    for (int i = 0; i < f->layerCount(); i++)
        if (f->getLayer(i) == f->activeLayer()) { act = i; break; }
    if (act >= 0)
        f->addLayerToGroup(act, g);

    // Park the keyboard cursor on the new header so a following
    // Ctrl+Shift+P targets this group without extra scrolling.
    std::vector<PanelItem> items;
    buildPanelItems(items);
    for (int i = 0; i < (int)items.size(); i++) {
        if (items[i].isHeader && items[i].index == g) {
            m_panelCursor = i;
            break;
        }
    }
    if (act >= 0)
        setStatus("Created %s with '%s'", buf, f->getLayer(act)->name());
    else
        setStatus("Created %s - grab layers with Ctrl+Shift+B", buf);
}

void MainWindow::grabActiveLayer() {
    syncPanelFrameState();
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer()) return;
    int idx = -1;
    for (int i = 0; i < f->layerCount(); i++)
        if (f->getLayer(i) == f->activeLayer()) { idx = i; break; }
    if (idx < 0) return;
    m_grabbedValid = true;
    m_grabbedLayer = idx;
    DebugLog::log("[MainWindow] Grabbed layer %d ('%s') - scroll to a group "
                  "and press Ctrl+Shift+P", idx, f->getLayer(idx)->name());
    setStatus("Grabbed '%s' - scroll to a group, Ctrl+Shift+P",
              f->getLayer(idx)->name());
}

bool MainWindow::dropGrabbedIntoGroup(int g) {
    syncPanelFrameState();
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return false;
    Frame* f = c->document().activeFrame();
    if (!f) return false;
    if (!m_grabbedValid || g < 0 || g >= f->groupCount() ||
        m_grabbedLayer < 0 || m_grabbedLayer >= f->layerCount())
        return false;
    const std::string& gname = f->getGroup(g).name;
    const char* lname = f->getLayer(m_grabbedLayer)->name();
    f->setGroupCollapsed(g, false);   // reveal the result immediately
    f->addLayerToGroup(m_grabbedLayer, g);
    m_grabbedValid = false;
    m_grabbedLayer = -1;
    DebugLog::log("[MainWindow] Placed '%s' into group '%s'", lname, gname.c_str());
    setStatus("Placed '%s' into %s", lname, gname.c_str());
    return true;
}

// Apply the plan a drag finished with. The module only plans; all document
// mutation (plus undo + feedback) lives here.
void MainWindow::applyLayerDrop(const DndPlan& plan) {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;

    switch (plan.action) {
    case DndAction::None:
        return;

    case DndAction::JoinGroup: {
        int li = m_layerDnd.layerIndex();
        if (li < 0 || li >= f->layerCount()) return;
        pushUndo();
        f->setGroupCollapsed(plan.groupIndex, false);   // reveal the result
        f->addLayerToGroup(li, plan.groupIndex);
        // The join appends at the run's end; land at the planned position.
        const auto& run = f->getGroup(plan.groupIndex).layerIndices;
        int end = (int)run.size() - 1;
        if (plan.memberInsert >= 0 && plan.memberInsert < end)
            f->reorderGroupMember(plan.groupIndex, end, plan.memberInsert);
        f->setActiveLayer(li);
        DebugLog::log("[MainWindow] Drag: placed '%s' into group '%s'",
                      f->getLayer(li)->name(), f->getGroup(plan.groupIndex).name.c_str());
        setStatus("Moved '%s' into %s", f->getLayer(li)->name(),
                  f->getGroup(plan.groupIndex).name.c_str());
        break;
    }

    case DndAction::MoveOuter: {
        pushUndo();
        // A spliced-out member re-enters exactly at its group's old slot.
        if (plan.ungroupFirst)
            f->removeLayerFromGroup(m_layerDnd.layerIndex());
        int cur = plan.stackFrom;
        int remaining = plan.stackSteps;
        while (remaining != 0 && f->moveStackItem(cur, remaining > 0 ? 1 : -1)) {
            cur += remaining > 0 ? 1 : -1;
            remaining += remaining > 0 ? -1 : 1;
        }
        if (m_layerDnd.item() == LayerDragDrop::Item::Group) {
            int g = m_layerDnd.groupIndex();
            if (g >= 0 && g < f->groupCount())
                setStatus("Moved %s", f->getGroup(g).name.c_str());
        } else {
            int li = m_layerDnd.layerIndex();
            if (li >= 0 && li < f->layerCount()) {
                f->setActiveLayer(li);
                setStatus("Moved '%s'", f->getLayer(li)->name());
            }
        }
        break;
    }

    case DndAction::MemberReorder:
        pushUndo();
        f->reorderGroupMember(plan.groupIndex, plan.memberFrom, plan.memberTo);
        {
            const auto& run = f->getGroup(plan.groupIndex).layerIndices;
            if (plan.memberTo >= 0 && plan.memberTo < (int)run.size())
                setStatus("Moved '%s'", f->getLayer(run[plan.memberTo])->name());
        }
        break;
    }

    m_panelCursor = -1;
}

// Build timeline-item snapshots for the drag-and-drop planner.  Geometry
// mirrors renderTimeline / the press handler: items start at
// LEFT_SIDEBAR_W+10, strip at tlY+TL_STRIP_Y, chipW from tlGroupChipWidth,
// tiles TL_TILE wide with TL_GAP spacing.
void MainWindow::buildTlDndItems(std::vector<TlDndItem>& items) {
    items.clear();
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    DrawingDocument& doc = c->document();
    float fbH = (float)sapp_height();
    if (fbH <= 0.0f) fbH = (float)m_framebufferHeight;
    float tlY = fbH - TIMELINE_H;
    float fx = LEFT_SIDEBAR_W + 10.0f;

    std::vector<TimelineItem> titems;
    buildTimelineItems(doc, titems);
    for (const TimelineItem& ti : titems) {
        TlDndItem d;
        if (ti.isHeader) {
            const DrawingDocument::FrameGroup& g = doc.getFrameGroup(ti.index);
            bool renaming = (m_renamingFrameGroupIndex == ti.index);
            float chipW = renaming ? 90.0f : tlGroupChipWidth(g.name.size());
            d.isHeader = true;
            d.index = ti.index;
            d.groupIdx = ti.index;
            d.memberPos = -1;
            d.x = fx;
            d.w = chipW;
            fx += chipW + TL_GAP;
        } else {
            int grp = doc.findGroupForFrame(ti.index);
            int mpos = -1;
            if (grp >= 0) {
                const auto& idxs = doc.getFrameGroup(grp).frameIndices;
                for (int k = 0; k < (int)idxs.size(); k++)
                    if (idxs[k] == ti.index) { mpos = k; break; }
            }
            d.isHeader = false;
            d.index = ti.index;
            d.groupIdx = grp;
            d.memberPos = mpos;
            d.x = fx;
            d.w = TL_TILE;
            fx += TL_TILE + TL_GAP;
        }
        items.push_back(d);
    }
}

void MainWindow::applyTimelineDrop(const TlDndPlan& plan) {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    DrawingDocument& doc = c->document();

    switch (plan.action) {
    case TlDndAction::None:
        return;

    case TlDndAction::FrameReorder: {
        int from = plan.frameIndex;
        int to = plan.targetIndex;
        if (from < 0 || from >= doc.frameCount()) return;
        if (to < 0 || to >= doc.frameCount()) return;
        pushUndo();
        // Compute destination in the post-erase array.
        int targetFinalPos = (to > from) ? to - 1 : to;
        int dest = plan.insertAfter ? targetFinalPos + 1 : targetFinalPos;
        doc.moveFrame(from, dest);
        setStatus("Reordered frame %d", dest + 1);
        break;
    }

    case TlDndAction::FrameJoinGroup: {
        int fi = plan.frameIndex;
        int gi = plan.groupIndex;
        if (fi < 0 || fi >= doc.frameCount()) return;
        if (gi < 0 || gi >= doc.frameGroupCount()) return;
        pushUndo();
        doc.addFrameToGroup(fi, gi);
        doc.setFrameGroupCollapsed(gi, false);
        setStatus("Joined %s", doc.getFrameGroup(gi).name.c_str());
        break;
    }

    case TlDndAction::FrameLeaveGroup: {
        int fi = plan.frameIndex;
        if (fi < 0 || fi >= doc.frameCount()) return;
        pushUndo();
        doc.removeFrameFromGroup(fi);
        setStatus("Ungrouped frame %d", fi + 1);
        break;
    }

    case TlDndAction::GroupReorder: {
        int from = plan.groupIndex;
        int to = plan.targetIndex;
        if (from < 0 || from >= doc.frameGroupCount()) return;
        if (to < 0 || to >= doc.frameGroupCount()) return;
        pushUndo();
        int targetFinalPos = (to > from) ? to - 1 : to;
        int dest = plan.insertAfter ? targetFinalPos + 1 : targetFinalPos;
        doc.moveFrameGroup(from, dest);
        setStatus("Reordered %s", doc.getFrameGroup(dest).name.c_str());
        break;
    }
    }
}

void MainWindow::placeGrabbedLayer() {
    syncPanelFrameState();
    DebugLog::log("[MainWindow] Place attempt: grabbedValid=%d grabbed=%d",
                  (int)m_grabbedValid, m_grabbedLayer);
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    if (!m_grabbedValid) {
        DebugLog::log("[MainWindow] Ctrl+Shift+P with nothing grabbed "
                      "(use Ctrl+Shift+B on a layer first)");
        setStatus("Nothing grabbed - select a layer, Ctrl+Shift+B");
        return;
    }
    std::vector<PanelItem> items;
    buildPanelItems(items);
    int cur = resolvePanelCursor(items);
    int g = -1;
    if (cur >= 0 && cur < (int)items.size() && items[cur].isHeader)
        g = items[cur].index;
    else if (f->groupCount() == 1)
        g = 0;   // only one group: unambiguous target
    if (g < 0) {
        DebugLog::log("[MainWindow] Place failed: cursor row %d is not a group "
                      "header (%d groups exist)", cur, f->groupCount());
        setStatus("Click a group header to drop the grabbed layer");
        return;
    }
    dropGrabbedIntoGroup(g);
}

void MainWindow::selectLayerAbove() {
    movePanelCursor(-1, false);
}

void MainWindow::selectLayerBelow() {
    movePanelCursor(+1, false);
}

void MainWindow::scrollLayerUp() {
    movePanelCursor(-1, true);
}

void MainWindow::scrollLayerDown() {
    movePanelCursor(+1, true);
}

void MainWindow::setLayerTagColor(int paletteIndex) {
    if (paletteIndex < 0 || paletteIndex >= kLayerTagPaletteCount) return;
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer()) return;
    f->activeLayer()->setColor(kLayerTagPalette[paletteIndex]);
}

void MainWindow::createAttributeLayer() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer()) return;
    Layer* holder = f->activeLayer();
    int holderIdx = 0;
    for (int i = 0; i < f->layerCount(); i++) {
        if (f->getLayer(i) == holder) { holderIdx = i; break; }
    }

    pushUndo();
    // Insertion shifts every layer index above it - drop stale panel state.
    m_panelCursor = -1;
    m_grabbedValid = false;
    m_grabbedLayer = -1;
    // Insert directly below the holder in the panel so the indented row sits
    // under it (lower index = lower in the layer list).
    std::string prefix = "Attr: ";
    std::string baseName = holder->name();
    while (baseName.compare(0, prefix.size(), prefix) == 0)
        baseName.erase(0, prefix.size());   // nested attrs share one label
    Layer* al = f->insertLayer(holderIdx, (prefix + baseName).c_str());
    if (!al) return;
    al->setAttributeLayer(true, holderIdx + 1);
}

void MainWindow::cycleAttributeSource() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer() || f->layerCount() < 2) return;
    Layer* l = f->activeLayer();
    if (!l->isAttributeLayer()) return;
    int selfIdx = 0;
    for (int i = 0; i < f->layerCount(); i++) {
        if (f->getLayer(i) == l) { selfIdx = i; break; }
    }
    int next = (l->attributeSourceIndex() + 1) % f->layerCount();
    if (next == selfIdx) next = (next + 1) % f->layerCount();
    l->setAttributeLayer(true, next);
}

void MainWindow::createLayer() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    pushUndo();
    f->addLayer();
}

void MainWindow::deleteActiveLayer() {
    syncPanelFrameState();
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer()) return;
    std::vector<PanelItem> items;
    buildPanelItems(items);
    int cur = resolvePanelCursor(items);

    // Cursor on a group header: remove that whole group instead of a layer.
    // Members splice back into the paint stack ungrouped, so nothing is
    // destroyed - empty groups simply disappear.
    if (cur >= 0 && cur < (int)items.size() && items[cur].isHeader) {
        int g = items[cur].index;
        std::string name = f->getGroup(g).name;
        int members = (int)f->getGroup(g).layerIndices.size();
        pushUndo();
        // Group removal renumbers groups above it - drop stale state.
        m_panelCursor = -1;
        m_grabbedValid = false;
        m_grabbedLayer = -1;
        f->removeGroup(g);
        DebugLog::log("[MainWindow] Removed layer group '%s' (%d layers kept)",
                      name.c_str(), members);
        setStatus("Removed %s - %d layers kept ungrouped", name.c_str(), members);
        return;
    }

    // Never delete down to zero layers: the frame always keeps one canvas.
    if (f->layerCount() <= 1) {
        setStatus("Can't delete the only layer");
        return;
    }

    int target = 0;
    bool haveTarget = false;
    if (cur >= 0 && cur < (int)items.size() && !items[cur].isHeader) {
        target = items[cur].index;   // the highlighted layer row
        haveTarget = true;
    }
    if (!haveTarget) {
        for (int i = 0; i < f->layerCount(); i++) {
            if (f->getLayer(i) == f->activeLayer()) { target = i; break; }
        }
    }
    pushUndo();
    // Removal (and its attribute cascade) shifts indices - drop stale state.
    m_panelCursor = -1;
    m_grabbedValid = false;
    m_grabbedLayer = -1;
    f->removeLayer(target);
}

// ---- Inline layer rename ----------------------------------------------------

// Nudge the active frame's playback-speed multiplier; 1.0 plays one beat at
// the global fps, 2.0 holds twice as long, 0.5 half as long.
void MainWindow::nudgeFrameDuration(float delta) {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f) return;
    float d = std::clamp(f->duration() + delta, 0.25f, 4.0f);
    f->setDuration(d);
    setStatus("Frame %d speed: %.2gx", c->document().activeFrameIndex() + 1, d);
}

void MainWindow::removeActiveFrameGroup() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    DrawingDocument& doc = c->document();
    int act = doc.activeFrameIndex();
    int g = doc.findGroupForFrame(act);
    if (g < 0) {
        setStatus("Current frame is not in a frame group");
        return;
    }
    std::string name = doc.getFrameGroup(g).name;
    int count = (int)doc.getFrameGroup(g).frameIndices.size();
    doc.removeFrameGroup(g);
    DebugLog::log("[MainWindow] Removed frame group '%s' (%d frames kept)", name.c_str(), count);
    setStatus("Removed %s - %d frames kept", name.c_str(), count);
}

void MainWindow::startLayerRename(int index) {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || index < 0 || index >= f->layerCount()) return;
    Layer* l = f->getLayer(index);
    if (!l) return;
    m_renamingLayerIndex = index;
    m_renamingGroupIndex = -1;
    m_renameBuffer = l->name();
}

void MainWindow::startGroupRename(int index) {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || index < 0 || index >= f->groupCount()) return;
    m_renamingGroupIndex = index;
    m_renamingLayerIndex = -1;
    m_renameBuffer = f->getGroup(index).name;
}

void MainWindow::startFrameRename(int index) {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().getFrame(index);
    if (!f) return;
    m_renamingFrameIndex = index;
    m_renamingFrameGroupIndex = -1;
    m_renameBuffer = f->name();   // may be empty: tile shows the number
}

void MainWindow::startFrameGroupRename(int index) {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    DrawingDocument& doc = c->document();
    if (index < 0 || index >= doc.frameGroupCount()) return;
    m_renamingFrameGroupIndex = index;
    m_renamingFrameIndex = -1;
    m_renameBuffer = doc.getFrameGroup(index).name;
}

// Commits whichever rename session is active; an empty buffer keeps the
// old name for layers/groups and clears a custom frame name.
void MainWindow::commitLayerRename() {
    std::string trimmed = trimName(m_renameBuffer);
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) {
        cancelLayerRename();
        return;
    }
    Frame* f = c->document().activeFrame();
    DrawingDocument& doc = c->document();
    if (!trimmed.empty()) {
        if (m_renamingFrameGroupIndex >= 0)
            doc.renameFrameGroup(m_renamingFrameGroupIndex, trimmed.c_str());
        else if (m_renamingFrameIndex >= 0) {
            Frame* fr = doc.getFrame(m_renamingFrameIndex);
            if (fr) fr->setName(trimmed.c_str());
        } else if (f && m_renamingGroupIndex >= 0)
            f->renameGroup(m_renamingGroupIndex, trimmed.c_str());
        else if (f && m_renamingLayerIndex >= 0)
            f->renameLayer(m_renamingLayerIndex, trimmed.c_str());
    }
    m_renamingLayerIndex = -1;
    m_renamingGroupIndex = -1;
    m_renamingFrameIndex = -1;
    m_renamingFrameGroupIndex = -1;
    m_renameBuffer.clear();
}

void MainWindow::cancelLayerRename() {
    m_renamingLayerIndex = -1;
    m_renamingGroupIndex = -1;
    m_renamingFrameIndex = -1;
    m_renamingFrameGroupIndex = -1;
    m_renameBuffer.clear();
}

void MainWindow::renameBackspace() {
    if (!anyRenameActive()) return;
    // Input is restricted to ASCII, so popping one byte == one character
    if (!m_renameBuffer.empty()) m_renameBuffer.pop_back();
}

void MainWindow::onChar(uint32_t code) {
    if (!anyRenameActive()) return;
    // The bitmap font atlas only covers printable ASCII
    if (code < 32 || code >= 127) return;
    if (m_renameBuffer.size() >= RENAME_MAX_CHARS) return;
    m_renameBuffer.push_back((char)code);
}

void MainWindow::onMouseMove(float x, float y, float dx, float dy, float pressure, int mods) {
    m_mouse.onMove(x, y, dx, dy);
    m_lastPressure = pressure;

    // Update modifier key state from event
    const int MOD_CTRL  = 2;   // Mod::Ctrl = 1 << 1 = 2
    const int MOD_SHIFT = 1;   // Mod::Shift = 1 << 0 = 1
    m_ctrlDown  = (mods & MOD_CTRL)  != 0;
    m_shiftDown = (mods & MOD_SHIFT) != 0;

    // Ctrl+drag canvas panning.
    if (m_panning) {
        DebugLog::log("[MainWindow] PANNING MOVE x=%.1f y=%.1f dx=%.1f dy=%.1f", x, y, dx, dy);
        Canvas* cv = m_canvasManager.activeCanvas();
        if (cv) {
            cv->setCamera(m_panCamStartX + (x - m_panStartX),
                          m_panCamStartY + (y - m_panStartY));
        }
        return;
    }

    // Ctrl+Shift+drag canvas rotation: horizontal mouse movement rotates
    // around the canvas point that was under the cursor at press time.
    if (m_rotating) {
        Canvas* cv = m_canvasManager.activeCanvas();
        if (cv) {
            Rect cr = canvasRect();

            float newAngle = m_rotateStartAngle + (x - m_rotateStartX) * 0.005f;

            float cw = (float)cv->document().width() * cv->zoom();
            float ch = (float)cv->document().height() * cv->zoom();
            // Offset from rotation center to the pivot point (camera-independent).
            float dx = m_rotatePivotCX * cv->zoom() - cw * 0.5f;
            float dy = m_rotatePivotCY * cv->zoom() - ch * 0.5f;
            float c = std::cos(newAngle);
            float s = std::sin(newAngle);
            // Solve for camera so that pivot maps to current mouse position.
            float newCamX = x - (dx * c - dy * s) - cr.x - cr.w * 0.5f;
            float newCamY = y - (dx * s + dy * c) - cr.y - cr.h * 0.5f;

            cv->setRotation(newAngle);
            cv->setCamera(newCamX, newCamY);
        }
        return;
    }

    if (m_drawing) {
        if (m_activeTool == Tool::Brush || m_activeTool == Tool::Eraser) {
            handleDrawing(m_lastPressure);
        }
    }

    if (m_draggingSV) {
        m_sat = std::clamp((x - SV_X) / SV_SIZE, 0.0f, 1.0f);
        m_val = std::clamp(1.0f - (y - SV_Y) / SV_SIZE, 0.0f, 1.0f);
        m_brush.setColor(Color::hsvToRgb(m_hue, m_sat, m_val));
    } else if (m_draggingHue) {
        m_hue = std::clamp((x - HUE_X) / HUE_W, 0.0f, 1.0f) * 360.0f;
        m_brush.setColor(Color::hsvToRgb(m_hue, m_sat, m_val));
    } else if (m_draggingSize) {
        float sw = LEFT_SIDEBAR_W - 20.0f;
        float sx = 10.0f;
        if (m_activeTool == Tool::Eraser) {
            m_eraserSize = std::clamp(((x - sx) / sw) * 64.0f + 1.0f, 1.0f, 64.0f);
            m_eraser.setSize(m_eraserSize);
        } else {
            m_brushSize = std::clamp(((x - sx) / sw) * 64.0f + 1.0f, 1.0f, 64.0f);
            m_brush.setSize(m_brushSize);
        }
    } else if (m_draggingOpacity) {
        float sw = LEFT_SIDEBAR_W - 20.0f;
        float sx = 10.0f;
        m_brushOpacity = std::clamp((x - sx) / sw, 0.05f, 1.0f);
        syncBrushOpacity();
    } else if (m_customDrag != CustomDrag::None) {
        handleCustomBrushDrag(x);
    } else if (m_layerDnd.armed() || m_layerDnd.active()) {
        std::vector<DndRow> rows;
        buildDndRows(rows);
        m_layerDnd.update(y, rows);
    } else if (m_tlDnd.armed() || m_tlDnd.active()) {
        std::vector<TlDndItem> items;
        buildTlDndItems(items);
        m_tlDnd.update(x, items);
    } else if (m_moveTool.isMoving()) {
        Rect cr = canvasRect();
        Canvas* c = m_canvasManager.activeCanvas();
        if (c) {
            Frame* f = c->document().activeFrame();
            if (f && f->activeLayer()) {
                float prevDx = m_moveTool.dragOffsetX();
                float prevDy = m_moveTool.dragOffsetY();
                m_moveTool.update(*f->activeLayer(), *c, cr, x, y);
                if (m_rectSelectTool.hasSelection()) {
                    float frameDx = m_moveTool.dragOffsetX() - prevDx;
                    float frameDy = m_moveTool.dragOffsetY() - prevDy;
                    if (frameDx != 0.0f || frameDy != 0.0f) {
                        m_rectSelectTool.moveSelection(frameDx, frameDy, *c, cr);
                    }
                }
                f->setDirty();
            }
        }
    } else if (m_rectSelectTool.isSelecting()) {
        m_rectSelectTool.update(x, y);
    }
}

void MainWindow::onMouseButton(float x, float y, int button, bool pressed, float pressure, int mods) {
    m_mouse.onMove(x, y, 0, 0);
    m_mouse.onButton(button, pressed);
    m_lastPressure = pressure;

    // Update modifier key state from event
    const int MOD_CTRL  = 2;   // Mod::Ctrl = 1 << 1 = 2
    const int MOD_SHIFT = 1;   // Mod::Shift = 1 << 0 = 1
    m_ctrlDown  = (mods & MOD_CTRL)  != 0;
    m_shiftDown = (mods & MOD_SHIFT) != 0;
    DebugLog::log("[MainWindow] onMouseButton x=%.1f y=%.1f button=%d pressed=%d mods=%d ctrl=%d shift=%d", 
                  x, y, button, pressed, mods, m_ctrlDown, m_shiftDown);

    if (!pressed) {
        // Mouse-release: apply layer drag-and-drop or deferred collapse toggle.
        if (m_layerDnd.active()) {
            applyLayerDrop(m_layerDnd.plan());
            m_layerDnd.cancel();
        } else if (m_layerDnd.armed()) {
            if (m_layerDnd.item() == LayerDragDrop::Item::Group) {
                Canvas* rc = m_canvasManager.activeCanvas();
                Frame* rf = rc ? rc->document().activeFrame() : nullptr;
                int g = m_layerDnd.groupIndex();
                if (rf && g >= 0 && g < rf->groupCount())
                    rf->setGroupCollapsed(g, !rf->isGroupCollapsed(g));
            }
            m_layerDnd.cancel();
        }

        // Timeline drag-and-drop: apply drop or cancel armed press.
        if (m_tlDnd.active()) {
            applyTimelineDrop(m_tlDnd.plan());
            m_tlDnd.cancel();
        } else if (m_tlDnd.armed()) {
            m_tlDnd.cancel();
        }

        // End canvas pan on release.
        if (m_panning) {
            m_panning = false;
        }

        // End canvas rotation on release.
        if (m_rotating) {
            m_rotating = false;
        }

        if (m_moveTool.isMoving()) {
            Canvas* c = m_canvasManager.activeCanvas();
            if (c) {
                Frame* f = c->document().activeFrame();
                if (f && f->activeLayer()) {
                    Layer* moveLayer = f->activeLayer();
                    m_moveTool.end(*moveLayer);
                    moveLayer->setDirty();
                    f->setDirty();
                }
            }
        }
        if (m_rectSelectTool.isSelecting()) {
            m_rectSelectTool.end();
        }
        if (m_drawing && m_activeTool == Tool::Eraser) {
            m_eraser.endStroke();
        }
        m_drawing = false;
        m_lastBrushPos = {-1, -1};
        m_draggingSV = false;
        m_draggingHue = false;
        m_draggingSize = false;
        m_draggingOpacity = false;
        m_customDrag = CustomDrag::None;
        m_customDragIndex = -1;
        return;
    }

    float fbW = (float)sapp_width();
    float fbH = (float)sapp_height();
    if (fbW <= 0.0f) fbW = (float)m_framebufferWidth;
    if (fbH <= 0.0f) fbH = (float)m_framebufferHeight;

    // Any press while renaming commits the edit; the click then continues
    // through normal handling below.
    if (anyRenameActive()) {
        commitLayerRename();
    }

    float sw = LEFT_SIDEBAR_W - 20.0f;
    float sx = 10.0f;
    float sliderH = 14.0f;

    // 1. Click in Left Sidebar
    if (x < LEFT_SIDEBAR_W) {
        float ty = 8.0f;
        const Tool tools[] = {Tool::Brush, Tool::Eraser, Tool::Eyedropper, Tool::Move, Tool::RectSelect};
        for (int i = 0; i < 5; i++) {
            if (x >= sx && x <= sx + sw && y >= ty && y <= ty + 26) {
                m_activeTool = tools[i];
                if (m_activeTool == Tool::Eraser) {
                    m_brush.setType(BrushType::Eraser);
                    m_eraser.setSize(m_eraserSize);
                    syncEraserOpacity();
                    m_renderer.hotSwapEraser(true);
                } else {
                    applyCustomBrushType();
                    m_renderer.hotSwapEraser(false);
                }
                return;
            }
            ty += 30.0f;
        }

        // SV square
        if (x >= SV_X && x < SV_X + SV_SIZE && y >= SV_Y && y < SV_Y + SV_SIZE) {
            m_sat = std::clamp((x - SV_X) / SV_SIZE, 0.0f, 1.0f);
            m_val = std::clamp(1.0f - (y - SV_Y) / SV_SIZE, 0.0f, 1.0f);
            m_brush.setColor(Color::hsvToRgb(m_hue, m_sat, m_val));
            m_draggingSV = true;
            return;
        }

        // Hue bar
        if (x >= HUE_X && x < HUE_X + HUE_W && y >= HUE_Y && y < HUE_Y + HUE_H) {
            m_hue = std::clamp((x - HUE_X) / HUE_W, 0.0f, 1.0f) * 360.0f;
            m_brush.setColor(Color::hsvToRgb(m_hue, m_sat, m_val));
            m_draggingHue = true;
            return;
        }

        // Size slider
        float szY = HUE_Y + HUE_H + 18.0f;
        if (x >= sx && x <= sx + sw && y >= szY && y <= szY + sliderH) {
            if (m_activeTool == Tool::Eraser) {
                m_eraserSize = std::clamp(((x - sx) / sw) * 64.0f + 1.0f, 1.0f, 64.0f);
                m_eraser.setSize(m_eraserSize);
            } else {
                m_brushSize = std::clamp(((x - sx) / sw) * 64.0f + 1.0f, 1.0f, 64.0f);
                m_brush.setSize(m_brushSize);
            }
            m_draggingSize = true;
            return;
        }

        // Opacity slider
        float opY = szY + sliderH + 18.0f;
        if (x >= sx && x <= sx + sw && y >= opY && y <= opY + sliderH) {
            if (m_activeTool == Tool::Eraser) {
                m_eraserOpacity = std::clamp((x - sx) / sw, 0.05f, 1.0f);
                syncEraserOpacity();
            } else {
                m_brushOpacity = std::clamp((x - sx) / sw, 0.05f, 1.0f);
                syncBrushOpacity();
            }
            m_draggingOpacity = true;
            return;
        }

        // Custom Brush section (Brush tool only)
        if (m_activeTool == Tool::Brush && handleCustomBrushClick(x, y)) {
            return;
        }
        return;
    }

    // 2. Click in Top Toolbar
    if (y < TOP_TOOLBAR_H && x >= LEFT_SIDEBAR_W && x < fbW - LAYER_PANEL_W) {
        float tbX = LEFT_SIDEBAR_W + 10.0f;
        float btnY = 6.0f;
        float btnH = 24.0f;

        // Undo
        if (x >= tbX && x <= tbX + 44 && y >= btnY && y <= btnY + btnH) {
            undo();
            return;
        }
        tbX += 50.0f;

        // Redo
        if (x >= tbX && x <= tbX + 44 && y >= btnY && y <= btnY + btnH) {
            redo();
            return;
        }
        tbX += 54.0f;

        // Save
        if (x >= tbX && x <= tbX + 50 && y >= btnY && y <= btnY + btnH) {
            saveCurrentFrame();
            return;
        }
        tbX += 65.0f;

        // Zoom -
        Canvas* c = m_canvasManager.activeCanvas();
        if (x >= tbX && x <= tbX + 26 && y >= btnY && y <= btnY + btnH) {
            if (c) c->setZoom(std::max(0.2f, c->zoom() * 0.8f));
            return;
        }
        tbX += 32.0f;

        // Zoom +
        if (x >= tbX && x <= tbX + 26 && y >= btnY && y <= btnY + btnH) {
            if (c) c->setZoom(std::min(128.0f, c->zoom() * 1.25f));
            return;
        }
        tbX += 32.0f;

        // 1:1
        if (x >= tbX && x <= tbX + 36 && y >= btnY && y <= btnY + btnH) {
            if (c) { c->setZoom(1.0f); c->setCamera(0.0f, 0.0f); }
            return;
        }
        return;
    }

    // 3. Click in Right Layer Panel
    if (x >= fbW - LAYER_PANEL_W && y < fbH - TIMELINE_H) {
        Canvas* c = m_canvasManager.activeCanvas();
        if (!c) return;
        Frame* frame = c->document().activeFrame();
        if (!frame) return;

        float panelX = fbW - LAYER_PANEL_W;
        float btnY = 38.0f;
        float btnH = 22.0f;

        // [+ Layer] button
        if (x >= panelX + 10.0f && x <= panelX + 74.0f && y >= btnY && y <= btnY + btnH) {
            createLayer();
            return;
        }
        // [- Layer] button
        if (x >= panelX + 80.0f && x <= panelX + 144.0f && y >= btnY && y <= btnY + btnH) {
            deleteActiveLayer();
            return;
        }
        // [+ Group] button
        if (x >= panelX + 150.0f && x <= panelX + 214.0f && y >= btnY && y <= btnY + btnH) {
            createLayerGroup();
            return;
        }

        syncPanelFrameState();
        std::vector<PanelItem> items;
        buildPanelItems(items);

        // Panel row clicks: with a grab pending, clicking a header DROPS the
        // grabbed layer into that group; otherwise headers toggle collapse.
    float itemY = 68.0f;
        for (int row = 0; row < (int)items.size(); row++) {
            const MainWindow::PanelItem& it = items[row];
            if (it.isHeader) {
                if (x >= panelX + 10.0f && x <= panelX + LAYER_PANEL_W - 10.0f &&
                    y >= itemY && y <= itemY + 24.0f) {
                    // Double-click starts an inline rename instead of
                    // toggling collapse again (mirrors layer rows).
                    auto now = std::chrono::steady_clock::now();
                    double msSince =
                        std::chrono::duration<double, std::milli>(now - m_lastRowClick).count();
                    bool isDoubleClick = (m_lastClickedGroup == it.index) && (msSince < 400.0);
                    m_lastRowClick = now;
                    m_lastClickedGroup = isDoubleClick ? -1 : it.index;
                    m_panelCursor = row;
                    if (isDoubleClick) {
                        startGroupRename(it.index);
                    } else if (dropGrabbedIntoGroup(it.index)) {
                        m_lastClickedGroup = isDoubleClick ? -1 : it.index;
                    } else {
                        // Defer collapse toggle to release so a drag doesn't
                        // flash it mid-motion.
                        PanelRowCtx hc = panelRowContext(frame, items, row);
                        m_layerDnd.press(LayerDragDrop::Item::Group, it.index,
                                         it.index, -1, hc.stackPos, y);
                    }
                    return;
                }
                itemY += 28.0f;
                continue;
            }

            int i = it.index;
            Layer* l = frame->getLayer(i);
            if (!l) { itemY += 28.0f; continue; }
            // Indent mirrors the renderer: group membership plus attribute depth.
            float visInd =
                (frame->findGroupForLayer(i) >= 0 ? GROUP_ROW_INDENT : 0.0f) +
                (l->isAttributeLayer()
                     ? ATTR_ROW_INDENT * (float)frame->attributeChainDepth(i)
                     : 0.0f);
            // Visibility toggle icon
            if (x >= panelX + 10.0f + visInd && x <= panelX + 32.0f + visInd &&
                y >= itemY && y <= itemY + 24.0f) {
                l->setVisible(!l->visible());
                return;
            }
            // Tag-color swatch: cycle through the preset palette
            if (y >= itemY && y <= itemY + 24.0f &&
                x >= panelX + LAYER_PANEL_W - 72.0f && x <= panelX + LAYER_PANEL_W - 56.0f) {
                uint32_t cur = l->color();
                int next = 0;
                for (int k = 0; k < kLayerTagPaletteCount; k++) {
                    if (kLayerTagPalette[k] == cur) { next = (k + 1) % kLayerTagPaletteCount; break; }
                }
                l->setColor(kLayerTagPalette[next]);
                return;
            }
            // Reorder arrows (checked before select - they sit inside the row).
            // Up = toward the panel top = higher Z. Members reorder within
            // their group; headers/ungrouped layers move in the outer stack,
            // so nesting never changes from the arrows alone.
            if (y >= itemY && y <= itemY + 24.0f && x >= panelX + LAYER_PANEL_W - 50.0f) {
                // Up arrow: move toward the top of the list = higher Z-order
                if (x >= panelX + LAYER_PANEL_W - 46.0f && x <= panelX + LAYER_PANEL_W - 32.0f) {
                    movePanelRow(frame, items, row, +1);
                    m_panelCursor = -1;
                    m_grabbedValid = false;
                    m_grabbedLayer = -1;
                    return;
                }
                // Down arrow: lower Z-order
                if (x >= panelX + LAYER_PANEL_W - 30.0f && x <= panelX + LAYER_PANEL_W - 16.0f) {
                    movePanelRow(frame, items, row, -1);
                    m_panelCursor = -1;
                    m_grabbedValid = false;
                    m_grabbedLayer = -1;
                    return;
                }
            }
            // Select layer (double-click on the same row starts an inline rename)
            if (x >= panelX + 36.0f + visInd && x <= panelX + LAYER_PANEL_W - 10.0f &&
                y >= itemY && y <= itemY + 24.0f) {
                auto now = std::chrono::steady_clock::now();
                double msSince = std::chrono::duration<double, std::milli>(now - m_lastRowClick).count();
                bool isDoubleClick = (m_lastClickedLayer == i) && (msSince < 400.0);
                m_lastRowClick = now;
                m_lastClickedLayer = isDoubleClick ? -1 : i;
                frame->setActiveLayer(i);
                m_panelCursor = row;
                if (isDoubleClick) {
                    startLayerRename(i);
                    return;
                }
                {
                    // Arm a layer drag; DndRows are built on each mouse-move.
                    PanelRowCtx lc = panelRowContext(frame, items, row);
                    int sp = lc.memberPos >= 0
                                 ? frame->stackPosForGroup(lc.groupIdx)
                                 : lc.stackPos;
                    m_layerDnd.press(LayerDragDrop::Item::Layer, i, lc.groupIdx,
                                     lc.memberPos, sp, y);
                }
                return;
            }
            itemY += 28.0f;
        }
        return;
    }

    // 4. Click in Bottom Timeline
    if (y >= fbH - TIMELINE_H) {
        Canvas* c = m_canvasManager.activeCanvas();
        if (!c) return;
        DrawingDocument& doc = c->document();

        float tlY = fbH - TIMELINE_H;
        float btnY = tlY + 8.0f;
        float btnH = 22.0f;

        // Play/Pause button
        if (x >= LEFT_SIDEBAR_W + 10.0f && x <= LEFT_SIDEBAR_W + 64.0f && y >= btnY && y <= btnY + btnH) {
            m_playing = !m_playing;
            if (m_playing) { m_playTimer = 0.0f; }
            return;
        }

        // [+ Frame] button: appends a fresh canvas to draw on
        if (x >= LEFT_SIDEBAR_W + 72.0f && x <= LEFT_SIDEBAR_W + 140.0f && y >= btnY && y <= btnY + btnH) {
            pushUndo();
            doc.addFrame();
            m_currentFrame = doc.frameCount() - 1;
            doc.setActiveFrame(m_currentFrame);
            return;
        }

        // [- Frame] button
        if (x >= LEFT_SIDEBAR_W + 148.0f && x <= LEFT_SIDEBAR_W + 216.0f && y >= btnY && y <= btnY + btnH) {
            pushUndo();
            doc.removeFrame(m_currentFrame);
            m_currentFrame = doc.activeFrameIndex();
            return;
        }

        // [Dup] button
        if (x >= LEFT_SIDEBAR_W + 224.0f && x <= LEFT_SIDEBAR_W + 274.0f && y >= btnY && y <= btnY + btnH) {
            pushUndo();
            doc.duplicateFrame(m_currentFrame);
            m_currentFrame = doc.activeFrameIndex();
            return;
        }

        // [+ Group] button: new empty frame group
        if (x >= LEFT_SIDEBAR_W + 282.0f && x <= LEFT_SIDEBAR_W + 346.0f && y >= btnY && y <= btnY + btnH) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Group %d", doc.frameGroupCount());
            doc.addFrameGroup(buf, kLayerTagPalette[doc.frameGroupCount() % kLayerTagPaletteCount]);
            setStatus("Created %s - use + on its chip to add the current frame", buf);
            return;
        }

        // Playback-speed widget for the active frame
        if (y >= btnY && y <= btnY + btnH && fbW > 200.0f) {
            if (x >= fbW - 124.0f && x <= fbW - 106.0f) { nudgeFrameDuration(-0.25f); return; }
            if (x >= fbW - 58.0f && x <= fbW - 40.0f) { nudgeFrameDuration(+0.25f); return; }
        }

        // Strip: chips and tiles (geometry mirrors renderTimeline)
        std::vector<TimelineItem> titems;
        buildTimelineItems(doc, titems);
        float fx = LEFT_SIDEBAR_W + 10.0f;
        float fy = tlY + TL_STRIP_Y;
        for (const TimelineItem& ti : titems) {
            if (ti.isHeader) {
                const DrawingDocument::FrameGroup& g = doc.getFrameGroup(ti.index);
                bool renaming = (m_renamingFrameGroupIndex == ti.index);
                float chipW = 90.0f;
                if (!renaming)
                    chipW = tlGroupChipWidth(g.name.size());
                if (x >= fx && x <= fx + chipW && y >= fy && y <= fy + 24.0f) {
                    if (renaming) return;   // clicks commit via the handler above
                    // Arm group drag for DnD; controls below return before
                    // the DnD state matters on release.
                    m_tlDnd.press(TimelineDragDrop::Item::Group, ti.index,
                                  ti.index, -1, x);
                    // Swatch: cycle the group color through the palette
                    if (x >= fx + chipW - 47.0f && x <= fx + chipW - 34.0f) {
                        uint32_t cur = g.color;
                        int next = 0;
                        for (int k = 0; k < kLayerTagPaletteCount; k++) {
                            if (kLayerTagPalette[k] == cur) { next = (k + 1) % kLayerTagPaletteCount; break; }
                        }
                        doc.setFrameGroupColor(ti.index, kLayerTagPalette[next]);
                        return;
                    }
                    // Membership toggle for the ACTIVE frame
                    if (x >= fx + chipW - 31.0f && x <= fx + chipW - 17.0f) {
                        int act = doc.activeFrameIndex();
                        if (doc.findGroupForFrame(act) == ti.index)
                            doc.removeFrameFromGroup(act);
                        else
                            doc.addFrameToGroup(act, ti.index);
                        return;
                    }
                    // Delete-group button: dissolves the group, frames survive
                    if (x >= fx + chipW - 15.0f && x <= fx + chipW - 2.0f) {
                        std::string name = g.name;
                        int count = (int)g.frameIndices.size();
                        doc.removeFrameGroup(ti.index);
                        DebugLog::log("[MainWindow] Removed frame group '%s' (%d frames kept)",
                                      name.c_str(), count);
                        setStatus("Removed %s - %d frames kept", name.c_str(), count);
                        return;
                    }
                    auto now = std::chrono::steady_clock::now();
                    double msSince =
                        std::chrono::duration<double, std::milli>(now - m_lastRowClick).count();
                    bool isDoubleClick =
                        (m_lastClickedTimelineGroup == ti.index) && (msSince < 400.0);
                    m_lastRowClick = now;
                    m_lastClickedTimelineGroup = isDoubleClick ? -1 : ti.index;
                    if (isDoubleClick) {
                        startFrameGroupRename(ti.index);
                    } else {
                        doc.setFrameGroupCollapsed(ti.index, !doc.isFrameGroupCollapsed(ti.index));
                    }
                    return;
                }
                fx += chipW + TL_GAP;
                continue;
            }

            int i = ti.index;
            if (x >= fx && x <= fx + TL_TILE && y >= fy && y <= fy + TL_TILE) {
                auto now = std::chrono::steady_clock::now();
                double msSince =
                    std::chrono::duration<double, std::milli>(now - m_lastRowClick).count();
                bool isDoubleClick = (m_lastClickedFrameTile == i) && (msSince < 400.0);
                m_lastRowClick = now;
                m_lastClickedFrameTile = isDoubleClick ? -1 : i;
                m_currentFrame = i;
                doc.setActiveFrame(i);   // fresh layer/group context per frame
                if (isDoubleClick) { startFrameRename(i); return; }
                // Arm frame drag for DnD.
                int grp = doc.findGroupForFrame(i);
                int mpos = -1;
                if (grp >= 0) {
                    const auto& idxs = doc.getFrameGroup(grp).frameIndices;
                    for (int k = 0; k < (int)idxs.size(); k++)
                        if (idxs[k] == i) { mpos = k; break; }
                }
                m_tlDnd.press(TimelineDragDrop::Item::Frame, i, grp, mpos, x);
                return;
            }
            fx += TL_TILE + TL_GAP;
        }
        return;
    }

    // 4b. Custom brush editor window (floats over the canvas)
    if (m_customWindowOpen && handleCustomWindowPress(x, y)) {
        return;
    }

    // 5. Click in Canvas area
    if (isInCanvas(x, y)) {
        // Ctrl+Shift+click = canvas rotation: intercept before pan/tools.
        if (m_ctrlDown && m_shiftDown) {
            Canvas* cv = m_canvasManager.activeCanvas();
            if (cv) {
                Rect cr = canvasRect();

                m_rotating = true;
                m_rotateStartX = x;
                m_rotateStartY = y;
                m_rotateStartAngle = cv->rotation();
                // Canvas point under cursor at press time.
                Vec2 p = cv->screenToCanvas(x - cr.x, y - cr.y, cr.w, cr.h);
                m_rotatePivotCX = p.x;
                m_rotatePivotCY = p.y;
            }
            return;
        }
        // Ctrl+click = canvas pan: intercept before tools.
        if (m_ctrlDown) {
            DebugLog::log("[MainWindow] PANNING START x=%.1f y=%.1f", x, y);
            Canvas* cv = m_canvasManager.activeCanvas();
            if (cv) {
                m_panning = true;
                m_panStartX = x;
                m_panStartY = y;
                m_panCamStartX = cv->cameraX();
                m_panCamStartY = cv->cameraY();
            }
            return;
        }
        switch (m_activeTool) {
            case Tool::Brush:
                pushUndo();
                m_drawing = true;
                m_lastBrushPos = {-1, -1};
                handleDrawing(m_lastPressure);
                break;
            case Tool::Eraser: {
                Canvas* cv = m_canvasManager.activeCanvas();
                if (cv) {
                    Frame* f = cv->document().activeFrame();
                    if (f && f->activeLayer()) {
                        pushUndo();
                        m_eraser.beginStroke(*f->activeLayer());
                    }
                }
                m_drawing = true;
                m_lastBrushPos = {-1, -1};
                handleDrawing(m_lastPressure);
                break;
            }
            case Tool::Eyedropper:
                handleEyedropper();
                break;
            case Tool::Move: {
                Canvas* cv = m_canvasManager.activeCanvas();
                if (!cv) break;
                pushUndo();
                Rect cr = canvasRect();
                Frame* f = cv->document().activeFrame();
                if (f && f->activeLayer()) {
                    const Rect* selRect = nullptr;
                    Rect selCanvas;
                    if (m_rectSelectTool.hasSelection()) {
                        selCanvas = m_rectSelectTool.getCanvasRect(*cv, cr);
                        selRect = &selCanvas;
                    }
                    Layer* layer = f->activeLayer();
                    m_moveTool.begin(*layer, *cv, cr, x, y, selRect);
                }
                break;
            }
            case Tool::RectSelect:
                m_moveTool.clearFloat();
                m_rectSelectTool.start(x, y);
                break;
        }
    }
}

void MainWindow::onScroll(float x, float y, int mods) {
    m_mouse.onScroll(x, y);
    DebugLog::log("[MainWindow] onScroll x=%.2f y=%.2f mods=%d mx=%.1f my=%.1f custom=%d", x, y, mods, m_mouse.position().x, m_mouse.position().y, m_customWindowOpen);

    // Scrolling over the editor window must not zoom/pan the canvas behind it.
    if (m_customWindowOpen && hit(cwRect(), m_mouse.position().x, m_mouse.position().y)) {
        DebugLog::log("[MainWindow] onScroll blocked by custom window");
        return;
    }

    Canvas* canvas = m_canvasManager.activeCanvas();
    if (!canvas) {
        DebugLog::log("[MainWindow] onScroll no canvas");
        return;
    }

    float mx = m_mouse.position().x;
    float my = m_mouse.position().y;
    Rect cr = canvasRect();
    bool inCanvas = isInCanvas(mx, my);
    if (!inCanvas) {
        DebugLog::log("[MainWindow] onScroll not in canvas mx=%.1f my=%.1f cr=[%.1f %.1f %.1f %.1f] - using center pivot", mx, my, cr.x, cr.y, cr.w, cr.h);
        mx = cr.x + cr.w * 0.5f;
        my = cr.y + cr.h * 0.5f;
        inCanvas = true;
    }

    const int MOD_CTRL  = 2;   // Mod::Ctrl = 1 << 1 = 2
    const int MOD_SHIFT = 1;   // Mod::Shift = 1 << 0 = 1
    bool ctrl  = (mods & MOD_CTRL)  || m_ctrlDown;
    bool shift = (mods & MOD_SHIFT) || m_shiftDown;

    // Vertical wheel zoom: no modifier, or Ctrl+Shift (so zoom works during pan/rotate)
    // Horizontal tilt wheel (x) always pans.
    bool isVerticalZoom = (y != 0);
    bool allowZoom = (!ctrl && !shift) || (ctrl && shift);
    if (allowZoom && isVerticalZoom) {
        float cx = mx - cr.x;
        float cy = my - cr.y;

        float oldZoom = canvas->zoom();
        float factor = (y > 0) ? 1.15f : 0.85f;
        float newZoom = std::clamp(oldZoom * factor, 0.2f, 128.0f);

        float docW = (float)canvas->document().width();
        float docH = (float)canvas->document().height();

        float totalOffX_old = (cr.w - docW * oldZoom) / 2.0f + canvas->cameraX();
        float totalOffY_old = (cr.h - docH * oldZoom) / 2.0f + canvas->cameraY();
        float docX = (cx - totalOffX_old) / oldZoom;
        float docY = (cy - totalOffY_old) / oldZoom;

        float totalOffX_new = cx - docX * newZoom;
        float totalOffY_new = cy - docY * newZoom;
        float newCamX = totalOffX_new - (cr.w - docW * newZoom) / 2.0f;
        float newCamY = totalOffY_new - (cr.h - docH * newZoom) / 2.0f;

        canvas->setZoom(newZoom);
        canvas->setCamera(newCamX, newCamY);
        return;
    }

    // Ctrl/Shift+wheel → pan canvas. Wheel alone or Ctrl+Shift+vertical already handled zoom above.
    // Horizontal tilt wheel (x) always pans.
    // Shift+vertical wheel → horizontal pan (when not zooming)
    float panSpeed = 30.0f;
    float dx = x;
    float dy = y;
    bool isZoomMod = ctrl && shift;
    if (!isZoomMod && shift && !ctrl && dx == 0 && dy != 0) {
        // Shift+vertical wheel → horizontal pan
        dx = dy;
        dy = 0;
    }
    // Invert so scroll down moves viewport down (canvas appears to scroll up)
    canvas->setCamera(canvas->cameraX() - dx * panSpeed,
                      canvas->cameraY() - dy * panSpeed);
}

void MainWindow::onResize(int fbW, int fbH) {
    m_framebufferWidth = fbW;
    m_framebufferHeight = fbH;
}

// PRESSURE PIPELINE: handleDrawing receives per-event pen pressure and stores
// it alongside each stroke point; the pressure->size/opacity mapping is applied
// later by Brush::radiusForPressure/opacityForPressure (drawing/Pressure.hpp).
void MainWindow::handleDrawing(float pressure) {
    Canvas* canvas = m_canvasManager.activeCanvas();
    if (!canvas) return;
    Frame* frame = canvas->document().activeFrame();
    if (!frame) return;

    Rect cr = canvasRect();
    Vec2 canvasPos = canvas->screenToCanvas(
        m_mouse.position().x - cr.x, m_mouse.position().y - cr.y,
        cr.w, cr.h);

    if (m_activeTool == Tool::Brush || m_activeTool == Tool::Eraser) {
        Layer* layer = frame->activeLayer();
        if (!layer || !layer->visible()) {
            m_lastBrushPos = canvasPos;
            frame->setDirty();
            return;
        }

        auto stampAt = [&](float px, float py) {
            if (m_activeTool == Tool::Eraser) {
                m_eraser.stamp(*layer, px, py, pressure);
            } else {
                m_brushEngine.applyStamp(*layer, px, py, m_brush, pressure);
            }
        };

        if (m_lastBrushPos.x < 0) {
            stampAt(canvasPos.x, canvasPos.y);
        } else {
            auto points = m_brush.interpolatePoints(m_lastBrushPos, canvasPos);
            for (const auto& pt : points) {
                stampAt(pt.x, pt.y);
            }
            stampAt(canvasPos.x, canvasPos.y);
        }

        m_lastBrushPos = canvasPos;
        layer->setDirty();
        frame->setDirty();
        return;
    }

    Layer* layer = frame->activeLayer();
    if (!layer || !layer->visible()) return;

    auto stampAt = [&](float px, float py) {
        m_brushEngine.applyStamp(*layer, px, py, m_brush, pressure);
    };

    if (m_lastBrushPos.x < 0) {
        stampAt(canvasPos.x, canvasPos.y);
    } else {
        auto points = m_brush.interpolatePoints(m_lastBrushPos, canvasPos);
        for (const auto& pt : points) {
            stampAt(pt.x, pt.y);
        }
        stampAt(canvasPos.x, canvasPos.y);
    }

    m_lastBrushPos = canvasPos;
    layer->setDirty();
    frame->setDirty();
}

void MainWindow::handleEyedropper() {
    Canvas* canvas = m_canvasManager.activeCanvas();
    if (!canvas) return;
    Frame* frame = canvas->document().activeFrame();
    if (!frame) return;
    Layer* layer = frame->activeLayer();
    if (!layer) return;

    Rect cr = canvasRect();
    Vec2 canvasPos = canvas->screenToCanvas(
        m_mouse.position().x - cr.x, m_mouse.position().y - cr.y,
        cr.w, cr.h);

    int px = (int)std::round(canvasPos.x);
    int py = (int)std::round(canvasPos.y);
    Color c = layer->getPixel(px, py);
    if (c.a > 0) {
        m_brush.setColor(c);
        Color::rgbToHsv(c.r, c.g, c.b, m_hue, m_sat, m_val);
    }
}

void MainWindow::onKeyDown(int keyCode) {
    switch (keyCode) {
        case SAPP_KEYCODE_1: case SAPP_KEYCODE_B:
            m_activeTool = Tool::Brush;
            applyCustomBrushType();
            m_renderer.hotSwapEraser(false);
            break;
        case SAPP_KEYCODE_2: case SAPP_KEYCODE_E:
            m_activeTool = Tool::Eraser;
            m_brush.setType(BrushType::Eraser);
            m_eraser.setSize(m_eraserSize);
            syncEraserOpacity();
            m_renderer.hotSwapEraser(true);
            break;
        case SAPP_KEYCODE_3: case SAPP_KEYCODE_P:
            m_activeTool = Tool::Eyedropper;
            m_renderer.hotSwapEraser(false);
            break;
        case SAPP_KEYCODE_4: case SAPP_KEYCODE_M:
            m_activeTool = Tool::Move;
            m_renderer.hotSwapEraser(false);
            break;
        case SAPP_KEYCODE_5: case SAPP_KEYCODE_S:
            m_activeTool = Tool::RectSelect;
            m_renderer.hotSwapEraser(false);
            break;
        case SAPP_KEYCODE_DELETE: case SAPP_KEYCODE_BACKSPACE: case SAPP_KEYCODE_C: {
            Canvas* c = m_canvasManager.activeCanvas();
            if (c) {
                Frame* f = c->document().activeFrame();
                if (f && f->activeLayer()) {
                    pushUndo();
                    Layer* delLayer = f->activeLayer();
                    if (m_rectSelectTool.hasSelection()) {
                        Rect cr = canvasRect();
                        Rect selCanvas = m_rectSelectTool.getCanvasRect(*c, cr);
                        m_rectSelectTool.deleteSelected(*delLayer, *c, cr);
                    } else {
                        m_renderer.hotSwapEraser(true);
                        m_renderer.jitPipeline().clearPixels()(f->activeLayer()->data(), f->activeLayer()->dataSize());
                    }
                    f->setDirty();
                }
            }
            break;
        }
        case SAPP_KEYCODE_LEFT_BRACKET:
            m_brushSize = std::max(1.0f, m_brushSize - 2.0f);
            m_brush.setSize(m_brushSize);
            break;
        case SAPP_KEYCODE_RIGHT_BRACKET:
            m_brushSize = std::min(64.0f, m_brushSize + 2.0f);
            m_brush.setSize(m_brushSize);
            break;
        case SAPP_KEYCODE_SPACE: {
            m_playing = !m_playing;
            if (m_playing) { m_playTimer = 0.0f; }
            break;
        }
        case SAPP_KEYCODE_Z: {
            Canvas* c = m_canvasManager.activeCanvas();
            if (c) c->setZoom(std::max(0.2f, c->zoom() * 0.8f));
            break;
        }
        case SAPP_KEYCODE_X: {
            Canvas* c = m_canvasManager.activeCanvas();
            if (c) c->setZoom(std::min(128.0f, c->zoom() * 1.25f));
            break;
        }
        default: break;
    }

    // Track Ctrl/Shift state for canvas panning and rotation.
    if (keyCode == SAPP_KEYCODE_LEFT_CONTROL || keyCode == SAPP_KEYCODE_RIGHT_CONTROL)
        m_ctrlDown = true;
    if (keyCode == SAPP_KEYCODE_LEFT_SHIFT || keyCode == SAPP_KEYCODE_RIGHT_SHIFT)
        m_shiftDown = true;
}

void MainWindow::onKeyUp(int keyCode) {
    if (keyCode == SAPP_KEYCODE_LEFT_CONTROL || keyCode == SAPP_KEYCODE_RIGHT_CONTROL) {
        m_ctrlDown = false;
        // If Ctrl is released mid-pan, end the pan cleanly.
        if (m_panning) {
            m_panning = false;
        }
        if (m_rotating) {
            m_rotating = false;
        }
    }
    if (keyCode == SAPP_KEYCODE_LEFT_SHIFT || keyCode == SAPP_KEYCODE_RIGHT_SHIFT) {
        m_shiftDown = false;
        if (m_rotating) {
            m_rotating = false;
        }
    }
}

void MainWindow::saveCurrentFrame() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* frame = c->document().activeFrame();
    if (!frame) return;

    int w = frame->width();
    int h = frame->height();
    std::vector<uint8_t> buf;
    int bufW, bufH;
    frame->compositeToBuffer(buf, bufW, bufH);

    FILE* f = fopen("frame.bmp", "wb");
    if (!f) return;

    int rowSize = (w * 3 + 3) & ~3;
    int imageSize = rowSize * h;
    int fileSize = 54 + imageSize;

    uint8_t header[54] = {};
    header[0] = 'B'; header[1] = 'M';
    header[2] = (uint8_t)fileSize; header[3] = (uint8_t)(fileSize >> 8);
    header[4] = (uint8_t)(fileSize >> 16); header[5] = (uint8_t)(fileSize >> 24);
    header[10] = 54;
    header[14] = 40;
    header[18] = (uint8_t)w; header[19] = (uint8_t)(w >> 8); header[20] = (uint8_t)(w >> 16); header[21] = (uint8_t)(w >> 24);
    header[22] = (uint8_t)h; header[23] = (uint8_t)(h >> 8); header[24] = (uint8_t)(h >> 16); header[25] = (uint8_t)(h >> 24);
    header[26] = 1; header[28] = 24; header[34] = (uint8_t)imageSize;
    header[35] = (uint8_t)(imageSize >> 8); header[36] = (uint8_t)(imageSize >> 16); header[37] = (uint8_t)(imageSize >> 24);
    fwrite(header, 1, 54, f);

    std::vector<uint8_t> row(rowSize, 0);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            size_t off = (y * w + x) * 4;
            row[x * 3 + 0] = buf[off + 2]; // B
            row[x * 3 + 1] = buf[off + 1]; // G
            row[x * 3 + 2] = buf[off + 0]; // R
        }
        fwrite(row.data(), 1, rowSize, f);
    }
    fclose(f);
    printf("[MainWindow] Exported current frame to frame.bmp (%dx%d)\n", w, h);
}

void MainWindow::update(float dt) {
    // NOTE: Continuous drawing is driven by onMouseMove events, not the update loop.
    // Calling handleDrawing() here would re-apply strokes at the same position every
    // frame causing duplicate draws and breaking undo (undo restores state but update
    // immediately re-draws the stroke). Mouse events are the sole drawing triggers.

    m_uiClock += dt;

    if (m_statusTimer > 0.0f)
        m_statusTimer -= dt;

    if (m_playing) {
        m_playTimer += dt;
        // Per-frame playback speed: each frame's duration is a multiplier of
        // one beat at the global fps (1.0 = default pace).
        Canvas* c = m_canvasManager.activeCanvas();
        float mult = 1.0f;
        if (c) {
            const Frame* cur = c->document().getFrame(m_currentFrame);
            if (cur && cur->duration() > 0.0f) mult = cur->duration();
        }
        float frameDur = mult / m_fps;
        if (m_playTimer >= frameDur) {
            m_playTimer -= frameDur;
            if (c && c->document().frameCount() > 0) {
                m_currentFrame = (m_currentFrame + 1) % c->document().frameCount();
                c->document().setActiveFrame(m_currentFrame);
            }
        }
    }

    m_mouse.endFrame();
}

void MainWindow::render() {
    Canvas* canvas = m_canvasManager.activeCanvas();

    float viewW = (float)sapp_width();
    float viewH = (float)sapp_height();
    if (viewW <= 0.0f) viewW = (float)m_framebufferWidth;
    if (viewH <= 0.0f) viewH = (float)m_framebufferHeight;

    m_renderer.beginFrame(viewW, viewH);

    // 1. Render Canvas Checkerboard & Composite Frame
    if (canvas) {
        canvas->update();
        Rect cr = canvasRect();

        if (!m_checkerTex.valid) {
            const int texW = 16;
            const int texH = 16;
            std::vector<uint8_t> data(texW * texH * 4);
            for (int i = 0; i < texW * texH; i++) {
                data[i * 4 + 0] = 255;
                data[i * 4 + 1] = 255;
                data[i * 4 + 2] = 255;
                data[i * 4 + 3] = 255;
            }
            m_checkerTex.update(data, texW, texH);
        }

        float cw = (float)canvas->document().width() * canvas->zoom();
        float ch = (float)canvas->document().height() * canvas->zoom();
        float offsetX = (cr.w - cw) / 2.0f + canvas->cameraX();
        float offsetY = (cr.h - ch) / 2.0f + canvas->cameraY();
        float screenX = cr.x + offsetX;
        float screenY = cr.y + offsetY;

        // Draw checkerboard for canvas document bounds
        if (m_checkerTex.valid) {
            m_renderer.queueQuad(screenX, screenY, cw, ch,
                m_checkerTex.image, m_checkerTex.view, m_checkerTex.sampler,
                0.0f, 0.0f, (float)canvas->document().width() / 16.0f, (float)canvas->document().height() / 16.0f,
                canvas->rotation());
        }

        // Draw composite frame buffer on top of checkerboard
        if (!canvas->compositeBuffer().empty()) {
            m_canvasTex.update(canvas->compositeBuffer(),
                              canvas->compositeWidth(), canvas->compositeHeight());
            if (m_canvasTex.valid) {
                m_renderer.queueQuad(screenX, screenY, cw, ch,
                    m_canvasTex.image, m_canvasTex.view, m_canvasTex.sampler,
                    0.0f, 0.0f, 1.0f, 1.0f,
                    canvas->rotation());
            }
        }

        // Canvas border outline (skipped when rotated — axis-aligned box
        // doesn't match the rotated edges).
        if (canvas->rotation() == 0.0f) {
        m_renderer.queueSolidRect(screenX - 1, screenY - 1, cw + 2, 1, Color(100, 100, 115, 255));
        m_renderer.queueSolidRect(screenX - 1, screenY + ch, cw + 2, 1, Color(100, 100, 115, 255));
        m_renderer.queueSolidRect(screenX - 1, screenY, 1, ch, Color(100, 100, 115, 255));
        m_renderer.queueSolidRect(screenX + cw, screenY, 1, ch, Color(100, 100, 115, 255));
        }

        // Pixel grid at >=16x — always black, constant alpha 40.
        if (canvas->rotation() == 0.0f && canvas->zoom() >= 16.0f) {
            int docW = canvas->document().width();
            int docH = canvas->document().height();
            float zoom = canvas->zoom();
            float vx0 = (cr.x - screenX) / zoom;
            float vy0 = (cr.y - screenY) / zoom;
            float vx1 = (cr.x + cr.w - screenX) / zoom;
            float vy1 = (cr.y + cr.h - screenY) / zoom;
            int visX0 = (int)std::floor(std::max(0.0f, vx0));
            int visY0 = (int)std::floor(std::max(0.0f, vy0));
            int visX1 = (int)std::ceil(std::min((float)docW, vx1));
            int visY1 = (int)std::ceil(std::min((float)docH, vy1));
            if (visX1 <= visX0) { visX0 = 0; visX1 = docW; }
            if (visY1 <= visY0) { visY0 = 0; visY1 = docH; }
            // Always black, constant alpha 40 — no adaptation, no blend.
            Color grid(0, 0, 0, 40);
            // Vertical lines
            for (int i = 1; i < docW; i++) {
                float x = screenX + (float)i * zoom;
                if (x < cr.x - 1 || x > cr.x + cr.w) continue;
                float y0 = std::max(screenY + (float)visY0 * zoom, cr.y);
                float y1 = std::min(screenY + (float)visY1 * zoom, cr.y + cr.h);
                y0 = std::max(y0, screenY); y1 = std::min(y1, screenY + (float)docH * zoom);
                if (y1 > y0) m_renderer.queueSolidRect(x, y0, 2, y1 - y0, grid);
            }
            // Horizontal lines
            for (int j = 1; j < docH; j++) {
                float y = screenY + (float)j * zoom;
                if (y < cr.y - 1 || y > cr.y + cr.h) continue;
                float x0 = std::max(screenX + (float)visX0 * zoom, cr.x);
                float x1 = std::min(screenX + (float)visX1 * zoom, cr.x + cr.w);
                x0 = std::max(x0, screenX); x1 = std::min(x1, screenX + (float)docW * zoom);
                if (x1 > x0) m_renderer.queueSolidRect(x0, y, x1 - x0, 2, grid);
            }
        }

        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline
        // Selection rectangle outline if selecting or has selection
        m_rectSelectTool.render(m_renderer);

        // Brush stamp preview overlay on canvas — accurate to canvas pixels
        float mx = m_mouse.position().x;
        float my = m_mouse.position().y;
        if (isInCanvas(mx, my) && (m_activeTool == Tool::Brush || m_activeTool == Tool::Eraser)) {
            bool isEraser = (m_activeTool == Tool::Eraser);
            // Regenerate preview when relevant settings change (brush or eraser)
            bool needUpdate = false;
            if (isEraser) {
                if (m_eraser.size() != m_previewBrushSize || m_eraser.opacity() != m_previewBrushOpacity) needUpdate = true;
            } else {
                if (m_brush.type() != m_previewBrushType ||
                    m_brush.size() != m_previewBrushSize ||
                    m_brush.opacity() != m_previewBrushOpacity ||
                    m_brush.hardness() != m_previewBrushHardness ||
                    m_brush.color() != m_previewBrushColor) needUpdate = true;
            }
            if (needUpdate) {
                m_previewBrushType = m_brush.type();
                m_previewBrushSize = isEraser ? m_eraser.size() : m_brush.size();
                m_previewBrushOpacity = isEraser ? m_eraser.opacity() : m_brush.opacity();
                m_previewBrushHardness = m_brush.hardness();
                m_previewBrushColor = m_brush.color();
                updateBrushPreview();
            }

            if (m_brushPreviewTex.valid) {
                float size = isEraser ? m_eraser.size() : m_brush.size();
                float radius = size * 0.5f;
                float extent = radius;
                if (!isEraser && m_brush.type() == BrushType::Custom) extent = radius * CUSTOM_MAX_HEIGHT;
                // Snap preview to canvas pixel grid so it matches where stamp will land
                Vec2 canvasPos = canvas->screenToCanvas(mx - cr.x, my - cr.y, cr.w, cr.h);
                canvasPos.x = std::round(canvasPos.x);
                canvasPos.y = std::round(canvasPos.y);
                Vec2 vp = canvas->canvasToScreen(canvasPos.x, canvasPos.y, cr.w, cr.h);
                float screenCenterX = cr.x + vp.x;
                float screenCenterY = cr.y + vp.y;
                float screenDiameter = extent * 2.0f * canvas->zoom();
                float buf = (float)m_brushPreviewTex.width;
                float texFrac = extent / std::max(1.0f, buf);
                texFrac = std::clamp(texFrac, 0.0f, 0.5f);
                // At size 1, force preview to exactly one grid cell (snap to pixel boundaries)
                if (size == 1.0f) {
                    screenDiameter = canvas->zoom();
                    screenCenterX = std::round(screenCenterX);
                    screenCenterY = std::round(screenCenterY);
                    texFrac = 0.5f / std::max(1.0f, buf);
                }
                m_renderer.queueQuad(
                    screenCenterX - screenDiameter * 0.5f, screenCenterY - screenDiameter * 0.5f,
                    screenDiameter, screenDiameter,
                    m_brushPreviewTex.image, m_brushPreviewTex.view,
                    m_brushPreviewSampler,
                    0.5f - texFrac, 0.5f - texFrac,
                    0.5f + texFrac, 0.5f + texFrac);
            }
        }
    }

    // Flush canvas textured quads (drawn behind UI overlays)
    m_renderer.flushQuads(viewW, viewH);

    // 2. UI overlays (Top Toolbar, Left Sidebar, Layer Panel, Timeline)
    renderTopToolbar();
    renderLeftSidebar();
    renderCustomBrushSection();
    renderLayerPanel();
    renderTimeline();
    renderCustomBrushWindow();

    // Flush all UI solid rectangles and text ONCE
    m_renderer.flushSolid(viewW, viewH);
    m_renderer.flushText(viewW, viewH);
    m_renderer.endFrame();
}

void MainWindow::renderTopToolbar() {
    float fbW = (float)sapp_width();
    if (fbW <= 0.0f) fbW = (float)m_framebufferWidth;

    Color bg(36, 36, 40, 255);
    Color border(52, 52, 58, 255);
    Color btnBg(48, 48, 54, 255);
    Color textC(220, 220, 220, 255);

    m_renderer.queueSolidRect(LEFT_SIDEBAR_W, 0, fbW - LEFT_SIDEBAR_W - LAYER_PANEL_W, TOP_TOOLBAR_H, bg);
    m_renderer.queueSolidRect(LEFT_SIDEBAR_W, TOP_TOOLBAR_H - 1, fbW - LEFT_SIDEBAR_W - LAYER_PANEL_W, 1, border);

    float tbX = LEFT_SIDEBAR_W + 10.0f;
    float btnY = 6.0f;
    float btnH = 24.0f;

    // Undo / Redo
    Color undoBg = m_undoStack.empty() ? Color(38, 38, 42, 255) : btnBg;
    Color undoText = m_undoStack.empty() ? Color(120, 120, 128, 255) : textC;
    m_renderer.queueSolidRect(tbX, btnY, 44, btnH, undoBg);
    m_renderer.drawText("Undo", tbX + 6, btnY + 7, 0.9f, undoText);
    tbX += 50.0f;

    Color redoBg = m_redoStack.empty() ? Color(38, 38, 42, 255) : btnBg;
    Color redoText = m_redoStack.empty() ? Color(120, 120, 128, 255) : textC;
    m_renderer.queueSolidRect(tbX, btnY, 44, btnH, redoBg);
    m_renderer.drawText("Redo", tbX + 6, btnY + 7, 0.9f, redoText);
    tbX += 54.0f;

    // Save
    m_renderer.queueSolidRect(tbX, btnY, 50, btnH, Color(60, 100, 160, 255));
    m_renderer.drawText("Save", tbX + 9, btnY + 7, 0.9f, Color::white());
    tbX += 65.0f;

    // Zoom Controls
    Canvas* c = m_canvasManager.activeCanvas();
    float zoom = c ? c->zoom() : 1.0f;

    m_renderer.queueSolidRect(tbX, btnY, 26, btnH, btnBg);
    m_renderer.drawText("-", tbX + 9, btnY + 7, 1.0f, textC);
    tbX += 32.0f;

    m_renderer.queueSolidRect(tbX, btnY, 26, btnH, btnBg);
    m_renderer.drawText("+", tbX + 8, btnY + 7, 1.0f, textC);
    tbX += 32.0f;

    m_renderer.queueSolidRect(tbX, btnY, 36, btnH, btnBg);
    m_renderer.drawText("1:1", tbX + 6, btnY + 7, 0.9f, textC);
    tbX += 45.0f;

    char zoomBuf[32];
    snprintf(zoomBuf, sizeof(zoomBuf), "%d%%", (int)(zoom * 100));
    m_renderer.drawText(zoomBuf, tbX, btnY + 7, 0.9f, Color(160, 160, 170, 255));
}

void MainWindow::renderLeftSidebar() {
    Color bg(30, 30, 34, 255);
    Color border(46, 46, 52, 255);
    Color textC(210, 210, 210, 255);

    float fbH = (float)sapp_height();
    if (fbH <= 0.0f) fbH = (float)m_framebufferHeight;

    m_renderer.queueSolidRect(0, 0, LEFT_SIDEBAR_W, fbH, bg);
    m_renderer.queueSolidRect(LEFT_SIDEBAR_W - 1, 0, 1, fbH, border);

    // Tools
    float ty = 8.0f;
    float sw = LEFT_SIDEBAR_W - 20.0f;
    float sx = 10.0f;
    const char* toolNames[] = {"1. Brush", "2. Eraser", "3. Eyedrop", "4. Move", "5. Select"};
    const Tool tools[] = {Tool::Brush, Tool::Eraser, Tool::Eyedropper, Tool::Move, Tool::RectSelect};

    for (int i = 0; i < 5; i++) {
        bool active = (m_activeTool == tools[i]);
        Color btnBg = active ? Color(60, 110, 180, 255) : Color(45, 45, 52, 255);
        m_renderer.queueSolidRect(sx, ty, sw, 26, btnBg);
        m_renderer.drawText(toolNames[i], sx + 6, ty + 7, 0.85f, active ? Color::white() : textC);
        ty += 30.0f;
    }

    // Color Picker - Sat/Val square
    m_renderer.queueSolidRect(SV_X - 1, SV_Y - 1, SV_SIZE + 2, SV_SIZE + 2, Color(20, 20, 24, 255));
    float step = SV_SIZE / (float)SV_GRID;
    for (int gy = 0; gy < SV_GRID; gy++) {
        for (int gx = 0; gx < SV_GRID; gx++) {
            float s = (float)gx / (float)(SV_GRID - 1);
            float v = 1.0f - (float)gy / (float)(SV_GRID - 1);
            Color cell = Color::hsvToRgb(m_hue, s, v);
            m_renderer.queueSolidRect(SV_X + gx * step, SV_Y + gy * step, step + 0.5f, step + 0.5f, cell);
        }
    }
    // SV marker
    float markerX = SV_X + m_sat * SV_SIZE;
    float markerY = SV_Y + (1.0f - m_val) * SV_SIZE;
    m_renderer.queueSolidRect(markerX - 3, markerY - 3, 6, 6, Color::black());
    m_renderer.queueSolidRect(markerX - 2, markerY - 2, 4, 4, Color::white());

    // Hue bar
    m_renderer.queueSolidRect(HUE_X - 1, HUE_Y - 1, HUE_W + 2, HUE_H + 2, Color(20, 20, 24, 255));
    float hueStep = HUE_W / (float)HUE_STEPS;
    for (int i = 0; i < HUE_STEPS; i++) {
        float h = ((float)i / (float)HUE_STEPS) * 360.0f;
        Color hc = Color::hsvToRgb(h, 1.0f, 1.0f);
        m_renderer.queueSolidRect(HUE_X + i * hueStep, HUE_Y, hueStep + 0.5f, HUE_H, hc);
    }
    // Hue marker
    float hMarkerX = HUE_X + (m_hue / 360.0f) * HUE_W;
    m_renderer.queueSolidRect(hMarkerX - 2, HUE_Y - 1, 4, HUE_H + 2, Color::white());

    // Size Slider
    float szY = HUE_Y + HUE_H + 18.0f;
    float activeSize = (m_activeTool == Tool::Eraser) ? m_eraserSize : m_brushSize;
    char sizeBuf[32];
    snprintf(sizeBuf, sizeof(sizeBuf), "Size: %dpx", (int)activeSize);
    m_renderer.drawText(sizeBuf, sx, szY - 12, 0.8f, textC);
    m_renderer.queueSolidRect(sx, szY, sw, 8, Color(24, 24, 28, 255));
    float sizeFrac = std::clamp((activeSize - 1.0f) / 63.0f, 0.0f, 1.0f);
    m_renderer.queueSolidRect(sx, szY, sw * sizeFrac, 8, Color(60, 110, 180, 255));
    m_renderer.queueSolidRect(sx + sw * sizeFrac - 3, szY - 2, 6, 12, Color::white());

    // Opacity Slider (only for Brush tool)
    float swatchY;
    if (m_activeTool == Tool::Brush) {
        float opY = szY + 28.0f;
        char opBuf[32];
        snprintf(opBuf, sizeof(opBuf), "Opacity: %d%%", (int)(m_brushOpacity * 100));
        m_renderer.drawText(opBuf, sx, opY - 12, 0.8f, textC);
        m_renderer.queueSolidRect(sx, opY, sw, 8, Color(24, 24, 28, 255));
        m_renderer.queueSolidRect(sx, opY, sw * m_brushOpacity, 8, Color(60, 110, 180, 255));
        m_renderer.queueSolidRect(sx + sw * m_brushOpacity - 3, opY - 2, 6, 12, Color::white());
        swatchY = opY + 24.0f;
    } else {
        swatchY = szY + 28.0f + 24.0f;
    }

    // Current color swatch
    m_renderer.drawText("Active Color", sx, swatchY, 0.8f, textC);
    m_renderer.queueSolidRect(sx - 1, swatchY + 14, sw + 2, 26, Color(20, 20, 24, 255));
    m_renderer.queueSolidRect(sx, swatchY + 15, sw, 24, Color::hsvToRgb(m_hue, m_sat, m_val));
}

void MainWindow::renderLayerPanel() {
    float fbW = (float)sapp_width();
    float fbH = (float)sapp_height();
    if (fbW <= 0.0f) fbW = (float)m_framebufferWidth;
    if (fbH <= 0.0f) fbH = (float)m_framebufferHeight;

    float panelX = fbW - LAYER_PANEL_W;
    Color bg(30, 30, 34, 255);
    Color border(46, 46, 52, 255);
    Color textC(210, 210, 210, 255);

    m_renderer.queueSolidRect(panelX, 0, LAYER_PANEL_W, fbH - TIMELINE_H, bg);
    m_renderer.queueSolidRect(panelX, 0, 1, fbH - TIMELINE_H, border);

    m_renderer.drawText("LAYERS", panelX + 12, 12, 1.0f, Color::white());

    // Layer action buttons: + Layer / - Layer / + Group
    {
        const float bw = 64.0f, bh = 22.0f;
        Color btnBg(48, 48, 54, 255);
        m_renderer.queueSolidRect(panelX + 10, 38, bw, bh, btnBg);
        m_renderer.drawText("+ Layer", panelX + 16, 43, 0.85f, textC);
        m_renderer.queueSolidRect(panelX + 80, 38, bw, bh, btnBg);
        m_renderer.drawText("- Layer", panelX + 86, 43, 0.85f, textC);
        m_renderer.queueSolidRect(panelX + 150, 38, bw, bh, btnBg);
        m_renderer.drawText("+ Group", panelX + 156, 43, 0.85f, textC);
    }

    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* frame = c->document().activeFrame();
    if (!frame) return;
    syncPanelFrameState();

    std::vector<PanelItem> items;
    buildPanelItems(items);
    int cursor = resolvePanelCursor(items);

    // While a grabbed layer is waiting, headers show a drop-target outline.
    const bool grabbing = m_grabbedValid;

    float itemY = 68.0f;
    float dndFirstY = -1.0f, dndLastBottom = -1.0f;
    for (int row = 0; row < (int)items.size(); row++) {
        const MainWindow::PanelItem& it = items[row];
        bool isCur = (row == cursor);
        if (dndFirstY < 0.0f) dndFirstY = itemY;
        dndLastBottom = itemY + 24.0f;

        if (it.isHeader) {
            const Frame::Group& g = frame->getGroup(it.index);
            Color headBg = isCur ? Color(55, 95, 150, 255) : Color(38, 38, 46, 255);
            m_renderer.queueSolidRect(panelX + 10, itemY, LAYER_PANEL_W - 20, 24, headBg);
            // Group-color accent bar on the left edge
            Color gc((uint8_t)(g.color & 0xFF), (uint8_t)((g.color >> 8) & 0xFF),
                     (uint8_t)((g.color >> 16) & 0xFF), 255);
            m_renderer.queueSolidRect(panelX + 10, itemY, 3, 24, gc);

            if (grabbing) {   // amber drop-target outline
                Color drop(230, 170, 40, 255);
                m_renderer.queueSolidRect(panelX + 10, itemY, LAYER_PANEL_W - 20, 1, drop);
                m_renderer.queueSolidRect(panelX + 10, itemY + 23, LAYER_PANEL_W - 20, 1, drop);
                m_renderer.queueSolidRect(panelX + 10, itemY, 1, 24, drop);
                m_renderer.queueSolidRect(panelX + LAYER_PANEL_W - 11, itemY, 1, 24, drop);
            }

            // Drag-and-drop nest-target highlight (amber outline).
            if (m_layerDnd.active() && m_layerDnd.plan().highlightGroup == it.index) {
                Color dd(230, 170, 40, 255);
                m_renderer.queueSolidRect(panelX + 10, itemY, LAYER_PANEL_W - 20, 1, dd);
                m_renderer.queueSolidRect(panelX + 10, itemY + 23, LAYER_PANEL_W - 20, 1, dd);
                m_renderer.queueSolidRect(panelX + 10, itemY, 1, 24, dd);
                m_renderer.queueSolidRect(panelX + LAYER_PANEL_W - 11, itemY, 1, 24, dd);
            }

            m_renderer.drawText(g.collapsed ? "[+]" : "[-]", panelX + 17, itemY + 6,
                                0.85f, isCur ? Color::white() : textC);
            if (m_renamingGroupIndex == it.index) {
                // Inline edit box over the name/count area while renaming
                float bx = panelX + 40.0f;
                float by = itemY + 2.0f;
                float bw = LAYER_PANEL_W - 56.0f;
                float bh = 20.0f;
                Color editBorder(90, 140, 210, 255);
                m_renderer.queueSolidRect(bx, by, bw, bh, Color(16, 16, 22, 255));
                m_renderer.queueSolidRect(bx, by, bw, 1, editBorder);
                m_renderer.queueSolidRect(bx, by + bh - 1.0f, bw, 1, editBorder);
                m_renderer.queueSolidRect(bx, by, 1, bh, editBorder);
                m_renderer.queueSolidRect(bx + bw - 1.0f, by, 1, bh, editBorder);
                m_renderer.drawText(m_renameBuffer.c_str(), bx + 4.0f, by + 4.0f,
                                    RENAME_TEXT_SCALE, Color::white());
                if (std::fmod(m_uiClock, 1.0f) < 0.5f) {
                    float caretX = bx + 4.0f +
                                   (float)m_renameBuffer.size() * (float)FONT_CHAR_W * RENAME_TEXT_SCALE;
                    m_renderer.queueSolidRect(caretX, by + 3.0f, 1.0f, bh - 6.0f,
                                              Color(220, 220, 235, 255));
                }
                itemY += 28.0f;
                continue;
            }
            m_renderer.drawText(g.name.c_str(), panelX + 44, itemY + 6, 0.85f,
                                isCur ? Color::white() : textC);
            char cnt[16];
            snprintf(cnt, sizeof(cnt), "(%d)", (int)g.layerIndices.size());
            float cntX = panelX + 44 +
                         (float)g.name.size() * FONT_CHAR_W * 0.85f + 6.0f;
            m_renderer.drawText(cnt, cntX, itemY + 6, 0.85f, Color(130, 130, 140, 255));
            itemY += 28.0f;
            continue;
        }

        int i = it.index;
        Layer* l = frame->getLayer(i);
        if (!l) { itemY += 28.0f; continue; }
        bool isActive = (l == frame->activeLayer());
        bool isGrabbed = grabbing && m_grabbedLayer == i;
        Color itemBg = isActive ? Color(55, 95, 150, 255) : Color(42, 42, 48, 255);
        if (isGrabbed)
            itemBg = Color(140, 100, 28, 255);   // amber tint while carried

        float ind = (frame->findGroupForLayer(i) >= 0 ? GROUP_ROW_INDENT : 0.0f) +
                    (l->isAttributeLayer()
                         ? ATTR_ROW_INDENT * (float)frame->attributeChainDepth(i)
                         : 0.0f);
        m_renderer.queueSolidRect(panelX + 10 + ind, itemY, LAYER_PANEL_W - 20 - ind, 24, itemBg);

        if (isGrabbed) {   // amber outline around the carried row
            Color drop(230, 170, 40, 255);
            float rx = panelX + 10 + ind, rw = LAYER_PANEL_W - 20 - ind;
            m_renderer.queueSolidRect(rx, itemY, rw, 1, drop);
            m_renderer.queueSolidRect(rx, itemY + 23, rw, 1, drop);
            m_renderer.queueSolidRect(rx, itemY, 1, 24, drop);
            m_renderer.queueSolidRect(rx + rw - 1, itemY, 1, 24, drop);
        }

        // Visibility indicator
        const char* vis = l->visible() ? "[V]" : "[ ]";
        Color visC = l->visible() ? Color(100, 220, 120, 255) : Color(120, 120, 130, 255);
        m_renderer.drawText(vis, panelX + 14 + ind, itemY + 6, 0.85f, visC);

        // Layer name (or inline edit box while renaming this row)
        if ((int)i == m_renamingLayerIndex) {
            float bx = panelX + 38.0f + ind;
            float by = itemY + 2.0f;
            float bw = LAYER_PANEL_W - 56.0f;
            float bh = 20.0f;
            Color editBorder(90, 140, 210, 255);
            m_renderer.queueSolidRect(bx, by, bw, bh, Color(16, 16, 22, 255));
            m_renderer.queueSolidRect(bx, by, bw, 1, editBorder);
            m_renderer.queueSolidRect(bx, by + bh - 1.0f, bw, 1, editBorder);
            m_renderer.queueSolidRect(bx, by, 1, bh, editBorder);
            m_renderer.queueSolidRect(bx + bw - 1.0f, by, 1, bh, editBorder);
            m_renderer.drawText(m_renameBuffer.c_str(), bx + 4.0f, by + 4.0f,
                                RENAME_TEXT_SCALE, Color::white());
            // Blinking caret after the last typed character
            if (std::fmod(m_uiClock, 1.0f) < 0.5f) {
                float caretX = bx + 4.0f +
                               (float)m_renameBuffer.size() * (float)FONT_CHAR_W * RENAME_TEXT_SCALE;
                m_renderer.queueSolidRect(caretX, by + 3.0f, 1.0f, bh - 6.0f,
                                          Color(220, 220, 235, 255));
            }
        } else if (l->isAttributeLayer()) {
            // Attribute layers don't draw their own pixels - badge + dimmed
            // name + indent under the holder row signal their modifier role.
            m_renderer.drawText("[A]", panelX + 38.0f + ind, itemY + 6, 0.85f,
                                Color(110, 200, 255, 255));
            m_renderer.drawText(l->name(), panelX + 66.0f + ind, itemY + 6, 0.85f,
                                isActive ? Color::white() : Color(150, 150, 165, 255));
        } else {
            m_renderer.drawText(l->name(), panelX + 42, itemY + 6, 0.85f,
                                isActive ? Color::white() : textC);
        }

        // Tag-color swatch (click cycles the preset palette)
        {
            uint32_t pc = l->color();
            Color sw((uint8_t)(pc & 0xFF), (uint8_t)((pc >> 8) & 0xFF),
                     (uint8_t)((pc >> 16) & 0xFF), (uint8_t)((pc >> 24) & 0xFF));
            float swX = panelX + LAYER_PANEL_W - 70.0f;
            m_renderer.queueSolidRect(swX - 1.0f, itemY + 5.0f, 14.0f, 14.0f, Color(18, 18, 22, 255));
            m_renderer.queueSolidRect(swX, itemY + 6.0f, 12.0f, 12.0f, sw);
        }


        // Reorder arrows: up = toward the top of the list = higher Z-order.
        // Dimmed at the edges of the row's own container (outer stack for
        // headers/ungrouped layers, group member list for members).
        {
            PanelRowCtx rc = panelRowContext(frame, items, row);
            bool canUp, canDown;
            if (rc.header || rc.memberPos < 0) {
                canUp = (rc.stackPos < frame->stackCount() - 1);
                canDown = (rc.stackPos > 0);
            } else {
                int memCount = (int)frame->getGroup(rc.groupIdx).layerIndices.size();
                canUp = (rc.memberPos < memCount - 1);
                canDown = (rc.memberPos > 0);
            }
            Color arrowC(190, 190, 200, 255);
            Color arrowDim(95, 95, 105, 255);
            float cxU = panelX + LAYER_PANEL_W - 39.0f;
            float cxD = panelX + LAYER_PANEL_W - 23.0f;
             float ay = itemY + 8.0f;
            for (int r = 0; r < 3; r++) {
                m_renderer.queueSolidRect(cxU - r, ay + r * 2.0f, (float)(r * 2 + 1), 2.0f,
                                          canUp ? arrowC : arrowDim);
                int rr = 2 - r;
                m_renderer.queueSolidRect(cxD - rr, ay + r * 2.0f, (float)(rr * 2 + 1), 2.0f,
                                          canDown ? arrowC : arrowDim);
            }
        }
        itemY += 28.0f;
    }

    // Drag-and-drop insertion line and cursor label.
    if (m_layerDnd.active()) {
        const DndPlan& dp = m_layerDnd.plan();
        if (dp.lineY >= dndFirstY - 2.0f && dp.lineY <= dndLastBottom + 2.0f) {
            Color line(90, 140, 210, 255);
            m_renderer.queueSolidRect(panelX + 10, dp.lineY, LAYER_PANEL_W - 20, 2, line);
        }
        Vec2 mp = m_mouse.position();
        const char* dragName = "";
        if (m_layerDnd.item() == LayerDragDrop::Item::Layer) {
            int li = m_layerDnd.layerIndex();
            if (li >= 0 && li < frame->layerCount())
                dragName = frame->getLayer(li)->name();
        } else {
            int gi = m_layerDnd.groupIndex();
            if (gi >= 0 && gi < frame->groupCount())
                dragName = frame->getGroup(gi).name.c_str();
        }
        float lx = std::clamp(mp.x + 12.0f, panelX + 10.0f, panelX + LAYER_PANEL_W - 100.0f);
        float ly = std::clamp(mp.y - 18.0f, 44.0f, fbH - TIMELINE_H - 24.0f);
        m_renderer.drawText(dragName, lx, ly, 0.85f, Color(235, 180, 60, 255));
    }

    // Group-op status line pinned to the panel bottom (auto-hides)
    if (m_statusTimer > 0.0f && !m_statusMsg.empty()) {
        size_t maxChars = (size_t)((LAYER_PANEL_W - 24.0f) / (FONT_CHAR_W * 0.85f));
        std::string msg = m_statusMsg.substr(0, maxChars);
        m_renderer.drawText(msg.c_str(), panelX + 12, fbH - TIMELINE_H - 18.0f,
                            0.85f, Color(235, 180, 60, 255));
    }
}

void MainWindow::renderTimeline() {
    float fbW = (float)sapp_width();
    float fbH = (float)sapp_height();
    if (fbW <= 0.0f) fbW = (float)m_framebufferWidth;
    if (fbH <= 0.0f) fbH = (float)m_framebufferHeight;

    float tlY = fbH - TIMELINE_H;
    Color bg(26, 26, 30, 255);
    Color border(46, 46, 52, 255);
    Color textC(210, 210, 210, 255);

    m_renderer.queueSolidRect(LEFT_SIDEBAR_W, tlY, fbW - LEFT_SIDEBAR_W, TIMELINE_H, bg);
    m_renderer.queueSolidRect(LEFT_SIDEBAR_W, tlY, fbW - LEFT_SIDEBAR_W, 1, border);

    m_renderer.drawText("TIMELINE", LEFT_SIDEBAR_W + 12, tlY + 10, 0.9f, Color::white());

    // Play/Pause button
    Color playBg = m_playing ? Color(180, 70, 70, 255) : Color(50, 130, 70, 255);
    m_renderer.queueSolidRect(LEFT_SIDEBAR_W + 10, tlY + 8, 54, 22, playBg);
    m_renderer.drawText(m_playing ? "Pause" : "Play", LEFT_SIDEBAR_W + 16, tlY + 13, 0.85f, Color::white());

    // Add / Delete / Duplicate / New-group frame buttons
    m_renderer.queueSolidRect(LEFT_SIDEBAR_W + 72, tlY + 8, 68, 22, Color(48, 48, 56, 255));
    m_renderer.drawText("+ Frame", LEFT_SIDEBAR_W + 78, tlY + 13, 0.85f, textC);

    m_renderer.queueSolidRect(LEFT_SIDEBAR_W + 148, tlY + 8, 68, 22, Color(48, 48, 56, 255));
    m_renderer.drawText("- Frame", LEFT_SIDEBAR_W + 154, tlY + 13, 0.85f, textC);

    m_renderer.queueSolidRect(LEFT_SIDEBAR_W + 224, tlY + 8, 50, 22, Color(48, 48, 56, 255));
    m_renderer.drawText("Dup", LEFT_SIDEBAR_W + 234, tlY + 13, 0.85f, textC);

    m_renderer.queueSolidRect(LEFT_SIDEBAR_W + 282, tlY + 8, 64, 22, Color(48, 48, 56, 255));
    m_renderer.drawText("+ Group", LEFT_SIDEBAR_W + 288, tlY + 13, 0.85f, textC);

    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    DrawingDocument& doc = c->document();

    // Playback-speed widget for the ACTIVE frame, right-aligned.
    {
        const Frame* af = doc.activeFrame();
        float mult = af ? af->duration() : 1.0f;
        char spd[16];
        snprintf(spd, sizeof(spd), "%.2gx", mult);
        m_renderer.drawText("Speed", fbW - 168.0f, tlY + 13, 0.85f, Color(150, 150, 160, 255));
        bool dec = mult > 0.251f;   // dim steppers at their clamp ends
        bool inc = mult < 3.999f;
        Color on(190, 190, 200, 255), off(95, 95, 105, 255);
        m_renderer.drawText("<", fbW - 118.0f, tlY + 13, 0.85f, dec ? on : off);
        m_renderer.drawText(spd, fbW - 100.0f, tlY + 13, 0.85f,
                            Color(235, 180, 60, 255));
        m_renderer.drawText(">", fbW - 52.0f, tlY + 13, 0.85f, inc ? on : off);
    }

    // Frame strip: group chips followed by their member tiles, then the
    // ungrouped frames. Layout order is organizational only.
    std::vector<TimelineItem> titems;
    buildTimelineItems(doc, titems);
    float fx = LEFT_SIDEBAR_W + 10.0f;
    float fy = tlY + TL_STRIP_Y;
    for (const TimelineItem& ti : titems) {
        if (ti.isHeader) {
            const DrawingDocument::FrameGroup& g = doc.getFrameGroup(ti.index);
            bool renaming = (m_renamingFrameGroupIndex == ti.index);
            float chipW = 90.0f;
            if (!renaming)
                chipW = tlGroupChipWidth(g.name.size());

            Color chipBg = Color(38, 38, 46, 255);
            m_renderer.queueSolidRect(fx, fy, chipW, 24, chipBg);
            Color gc((uint8_t)(g.color & 0xFF), (uint8_t)((g.color >> 8) & 0xFF),
                     (uint8_t)((g.color >> 16) & 0xFF), 255);
            m_renderer.queueSolidRect(fx, fy, 3, 24, gc);   // color accent
            m_renderer.drawText(g.collapsed ? "[+]" : "[-]", fx + 7, fy + 6, 0.85f, textC);

            if (renaming) {
                float bx = fx + 26.0f, by = fy + 2.0f;
                float bw = chipW - 26.0f, bh = 20.0f;
                Color eb(90, 140, 210, 255);
                m_renderer.queueSolidRect(bx, by, bw, bh, Color(16, 16, 22, 255));
                m_renderer.queueSolidRect(bx, by, bw, 1, eb);
                m_renderer.queueSolidRect(bx, by + bh - 1.0f, bw, 1, eb);
                m_renderer.queueSolidRect(bx, by, 1, bh, eb);
                m_renderer.queueSolidRect(bx + bw - 1.0f, by, 1, bh, eb);
                m_renderer.drawText(m_renameBuffer.c_str(), bx + 4.0f, by + 4.0f,
                                    RENAME_TEXT_SCALE, Color::white());
                if (std::fmod(m_uiClock, 1.0f) < 0.5f) {
                    float caretX = bx + 4.0f + (float)m_renameBuffer.size() *
                                                (float)FONT_CHAR_W * RENAME_TEXT_SCALE;
                    m_renderer.queueSolidRect(caretX, by + 3.0f, 1.0f, bh - 6.0f,
                                              Color(220, 220, 235, 255));
                }
            } else {
                std::string nm = g.name.substr(0, 14);
                m_renderer.drawText(nm.c_str(), fx + 28, fy + 6, 0.85f, textC);
                char cnt[16];
                snprintf(cnt, sizeof(cnt), "(%d)", (int)g.frameIndices.size());
                m_renderer.drawText(cnt, fx + chipW - 77.0f, fy + 6, 0.85f,
                                    Color(130, 130, 140, 255));
                // Group color swatch: click cycles the palette like layers
                m_renderer.queueSolidRect(fx + chipW - 47.0f, fy + 5.0f, 12.0f, 14.0f,
                                          Color(18, 18, 22, 255));
                m_renderer.queueSolidRect(fx + chipW - 46.0f, fy + 6.0f, 10.0f, 12.0f, gc);
                // Membership toggle: adds/removes the ACTIVE frame
                int act = doc.activeFrameIndex();
                bool member = (doc.findGroupForFrame(act) == ti.index);
                Color mb = member ? Color(70, 130, 80, 255) : Color(60, 60, 70, 255);
                m_renderer.queueSolidRect(fx + chipW - 31.0f, fy + 5.0f, 14.0f, 14.0f, mb);
                m_renderer.drawText(member ? "-" : "+", fx + chipW - 27.0f, fy + 6.0f,
                                    0.85f, Color::white());
                // Delete-group button: dissolves the group, frames survive
                m_renderer.queueSolidRect(fx + chipW - 15.0f, fy + 5.0f, 13.0f, 14.0f,
                                          Color(122, 52, 52, 255));
                m_renderer.drawText("x", fx + chipW - 12.0f, fy + 6.0f, 0.85f,
                                    Color::white());
            }
            fx += chipW + TL_GAP;
            continue;
        }

        int i = ti.index;
        bool isCur = (i == m_currentFrame);
        bool renamingF = (m_renamingFrameIndex == i);
        Color fBg = isCur ? Color(60, 110, 190, 255) : Color(42, 42, 48, 255);
        m_renderer.queueSolidRect(fx, fy, TL_TILE, TL_TILE, fBg);
        m_renderer.queueSolidRect(fx, fy, TL_TILE, 1, Color(60, 60, 70, 255));

        if (renamingF) {
            float bx = fx - 8.0f, by = fy + 2.0f;
            float bw = TL_TILE + 40.0f, bh = 20.0f;
            Color eb(90, 140, 210, 255);
            m_renderer.queueSolidRect(bx, by, bw, bh, Color(16, 16, 22, 255));
            m_renderer.queueSolidRect(bx, by, bw, 1, eb);
            m_renderer.queueSolidRect(bx, by + bh - 1.0f, bw, 1, eb);
            m_renderer.queueSolidRect(bx, by, 1, bh, eb);
            m_renderer.queueSolidRect(bx + bw - 1.0f, by, 1, bh, eb);
            m_renderer.drawText(m_renameBuffer.c_str(), bx + 3.0f, by + 4.0f,
                                RENAME_TEXT_SCALE, Color::white());
            if (std::fmod(m_uiClock, 1.0f) < 0.5f) {
                float caretX = bx + 3.0f + (float)m_renameBuffer.size() *
                                            (float)FONT_CHAR_W * RENAME_TEXT_SCALE;
                m_renderer.queueSolidRect(caretX, by + 3.0f, 1.0f, bh - 6.0f,
                                          Color(220, 220, 235, 255));
            }
        } else {
            const Frame* fr = doc.getFrame(i);
            const char* nm = fr ? fr->name() : "";
            if (nm[0]) {
                std::string shortNm = std::string(nm).substr(0, 6);
                m_renderer.drawText(shortNm.c_str(), fx + 2, fy + 12, 0.85f,
                                    isCur ? Color::white() : textC);
            } else {
                char fNum[16];
                snprintf(fNum, sizeof(fNum), "%d", i + 1);
                m_renderer.drawText(fNum, fx + 12, fy + 12, 0.9f,
                                    isCur ? Color::white() : textC);
            }
        }
        // Per-frame playback speed badge (hidden at the default pace)
        const Frame* fr = doc.getFrame(i);
        if (fr && (fr->duration() < 0.995f || fr->duration() > 1.005f)) {
            char bdg[8];
            snprintf(bdg, sizeof(bdg), "%.2gx", fr->duration());
            m_renderer.drawText(bdg, fx + TL_TILE - 25.0f, fy + TL_TILE - 11.0f, 0.7f,
                                Color(235, 180, 60, 255));
        }
        fx += TL_TILE + TL_GAP;
    }

    // Timeline drag-and-drop visual feedback
    if (m_tlDnd.active()) {
        const TlDndPlan& plan = m_tlDnd.plan();
        // Insertion line
        if (plan.lineX >= 0.0f) {
            m_renderer.queueSolidRect(plan.lineX, fy, 2.0f, TL_TILE,
                                      Color(90, 160, 220, 255));
        }
        // Group join highlight
        if (plan.highlightGroup >= 0) {
            // Find the chip position for this group
            float hx = LEFT_SIDEBAR_W + 10.0f;
            for (const TimelineItem& ti : titems) {
                if (ti.isHeader) {
                    const DrawingDocument::FrameGroup& hg = doc.getFrameGroup(ti.index);
                    float hw = tlGroupChipWidth(hg.name.size());
                    if (ti.index == plan.highlightGroup) {
                        m_renderer.queueSolidRect(hx, fy, hw, 24.0f,
                                                  Color(235, 180, 60, 80));
                        m_renderer.queueSolidRect(hx, fy, hw, 1.0f,
                                                  Color(235, 180, 60, 200));
                        m_renderer.queueSolidRect(hx, fy + 23.0f, hw, 1.0f,
                                                  Color(235, 180, 60, 200));
                        break;
                    }
                    hx += hw + TL_GAP;
                } else {
                    hx += TL_TILE + TL_GAP;
                }
            }
        }
        // Drag label near cursor
        Vec2 mp = m_mouse.position();
        const char* dragName = "";
        if (m_tlDnd.item() == TimelineDragDrop::Item::Frame) {
            int fi = m_tlDnd.frameIndex();
            if (fi >= 0 && fi < doc.frameCount()) {
                const Frame* fr = doc.getFrame(fi);
                dragName = fr && fr->name()[0] ? fr->name() : "";
            }
        } else if (m_tlDnd.item() == TimelineDragDrop::Item::Group) {
            int gi = m_tlDnd.groupIndex();
            if (gi >= 0 && gi < doc.frameGroupCount())
                dragName = doc.getFrameGroup(gi).name.c_str();
        }
        if (dragName[0]) {
            float lx = std::clamp(mp.x + 12.0f, LEFT_SIDEBAR_W + 10.0f, fbW - 100.0f);
            float ly = std::clamp(mp.y - 18.0f, tlY - 24.0f, tlY + TL_STRIP_Y);
            m_renderer.drawText(dragName, lx, ly, 0.85f, Color(235, 180, 60, 255));
        }
    }
}