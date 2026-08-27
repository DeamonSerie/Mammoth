// Headless unit tests for the SDL -> Input::Event translation. These do NOT
// need a display: we construct SDL_Event structs in memory and call the
// stateless SdlTranslator::Translate(), which never touches the SDL API.
#include "../src/app/SdlTranslator.hpp"
#include <SDL3/SDL.h>
#include <cstring>
#include <cstdio>

static int g_fail = 0;
static int g_pass = 0;

static void check(bool cond, const char* msg) {
    if (cond) { g_pass++; }
    else { g_fail++; printf("FAIL: %s\n", msg); }
}

#define CHECK(c) check((c), #c)

static SDL_Event ev() { SDL_Event e; std::memset(&e, 0, sizeof(e)); return e; }

int main() {
    const int pixW = 1280, pixH = 800, logW = 1280, logH = 800; // scale 1.0
    float pen = 1.0f;
    Input::Event out;

    // --- mouse ---
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        e.button.button = 1;            // left -> button 0
        e.button.x = 100; e.button.y = 50;
        bool ok = SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(ok);
        CHECK(out.type == Input::Type::Down);
        CHECK(out.button == 0);
        CHECK(out.pointer == Input::Pointer::Mouse);
        CHECK(out.x == 100.0f && out.y == 50.0f);
        CHECK(out.pressure == 1.0f);
    }
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        e.button.button = 3;            // right -> button 2
        SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(out.button == 2);
    }
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_MOUSE_MOTION;
        e.motion.x = 10; e.motion.y = 20; e.motion.xrel = 2; e.motion.yrel = 3;
        SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(out.type == Input::Type::Move);
        CHECK(out.x == 10.0f && out.y == 20.0f);
        CHECK(out.dx == 2.0f && out.dy == 3.0f);
    }
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_MOUSE_WHEEL;
        e.wheel.x = 0; e.wheel.y = -1;
        SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(out.type == Input::Type::Scroll);
        CHECK(out.dx == 0.0f && out.dy == -1.0f);
    }

    // --- pen pressure pipeline ---
    {
        SDL_Event axis = ev();
        axis.type = SDL_EVENT_PEN_AXIS;
        axis.paxis.axis = SDL_PEN_AXIS_PRESSURE;
        axis.paxis.value = 0.5f;
        bool ok = SdlTranslator::Translate(axis, pixW, pixH, logW, logH, pen, out);
        CHECK(!ok);                 // axis event is not forwarded
        CHECK(pen == 0.5f);         // pressure state updated

        SDL_Event down = ev();
        down.type = SDL_EVENT_PEN_DOWN;
        down.ptouch.x = 200; down.ptouch.y = 150;
        SdlTranslator::Translate(down, pixW, pixH, logW, logH, pen, out);
        CHECK(out.type == Input::Type::PenDown);
        CHECK(out.pointer == Input::Pointer::Pen);
        CHECK(out.pressure == 0.5f);
        CHECK(out.x == 200.0f && out.y == 150.0f);
    }

    // --- finger ---
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_FINGER_DOWN;
        e.tfinger.x = 0.5f; e.tfinger.y = 0.25f; e.tfinger.pressure = 0.0f; // tap
        SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(out.type == Input::Type::FingerDown);
        CHECK(out.pointer == Input::Pointer::Touch);
        CHECK(out.pressure == 1.0f);                 // zero pressure -> treated as 1.0
        CHECK(out.x == 640.0f && out.y == 200.0f);   // normalized * pixel size
    }

    // --- keyboard + text ---
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = SDLK_S; e.key.mod = SDL_KMOD_CTRL;
        SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(out.type == Input::Type::KeyDown);
        CHECK(out.key == (int)Input::Key::S);
        CHECK(out.mods == (int)Input::Mod::Ctrl);
    }
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = SDLK_Z; e.key.mod = (SDL_Keymod)(SDL_KMOD_CTRL | SDL_KMOD_SHIFT);
        SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(out.key == (int)Input::Key::Z);
        CHECK(out.mods == ((int)Input::Mod::Ctrl | (int)Input::Mod::Shift));
    }
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_TEXT_INPUT;
        e.text.text = "A";
        SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(out.type == Input::Type::Char);
        CHECK(out.codepoint == (uint32_t)'A');
    }
    {
        SDL_Event e = ev();
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = SDLK_ESCAPE; e.key.mod = SDL_KMOD_NONE;
        SdlTranslator::Translate(e, pixW, pixH, logW, logH, pen, out);
        CHECK(out.key == (int)Input::Key::Escape);
    }

    printf("\nSDL translator tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
