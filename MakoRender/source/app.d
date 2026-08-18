module app;

import sg = sokol.gfx;
import sapp = sokol.app;
import log = sokol.log;
import sglue = sokol.glue;

import render.shaders;
import render.shaders2d : VsParams2D, shader2DDesc;
import render.cube;
import render.quad;
import render.state;

import jit.codegen;
import jit.pipeline;
import jit.memory;

import std.math : sin, cos;
import std.stdio : writefln;

extern (C):

static RendererState state;
static JitPipeline jitPipeline;
static RotationFn fnRotation;
static Mat4MulFn fnMat4Mul;
static FaceColorsFn fnFaceColors;
static Rotation2DFn fnRotation2D;
static uint frameCount;
static bool hotSwapped;
static bool mode3D = true;

static float[24] faceColors;

static sg.Pipeline pip2D;
static sg.Bindings bind2D;

static float angle2D = 0;

static float[24] quadVerts;

static void init() @system
{
    sg.Desc gfxd;
    gfxd.environment = sglue.environment();
    gfxd.logger.func = &log.func;
    sg.setup(gfxd);

    writefln("[MakoRender] sokol_gfx initialized");
    writefln("[MakoRender] Backend: %s", sg.queryBackend());

    jitPipeline.initialize();
    fnRotation = jitPipeline.getFunction!RotationFn("computeRotation");
    fnRotation2D = jitPipeline.getFunction!Rotation2DFn("computeRotation2D");
    fnMat4Mul = jitPipeline.getFunction!Mat4MulFn("mat4Multiply");
    fnFaceColors = jitPipeline.getFunction!FaceColorsFn("computeFaceColors");
    writefln("[MakoRender] JIT functions loaded into pipeline");

    sg.BufferDesc vbufd;
    vbufd.data.ptr = cubeVertices.ptr;
    vbufd.data.size = cubeVertices.sizeof;
    state.bind.vertex_buffers[0] = sg.makeBuffer(vbufd);

    sg.BufferDesc ibufd;
    ibufd.usage.index_buffer = true;
    ibufd.data.ptr = cubeIndices.ptr;
    ibufd.data.size = cubeIndices.sizeof;
    state.bind.index_buffer = sg.makeBuffer(ibufd);

    sg.PipelineDesc pld;
    pld.layout.attrs[ATTR_POS].format = sg.VertexFormat.Float3;
    pld.layout.attrs[ATTR_COLOR].format = sg.VertexFormat.Float4;
    pld.shader = sg.makeShader(cubeShaderDesc(sg.queryBackend()));
    pld.index_type = sg.IndexType.Uint16;
    pld.cull_mode = sg.CullMode.Back;
    pld.depth.write_enabled = true;
    pld.depth.compare = sg.CompareFunc.Less_equal;
    state.pip = sg.makePipeline(pld);

    quadVerts[] = quadVertices[];

    sg.BufferDesc vbuf2d;
    vbuf2d.data.ptr = quadVerts.ptr;
    vbuf2d.data.size = quadVerts.sizeof;
    bind2D.vertex_buffers[0] = sg.makeBuffer(vbuf2d);

    sg.BufferDesc ibuf2d;
    ibuf2d.usage.index_buffer = true;
    ibuf2d.data.ptr = quadIndices.ptr;
    ibuf2d.data.size = quadIndices.sizeof;
    bind2D.index_buffer = sg.makeBuffer(ibuf2d);

    sg.PipelineDesc pld2d;
    pld2d.layout.attrs[0].format = sg.VertexFormat.Float2;
    pld2d.layout.attrs[1].format = sg.VertexFormat.Float4;
    pld2d.shader = sg.makeShader(cubeShaderDesc(sg.queryBackend()));
    pld2d.index_type = sg.IndexType.Uint16;
    pip2D = sg.makePipeline(pld2d);

    computeView();
    writefln("[MakoRender] Renderer ready. Press SPACE to toggle 3D/2D.");

    auto features = sg.queryFeatures();
    auto backend = sg.queryBackend();
    if (features.compute)
    {
        state.gpuCompute = true;

        sg.BufferDesc storageBufDesc;
        storageBufDesc.size = 6 * 16;
        storageBufDesc.usage.storage_buffer = true;
        state.storageBuf = sg.makeBuffer(storageBufDesc);

        sg.ViewDesc viewDesc;
        viewDesc.storage_buffer.buffer = state.storageBuf;
        state.storageView = sg.makeView(viewDesc);

        state.computeBind.views[0] = state.storageView;
        state.bind.views[0] = state.storageView;

        sg.ShaderDesc csDesc = cubeComputeLightingDesc(backend);
        sg.PipelineDesc csPld;
        csPld.compute = true;
        csPld.shader = sg.makeShader(csDesc);
        state.computePip = sg.makePipeline(csPld);

        sg.ShaderDesc rlDesc = cubeRasterLightingDesc(backend);
        sg.PipelineDesc rlPld;
        rlPld.layout.attrs[ATTR_POS].format = sg.VertexFormat.Float3;
        rlPld.layout.attrs[ATTR_COLOR].format = sg.VertexFormat.Float4;
        rlPld.shader = sg.makeShader(rlDesc);
        rlPld.index_type = sg.IndexType.Uint16;
        rlPld.cull_mode = sg.CullMode.Back;
        rlPld.depth.write_enabled = true;
        rlPld.depth.compare = sg.CompareFunc.Less_equal;
        state.rasterLightingPip = sg.makePipeline(rlPld);

        writefln("[MakoRender] GPU compute lighting enabled");
    }
    else
    {
        writefln("[MakoRender] GPU compute not supported, using CPU JIT fallback");
    }
}

static void frame() @system
{
    float raw_t = cast(float)(sapp.frameDuration() * 60.0);
    if (raw_t > 2.0f) raw_t = 2.0f;
    if (raw_t < 0.0f) raw_t = 0.0f;
    immutable float t = raw_t;

    if (mode3D)
        frame3D(t);
    else
        frame2D(t);

    frameCount++;
    if (frameCount == 120 && !hotSwapped)
    {
        writefln("[MakoRender] Hot-swapping face color computation...");
        jitPipeline.regenColors();
        fnFaceColors = jitPipeline.getFunction!FaceColorsFn("computeFaceColors");
        hotSwapped = true;
        writefln("[MakoRender] Hot-swap complete.");
    }
}

static void frame3D(float t) @system
{
    state.rx += 1.0 * t;
    state.ry += 2.0 * t;

    immutable float rad = 3.14159265f / 180.0f;
    immutable float srx = sin(state.rx * rad);
    immutable float crx = cos(state.rx * rad);
    immutable float sry = sin(state.ry * rad);
    immutable float cry = cos(state.ry * rad);

    fnRotation(state.rotation.ptr, srx, crx, sry, cry);

    immutable float aspect = sapp.widthf() / sapp.heightf();
    computeProjection(aspect);

    fnMat4Mul(state.tempA.ptr, state.view.ptr, state.rotation.ptr);
    fnMat4Mul(state.mvp.ptr, state.projection.ptr, state.tempA.ptr);

    VsParams vsParams;
    vsParams.mvp[] = state.mvp[];

    if (state.gpuCompute)
    {
        immutable float lAngle = state.ry * rad;
        LightParams lp;
        lp.lightDir[0] = sin(lAngle);
        lp.lightDir[1] = 0.3f;
        lp.lightDir[2] = cos(lAngle);
        lp.lightDir[3] = 0.0f;

        sg.Pass csPass;
        csPass.compute = true;
        sg.beginPass(csPass);
        sg.applyPipeline(state.computePip);
        sg.applyBindings(state.computeBind);
        sg.Range csRange;
        csRange.ptr = &lp;
        csRange.size = lp.sizeof;
        sg.applyUniforms(0, csRange);
        sg.dispatch(1, 1, 1);
        sg.endPass();

        sg.Pass pass;
        pass.action = state.passAction;
        pass.swapchain = sglue.swapchain();
        sg.beginPass(pass);
        sg.applyPipeline(state.rasterLightingPip);
        sg.applyBindings(state.bind);
        sg.Range r;
        r.ptr = &vsParams;
        r.size = vsParams.sizeof;
        sg.applyUniforms(UB_VS_PARAMS, r);
        sg.draw(0, 36, 1);
        sg.endPass();
        sg.commit();
    }
    else
    {
        immutable float lAngle = state.ry * rad;
        fnFaceColors(faceColors.ptr, sin(lAngle), 0.3f, cos(lAngle));

        updateVertexColors();

        sg.Range vdata;
        vdata.ptr = cubeVertices.ptr;
        vdata.size = cubeVertices.sizeof;
        sg.updateBuffer(state.bind.vertex_buffers[0], vdata);

        sg.Pass pass;
        pass.action = state.passAction;
        pass.swapchain = sglue.swapchain();
        sg.beginPass(pass);
        sg.applyPipeline(state.pip);
        sg.applyBindings(state.bind);
        sg.Range r;
        r.ptr = &vsParams;
        r.size = vsParams.sizeof;
        sg.applyUniforms(UB_VS_PARAMS, r);
        sg.draw(0, 36, 1);
        sg.endPass();
        sg.commit();
    }
}

static void frame2D(float t) @system
{
    angle2D += 2.0f * t;

    immutable float rad = 3.14159265f / 180.0f;
    immutable float a = angle2D * rad;
    fnRotation2D(state.rotation.ptr, sin(a), cos(a));

    immutable float w = sapp.widthf();
    immutable float h = sapp.heightf();
    if (w < 1.0f || h < 1.0f)
        return;
    immutable float aspect = w / h;

    float[16] ortho;
    if (aspect >= 1.0f)
    {
        ortho = [
            1.0f / aspect, 0.0f, 0.0f, 0.0f,
            0.0f,          1.0f, 0.0f, 0.0f,
            0.0f,          0.0f, -1.0f, 0.0f,
            0.0f,          0.0f,  0.0f, 1.0f,
        ];
    }
    else
    {
        ortho = [
            1.0f, 0.0f,         0.0f, 0.0f,
            0.0f, aspect,       0.0f, 0.0f,
            0.0f, 0.0f,        -1.0f, 0.0f,
            0.0f, 0.0f,         0.0f, 1.0f,
        ];
    }

    fnMat4Mul(state.mvp.ptr, ortho.ptr, state.rotation.ptr);

    VsParams2D vsParams;
    vsParams.mvp[] = state.mvp[];

    sg.Pass pass;
    pass.action = state.passAction;
    pass.swapchain = sglue.swapchain();
    sg.beginPass(pass);
    sg.applyPipeline(pip2D);
    sg.applyBindings(bind2D);
    sg.Range r;
    r.ptr = &vsParams;
    r.size = vsParams.sizeof;
    sg.applyUniforms(UB_VS_PARAMS, r);
    sg.draw(0, 6, 1);
    sg.endPass();
    sg.commit();
}

static void cleanup()
{
    writefln("[MakoRender] Cleaning up...");
    sg.shutdown();
}

void computeView()
{
    state.view = [
         1.0f,  0.0f,        0.0f,       0.0f,
         0.0f,  0.97014f,    0.24253f,   0.0f,
         0.0f, -0.24253f,    0.97014f,   0.0f,
         0.0f,  0.0f,       -6.1847f,    1.0f,
    ];
}

void computeProjection(float aspect)
{
    immutable float f = 1.7320508f;
    immutable float near = 0.01f;
    immutable float far = 10.0f;
    immutable float rangeInv = 1.0f / (near - far);

    float[16] proj = [
        f / aspect, 0.0f, 0.0f,                         0.0f,
        0.0f,       f,    0.0f,                         0.0f,
        0.0f,       0.0f, (far + near) * rangeInv,      -1.0f,
        0.0f,       0.0f, 2.0f * far * near * rangeInv, 0.0f,
    ];
    state.projection = proj;
}

void updateVertexColors()
{
    immutable float[24] baseColors = [
        1.0, 0.0, 0.0, 1.0,
        0.0, 1.0, 0.0, 1.0,
        0.0, 0.0, 1.0, 1.0,
        1.0, 0.5, 0.0, 1.0,
        0.0, 0.5, 1.0, 1.0,
        1.0, 0.0, 0.5, 1.0,
    ];

    foreach (face; 0 .. 6)
    {
        float intensity = faceColors[face * 4 + 0];
        float r = baseColors[face * 4 + 0] * intensity;
        float g = baseColors[face * 4 + 1] * intensity;
        float b = baseColors[face * 4 + 2] * intensity;
        float a = baseColors[face * 4 + 3];

        foreach (v; 0 .. 4)
        {
            size_t idx = (face * 4 + v) * 7;
            cubeVertices[idx + 3] = r;
            cubeVertices[idx + 4] = g;
            cubeVertices[idx + 5] = b;
            cubeVertices[idx + 6] = a;
        }
    }
}

extern(C) void event_cb(const sapp.Event* ev) @system
{
    if (ev.type == sapp.EventType.Key_down)
    {
        if (ev.key_code == sapp.Keycode.Space)
        {
            mode3D = !mode3D;
            if (mode3D)
                writefln("[MakoRender] Mode: 3D");
            else
                writefln("[MakoRender] Mode: 2D");
        }
    }
}

void main()
{
    sapp.Desc runner;
    runner.window_title = "MakoRender - JIT-Enabled Renderer (SPACE to toggle 3D/2D)";
    runner.init_cb = &init;
    runner.frame_cb = &frame;
    runner.cleanup_cb = &cleanup;
    runner.event_cb = &event_cb;
    runner.width = 800;
    runner.height = 600;
    runner.sample_count = 1;
    runner.swap_interval = 1;
    runner.icon.sokol_default = true;
    runner.logger.func = &log.func;
    sapp.run(runner);
}
