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

void MainWindow::selectLayerAbove() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer()) return;
    int idx = 0;
    for (int i = 0; i < f->layerCount(); i++) {
        if (f->getLayer(i) == f->activeLayer()) { idx = i; break; }
    }
    if (idx + 1 < f->layerCount())
        f->setActiveLayer(idx + 1);
}

void MainWindow::selectLayerBelow() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer()) return;
    int idx = 0;
    for (int i = 0; i < f->layerCount(); i++) {
        if (f->getLayer(i) == f->activeLayer()) { idx = i; break; }
    }
    if (idx - 1 >= 0)
        f->setActiveLayer(idx - 1);
}

void MainWindow::scrollLayerUp() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || f->layerCount() == 0) return;
    int idx = 0;
    for (int i = 0; i < f->layerCount(); i++) {
        if (f->getLayer(i) == f->activeLayer()) { idx = i; break; }
    }
    f->setActiveLayer((idx + 1) % f->layerCount());
}

void MainWindow::scrollLayerDown() {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || f->layerCount() == 0) return;
    int n = f->layerCount();
    int idx = 0;
    for (int i = 0; i < n; i++) {
        if (f->getLayer(i) == f->activeLayer()) { idx = i; break; }
    }
    f->setActiveLayer((idx - 1 + n) % n);
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
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || !f->activeLayer()) return;
    int activeIdx = 0;
    for (int i = 0; i < f->layerCount(); i++) {
        if (f->getLayer(i) == f->activeLayer()) { activeIdx = i; break; }
    }
    pushUndo();
    f->removeLayer(activeIdx);
}

// ---- Inline layer rename -----------------------------------------------------

void MainWindow::startLayerRename(int index) {
    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* f = c->document().activeFrame();
    if (!f || index < 0 || index >= f->layerCount()) return;
    Layer* l = f->getLayer(index);
    if (!l) return;
    m_renamingLayerIndex = index;
    m_renameBuffer = l->name();
}

void MainWindow::commitLayerRename() {
    if (m_renamingLayerIndex < 0) return;
    std::string trimmed = trimName(m_renameBuffer);
    Canvas* c = m_canvasManager.activeCanvas();
    if (c && !trimmed.empty()) {
        Frame* f = c->document().activeFrame();
        if (f) f->renameLayer(m_renamingLayerIndex, trimmed.c_str());
    }
    m_renamingLayerIndex = -1;
    m_renameBuffer.clear();
}

void MainWindow::cancelLayerRename() {
    m_renamingLayerIndex = -1;
    m_renameBuffer.clear();
}

void MainWindow::renameBackspace() {
    if (m_renamingLayerIndex < 0) return;
    // Input is restricted to ASCII, so popping one byte == one character
    if (!m_renameBuffer.empty()) m_renameBuffer.pop_back();
}

void MainWindow::onChar(uint32_t code) {
    if (m_renamingLayerIndex < 0) return;
    // The bitmap font atlas only covers printable ASCII
    if (code < 32 || code >= 127) return;
    if (m_renameBuffer.size() >= RENAME_MAX_CHARS) return;
    m_renameBuffer.push_back((char)code);
}

void MainWindow::onMouseMove(float x, float y, float dx, float dy) {
    m_mouse.onMove(x, y, dx, dy);

    if (m_drawing) {
        if (m_activeTool == Tool::Brush || m_activeTool == Tool::Eraser) {
            handleDrawing();
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
        if (m_activeTool == Tool::Eraser) {
            m_eraserOpacity = std::clamp((x - sx) / sw, 0.05f, 1.0f);
            syncEraserOpacity();
        } else {
            m_brushOpacity = std::clamp((x - sx) / sw, 0.05f, 1.0f);
            syncBrushOpacity();
        }
    } else if (m_customDrag != CustomDrag::None) {
        handleCustomBrushDrag(x);
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

void MainWindow::onMouseButton(float x, float y, int button, bool pressed) {
    m_mouse.onMove(x, y, 0, 0);
    m_mouse.onButton(button, pressed);

    if (!pressed) {
        if (m_moveTool.isMoving()) {
            Canvas* c = m_canvasManager.activeCanvas();
            if (c) {
                Frame* f = c->document().activeFrame();
                if (f && f->activeLayer()) {
                    m_moveTool.end(*f->activeLayer());
                    f->activeLayer()->setDirty();
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
    if (m_renamingLayerIndex >= 0) {
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
            if (c) c->setZoom(std::min(32.0f, c->zoom() * 1.25f));
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
        if (x >= panelX + 10.0f && x <= panelX + 90.0f && y >= btnY && y <= btnY + btnH) {
            createLayer();
            return;
        }
        // [- Layer] button
        if (x >= panelX + 100.0f && x <= panelX + 180.0f && y >= btnY && y <= btnY + btnH) {
            deleteActiveLayer();
            return;
        }

        // Layer item clicks
        float itemY = 68.0f;
        for (int i = frame->layerCount() - 1; i >= 0; i--) {
            Layer* l = frame->getLayer(i);
            if (!l) continue;
            // Visibility toggle icon (x range must mirror the indent used when
            // rendering attribute layers, which grows with nesting depth)
            float visInd = l->isAttributeLayer()
                               ? ATTR_ROW_INDENT * (float)frame->attributeChainDepth(i)
                               : 0.0f;
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
            // Reorder arrows (checked before select - they sit inside the row)
            if (y >= itemY && y <= itemY + 24.0f && x >= panelX + LAYER_PANEL_W - 50.0f) {
                // Up arrow: move toward the top of the list = higher Z-order
                if (x >= panelX + LAYER_PANEL_W - 46.0f && x <= panelX + LAYER_PANEL_W - 32.0f) {
                    frame->reorderLayer(i, i + 1);
                    return;
                }
                // Down arrow: lower Z-order
                if (x >= panelX + LAYER_PANEL_W - 30.0f && x <= panelX + LAYER_PANEL_W - 16.0f) {
                    frame->reorderLayer(i, i - 1);
                    return;
                }
            }
            // Select layer (double-click on the same row starts an inline rename)
            if (x >= panelX + 36.0f && x <= panelX + LAYER_PANEL_W - 10.0f && y >= itemY && y <= itemY + 24.0f) {
                auto now = std::chrono::steady_clock::now();
                double msSince = std::chrono::duration<double, std::milli>(now - m_lastRowClick).count();
                bool isDoubleClick = (m_lastClickedLayer == i) && (msSince < 400.0);
                m_lastRowClick = now;
                m_lastClickedLayer = isDoubleClick ? -1 : i;
                frame->setActiveLayer(i);
                if (isDoubleClick) {
                    startLayerRename(i);
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

        float tlY = fbH - TIMELINE_H;
        float btnY = tlY + 8.0f;
        float btnH = 22.0f;

        // Play/Pause button
        if (x >= LEFT_SIDEBAR_W + 10.0f && x <= LEFT_SIDEBAR_W + 64.0f && y >= btnY && y <= btnY + btnH) {
            m_playing = !m_playing;
            if (m_playing) { m_playTimer = 0.0f; }
            return;
        }

        // [+ Frame] button
        if (x >= LEFT_SIDEBAR_W + 72.0f && x <= LEFT_SIDEBAR_W + 140.0f && y >= btnY && y <= btnY + btnH) {
            pushUndo();
            c->document().addFrame();
            m_currentFrame = c->document().frameCount() - 1;
            c->document().setActiveFrame(m_currentFrame);
            return;
        }

        // [- Frame] button
        if (x >= LEFT_SIDEBAR_W + 148.0f && x <= LEFT_SIDEBAR_W + 216.0f && y >= btnY && y <= btnY + btnH) {
            pushUndo();
            c->document().removeFrame(m_currentFrame);
            m_currentFrame = c->document().activeFrameIndex();
            return;
        }

        // [Dup] button
        if (x >= LEFT_SIDEBAR_W + 224.0f && x <= LEFT_SIDEBAR_W + 274.0f && y >= btnY && y <= btnY + btnH) {
            pushUndo();
            c->document().duplicateFrame(m_currentFrame);
            m_currentFrame = c->document().activeFrameIndex();
            return;
        }

        // Frame thumbnail / box clicks
        float fx = LEFT_SIDEBAR_W + 10.0f;
        float fy = tlY + 36.0f;
        float fSize = 36.0f;
        for (int i = 0; i < c->document().frameCount(); i++) {
            if (x >= fx && x <= fx + fSize && y >= fy && y <= fy + fSize) {
                m_currentFrame = i;
                c->document().setActiveFrame(i);
                return;
            }
            fx += fSize + 6.0f;
        }
        return;
    }

    // 4b. Custom brush editor window (floats over the canvas)
    if (m_customWindowOpen && handleCustomWindowPress(x, y)) {
        return;
    }

    // 5. Click in Canvas area
    if (isInCanvas(x, y)) {
        switch (m_activeTool) {
            case Tool::Brush:
                pushUndo();
                m_drawing = true;
                m_lastBrushPos = {-1, -1};
                handleDrawing();
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
                handleDrawing();
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
                    m_moveTool.begin(*f->activeLayer(), *cv, cr, x, y, selRect);
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

void MainWindow::onScroll(float x, float y) {
    m_mouse.onScroll(x, y);

    // Scrolling over the editor window must not zoom the canvas behind it.
    if (m_customWindowOpen && hit(cwRect(), x, y)) return;

    Canvas* canvas = m_canvasManager.activeCanvas();
    if (!canvas) return;

    float mx = m_mouse.position().x;
    float my = m_mouse.position().y;
    if (!isInCanvas(mx, my)) return;

    Rect cr = canvasRect();
    float cx = mx - cr.x;
    float cy = my - cr.y;

    float oldZoom = canvas->zoom();
    float factor = (y > 0) ? 1.15f : (y < 0) ? 0.85f : 1.0f;
    float newZoom = std::clamp(oldZoom * factor, 0.2f, 32.0f);

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
}

void MainWindow::onResize(int fbW, int fbH) {
    m_framebufferWidth = fbW;
    m_framebufferHeight = fbH;
}

void MainWindow::handleDrawing() {
    Canvas* canvas = m_canvasManager.activeCanvas();
    if (!canvas) return;
    Frame* frame = canvas->document().activeFrame();
    if (!frame) return;
    Layer* layer = frame->activeLayer();
    if (!layer || !layer->visible()) return;

    Rect cr = canvasRect();
    Vec2 canvasPos = canvas->screenToCanvas(
        m_mouse.position().x - cr.x, m_mouse.position().y - cr.y,
        cr.w, cr.h);

    auto stampAt = [&](float px, float py) {
        m_brushEngine.applyStamp(*layer, px, py, m_brush);
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
                    if (m_rectSelectTool.hasSelection()) {
                        Rect cr = canvasRect();
                        m_rectSelectTool.deleteSelected(*f->activeLayer(), *c, cr);
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
            if (c) c->setZoom(std::min(32.0f, c->zoom() * 1.25f));
            break;
        }
        default: break;
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

    if (m_playing) {
        m_playTimer += dt;
        float frameDur = 1.0f / m_fps;
        if (m_playTimer >= frameDur) {
            m_playTimer -= frameDur;
            Canvas* c = m_canvasManager.activeCanvas();
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
                0.0f, 0.0f, (float)canvas->document().width() / 16.0f, (float)canvas->document().height() / 16.0f);
        }

        // Draw composite frame buffer on top of checkerboard
        if (!canvas->compositeBuffer().empty()) {
            m_canvasTex.update(canvas->compositeBuffer(),
                              canvas->compositeWidth(), canvas->compositeHeight());
            if (m_canvasTex.valid) {
                m_renderer.queueQuad(screenX, screenY, cw, ch,
                    m_canvasTex.image, m_canvasTex.view, m_canvasTex.sampler);
            }
        }

        // Canvas border outline
        m_renderer.queueSolidRect(screenX - 1, screenY - 1, cw + 2, 1, Color(100, 100, 115, 255));
        m_renderer.queueSolidRect(screenX - 1, screenY + ch, cw + 2, 1, Color(100, 100, 115, 255));
        m_renderer.queueSolidRect(screenX - 1, screenY, 1, ch, Color(100, 100, 115, 255));
        m_renderer.queueSolidRect(screenX + cw, screenY, 1, ch, Color(100, 100, 115, 255));

        // Selection rectangle outline if selecting or has selection
        m_rectSelectTool.render(m_renderer);

        // Brush cursor indicator on canvas
        float mx = m_mouse.position().x;
        float my = m_mouse.position().y;
        if (isInCanvas(mx, my) && (m_activeTool == Tool::Brush || m_activeTool == Tool::Eraser)) {
            float cursorSize = (m_activeTool == Tool::Eraser) ? m_eraserSize : m_brushSize;
            float bRad = (cursorSize * 0.5f) * canvas->zoom();
            Color curC = (m_activeTool == Tool::Eraser) ? Color(255, 100, 100, 180) : Color(255, 255, 255, 180);
            m_renderer.queueSolidRect(mx - bRad, my - bRad, bRad * 2, 1, curC);
            m_renderer.queueSolidRect(mx - bRad, my + bRad, bRad * 2, 1, curC);
            m_renderer.queueSolidRect(mx - bRad, my - bRad, 1, bRad * 2, curC);
            m_renderer.queueSolidRect(mx + bRad, my - bRad, 1, bRad * 2, curC);
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

    // Opacity Slider
    float opY = szY + 28.0f;
    float activeOpacity = (m_activeTool == Tool::Eraser) ? m_eraserOpacity : m_brushOpacity;
    char opBuf[32];
    snprintf(opBuf, sizeof(opBuf), "Opacity: %d%%", (int)(activeOpacity * 100));
    m_renderer.drawText(opBuf, sx, opY - 12, 0.8f, textC);
    m_renderer.queueSolidRect(sx, opY, sw, 8, Color(24, 24, 28, 255));
    m_renderer.queueSolidRect(sx, opY, sw * activeOpacity, 8, Color(60, 110, 180, 255));
    m_renderer.queueSolidRect(sx + sw * activeOpacity - 3, opY - 2, 6, 12, Color::white());

    // Current color swatch
    float swatchY = opY + 24.0f;
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

    // Layer action buttons
    m_renderer.queueSolidRect(panelX + 10, 38, 80, 22, Color(48, 48, 54, 255));
    m_renderer.drawText("+ Layer", panelX + 18, 43, 0.85f, textC);

    m_renderer.queueSolidRect(panelX + 100, 38, 80, 22, Color(48, 48, 54, 255));
    m_renderer.drawText("- Layer", panelX + 18 + 90, 43, 0.85f, textC);

    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;
    Frame* frame = c->document().activeFrame();
    if (!frame) return;

    float itemY = 68.0f;
    for (int i = frame->layerCount() - 1; i >= 0; i--) {
        Layer* l = frame->getLayer(i);
        if (!l) continue;
        bool isActive = (l == frame->activeLayer());
        Color itemBg = isActive ? Color(55, 95, 150, 255) : Color(42, 42, 48, 255);

        float ind = l->isAttributeLayer()
                        ? ATTR_ROW_INDENT * (float)frame->attributeChainDepth(i)
                        : 0.0f;
        m_renderer.queueSolidRect(panelX + 10 + ind, itemY, LAYER_PANEL_W - 20 - ind, 24, itemBg);

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

        // Reorder arrows: up = toward the top of the list = higher Z-order
        {
            bool canUp = (i < frame->layerCount() - 1);
            bool canDown = (i > 0);
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

    // Add / Delete / Duplicate frame buttons
    m_renderer.queueSolidRect(LEFT_SIDEBAR_W + 72, tlY + 8, 68, 22, Color(48, 48, 56, 255));
    m_renderer.drawText("+ Frame", LEFT_SIDEBAR_W + 78, tlY + 13, 0.85f, textC);

    m_renderer.queueSolidRect(LEFT_SIDEBAR_W + 148, tlY + 8, 68, 22, Color(48, 48, 56, 255));
    m_renderer.drawText("- Frame", LEFT_SIDEBAR_W + 154, tlY + 13, 0.85f, textC);

    m_renderer.queueSolidRect(LEFT_SIDEBAR_W + 224, tlY + 8, 50, 22, Color(48, 48, 56, 255));
    m_renderer.drawText("Dup", LEFT_SIDEBAR_W + 234, tlY + 13, 0.85f, textC);

    Canvas* c = m_canvasManager.activeCanvas();
    if (!c) return;

    // Frame strip
    float fx = LEFT_SIDEBAR_W + 10.0f;
    float fy = tlY + 36.0f;
    float fSize = 36.0f;
    for (int i = 0; i < c->document().frameCount(); i++) {
        bool isCur = (i == m_currentFrame);
        Color fBg = isCur ? Color(60, 110, 190, 255) : Color(42, 42, 48, 255);
        m_renderer.queueSolidRect(fx, fy, fSize, fSize, fBg);
        m_renderer.queueSolidRect(fx, fy, fSize, 1, Color(60, 60, 70, 255));

        char fNum[16];
        snprintf(fNum, sizeof(fNum), "%d", i + 1);
        m_renderer.drawText(fNum, fx + 12, fy + 12, 0.9f, isCur ? Color::white() : textC);
        fx += fSize + 6.0f;
    }
}