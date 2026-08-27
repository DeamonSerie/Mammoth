// sokol implementation translation unit. sokol_gfx is the portable rendering
// backend (GL / Metal / D3D / WebGPU depending on platform), which is what
// keeps Mammoth compatible across different graphics processors.
//
// Two build modes:
//   * default : sokol_app owns the window + GL context (sglue_* come from
//               sokol_glue's sokol_app-backed implementation).
//   * USE_SDL : an SDL-owned GL context (see SdlTranslator). sokol_gfx runs in
//               "foreign context" mode, so we provide sglue_environment /
//               sglue_swapchain / sapp_width / sapp_height ourselves, fed by
//               the real SDL window size.
#ifdef USE_SDL
// Include sokol_app.h (via InputEvent.hpp) for the SAPP_KEYCODE_* map BEFORE
// SOKOL_IMPL is defined, so it only emits declarations. sokol_gfx then runs
// in "foreign context" mode; we provide sglue_*/sapp_* ourselves. We avoid
// including sokol_glue.h here because it would force its sokol_app-backed
// implementation once SOKOL_IMPL is set.
#include "app/SdlTranslator.hpp"
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_gfx.h"
#include "sokol_log.h"

extern "C" {

int sapp_width(void)  { return SdlGl::g_width; }
int sapp_height(void) { return SdlGl::g_height; }

// Declarations matching sokol_glue.h (Renderer.cpp sees these via sokol_glue.h).
sg_environment sglue_environment(void);
sg_swapchain sglue_swapchain(void);

sg_environment sglue_environment(void) {
    sg_environment env;
    memset(&env, 0, sizeof(env));
    env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
    env.defaults.depth_format = SG_PIXELFORMAT_NONE;
    env.defaults.sample_count = 1;
    return env;
}

sg_swapchain sglue_swapchain(void) {
    sg_swapchain sc;
    memset(&sc, 0, sizeof(sc));
    sc.width = SdlGl::g_width;
    sc.height = SdlGl::g_height;
    sc.sample_count = 1;
    sc.color_format = SG_PIXELFORMAT_RGBA8;
    sc.depth_format = SG_PIXELFORMAT_NONE;
    sc.gl.framebuffer = 0;   // default framebuffer
    return sc;
}

}

#else
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#endif
