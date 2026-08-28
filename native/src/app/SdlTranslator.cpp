#include "SdlTranslator.hpp"
#include "../DebugLog.h"
#include <cstring>
#include <cmath>

namespace SdlGl {
    int g_width = 0;
    int g_height = 0;
}

// --- SDL key / modifier -> neutral mapping --------------------------------

static Input::Key keyFromSdl(SDL_Keycode k) {
    using Key = Input::Key;
    // NOTE: Input::Key letters are NOT contiguous (A,B,E,G,P,Q,S,U,W,Y,Z),
    // so map explicitly to match InputEvent.hpp::keyFromSokol.
    switch (k) {
        case SDLK_A: return Key::A;
        case SDLK_B: return Key::B;
        case SDLK_E: return Key::E;
        case SDLK_G: return Key::G;
        case SDLK_P: return Key::P;
        case SDLK_Q: return Key::Q;
        case SDLK_S: return Key::S;
        case SDLK_U: return Key::U;
        case SDLK_W: return Key::W;
        case SDLK_Y: return Key::Y;
        case SDLK_Z: return Key::Z;
        case SDLK_0: case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
        case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
            return (Key)((int)Key::Digit0 + (k - SDLK_0));
        case SDLK_ESCAPE:       return Key::Escape;
        case SDLK_RETURN:       return Key::Enter;
        case SDLK_KP_ENTER:     return Key::KPEnter;
        case SDLK_BACKSPACE:    return Key::Backspace;
        case SDLK_TAB:          return Key::Tab;
        case SDLK_SPACE:        return Key::Space;
        case SDLK_LEFT:         return Key::Left;
        case SDLK_RIGHT:        return Key::Right;
        case SDLK_UP:           return Key::Up;
        case SDLK_DOWN:         return Key::Down;
        case SDLK_EQUALS:       return Key::Equal;
        case SDLK_KP_PLUS:      return Key::KPAdd;
        case SDLK_MINUS:        return Key::Minus;
        case SDLK_KP_MINUS:     return Key::KPSubtract;
        default:                return Key::NoKey;
    }
}

static int modsFromSdl(SDL_Keymod m) {
    using Mod = Input::Mod;
    int r = 0;
    if (m & SDL_KMOD_SHIFT) r |= (int)Mod::Shift;
    if (m & SDL_KMOD_CTRL)  r |= (int)Mod::Ctrl;
    if (m & SDL_KMOD_ALT)   r |= (int)Mod::Alt;
    if (m & SDL_KMOD_GUI)   r |= (int)Mod::Super;
    return r;
}

static uint32_t firstCodepoint(const char* s) {
    if (!s || !*s) return 0;
    // ASCII fast path; otherwise decode the first UTF-8 sequence.
    if ((unsigned char)s[0] < 0x80) return (uint32_t)(unsigned char)s[0];
    uint32_t cp = 0;
    int n = 0;
    unsigned char b = (unsigned char)s[0];
    if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; n = 1; }
    else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; n = 2; }
    else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; n = 3; }
    else return 0;
    for (int i = 0; i < n && s[1 + i]; i++)
        cp = (cp << 6) | ((unsigned char)s[1 + i] & 0x3F);
    return cp;
}

// --- SdlTranslator ----------------------------------------------------------

bool SdlTranslator::init(int width, int height) {
    // SDL3's SDL_Init returns `true` on SUCCESS (unlike SDL2's 0).
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DebugLog::log("[SDL] SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    // sokol's GL backend uses Direct-State-Access (glCreateBuffers etc.,
    // GL 4.5 core), so we MUST request a 4.5+ core profile context.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    m_window = SDL_CreateWindow("Mammoth", width, height, SDL_WINDOW_OPENGL);
    if (!m_window) {
        DebugLog::log("[SDL] SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    m_glctx = SDL_GL_CreateContext(m_window);
    if (!m_glctx) {
        DebugLog::log("[SDL] SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(m_window, m_glctx);
    SDL_GL_SetSwapInterval(1);

    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(m_window, &pw, &ph);
    m_width = pw; m_height = ph;
    SdlGl::g_width = pw; SdlGl::g_height = ph;

    int lw = 0, lh = 0;
    SDL_GetWindowSize(m_window, &lw, &lh);
    m_logicalW = lw; m_logicalH = lh;

    DebugLog::log("[SDL] Window + GL 4.5 context created (%dx%d)", pw, ph);
    return true;
}

void SdlTranslator::shutdown() {
    if (m_glctx) SDL_GL_DestroyContext(m_glctx);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

std::optional<Input::Event> SdlTranslator::translate(const SDL_Event& e) {
    // Resize must update our cached sizes (and the sokol glue globals) before
    // producing the Input::Resize event.
    if (e.type == SDL_EVENT_WINDOW_RESIZED ||
        e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(m_window, &pw, &ph);
        m_width = pw; m_height = ph;
        SdlGl::g_width = pw; SdlGl::g_height = ph;
        Input::Event out;
        out.type = Input::Type::Resize;
        out.x = (float)pw; out.y = (float)ph;
        return out;
    }

    Input::Event out;
    if (Translate(e, m_width, m_height, m_logicalW, m_logicalH, m_penPressure, out))
        return out;
    return std::nullopt;
}

bool SdlTranslator::Translate(const SDL_Event& e,
                              int pixW, int pixH, int logW, int logH,
                              float& penPressure, Input::Event& out) {
    // Convert logical window coords -> backbuffer pixels to match sapp_width().
    float sx = logW ? (float)pixW / (float)logW : 1.0f;
    float sy = logH ? (float)pixH / (float)logH : 1.0f;

    out.pressure = 1.0f;
    out.pointer = Input::Pointer::Mouse;
    out.mods = modsFromSdl(SDL_GetModState());

    switch (e.type) {
        case SDL_EVENT_MOUSE_MOTION:
            out.type = Input::Type::Move;
            out.x = e.motion.x * sx; out.y = e.motion.y * sy;
            out.dx = e.motion.xrel * sx; out.dy = e.motion.yrel * sy;
            return true;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            out.type = Input::Type::Down;
            out.button = (int)e.button.button - 1;   // SDL 1..3 -> 0..2
            out.x = e.button.x * sx; out.y = e.button.y * sy;
            return true;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            out.type = Input::Type::Up;
            out.button = (int)e.button.button - 1;
            out.x = e.button.x * sx; out.y = e.button.y * sy;
            return true;

        case SDL_EVENT_MOUSE_WHEEL: {
            out.type = Input::Type::Scroll;
            // Handle natural scroll direction (FLIPPED) by inverting
            float wx = e.wheel.x;
            float wy = e.wheel.y;
            if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                wx = -wx;
                wy = -wy;
            }
            // SDL 3.2.12+ provides integer_x/y for whole ticks; fall back if precise x/y are 0
            if (wx == 0.0f && wy == 0.0f && (e.wheel.integer_x != 0 || e.wheel.integer_y != 0)) {
                wx = (float)e.wheel.integer_x;
                wy = (float)e.wheel.integer_y;
            }
            out.dx = wx; out.dy = wy;
            out.x = e.wheel.mouse_x * sx; out.y = e.wheel.mouse_y * sy;
            DebugLog::log("[SdlTranslator] wheel dx=%.2f dy=%.2f x=%.1f y=%.1f dir=%d", wx, wy, out.x, out.y, (int)e.wheel.direction);
            return true;
        }

        case SDL_EVENT_PEN_AXIS:
            if (e.paxis.axis == SDL_PEN_AXIS_PRESSURE)
                penPressure = e.paxis.value;
            return false;   // axis updates are not discrete input events

        case SDL_EVENT_PEN_MOTION:
            out.type = Input::Type::PenMove;
            out.pointer = Input::Pointer::Pen;
            out.pressure = penPressure;
            out.x = e.pmotion.x * sx; out.y = e.pmotion.y * sy;
            return true;

        case SDL_EVENT_PEN_DOWN:
            out.type = Input::Type::PenDown;
            out.pointer = Input::Pointer::Pen;
            out.pressure = penPressure;
            out.x = e.ptouch.x * sx; out.y = e.ptouch.y * sy;
            return true;

        case SDL_EVENT_PEN_UP:
            out.type = Input::Type::PenUp;
            out.pointer = Input::Pointer::Pen;
            out.pressure = penPressure;
            out.x = e.ptouch.x * sx; out.y = e.ptouch.y * sy;
            return true;

        case SDL_EVENT_FINGER_DOWN:
            out.type = Input::Type::FingerDown;
            out.pointer = Input::Pointer::Touch;
            out.pressure = e.tfinger.pressure > 0.0f ? e.tfinger.pressure : 1.0f;
            out.x = e.tfinger.x * (float)pixW;
            out.y = e.tfinger.y * (float)pixH;
            return true;

        case SDL_EVENT_FINGER_MOTION:
            out.type = Input::Type::FingerMove;
            out.pointer = Input::Pointer::Touch;
            out.pressure = e.tfinger.pressure > 0.0f ? e.tfinger.pressure : 1.0f;
            out.x = e.tfinger.x * (float)pixW;
            out.y = e.tfinger.y * (float)pixH;
            return true;

        case SDL_EVENT_FINGER_UP:
            out.type = Input::Type::FingerUp;
            out.pointer = Input::Pointer::Touch;
            out.pressure = e.tfinger.pressure > 0.0f ? e.tfinger.pressure : 1.0f;
            out.x = e.tfinger.x * (float)pixW;
            out.y = e.tfinger.y * (float)pixH;
            return true;

        case SDL_EVENT_KEY_DOWN:
            out.type = Input::Type::KeyDown;
            out.key = (int)keyFromSdl(e.key.key);
            out.mods = modsFromSdl(e.key.mod);
            return true;

        case SDL_EVENT_KEY_UP:
            out.type = Input::Type::KeyUp;
            out.key = (int)keyFromSdl(e.key.key);
            out.mods = modsFromSdl(e.key.mod);
            return true;

        case SDL_EVENT_TEXT_INPUT:
            out.type = Input::Type::Char;
            out.codepoint = firstCodepoint(e.text.text);
            return true;

        default:
            return false;
    }
}
