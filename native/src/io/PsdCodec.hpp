#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

// Shared PSD binary helpers, PackBits, Mammoth private-data, and PNG thumbs.
namespace Psd {

constexpr uint16_t RESOURCE_MAMMOTH = 4000;
constexpr const char* RESOURCE_NAME = "Mammoth";

struct ByteWriter {
    std::vector<uint8_t> buf;

    void u8(uint8_t v) { buf.push_back(v); }
    void u16(uint16_t v) {
        buf.push_back((uint8_t)(v >> 8));
        buf.push_back((uint8_t)(v));
    }
    void u32(uint32_t v) {
        buf.push_back((uint8_t)(v >> 24));
        buf.push_back((uint8_t)(v >> 16));
        buf.push_back((uint8_t)(v >> 8));
        buf.push_back((uint8_t)(v));
    }
    void i16(int16_t v) { u16((uint16_t)v); }
    void i32(int32_t v) { u32((uint32_t)v); }
    void f32(float v) {
        uint32_t u;
        std::memcpy(&u, &v, 4);
        // store as little-endian IEEE in private data; PSD public fields use BE
        buf.push_back((uint8_t)u);
        buf.push_back((uint8_t)(u >> 8));
        buf.push_back((uint8_t)(u >> 16));
        buf.push_back((uint8_t)(u >> 24));
    }
    void raw(const void* p, size_t n) {
        if (n == 0) return;
        const uint8_t* b = (const uint8_t*)p;
        buf.insert(buf.end(), b, b + n);
    }
    void str(const char* s, size_t n) { raw(s, n); }
    void padTo(size_t align) {
        while (buf.size() % align) buf.push_back(0);
    }
    void padEven() { if (buf.size() & 1) buf.push_back(0); }
};

struct ByteReader {
    const uint8_t* p = nullptr;
    size_t n = 0;
    size_t i = 0;

    bool ok() const { return i <= n; }
    size_t remain() const { return i < n ? n - i : 0; }
    bool need(size_t k) const { return i + k <= n; }

    uint8_t u8() {
        if (!need(1)) { i = n + 1; return 0; }
        return p[i++];
    }
    uint16_t u16() {
        if (!need(2)) { i = n + 1; return 0; }
        uint16_t v = ((uint16_t)p[i] << 8) | p[i + 1];
        i += 2;
        return v;
    }
    uint32_t u32() {
        if (!need(4)) { i = n + 1; return 0; }
        uint32_t v = ((uint32_t)p[i] << 24) | ((uint32_t)p[i + 1] << 16) |
                     ((uint32_t)p[i + 2] << 8) | p[i + 3];
        i += 4;
        return v;
    }
    int16_t i16() { return (int16_t)u16(); }
    int32_t i32() { return (int32_t)u32(); }
    float f32() {
        if (!need(4)) { i = n + 1; return 0; }
        uint32_t u = (uint32_t)p[i] | ((uint32_t)p[i + 1] << 8) |
                     ((uint32_t)p[i + 2] << 16) | ((uint32_t)p[i + 3] << 24);
        i += 4;
        float v;
        std::memcpy(&v, &u, 4);
        return v;
    }
    void skip(size_t k) { i = (i + k > n) ? n + 1 : i + k; }
    const uint8_t* ptr() const { return p + i; }
};

std::vector<uint8_t> packBits(const uint8_t* data, int len);
bool unpackBits(const uint8_t* src, int srcLen, uint8_t* dst, int dstLen);

// Split interleaved RGBA into planar R,G,B,A (tight bounds).
void splitPlanes(const uint8_t* rgba, int canvasW, int canvasH,
                  int left, int top, int width, int height,
                  std::vector<uint8_t>& r, std::vector<uint8_t>& g,
                  std::vector<uint8_t>& b, std::vector<uint8_t>& a);
void findOpaqueBounds(const uint8_t* rgba, int w, int h,
                       int& left, int& top, int& right, int& bottom);

// RLE-compress one planar channel (PSD layer-channel payload including
// the 2-byte compression code). Returns the payload; `outLen` is its size.
std::vector<uint8_t> compressChannelRLE(const uint8_t* plane, int width, int height);

bool decompressChannel(const uint8_t* src, int srcLen, int width, int height,
                       std::vector<uint8_t>& out);

// Mammoth private resource (Image Resource 4000).
struct MammothMeta {
    int formatVersion = 1;
    std::string docName;
    int width = 0, height = 0;

    int brushType = 0;
    float brushSize = 12.0f;
    float brushOpacity = 1.0f;
    float brushHardness = 0.8f;
    bool customEnabled = false;
    std::string customBrush;
    float pressureSize = 0.85f;
    float pressureOpacity = 0.70f;
    float pressureEase = 1.80f;
    bool pressureEnabled = true;

    struct FrameExtra {
        std::string name;
        float duration = 1.0f;
        float opacity = 1.0f;
        bool visible = true;
        struct LayerExtra {
            bool isAttribute = false;
            int attrSource = -1;
            float attrOpacity = 1.0f;
            uint32_t attrTint = 0;
            uint32_t color = 0xFFFFFFFF;
        };
        std::vector<LayerExtra> layers;
    };
    std::vector<FrameExtra> frames;

    struct FrameGroupExtra {
        std::string name;
        uint32_t color = 0;
        bool collapsed = false;
        std::vector<int> frameIndices;
    };
    std::vector<FrameGroupExtra> frameGroups;

    // In-frame layer-group colors, one entry per frame in paint order
    // (bottom group first) — the same order the writer emits group markers
    // and the reader restores them. Group colors are not part of the PSD
    // lsct records, so they travel in this private block. Empty for files
    // written before this field existed.
    std::vector<std::vector<uint32_t>> frameLayerGroupColors;
};

std::vector<uint8_t> encodeMeta(const MammothMeta& m);
bool decodeMeta(const uint8_t* data, size_t n, MammothMeta& out);

bool writePngRGBA(const std::string& path, const uint8_t* rgba, int w, int h);
void resizeNearest(const uint8_t* src, int sw, int sh,
                    uint8_t* dst, int dw, int dh);

} // namespace Psd
