// SDL backend input translator. Owns the SDL window + OpenGL context and
// translates SDL events into backend-neutral Input::Event values that are fed
// to Application::dispatchInput(). Pressure-aware: stylus pressure arrives via
// SDL_EVENT_PEN_AXIS (SDL_PEN_AXIS_PRESSURE) and is tracked here so every
// pen move/down/up carries the live pressure. sokol_gfx renders into the GL
// context this class creates (foreign-context mode), keeping Mammoth portable.
#pragma once
#include "InputEvent.hpp"
#include <SDL3/SDL.h>
#include <optional>

// Window pixel size shared with the sokol glue shims (sapp_width/height,
// sglue_environment/swapchain) so the renderer matches the real backbuffer.
namespace SdlGl {
    extern int g_width;
    extern int g_height;
}

class SdlTranslator {
public:
    bool init(int width, int height);
    void shutdown();
    SDL_Window* window() const { return m_window; }

    // Translate one SDL event into a neutral Input::Event (or nullopt when the
    // event is not input we forward). Updates internal pen-pressure state.
    std::optional<Input::Event> translate(const SDL_Event& e);

    // Stateless core used by both translate() and the headless unit test.
    // pixW/H and logW/H are the backbuffer pixel size and logical window size
    // (for coord scaling); penPressure is carried across calls so pen axis
    // events update the live pressure. Returns true if an Input::Event was
    // produced (false for axis-only / non-forwarded events).
    static bool Translate(const SDL_Event& e,
                          int pixW, int pixH, int logW, int logH,
                          float& penPressure, Input::Event& out);

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glctx = nullptr;
    int m_width = 0, m_height = 0;       // backbuffer pixel size
    int m_logicalW = 0, m_logicalH = 0;  // logical window size
    float m_penPressure = 1.0f;   // last SDL_PEN_AXIS_PRESSURE value
};
