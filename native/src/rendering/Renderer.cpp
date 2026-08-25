#include "Renderer.hpp"
#include "../ui/Font.hpp"
#include "sokol_log.h"
#include "../DebugLog.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cmath>
#include <algorithm>

enum { SLOT_vs_mvp = 0, SLOT_tex_view = 0, SLOT_tex_smp = 0 };

static Renderer* s_instance = nullptr;

static const char* VERT_SRC = R"(
#version 330
uniform vec4 uMVP[4];
in vec2 aPosition;
in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = mat4(uMVP[0], uMVP[1], uMVP[2], uMVP[3]) * vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* FRAG_TEXTURE_SRC = R"(
#version 330
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uTexture;
void main() {
    vec4 color = texture(uTexture, vTexCoord);
    fragColor = color;
}
)";

static const char* SOLID_VERT_SRC = R"(
#version 330
uniform vec4 uMVP[4];
in vec2 aPosition;
in vec4 aColor;
out vec4 vColor;
void main() {
    gl_Position = mat4(uMVP[0], uMVP[1], uMVP[2], uMVP[3]) * vec4(aPosition, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* SOLID_FRAG_SRC = R"(
#version 330
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)";

static const char* TEXT_VERT_SRC = R"(
#version 330
uniform vec4 uMVP[4];
in vec2 aPosition;
in vec2 aTexCoord;
in vec4 aColor;
out vec2 vTexCoord;
out vec4 vColor;
void main() {
    gl_Position = mat4(uMVP[0], uMVP[1], uMVP[2], uMVP[3]) * vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";

static const char* TEXT_FRAG_SRC = R"(
#version 330
in vec2 vTexCoord;
in vec4 vColor;
out vec4 fragColor;
uniform sampler2D uTexture;
void main() {
    float a = texture(uTexture, vTexCoord).r;
    fragColor = vec4(vColor.rgb, vColor.a * a);
}
)";

static uint16_t genQuadIndices_buf[8192 * 6];
static bool quadIndicesInit = false;

static void ensureQuadIndices() {
    if (quadIndicesInit) return;
    quadIndicesInit = true;
    for (int i = 0; i < 8192; i++) {
        genQuadIndices_buf[i * 6 + 0] = i * 4 + 0;
        genQuadIndices_buf[i * 6 + 1] = i * 4 + 1;
        genQuadIndices_buf[i * 6 + 2] = i * 4 + 2;
        genQuadIndices_buf[i * 6 + 3] = i * 4 + 0;
        genQuadIndices_buf[i * 6 + 4] = i * 4 + 2;
        genQuadIndices_buf[i * 6 + 5] = i * 4 + 3;
    }
}

Renderer::Renderer() {
    DebugLog::log("[Renderer] Constructor");
}
Renderer::~Renderer() { shutdown(); }

sg_sampler Renderer::defaultSampler() {
    if (s_instance) return s_instance->m_defaultSampler;
    sg_sampler s = {}; return s;
}

void Renderer::computeMvp(float viewW, float viewH, float* outMvp) {
    if (viewW <= 0.0f) viewW = 1.0f;
    if (viewH <= 0.0f) viewH = 1.0f;
    m_jitPipeline.ortho2D()(outMvp, 0.0f, viewW, viewH, 0.0f);
}

void Renderer::init() {
    if (m_initialized) return;
    DebugLog::log("[Renderer] init()");
    ensureQuadIndices();
    s_instance = this;

    // Initialize MakoRender JIT compiler pipeline
    m_jitPipeline.initialize();

    sg_desc desc = {};
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(desc);

    m_passAction.colors[0].load_action = SG_LOADACTION_CLEAR;
    m_passAction.colors[0].clear_value = {0.11f, 0.11f, 0.13f, 1.0f};

    createShadersAndPipelines();
    initFont();

    m_quadVerts.resize(MAX_QUADS * 16);
    m_solidVerts.resize(MAX_SOLID * 4);
    m_textVerts.resize(MAX_TEXT * 4);

    m_initialized = true;
    DebugLog::log("[Renderer] Initialized successfully");
}

void Renderer::shutdown() {
    DebugLog::log("[Renderer] shutdown()");
    if (!m_initialized) return;
    sg_destroy_pipeline(m_texturePip);
    sg_destroy_shader(m_textureShader);
    sg_destroy_buffer(m_quadVBuf);
    sg_destroy_buffer(m_quadIBuf);
    sg_destroy_pipeline(m_solidPip);
    sg_destroy_shader(m_solidShader);
    sg_destroy_buffer(m_solidVBuf);
    sg_destroy_buffer(m_solidIBuf);
    sg_destroy_pipeline(m_textPip);
    sg_destroy_shader(m_textShader);
    sg_destroy_buffer(m_textVBuf);
    sg_destroy_buffer(m_textIBuf);
    if (m_fontAtlas.id) sg_destroy_image(m_fontAtlas);
    if (m_fontView.id) sg_destroy_view(m_fontView);
    if (m_fontSmp.id) sg_destroy_sampler(m_fontSmp);
    if (m_defaultSampler.id) sg_destroy_sampler(m_defaultSampler);
    sg_shutdown();

    m_jitPipeline.shutdown();
    m_initialized = false;
    s_instance = nullptr;
}

void Renderer::createShadersAndPipelines() {
    // 1. Textured Quad 2D Pipeline
    {
        sg_shader_desc d = {};
        d.label = "makorender_texture_shader";
        d.vertex_func.source = VERT_SRC;
        d.vertex_func.entry = "main";
        d.fragment_func.source = FRAG_TEXTURE_SRC;
        d.fragment_func.entry = "main";
        d.attrs[0].glsl_name = "aPosition";
        d.attrs[1].glsl_name = "aTexCoord";
        d.uniform_blocks[SLOT_vs_mvp].stage = SG_SHADERSTAGE_VERTEX;
        d.uniform_blocks[SLOT_vs_mvp].layout = SG_UNIFORMLAYOUT_STD140;
        d.uniform_blocks[SLOT_vs_mvp].size = sizeof(VsParams2D);
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].array_count = 4;
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].glsl_name = "uMVP";
        d.views[SLOT_tex_view].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        d.views[SLOT_tex_view].texture.image_type = SG_IMAGETYPE_2D;
        d.views[SLOT_tex_view].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        d.samplers[SLOT_tex_smp].stage = SG_SHADERSTAGE_FRAGMENT;
        d.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
        d.texture_sampler_pairs[0].view_slot = SLOT_tex_view;
        d.texture_sampler_pairs[0].sampler_slot = SLOT_tex_smp;
        d.texture_sampler_pairs[0].glsl_name = "uTexture";
        m_textureShader = sg_make_shader(d);
    }
    {
        sg_pipeline_desc d = {};
        d.label = "makorender_texture_pip";
        d.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
        d.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
        d.shader = m_textureShader;
        d.index_type = SG_INDEXTYPE_UINT16;
        d.colors[0].blend.enabled = true;
        d.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        d.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        d.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
        d.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        d.colors[0].blend.op_rgb = SG_BLENDOP_ADD;
        d.colors[0].blend.op_alpha = SG_BLENDOP_ADD;
        d.colors[0].write_mask = SG_COLORMASK_RGBA;
        m_texturePip = sg_make_pipeline(d);
    }
    {
        sg_buffer_desc bd = {};
        bd.usage.dynamic_update = true;
        bd.usage.vertex_buffer = true;
        bd.size = MAX_QUADS * 16 * sizeof(float);
        m_quadVBuf = sg_make_buffer(bd);
        sg_buffer_desc ibd = {};
        ibd.usage.index_buffer = true;
        ibd.data = sg_range{genQuadIndices_buf, (size_t)(MAX_QUADS * 6 * sizeof(uint16_t))};
        m_quadIBuf = sg_make_buffer(ibd);
    }

    // 2. Solid Color 2D Pipeline
    {
        sg_shader_desc d = {};
        d.label = "makorender_solid_shader";
        d.vertex_func.source = SOLID_VERT_SRC;
        d.vertex_func.entry = "main";
        d.fragment_func.source = SOLID_FRAG_SRC;
        d.fragment_func.entry = "main";
        d.attrs[0].glsl_name = "aPosition";
        d.attrs[1].glsl_name = "aColor";
        d.uniform_blocks[SLOT_vs_mvp].stage = SG_SHADERSTAGE_VERTEX;
        d.uniform_blocks[SLOT_vs_mvp].layout = SG_UNIFORMLAYOUT_STD140;
        d.uniform_blocks[SLOT_vs_mvp].size = sizeof(VsParams2D);
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].array_count = 4;
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].glsl_name = "uMVP";
        m_solidShader = sg_make_shader(d);
    }
    {
        sg_pipeline_desc d = {};
        d.label = "makorender_solid_pip";
        d.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
        d.layout.attrs[0].offset = 0;
        d.layout.attrs[1].format = SG_VERTEXFORMAT_UBYTE4N;
        d.layout.attrs[1].offset = 8;
        d.layout.buffers[0].stride = sizeof(SolidVertex);
        d.shader = m_solidShader;
        d.index_type = SG_INDEXTYPE_UINT16;
        d.colors[0].blend.enabled = true;
        d.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        d.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        d.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
        d.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        d.colors[0].blend.op_rgb = SG_BLENDOP_ADD;
        d.colors[0].blend.op_alpha = SG_BLENDOP_ADD;
        d.colors[0].write_mask = SG_COLORMASK_RGBA;
        m_solidPip = sg_make_pipeline(d);
    }
    {
        sg_buffer_desc bd = {};
        bd.usage.dynamic_update = true;
        bd.usage.vertex_buffer = true;
        bd.size = MAX_SOLID * 4 * sizeof(SolidVertex);
        m_solidVBuf = sg_make_buffer(bd);
        sg_buffer_desc ibd = {};
        ibd.usage.index_buffer = true;
        ibd.data = sg_range{genQuadIndices_buf, (size_t)(MAX_SOLID * 6 * sizeof(uint16_t))};
        m_solidIBuf = sg_make_buffer(ibd);
    }

    // 3. Text & Glyph 2D Pipeline
    {
        sg_shader_desc d = {};
        d.label = "makorender_text_shader";
        d.vertex_func.source = TEXT_VERT_SRC;
        d.vertex_func.entry = "main";
        d.fragment_func.source = TEXT_FRAG_SRC;
        d.fragment_func.entry = "main";
        d.attrs[0].glsl_name = "aPosition";
        d.attrs[1].glsl_name = "aTexCoord";
        d.attrs[2].glsl_name = "aColor";
        d.uniform_blocks[SLOT_vs_mvp].stage = SG_SHADERSTAGE_VERTEX;
        d.uniform_blocks[SLOT_vs_mvp].layout = SG_UNIFORMLAYOUT_STD140;
        d.uniform_blocks[SLOT_vs_mvp].size = sizeof(VsParams2D);
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].array_count = 4;
        d.uniform_blocks[SLOT_vs_mvp].glsl_uniforms[0].glsl_name = "uMVP";
        d.views[SLOT_tex_view].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        d.views[SLOT_tex_view].texture.image_type = SG_IMAGETYPE_2D;
        d.views[SLOT_tex_view].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        d.samplers[SLOT_tex_smp].stage = SG_SHADERSTAGE_FRAGMENT;
        d.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
        d.texture_sampler_pairs[0].view_slot = SLOT_tex_view;
        d.texture_sampler_pairs[0].sampler_slot = SLOT_tex_smp;
        d.texture_sampler_pairs[0].glsl_name = "uTexture";
        m_textShader = sg_make_shader(d);
    }
    {
        sg_pipeline_desc d = {};
        d.label = "makorender_text_pip";
        d.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
        d.layout.attrs[0].offset = 0;
        d.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
        d.layout.attrs[1].offset = 8;
        d.layout.attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
        d.layout.attrs[2].offset = 16;
        d.layout.buffers[0].stride = sizeof(TextVertex);
        d.shader = m_textShader;
        d.index_type = SG_INDEXTYPE_UINT16;
        d.colors[0].blend.enabled = true;
        d.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        d.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        d.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
        d.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        d.colors[0].blend.op_rgb = SG_BLENDOP_ADD;
        d.colors[0].blend.op_alpha = SG_BLENDOP_ADD;
        d.colors[0].write_mask = SG_COLORMASK_RGBA;
        m_textPip = sg_make_pipeline(d);
    }
    {
        sg_buffer_desc bd = {};
        bd.usage.dynamic_update = true;
        bd.usage.vertex_buffer = true;
        bd.size = MAX_TEXT * 4 * sizeof(TextVertex);
        m_textVBuf = sg_make_buffer(bd);
        sg_buffer_desc ibd = {};
        ibd.usage.index_buffer = true;
        ibd.data = sg_range{genQuadIndices_buf, (size_t)(MAX_TEXT * 6 * sizeof(uint16_t))};
        m_textIBuf = sg_make_buffer(ibd);
    }

    sg_sampler_desc sd = {};
    sd.min_filter = SG_FILTER_NEAREST;
    sd.mag_filter = SG_FILTER_NEAREST;
    m_defaultSampler = sg_make_sampler(sd);
}

void Renderer::initFont() {
    DebugLog::log("[Renderer] initFont()");
    std::vector<uint8_t> data(FONT_ATLAS_W * FONT_ATLAS_H, 0);
    for (int ch = 0; ch < FONT_NUM_CHARS; ch++) {
        int col = ch % FONT_ATLAS_COLS;
        int row = ch / FONT_ATLAS_COLS;
        for (int py = 0; py < FONT_CHAR_H; py++) {
            uint8_t bits = FONT_8x8[ch][py];
            for (int px = 0; px < FONT_CHAR_W; px++) {
                if (bits & (0x01 << px)) {
                    int gx = col * FONT_CHAR_W + px;
                    int gy = row * FONT_CHAR_H + py;
                    data[gy * FONT_ATLAS_W + gx] = 255;
                }
            }
        }
    }

    sg_image_desc id = {};
    id.width = FONT_ATLAS_W;
    id.height = FONT_ATLAS_H;
    id.pixel_format = SG_PIXELFORMAT_R8;
    id.data.mip_levels[0] = sg_range{data.data(), data.size()};
    m_fontAtlas = sg_make_image(id);

    sg_view_desc vd = {};
    vd.texture.image = m_fontAtlas;
    m_fontView = sg_make_view(&vd);

    sg_sampler_desc sd = {};
    sd.min_filter = SG_FILTER_NEAREST;
    sd.mag_filter = SG_FILTER_NEAREST;
    m_fontSmp = sg_make_sampler(sd);

    DebugLog::log("[Renderer] Font atlas initialized (%dx%d)", FONT_ATLAS_W, FONT_ATLAS_H);
}

void Renderer::beginFrame(float, float) {
    DebugLog::log("[Renderer] beginFrame");
    sg_pass pass = {};
    pass.action = m_passAction;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(pass);
    m_quadQueue.clear();
    m_solidQuadCount = 0;
    m_textQuadCount = 0;
}

void Renderer::endFrame() {
    DebugLog::log("[Renderer] endFrame");
    sg_end_pass();
    sg_commit();
}

void Renderer::queueQuad(float x, float y, float w, float h,
                          sg_image img, sg_view view, sg_sampler smp,
                          float u0, float v0, float u1, float v1,
                          float rotation)
{
    if ((int)m_quadQueue.size() >= MAX_QUADS) return;
    QuadCmd cmd = {};
    cmd.x = x; cmd.y = y; cmd.w = w; cmd.h = h; cmd.rotation = rotation;
    cmd.image = img; cmd.view = view; cmd.sampler = smp;
    cmd.u0 = u0; cmd.v0 = v0; cmd.u1 = u1; cmd.v1 = v1;
    m_quadQueue.push_back(cmd);
}

void Renderer::queueQuad(float x, float y, float w, float h,
                          sg_image img, sg_sampler smp,
                          float u0, float v0, float u1, float v1,
                          float rotation)
{
    queueQuad(x, y, w, h, img, sg_view{}, smp, u0, v0, u1, v1, rotation);
}

void Renderer::flushQuads(float viewW, float viewH) {
    if (m_quadQueue.empty()) return;
    DebugLog::log("[Renderer] flushQuads count=%zu viewW=%.1f viewH=%.1f", m_quadQueue.size(), viewW, viewH);

    VsParams2D vsParams;
    computeMvp(viewW, viewH, vsParams.mvp);

    int quadCount = (int)m_quadQueue.size();

    for (int i = 0; i < quadCount; i++) {
        auto& cmd = m_quadQueue[i];
        float x = cmd.x, y = cmd.y, w = cmd.w, h = cmd.h;
        int vi = i * 16;
        if (cmd.rotation != 0.0f) {
            // Rotate the four corners around the quad center.
            float cx = x + w * 0.5f;
            float cy = y + h * 0.5f;
            float c = std::cos(cmd.rotation);
            float s = std::sin(cmd.rotation);
            auto rot = [&](float px, float py) -> std::pair<float,float> {
                float dx = px - cx, dy = py - cy;
                return {dx * c - dy * s + cx, dx * s + dy * c + cy};
            };
            auto [x0, y0] = rot(x,     y);
            auto [x1, y1] = rot(x + w, y);
            auto [x2, y2] = rot(x + w, y + h);
            auto [x3, y3] = rot(x,     y + h);
            m_quadVerts[vi +  0] = x0; m_quadVerts[vi +  1] = y0; m_quadVerts[vi +  2] = cmd.u0; m_quadVerts[vi +  3] = cmd.v0;
            m_quadVerts[vi +  4] = x1; m_quadVerts[vi +  5] = y1; m_quadVerts[vi +  6] = cmd.u1; m_quadVerts[vi +  7] = cmd.v0;
            m_quadVerts[vi +  8] = x2; m_quadVerts[vi +  9] = y2; m_quadVerts[vi + 10] = cmd.u1; m_quadVerts[vi + 11] = cmd.v1;
            m_quadVerts[vi + 12] = x3; m_quadVerts[vi + 13] = y3; m_quadVerts[vi + 14] = cmd.u0; m_quadVerts[vi + 15] = cmd.v1;
        } else {
            m_quadVerts[vi +  0] = x;     m_quadVerts[vi +  1] = y;     m_quadVerts[vi +  2] = cmd.u0; m_quadVerts[vi +  3] = cmd.v0;
            m_quadVerts[vi +  4] = x + w; m_quadVerts[vi +  5] = y;     m_quadVerts[vi +  6] = cmd.u1; m_quadVerts[vi +  7] = cmd.v0;
            m_quadVerts[vi +  8] = x + w; m_quadVerts[vi +  9] = y + h; m_quadVerts[vi + 10] = cmd.u1; m_quadVerts[vi + 11] = cmd.v1;
            m_quadVerts[vi + 12] = x;     m_quadVerts[vi + 13] = y + h; m_quadVerts[vi + 14] = cmd.u0; m_quadVerts[vi + 15] = cmd.v1;
        }
    }

    sg_update_buffer(m_quadVBuf, sg_range{m_quadVerts.data(), (size_t)(quadCount * 16 * sizeof(float))});
    sg_apply_pipeline(m_texturePip);

    for (int i = 0; i < quadCount; i++) {
        auto& cmd = m_quadQueue[i];
        sg_view texView = cmd.view;
        bool createdView = false;
        if (!texView.id) {
            sg_view_desc vd = {};
            vd.texture.image = cmd.image;
            texView = sg_make_view(&vd);
            createdView = true;
        }

        sg_bindings bind = {};
        bind.vertex_buffers[0] = m_quadVBuf;
        bind.index_buffer = m_quadIBuf;
        bind.views[SLOT_tex_view] = texView;
        bind.samplers[SLOT_tex_smp] = cmd.sampler;
        sg_apply_bindings(bind);
        sg_apply_uniforms(SLOT_vs_mvp, sg_range{&vsParams, sizeof(VsParams2D)});
        sg_draw(i * 6, 6, 1);

        if (createdView) {
            sg_destroy_view(texView);
        }
    }
    m_quadQueue.clear();
}

void Renderer::queueSolidRect(float x, float y, float w, float h, Color color) {
    if (m_solidQuadCount >= MAX_SOLID) return;
    uint32_t c = color.pack();
    int i = m_solidQuadCount;
    SolidVertex* v = m_solidVerts.data() + i * 4;
    v[0] = {x,     y,     c};
    v[1] = {x + w, y,     c};
    v[2] = {x + w, y + h, c};
    v[3] = {x,     y + h, c};
    m_solidQuadCount++;
}

void Renderer::flushSolid(float viewW, float viewH) {
    if (m_solidQuadCount == 0) return;
    DebugLog::log("[Renderer] flushSolid count=%d viewW=%.1f viewH=%.1f", m_solidQuadCount, viewW, viewH);

    VsParams2D vsParams;
    computeMvp(viewW, viewH, vsParams.mvp);

    sg_update_buffer(m_solidVBuf, sg_range{
        m_solidVerts.data(),
        (size_t)(m_solidQuadCount * 4 * sizeof(SolidVertex))
    });

    sg_apply_pipeline(m_solidPip);
    sg_bindings bind = {};
    bind.vertex_buffers[0] = m_solidVBuf;
    bind.index_buffer = m_solidIBuf;
    sg_apply_bindings(bind);
    sg_apply_uniforms(SLOT_vs_mvp, sg_range{&vsParams, sizeof(VsParams2D)});
    sg_draw(0, m_solidQuadCount * 6, 1);
    m_solidQuadCount = 0;
}

void Renderer::drawText(const char* text, float x, float y, float scale, Color color) {
    if (!text) return;
    DebugLog::log("[Renderer] drawText text='%s' x=%.1f y=%.1f scale=%.1f", text, x, y, scale);
    uint32_t c = color.pack();
    int len = (int)strlen(text);
    float charW = FONT_CHAR_W * scale;
    float charH = FONT_CHAR_H * scale;
    float cursorX = x;

    for (int i = 0; i < len; i++) {
        if (m_textQuadCount >= MAX_TEXT) return;
        if (text[i] == ' ') { cursorX += charW; continue; }
        int ci = (int)(uint8_t)text[i] - FONT_FIRST_CHAR;
        if (ci < 0 || ci >= FONT_NUM_CHARS) { cursorX += charW; continue; }

        int col = ci % FONT_ATLAS_COLS;
        int row = ci / FONT_ATLAS_COLS;
        float u0 = (float)(col * FONT_CHAR_W) / (float)FONT_ATLAS_W;
        float v0 = (float)(row * FONT_CHAR_H) / (float)FONT_ATLAS_H;
        float u1 = u0 + (float)FONT_CHAR_W / (float)FONT_ATLAS_W;
        float v1 = v0 + (float)FONT_CHAR_H / (float)FONT_ATLAS_H;

        int vi = m_textQuadCount * 4;
        TextVertex* v = m_textVerts.data() + vi;
        v[0] = {cursorX,         y,         u0, v0, c};
        v[1] = {cursorX + charW, y,         u1, v0, c};
        v[2] = {cursorX + charW, y + charH, u1, v1, c};
        v[3] = {cursorX,         y + charH, u0, v1, c};
        m_textQuadCount++;
        cursorX += charW;
    }
}

void Renderer::flushText(float viewW, float viewH) {
    if (m_textQuadCount == 0) return;
    DebugLog::log("[Renderer] flushText count=%d viewW=%.1f viewH=%.1f", m_textQuadCount, viewW, viewH);

    VsParams2D vsParams;
    computeMvp(viewW, viewH, vsParams.mvp);

    sg_update_buffer(m_textVBuf, sg_range{
        m_textVerts.data(),
        (size_t)(m_textQuadCount * 4 * sizeof(TextVertex))
    });

    sg_view_desc viewDesc = {};
    viewDesc.texture.image = m_fontAtlas;
    sg_view texView = sg_make_view(&viewDesc);

    sg_apply_pipeline(m_textPip);
    sg_bindings bind = {};
    bind.vertex_buffers[0] = m_textVBuf;
    bind.index_buffer = m_textIBuf;
    bind.views[SLOT_tex_view] = texView;
    bind.samplers[SLOT_tex_smp] = m_fontSmp;
    sg_apply_bindings(bind);
    sg_apply_uniforms(SLOT_vs_mvp, sg_range{&vsParams, sizeof(VsParams2D)});
    sg_draw(0, m_textQuadCount * 6, 1);

    sg_destroy_view(texView);
    m_textQuadCount = 0;
}
