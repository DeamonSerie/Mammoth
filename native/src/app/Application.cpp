#include "Application.hpp"
#include "sokol_app.h"
#include <cstdio>

Application& Application::instance() {
    static Application app;
    return app;
}

void Application::init() {
    if (m_initialized) return;
    m_mainWindow.init();
    m_initialized = true;
    printf("[Application] Initialized\n");
}

void Application::frame() {
    if (!m_initialized) return;
    m_mainWindow.update(1.0f / 60.0f);
    m_mainWindow.render();
}

void Application::cleanup() {
    m_mainWindow.cleanup();
    printf("[Application] Cleaned up\n");
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
            if (ev->key_code == SAPP_KEYCODE_ESCAPE)
                sapp_request_quit();
            else if (ev->key_code == SAPP_KEYCODE_S && (ev->modifiers & SAPP_MODIFIER_CTRL))
                m_mainWindow.saveCurrentFrame();
            else
                m_mainWindow.onKeyDown(ev->key_code);
            break;

        default:
            break;
    }
}
