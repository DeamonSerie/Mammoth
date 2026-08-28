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
    if (!m_inputQueue.empty()) drainInput();
    m_mainWindow.update(1.0f / 60.0f);
    m_mainWindow.render();
}

void Application::postInput(const Input::Event& ev) {
    std::lock_guard<std::mutex> lk(m_inputMutex);
    m_inputQueue.push_back(ev);
}

void Application::drainInput() {
    std::unique_lock<std::mutex> lk(m_inputMutex);
    while (!m_inputQueue.empty()) {
        Input::Event e = m_inputQueue.front();
        m_inputQueue.pop_front();
        lk.unlock();
        dispatchInput(e);
        lk.lock();
    }
}

void Application::requestQuit() {
#ifdef USE_SDL
    m_shouldQuit = true;
#else
    sapp_request_quit();
#endif
}

void Application::cleanup() {
    m_mainWindow.cleanup();
    DebugLog::log("[Application] Cleaned up");
    DebugLog::shutdown();
}

void Application::event(const sapp_event* ev) {
    if (!m_initialized) return;

    // Translate the sokol event into a backend-neutral Input::Event. The
    // vendored sokol version has no pen/pressure support, so mouse pressure
    // is left at 1.0; the SDL backend fills real pressure.
    Input::Event e;
    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_MOVE:    e.type = Input::Type::Move; break;
        case SAPP_EVENTTYPE_MOUSE_DOWN:    e.type = Input::Type::Down; break;
        case SAPP_EVENTTYPE_MOUSE_UP:      e.type = Input::Type::Up; break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:  e.type = Input::Type::Scroll;
                                           e.dx = ev->scroll_x; e.dy = ev->scroll_y; break;
        case SAPP_EVENTTYPE_RESIZED:       e.type = Input::Type::Resize;
                                           e.x = ev->framebuffer_width; e.y = ev->framebuffer_height; break;
        case SAPP_EVENTTYPE_KEY_DOWN:      e.type = Input::Type::KeyDown; break;
        case SAPP_EVENTTYPE_KEY_UP:        e.type = Input::Type::KeyUp; break;
        case SAPP_EVENTTYPE_CHAR:          e.type = Input::Type::Char; break;
        default: return;
    }
    e.x = ev->mouse_x; e.y = ev->mouse_y;
    e.dx = ev->mouse_dx; e.dy = ev->mouse_dy;
    e.button = (int)ev->mouse_button;
    e.pressure = 1.0f;
    e.key = (int)Input::keyFromSokol(ev->key_code);
    e.mods = Input::modsFromSokol(ev->modifiers);
    e.codepoint = ev->char_code;
    dispatchInput(e);
}

void Application::dispatchInput(const Input::Event& ev) {
    if (!m_initialized) return;

    // PRESSURE PIPELINE: pointer (mouse / pen / finger) routes straight to the
    // drawing handlers, carrying pen/finger pressure so sketching responds to a
    // real stylus. The pressure->stroke math itself lives in drawing/Pressure.hpp.
    if (ev.isMove()) {
        DebugLog::log("[Application] dispatch Move x=%.1f y=%.1f mods=%d", ev.x, ev.y, ev.mods);
        m_mainWindow.onMouseMove(ev.x, ev.y, ev.dx, ev.dy, ev.pressure, ev.mods);
        return;
    }
    if (ev.isPress()) {
        DebugLog::log("[Application] dispatch Press x=%.1f y=%.1f button=%d mods=%d", ev.x, ev.y, ev.button, ev.mods);
        m_mainWindow.onMouseButton(ev.x, ev.y, ev.button, true, ev.pressure, ev.mods);
        return;
    }
    if (ev.isRelease()) {
        DebugLog::log("[Application] dispatch Release x=%.1f y=%.1f button=%d mods=%d", ev.x, ev.y, ev.button, ev.mods);
        m_mainWindow.onMouseButton(ev.x, ev.y, ev.button, false, ev.pressure, ev.mods);
        return;
    }

    using Key = Input::Key;
    using Mod = Input::Mod;

    switch (ev.type) {
        case Input::Type::Scroll:
            // Keep mouse position in sync for isInCanvas check (wheel events carry mouse coords)
            if (ev.x != 0.0f || ev.y != 0.0f) {
                m_mainWindow.mouse().onMove(ev.x, ev.y, 0, 0);
            }
            m_mainWindow.onScroll(ev.dx, ev.dy, ev.mods);
            break;

        case Input::Type::Resize:
            m_mainWindow.onResize((int)ev.x, (int)ev.y);
            break;

        case Input::Type::KeyDown: {
            // Trace modified chords so swallowed/aliased shortcuts are visible
            // in the debug log (Ctrl/Alt/Super only; plain + Shift keys type).
            if (ev.mods & ((int)Mod::Ctrl | (int)Mod::Alt | (int)Mod::Super)) {
                DebugLog::log("[Input] key=%d mods=0x%x ctrl=%d shift=%d alt=%d",
                              ev.key, ev.mods,
                              (ev.mods & (int)Mod::Ctrl) != 0,
                              (ev.mods & (int)Mod::Shift) != 0,
                              (ev.mods & (int)Mod::Alt) != 0);
            }
            // While any inline rename is in progress, capture editing keys and
            // swallow all other shortcuts (Escape must cancel).
            if (m_mainWindow.anyRenameActive()) {
                switch ((Key)ev.key) {
                    case Key::Escape: m_mainWindow.cancelLayerRename(); break;
                    case Key::Enter:
                    case Key::KPEnter: m_mainWindow.commitLayerRename(); break;
                    case Key::Backspace: m_mainWindow.renameBackspace(); break;
                    default: break;
                }
                break;
            }
            if ((Key)ev.key == Key::Escape) {
                requestQuit();
            } else if ((Key)ev.key == Key::S && (ev.mods & (int)Mod::Ctrl)) {
                m_mainWindow.saveCurrentFrame();
            } else if ((Key)ev.key == Key::Z && (ev.mods & (int)Mod::Ctrl)) {
                if (ev.mods & (int)Mod::Shift) m_mainWindow.redo();
                else m_mainWindow.undo();
            } else if ((Key)ev.key == Key::Y && (ev.mods & (int)Mod::Ctrl)) {
                m_mainWindow.redo();
            } else if ((ev.mods & (int)Mod::Ctrl) && (ev.mods & (int)Mod::Shift) && (Key)ev.key == Key::Q) {
                m_mainWindow.scrollLayerUp();
            } else if ((ev.mods & (int)Mod::Ctrl) && (ev.mods & (int)Mod::Shift) && (Key)ev.key == Key::E) {
                m_mainWindow.scrollLayerDown();
            } else if ((ev.mods & (int)Mod::Ctrl) && (ev.mods & (int)Mod::Shift) && (Key)ev.key == Key::B) {
                m_mainWindow.grabActiveLayer();
            } else if ((ev.mods & (int)Mod::Ctrl) && ((Key)ev.key == Key::P || (Key)ev.key == Key::W)) {
                m_mainWindow.placeGrabbedLayer();
            } else if ((ev.mods & (int)Mod::Ctrl) && (ev.mods & (int)Mod::Shift) && (Key)ev.key == Key::G) {
                m_mainWindow.removeActiveFrameGroup();
            } else if ((Key)ev.key == Key::G && (ev.mods & (int)Mod::Ctrl)) {
                m_mainWindow.createLayerGroup();
            } else if ((Key)ev.key == Key::U && (ev.mods & (int)Mod::Ctrl)) {
                m_mainWindow.selectLayerAbove();
            } else if ((Key)ev.key == Key::B && (ev.mods & (int)Mod::Ctrl)) {
                m_mainWindow.selectLayerBelow();
            } else if ((ev.mods & (int)Mod::Ctrl) && (ev.mods & (int)Mod::Shift) &&
                       (Key)ev.key >= Key::Digit1 && (Key)ev.key <= Key::Digit9) {
                 m_mainWindow.setLayerTagColor((int)(Key)ev.key - (int)Key::Digit1);
            } else if ((ev.mods & (int)Mod::Ctrl) && (ev.mods & (int)Mod::Shift) && (Key)ev.key == Key::A) {
                m_mainWindow.createAttributeLayer();
            } else if ((ev.mods & (int)Mod::Ctrl) && (ev.mods & (int)Mod::Shift) && (Key)ev.key == Key::S) {
                m_mainWindow.cycleAttributeSource();
            } else if ((ev.mods & (int)Mod::Ctrl) && ((Key)ev.key == Key::Equal || (Key)ev.key == Key::KPAdd)) {
                m_mainWindow.createLayer();
            } else if ((ev.mods & (int)Mod::Ctrl) && ((Key)ev.key == Key::Minus || (Key)ev.key == Key::KPSubtract)) {
                m_mainWindow.deleteActiveLayer();
            } else if ((Key)ev.key == Key::P &&
                       !(ev.mods & ((int)Mod::Ctrl | (int)Mod::Alt | (int)Mod::Super)) &&
                       m_mainWindow.grabbedLayerValid()) {
                m_mainWindow.placeGrabbedLayer();
            } else {
                m_mainWindow.onKeyDown(ev.key);
            }
            break;
        }

        case Input::Type::KeyUp:
            m_mainWindow.onKeyUp(ev.key);
            break;

        case Input::Type::Char:
            m_mainWindow.onChar(ev.codepoint);
            break;

        default:
            break;
    }
}
