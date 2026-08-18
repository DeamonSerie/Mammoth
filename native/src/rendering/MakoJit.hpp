#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <cstdio>
#include <string>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace MakoRender {

typedef void (*Mat4MulFn)(float* dst, const float* a, const float* b);
typedef void (*Ortho2DFn)(float* out, float left, float right, float bottom, float top);
typedef void (*Rotation2DFn)(float* out, float sinA, float cosA);
typedef void (*ClearPixelsFn)(uint8_t* pixels, size_t numBytes);

// Hot-swappable stamp kernel: renders one circular brush stamp onto a layer pixel buffer.
// Signature: pixels, stride_in_bytes(=width*4), width, height, cx, cy, radius, r,g,b,a
// When paint kernel is active: alpha-blends the color onto each pixel inside the circle.
// When erase kernel is active: zeroes RGBA for each pixel inside the circle.
typedef void (*StampKernelFn)(uint8_t* pixels, int width, int height,
                               int cx, int cy, int radius,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a);

struct MakoExecutableRegion {
    void* ptr = nullptr;
    size_t size = 0;

    template <typename T>
    T getFunction() const {
        return reinterpret_cast<T>(ptr);
    }

    bool isValid() const {
        return ptr != nullptr && ptr != MAP_FAILED;
    }
};

inline size_t alignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

inline MakoExecutableRegion allocExecutable(size_t codeSize) {
    size_t pageSize = 4096;
#if defined(__linux__) || defined(__APPLE__)
    size_t allocSize = alignUp(codeSize, pageSize);
    void* mem = mmap(nullptr, allocSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mem == nullptr || mem == MAP_FAILED) {
        return {nullptr, 0};
    }
    return {mem, allocSize};
#else
    return {nullptr, 0};
#endif
}

inline bool makeExecutable(MakoExecutableRegion& region) {
    if (!region.isValid()) return false;
#if defined(__linux__) || defined(__APPLE__)
    return mprotect(region.ptr, region.size, PROT_READ | PROT_EXEC) == 0;
#else
    return false;
#endif
}

inline void freeExecutable(MakoExecutableRegion& region) {
    if (region.isValid()) {
#if defined(__linux__) || defined(__APPLE__)
        munmap(region.ptr, region.size);
#endif
        region.ptr = nullptr;
        region.size = 0;
    }
}

inline bool writeCode(MakoExecutableRegion& region, const uint8_t* code, size_t len) {
    if (!region.isValid() || len > region.size) return false;
    std::memcpy(region.ptr, code, len);
    return true;
}

class MakoCodeBuffer {
public:
    explicit MakoCodeBuffer(size_t capacity = 1024) {
        m_buffer.reserve(capacity);
    }

    void emit(uint8_t b) { m_buffer.push_back(b); }

    void emit32(uint32_t val) {
        emit((uint8_t)(val & 0xFF));
        emit((uint8_t)((val >> 8) & 0xFF));
        emit((uint8_t)((val >> 16) & 0xFF));
        emit((uint8_t)((val >> 24) & 0xFF));
    }

    void emit64(uint64_t val) {
        for (int i = 0; i < 8; i++) {
            emit((uint8_t)((val >> (i * 8)) & 0xFF));
        }
    }

    size_t length() const { return m_buffer.size(); }
    const uint8_t* code() const { return m_buffer.data(); }

    void emitRex(bool needR, bool needB, bool is64Bit = false) {
        uint8_t rex = 0x40;
        if (is64Bit) rex |= 0x08;
        if (needR) rex |= 0x04;
        if (needB) rex |= 0x01;
        if (rex != 0x40 || is64Bit) emit(rex);
    }

    uint8_t modrm(int mod, int reg, int rm) {
        return (uint8_t)(((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
    }

    void emitModrmSib(int mod, int reg, int base, int disp) {
        bool needSib = (base & 7) == 4; // RSP
        emit(modrm(mod, reg, base));
        if (needSib) emit(0x24);
        if (mod == 1) emit((uint8_t)(disp & 0xFF));
        else if (mod == 2) emit32((uint32_t)disp);
    }

    void emitMovssLoad(int xmm, int base, int disp) {
        emitRex(xmm >= 8, base >= 8);
        emit(0xF3); emit(0x0F); emit(0x10);
        emitModrmSib(1, xmm, base, disp);
    }

    void emitMovssStore(int base, int disp, int xmm) {
        emitRex(xmm >= 8, base >= 8);
        emit(0xF3); emit(0x0F); emit(0x11);
        emitModrmSib(1, xmm, base, disp);
    }

    void emitMovssReg(int dst, int src) {
        emitRex(dst >= 8, src >= 8);
        emit(0xF3); emit(0x0F); emit(0x10);
        emit(modrm(3, dst, src));
    }

    void emitBinopXmm(uint8_t opcode, int dst, int src) {
        emitRex(dst >= 8, src >= 8);
        emit(0xF3); emit(0x0F); emit(opcode);
        emit(modrm(3, dst, src));
    }

    void emitXorps(int dst, int src) {
        emitRex(dst >= 8, src >= 8);
        emit(0x0F); emit(0x57);
        emit(modrm(3, dst, src));
    }

    void emitMulss(int dst, int src) { emitBinopXmm(0x59, dst, src); }
    void emitAddss(int dst, int src) { emitBinopXmm(0x58, dst, src); }
    void emitSubss(int dst, int src) { emitBinopXmm(0x5C, dst, src); }

    void emitMovImm32Mem(int base, int disp, uint32_t val) {
        emitRex(false, base >= 8);
        emit(0xC7);
        emitModrmSib(1, 0, base, disp);
        emit32(val);
    }

    void emitRet() { emit(0xC3); }

    MakoExecutableRegion finalize() {
        auto region = allocExecutable(m_buffer.size());
        if (!region.isValid()) return region;
        writeCode(region, m_buffer.data(), m_buffer.size());
        makeExecutable(region);
        return region;
    }

private:
    std::vector<uint8_t> m_buffer;
};

// JIT generator for 4x4 matrix multiplication
// C ABI: rdi = dst, rsi = a, rdx = b
inline MakoExecutableRegion genMat4Multiply() {
    MakoCodeBuffer buf(4096);
    enum { RAX=0, RCX=1, RDX=2, RBX=3, RSP=4, RBP=5, RSI=6, RDI=7 };

    for (int j = 0; j < 4; j++) {
        for (int k = 0; k < 4; k++) {
            buf.emitMovssLoad(k, RDX, j * 16 + k * 4);
        }

        for (int i = 0; i < 4; i++) {
            buf.emitMovssLoad(4, RSI, i * 4);
            buf.emitMulss(4, 0);

            buf.emitMovssLoad(5, RSI, 16 + i * 4);
            buf.emitMulss(5, 1);
            buf.emitAddss(4, 5);

            buf.emitMovssLoad(5, RSI, 32 + i * 4);
            buf.emitMulss(5, 2);
            buf.emitAddss(4, 5);

            buf.emitMovssLoad(5, RSI, 48 + i * 4);
            buf.emitMulss(5, 3);
            buf.emitAddss(4, 5);

            buf.emitMovssStore(RDI, j * 16 + i * 4, 4);
        }
    }
    buf.emitRet();
    return buf.finalize();
}

// JIT generator for 2D rotation matrix:
// C ABI: rdi = out, xmm0 = sinA, xmm1 = cosA
inline MakoExecutableRegion genComputeRotation2D() {
    MakoCodeBuffer buf(1024);
    enum { RDI=7 };
    enum { sinA=0, cosA=1, t0=2, zero=3 };

    buf.emitXorps(zero, zero);

    // Row 0: cosA, -sinA, 0, 0
    buf.emitMovssStore(RDI, 0, cosA);
    buf.emitXorps(t0, t0);
    buf.emitSubss(t0, sinA);
    buf.emitMovssStore(RDI, 4, t0);
    buf.emitMovssStore(RDI, 8, zero);
    buf.emitMovssStore(RDI, 12, zero);

    // Row 1: sinA, cosA, 0, 0
    buf.emitMovssStore(RDI, 16, sinA);
    buf.emitMovssStore(RDI, 20, cosA);
    buf.emitMovssStore(RDI, 24, zero);
    buf.emitMovssStore(RDI, 28, zero);

    // Row 2: 0, 0, 1, 0
    buf.emitMovssStore(RDI, 32, zero);
    buf.emitMovssStore(RDI, 36, zero);
    buf.emitMovImm32Mem(RDI, 40, 0x3F800000); // 1.0f
    buf.emitMovssStore(RDI, 44, zero);

    // Row 3: 0, 0, 0, 1
    buf.emitMovssStore(RDI, 48, zero);
    buf.emitMovssStore(RDI, 52, zero);
    buf.emitMovssStore(RDI, 56, zero);
    buf.emitMovImm32Mem(RDI, 60, 0x3F800000); // 1.0f

    buf.emitRet();
    return buf.finalize();
}

// JIT generator for vectorized pixel clearing (zeroing out alpha and color)
// C ABI: rdi = pixels, rsi = numBytes
inline MakoExecutableRegion genClearPixels() {
    MakoCodeBuffer buf(512);
    // xorps xmm0, xmm0
    buf.emit(0x0F); buf.emit(0x57); buf.emit(0xC0);
    // cmp rsi, 16
    buf.emit(0x48); buf.emit(0x83); buf.emit(0xFE); buf.emit(0x10);
    // jb tail
    buf.emit(0x72); buf.emit(0x0F);

    // loop:
    // movups [rdi], xmm0
    buf.emit(0x0F); buf.emit(0x11); buf.emit(0x07);
    // add rdi, 16
    buf.emit(0x48); buf.emit(0x83); buf.emit(0xC7); buf.emit(0x10);
    // sub rsi, 16
    buf.emit(0x48); buf.emit(0x83); buf.emit(0xEE); buf.emit(0x10);
    // cmp rsi, 16
    buf.emit(0x48); buf.emit(0x83); buf.emit(0xFE); buf.emit(0x10);
    // jae loop (jump back 15 bytes)
    buf.emit(0x73); buf.emit(0xF0);

    // tail:
    // test rsi, rsi
    buf.emit(0x48); buf.emit(0x85); buf.emit(0xF6);
    // jz end
    buf.emit(0x74); buf.emit(0x0A);
    // byte_loop:
    // mov byte ptr [rdi], 0
    buf.emit(0xC6); buf.emit(0x07); buf.emit(0x00);
    // inc rdi
    buf.emit(0x48); buf.emit(0xFF); buf.emit(0xC7);
    // dec rsi
    buf.emit(0x48); buf.emit(0xFF); buf.emit(0xCE);
    // jnz byte_loop (jump back 8 bytes)
    buf.emit(0x75); buf.emit(0xF6);

    // end:
    buf.emitRet();
    return buf.finalize();
}

// Fallback software implementations

inline void fallbackMat4Mul(float* dst, const float* a, const float* b) {
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            dst[j * 4 + i] = a[i] * b[j * 4 + 0] +
                             a[4 + i] * b[j * 4 + 1] +
                             a[8 + i] * b[j * 4 + 2] +
                             a[12 + i] * b[j * 4 + 3];
        }
    }
}

inline void fallbackRotation2D(float* out, float sinA, float cosA) {
    out[0]  =  cosA; out[1]  = -sinA; out[2]  = 0.0f; out[3]  = 0.0f;
    out[4]  =  sinA; out[5]  =  cosA; out[6]  = 0.0f; out[7]  = 0.0f;
    out[8]  =  0.0f; out[9]  =  0.0f; out[10] = 1.0f; out[11] = 0.0f;
    out[12] =  0.0f; out[13] =  0.0f; out[14] = 0.0f; out[15] = 1.0f;
}

inline void fallbackOrtho2D(float* out, float left, float right, float bottom, float top) {
    for (int i = 0; i < 16; i++) out[i] = 0.0f;
    out[0]  =  2.0f / (right - left);
    out[5]  =  2.0f / (top - bottom);
    out[10] = -1.0f;
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[15] =  1.0f;
}

inline void fallbackClearPixels(uint8_t* pixels, size_t numBytes) {
    std::memset(pixels, 0, numBytes);
}

// ── Stamp kernel fallbacks (always compiled, used as hot-swap targets) ──────────

// Paint kernel: alpha-blend srcColor into the circular region on the canvas.
// This is what the "brush stroke renderer" does when in Paint mode.
inline void fallbackPaintStampKernel(uint8_t* pixels, int width, int height,
                                      int cx, int cy, int radius,
                                      uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!pixels || width <= 0 || height <= 0 || radius <= 0 || a == 0) return;
    int r2 = radius * radius;
    float sa = a / 255.0f;
    for (int dy = -radius; dy <= radius; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= height) continue;
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int px = cx + dx;
                if (px < 0 || px >= width) continue;
                size_t off = ((size_t)py * width + px) * 4;
                uint8_t* dst = pixels + off;
                float da = dst[3] / 255.0f;
                float outA = sa + da * (1.0f - sa);
                if (outA < 0.001f) {
                    dst[0] = dst[1] = dst[2] = dst[3] = 0;
                } else {
                    dst[0] = (uint8_t)((r * sa + dst[0] * da * (1.0f - sa)) / outA);
                    dst[1] = (uint8_t)((g * sa + dst[1] * da * (1.0f - sa)) / outA);
                    dst[2] = (uint8_t)((b * sa + dst[2] * da * (1.0f - sa)) / outA);
                    dst[3] = (uint8_t)(outA * 255.0f);
                }
            }
        }
    }
}

// Erase kernel: zero RGBA for every pixel in the circular region.
// This is what the brush stroke renderer is hot-swapped to in Eraser mode.
// The "rendered" brush stroke shape is the same; only the per-pixel operation changes.
inline void fallbackEraseStampKernel(uint8_t* pixels, int width, int height,
                                      int cx, int cy, int radius,
                                      uint8_t /*r*/, uint8_t /*g*/, uint8_t /*b*/, uint8_t /*a*/)
{
    if (!pixels || width <= 0 || height <= 0 || radius <= 0) return;
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= height) continue;
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int px = cx + dx;
                if (px < 0 || px >= width) continue;
                size_t off = ((size_t)py * width + px) * 4;
                pixels[off + 0] = 0;
                pixels[off + 1] = 0;
                pixels[off + 2] = 0;
                pixels[off + 3] = 0;
            }
        }
    }
}

// ── MakoJitPipeline ─────────────────────────────────────────────────────────────

class MakoJitPipeline {
public:
    MakoJitPipeline() = default;
    ~MakoJitPipeline() { shutdown(); }

    void initialize() {
        printf("[MakoRender JIT] Initializing x86-64 SIMD pipeline...\n");
        m_regionMat4Mul = genMat4Multiply();
        m_regionRotation2D = genComputeRotation2D();
        m_regionClearPixels = genClearPixels();

        if (m_regionMat4Mul.isValid()) {
            m_fnMat4Mul = m_regionMat4Mul.getFunction<Mat4MulFn>();
            printf("[MakoRender JIT] Loaded JIT mat4Multiply (SSE vectorization)\n");
        } else {
            m_fnMat4Mul = &fallbackMat4Mul;
            printf("[MakoRender JIT] Using fallback mat4Multiply\n");
        }

        if (m_regionRotation2D.isValid()) {
            m_fnRotation2D = m_regionRotation2D.getFunction<Rotation2DFn>();
            printf("[MakoRender JIT] Loaded JIT computeRotation2D\n");
        } else {
            m_fnRotation2D = &fallbackRotation2D;
            printf("[MakoRender JIT] Using fallback computeRotation2D\n");
        }

        if (m_regionClearPixels.isValid()) {
            m_fnClearPixels = m_regionClearPixels.getFunction<ClearPixelsFn>();
            printf("[MakoRender JIT] Loaded JIT clearPixels (SIMD vector zeroing)\n");
        } else {
            m_fnClearPixels = &fallbackClearPixels;
            printf("[MakoRender JIT] Using fallback clearPixels\n");
        }

        m_fnOrtho2D = &fallbackOrtho2D;

        // Both stamp kernels start as fallbacks; hot-swap selects between them.
        m_fnPaintStamp = &fallbackPaintStampKernel;
        m_fnEraseStamp = &fallbackEraseStampKernel;

        // Active stamp kernel starts as paint (default tool).
        m_fnActiveStamp = m_fnPaintStamp;

        m_generation = 1;
        m_eraserActive = false;
        printf("[MakoRender JIT] Stamp kernels loaded: Paint + Erase (hot-swappable)\n");
    }

    void shutdown() {
        freeExecutable(m_regionMat4Mul);
        freeExecutable(m_regionRotation2D);
        freeExecutable(m_regionClearPixels);
        m_fnMat4Mul = nullptr;
        m_fnRotation2D = nullptr;
        m_fnClearPixels = nullptr;
        m_fnOrtho2D = nullptr;
        m_fnPaintStamp = nullptr;
        m_fnEraseStamp = nullptr;
        m_fnActiveStamp = nullptr;
    }

    // MakoRender Hot-swap: swaps the active canvas rendering stamp kernel at runtime.
    // In Paint mode  → m_fnActiveStamp = m_fnPaintStamp  (alpha-blend brush onto canvas)
    // In Eraser mode → m_fnActiveStamp = m_fnEraseStamp  (zero-out canvas pixels in circle)
    // handleDrawing() always calls activeStamp() — no tool-branching needed in the application.
    void hotSwapEraser(bool enableEraser) {
        if (m_eraserActive == enableEraser) return;
        m_eraserActive = enableEraser;
        m_generation++;
        m_swapCount++;

        if (m_eraserActive) {
            m_fnActiveStamp = m_fnEraseStamp;
            printf("[MakoRender JIT] Hot-swap -> ERASE KERNEL (gen=%u, swaps=%u): "
                   "brush stroke renderer now writes zeros to canvas pixels\n",
                   m_generation, m_swapCount);
        } else {
            m_fnActiveStamp = m_fnPaintStamp;
            printf("[MakoRender JIT] Hot-swap -> PAINT KERNEL (gen=%u, swaps=%u): "
                   "brush stroke renderer now alpha-blends onto canvas pixels\n",
                   m_generation, m_swapCount);
        }
    }

    // Returns the currently hot-loaded stamp kernel.
    // handleDrawing() calls this for every stamp regardless of tool mode —
    // the pipeline hot-swap determines what actually happens to the canvas pixels.
    StampKernelFn activeStamp() const {
        return m_fnActiveStamp ? m_fnActiveStamp : &fallbackPaintStampKernel;
    }

    // Individual kernel accessors (for clear-all operations etc.)
    StampKernelFn paintStamp() const  { return m_fnPaintStamp  ? m_fnPaintStamp  : &fallbackPaintStampKernel; }
    StampKernelFn eraseStamp() const  { return m_fnEraseStamp  ? m_fnEraseStamp  : &fallbackEraseStampKernel; }
    ClearPixelsFn clearPixels() const { return m_fnClearPixels ? m_fnClearPixels : &fallbackClearPixels; }

    Mat4MulFn    mat4Mul()    const { return m_fnMat4Mul    ? m_fnMat4Mul    : &fallbackMat4Mul; }
    Rotation2DFn rotation2D() const { return m_fnRotation2D ? m_fnRotation2D : &fallbackRotation2D; }
    Ortho2DFn    ortho2D()    const { return m_fnOrtho2D    ? m_fnOrtho2D    : &fallbackOrtho2D; }

    bool     isEraserActive() const { return m_eraserActive; }
    uint32_t generation()     const { return m_generation; }
    uint32_t swapCount()      const { return m_swapCount; }

private:
    MakoExecutableRegion m_regionMat4Mul;
    MakoExecutableRegion m_regionRotation2D;
    MakoExecutableRegion m_regionClearPixels;

    Mat4MulFn    m_fnMat4Mul    = nullptr;
    Rotation2DFn m_fnRotation2D = nullptr;
    ClearPixelsFn m_fnClearPixels = nullptr;
    Ortho2DFn    m_fnOrtho2D    = nullptr;

    StampKernelFn m_fnPaintStamp  = nullptr;  // Paint: alpha-blend onto canvas
    StampKernelFn m_fnEraseStamp  = nullptr;  // Erase: zero RGBA on canvas
    StampKernelFn m_fnActiveStamp = nullptr;  // Hot-swapped pointer: either paint or erase

    uint32_t m_generation  = 0;
    uint32_t m_swapCount   = 0;
    bool     m_eraserActive = false;
};

} // namespace MakoRender
