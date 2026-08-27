// SDL entry point. Creates the SDL window/GL context (via SdlTranslator),
// boots the application, then pumps SDL events into Application::postInput
// and drives frames. sokol_gfx renders into the SDL-owned GL context.
#include "Application.hpp"
#include "SdlTranslator.hpp"
#include "sokol_log.h"
#include <SDL3/SDL.h>

int main(int, char**) {
    SdlTranslator translator;
    if (!translator.init(1280, 800))
        return 1;

    Application::instance().init();

    while (!Application::instance().shouldQuit()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                Application::instance().requestQuit();
                continue;
            }
            auto ne = translator.translate(ev);
            if (ne) Application::instance().postInput(*ne);
        }
        Application::instance().frame();
        SDL_GL_SwapWindow(translator.window());
    }

    Application::instance().cleanup();
    translator.shutdown();
    return 0;
}
