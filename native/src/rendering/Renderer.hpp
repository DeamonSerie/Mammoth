#pragma once
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "MakoJit.hpp"
#include "../app/Types.hpp"
#include <vector>
#include <cstdint>
#include <cstring>

struct QuadCmd {
    float x, y, w, h;
    float rotation = 0.0f;  // radians, rotation around (x + w/2, y + h/2)
    sg_image image;
    sg_view view;
    sg_sampler sampler;
    float u0, v0, u1, v1;
};

struct SolidVertex {
    float x, y;
    uint32_t color;
};

struct TextVertex {
    float x, y;
    float u, v;
    uint32_t color;
};

struct VsParams2D {
    alignas(16) float mvp[16];
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init();
    void shutdown();

    void beginFrame(float winW, float winH);
    void endFrame();

    void queueQuad(float x, float y, float w, float h,
                   sg_image img, sg_view view, sg_sampler smp,
                   float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1,
                   float rotation = 0.0f);
    void queueQuad(float x, float y, float w, float h,
                   sg_image img, sg_sampler smp,
                   float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1,
                   float rotation = 0.0f);
    void flushQuads(float viewW, float viewH);

    void queueSolidRect(float x, float y, float w, float h, Color color, float rotation = 0.0f);
    void flushSolid(float viewW, float viewH);

    void drawText(const char* text, float x, float y, float scale, Color color);
    void flushText(float viewW, float viewH);

    bool initialized() const { return m_initialized; }
    static sg_sampler defaultSampler();

    sg_sampler fontSampler() const { return m_fontSmp; }

    MakoRender::MakoJitPipeline& jitPipeline() { return m_jitPipeline; }
    const MakoRender::MakoJitPipeline& jitPipeline() const { return m_jitPipeline; }
    void hotSwapEraser(bool enable) { m_jitPipeline.hotSwapEraser(enable); }

private:
    void createShadersAndPipelines();
    void initFont();
    void computeMvp(float viewW, float viewH, float* outMvp);

    MakoRender::MakoJitPipeline m_jitPipeline;

    sg_shader m_textureShader = {};
    sg_pipeline m_texturePip = {};
    sg_buffer m_quadVBuf = {};
    sg_buffer m_quadIBuf = {};
    bool m_initialized = false;
    sg_pass_action m_passAction = {};

    static constexpr int MAX_QUADS = 512;
    std::vector<QuadCmd> m_quadQueue;
    std::vector<float> m_quadVerts;

    sg_shader m_solidShader = {};
    sg_pipeline m_solidPip = {};
    sg_buffer m_solidVBuf = {};
    sg_buffer m_solidIBuf = {};
    static constexpr int MAX_SOLID = 4096;
    std::vector<SolidVertex> m_solidVerts;
    int m_solidQuadCount = 0;

    sg_shader m_textShader = {};
    sg_pipeline m_textPip = {};
    sg_buffer m_textVBuf = {};
    sg_buffer m_textIBuf = {};
    static constexpr int MAX_TEXT = 8192;
    std::vector<TextVertex> m_textVerts;
    int m_textQuadCount = 0;

    sg_image m_fontAtlas = {};
    sg_view m_fontView = {};
    sg_sampler m_fontSmp = {};

    sg_sampler m_defaultSampler = {};
};
