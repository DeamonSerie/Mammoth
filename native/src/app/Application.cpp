#include "Application.hpp"
#include "sokol_app.h"
#include "../DebugLog.h"
#include <cstdio>

Application& Application::instance() {
    static Application app;
    return app;
}

void Application::init() {
    if (m_initialized) return;
    DebugLog::init();
    DebugLog::log("[Application] Initializing...");
    m_mainWindow.init();
    m_initialized = true;
    DebugLog::log("[Application] Initialized");
}

void Application::frame() {
    if (!m_initialized) return;
    m_mainWindow.update(1.0f / 60.0f);
    m_mainWindow.render();
}

void Application::cleanup() {
    m_mainWindow.cleanup();
    DebugLog::log("[Application] Cleaned up");
    DebugLog::shutdown();
}

void Application::event(const sapp_event* ev) {
    if (!m_initialized) return;

    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            m_mainWindow.onMouseMove(ev->mouse_x, ev->mouse_y, ev->mouse_dx, ev->mouse_dy);
            break;

        case SAPP_EVENTTYPE_MOUSE_DOWN:
            m_mainWindow.onMouseButton(ev->mouse_x, ev->mouse_y, (int)ev->mouse_button, true);
            break;

        case SAPP_EVENTTYPE_MOUSE_UP:
            m_mainWindow.onMouseButton(ev->mouse_x, ev->mouse_y, (int)ev->mouse_button, false);
            break;

        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            m_mainWindow.onScroll(ev->scroll_x, ev->scroll_y);
            break;

        case SAPP_EVENTTYPE_RESIZED:
            m_mainWindow.onResize(ev->framebuffer_width, ev->framebuffer_height);
            break;

        case SAPP_EVENTTYPE_KEY_DOWN:
            // Trace modified chords so swallowed/aliased shortcuts are visible
            // in the debug log (Ctrl/Alt/Super only; plain + Shift keys type).
            if (ev->modifiers & (SAPP_MODIFIER_CTRL | SAPP_MODIFIER_ALT |
                                 SAPP_MODIFIER_SUPER)) {
                DebugLog::log("[Input] key=%d mods=0x%x ctrl=%d shift=%d alt=%d",
                              (int)ev->key_code, (int)ev->modifiers,
                              (int)(ev->modifiers & SAPP_MODIFIER_CTRL) != 0,
                              (int)(ev->modifiers & SAPP_MODIFIER_SHIFT) != 0,
                              (int)(ev->modifiers & SAPP_MODIFIER_ALT) != 0);
            }
            // While a layer rename is in progress, capture editing keys and
            // swallow all other shortcuts (Escape must cancel, not quit).
            if (m_mainWindow.layerRenameActive()) {
                switch (ev->key_code) {
                    case SAPP_KEYCODE_ESCAPE:
                        m_mainWindow.cancelLayerRename();
                        break;
                    case SAPP_KEYCODE_ENTER:
                    case SAPP_KEYCODE_KP_ENTER:
                        m_mainWindow.commitLayerRename();
                        break;
                    case SAPP_KEYCODE_BACKSPACE:
                        m_mainWindow.renameBackspace();
                        break;
                    default:
                        break;
                }
                break;
            }
            if (ev->key_code == SAPP_KEYCODE_ESCAPE) {
                sapp_request_quit();
            } else if (ev->key_code == SAPP_KEYCODE_S && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
                m_mainWindow.saveCurrentFrame();
            } else if (ev->key_code == SAPP_KEYCODE_Z && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
                if (ev->modifiers & SAPP_MODIFIER_SHIFT) {
                    m_mainWindow.redo();
                } else {
                    m_mainWindow.undo();
                }
            } else if (ev->key_code == SAPP_KEYCODE_Y && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
                m_mainWindow.redo();
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) && (ev->modifiers & SAPP_MODIFIER_SHIFT) &&
                       ev->key_code == SAPP_KEYCODE_Q) {
                // Must be matched before the plain-Ctrl branch below
                m_mainWindow.scrollLayerUp();
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) && (ev->modifiers & SAPP_MODIFIER_SHIFT) &&
                       ev->key_code == SAPP_KEYCODE_E) {
                m_mainWindow.scrollLayerDown();
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) && (ev->modifiers & SAPP_MODIFIER_SHIFT) &&
                       ev->key_code == SAPP_KEYCODE_B) {
                // Grab the active layer; matched before the plain-Ctrl+B
                // layer-navigation branch below.
                m_mainWindow.grabActiveLayer();
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) &&
                       (ev->key_code == SAPP_KEYCODE_P ||
                        ev->key_code == SAPP_KEYCODE_W)) {
                // Place the grabbed layer. Shift is deliberately optional:
                // keyboards sometimes drop Shift before a far-right key like P
                // registers, which used to land here as bare Ctrl+P and
                // silently switch to the eyedropper. W covers that same roll.
                m_mainWindow.placeGrabbedLayer();
            } else if (ev->key_code == SAPP_KEYCODE_G && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
                m_mainWindow.createLayerGroup();
            } else if (ev->key_code == SAPP_KEYCODE_U && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
                m_mainWindow.selectLayerAbove();
            } else if (ev->key_code == SAPP_KEYCODE_B && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
                m_mainWindow.selectLayerBelow();
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) && (ev->modifiers & SAPP_MODIFIER_SHIFT) &&
                       ev->key_code >= SAPP_KEYCODE_1 && ev->key_code <= SAPP_KEYCODE_9) {
                m_mainWindow.setLayerTagColor((int)(ev->key_code - SAPP_KEYCODE_1));
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) && (ev->modifiers & SAPP_MODIFIER_SHIFT) &&
                       ev->key_code == SAPP_KEYCODE_A) {
                m_mainWindow.createAttributeLayer();
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) && (ev->modifiers & SAPP_MODIFIER_SHIFT) &&
                       ev->key_code == SAPP_KEYCODE_S) {
                m_mainWindow.cycleAttributeSource();
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) &&
                       (ev->key_code == SAPP_KEYCODE_EQUAL || ev->key_code == SAPP_KEYCODE_KP_ADD)) {
                m_mainWindow.createLayer();
            } else if ((ev->modifiers & SAPP_MODIFIER_CTRL) &&
                       (ev->key_code == SAPP_KEYCODE_MINUS || ev->key_code == SAPP_KEYCODE_KP_SUBTRACT)) {
                m_mainWindow.deleteActiveLayer();
            } else if (ev->key_code == SAPP_KEYCODE_P &&
                       !(ev->modifiers & (SAPP_MODIFIER_CTRL | SAPP_MODIFIER_ALT |
                                          SAPP_MODIFIER_SUPER)) &&
                       m_mainWindow.grabbedLayerValid()) {
                // Plain P places while a grab is pending: keyboards with
                // 2-key rollover drop P entirely from Ctrl+Shift+P combos
                // (the trace showed zero P events arriving), so the chord
                // can't be relied on. Without a grab, P stays the eyedropper.
                m_mainWindow.placeGrabbedLayer();
            } else {
                m_mainWindow.onKeyDown(ev->key_code);
            }
            break;

        case SAPP_EVENTTYPE_CHAR:
            m_mainWindow.onChar(ev->char_code);
            break;

        default:
            break;
    }
}
