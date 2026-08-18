module render.state;

import sg = sokol.gfx;

struct RendererState
{
    float rx = 0;
    float ry = 0;
    float[16] mvp;
    float[16] rotation;
    float[16] view;
    float[16] projection;
    float[16] tempA;
    float[16] tempB;

    sg.Pipeline pip;
    sg.Pipeline rasterLightingPip;
    sg.Pipeline computePip;
    sg.Bindings bind;
    sg.Bindings computeBind;
    sg.Buffer storageBuf;
    sg.View storageView;
    bool gpuCompute = false;
    sg.PassAction passAction = {
        colors: [
            {
                load_action: sg.LoadAction.Clear,
                clear_value: {r: 0.2, g: 0.3, b: 0.4, a: 1.0}
            }
        ]
    };
}
