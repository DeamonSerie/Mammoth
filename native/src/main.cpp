#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "app/Application.hpp"

static void init_cb() {
    Application::instance().init();
}

static void frame_cb() {
    Application::instance().frame();
}

static void cleanup_cb() {
    Application::instance().cleanup();
}

static void event_cb(const sapp_event* ev) {
    Application::instance().event(ev);
}

sapp_desc sokol_main(int, char*[]) {
    sapp_desc desc = {};
    desc.width = 1280;
    desc.height = 720;
    desc.window_title = "Mammoth";
    desc.init_cb = init_cb;
    desc.frame_cb = frame_cb;
    desc.cleanup_cb = cleanup_cb;
    desc.event_cb = event_cb;
    desc.sample_count = 1;
    desc.swap_interval = 1;
    desc.logger.func = slog_func;
    return desc;
}
