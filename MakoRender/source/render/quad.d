module render.quad;

float[24] quadVertices = [
    -0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 1.0f,
     0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 1.0f,
     0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f,
    -0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f,
];

ushort[6] quadIndices = [
    0, 1, 2,
    0, 2, 3,
];
