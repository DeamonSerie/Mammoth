#include "ImageExport.hpp"
#include "../document/DrawingDocument.hpp"
#include "PsdWriter.hpp"
#include "../DebugLog.h"
#include <stb_image_write.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace ImageExport {

namespace {

// Composite a single frame into a straight-alpha RGBA buffer (no background).
bool frameCompositeRGBA(const DrawingDocument& doc, int frameIndex,
                        std::vector<uint8_t>& out, int& w, int& h) {
    const Frame* f = doc.getFrame(frameIndex);
    if (!f) return false;
    std::vector<uint8_t> comp;
    int cw, ch;
    f->compositeToBuffer(comp, cw, ch, 1.0f);
    w = doc.width();
    h = doc.height();
    out = std::move(comp);
    if (cw != w || ch != h) { w = cw; h = ch; }
    return !out.empty();
}

// Composite a single frame over an opaque white background into a
// width*height*4 RGBA buffer (alpha forced to 255). Channel layout RGBA.
bool frameOpaqueRGBA(const DrawingDocument& doc, int frameIndex,
                     std::vector<uint8_t>& out, int& w, int& h) {
    const Frame* f = doc.getFrame(frameIndex);
    if (!f) return false;
    std::vector<uint8_t> comp;
    int cw, ch;
    f->compositeToBuffer(comp, cw, ch, 1.0f);
    w = doc.width();
    h = doc.height();
    out.assign((size_t)w * (size_t)h * 4, 255);
    if (cw != w || ch != h) {
        w = cw; h = ch;
        out.assign((size_t)w * (size_t)h * 4, 255);
    }
    for (int i = 0; i < w * h; i++) {
        int sr = comp[i * 4 + 0], sg = comp[i * 4 + 1], sb = comp[i * 4 + 2], sa = comp[i * 4 + 3];
        out[i * 4 + 0] = (uint8_t)std::min(255, sr + (255 - sa) * 255 / 255);
        out[i * 4 + 1] = (uint8_t)std::min(255, sg + (255 - sa) * 255 / 255);
        out[i * 4 + 2] = (uint8_t)std::min(255, sb + (255 - sa) * 255 / 255);
        out[i * 4 + 3] = 255;
    }
    return true;
}

// ---------------------------------------------------------------------------
// GIF89a encoder (self-contained). Emits one logical screen, a looping
// extension, and one full-resolution image per frame. Each frame gets its own
// 256-colour local colour table and an LZW-compressed index stream.
// ---------------------------------------------------------------------------

// Build a local 256-colour palette + index buffer for one opaque RGB frame.
struct GifFrame { std::vector<uint8_t> palette; int paletteSize = 0; std::vector<uint8_t> indices; };

void buildLocalPalette(const uint8_t* rgb, int n, GifFrame& f) {
    std::vector<uint32_t> table;
    table.reserve(256);
    std::vector<uint32_t> remap(n);
    bool needsQuant = false;
    for (int i = 0; i < n; i++) {
        uint32_t c = ((uint32_t)rgb[i * 3] << 16) | ((uint32_t)rgb[i * 3 + 1] << 8) | rgb[i * 3 + 2];
        int idx = -1;
        for (int k = 0; k < (int)table.size(); k++) if (table[k] == c) { idx = k; break; }
        if (idx < 0) {
            if (table.size() < 256) { idx = (int)table.size(); table.push_back(c); }
            else { needsQuant = true; break; }
        }
        remap[i] = (uint32_t)idx;
    }

    if (!needsQuant) {
        f.paletteSize = (int)table.size();
        f.palette.assign((size_t)f.paletteSize * 3, 0);
        for (int k = 0; k < f.paletteSize; k++) {
            f.palette[k * 3 + 0] = (uint8_t)(table[k] >> 16);
            f.palette[k * 3 + 1] = (uint8_t)(table[k] >> 8);
            f.palette[k * 3 + 2] = (uint8_t)(table[k]);
        }
        f.indices.assign(n, 0);
        for (int i = 0; i < n; i++) f.indices[i] = (uint8_t)remap[i];
        return;
    }

    // >256 distinct colours: 4-4-4 uniform quantisation.
    f.paletteSize = 0;
    f.palette.clear();
    std::vector<std::array<int, 3>> map;
    map.reserve(256 * 3);
    f.indices.assign(n, 0);
    auto findOrAdd = [&](int r, int g, int b) -> int {
        for (int k = 0; k < (int)map.size(); k++)
            if (map[k][0] == r && map[k][1] == g && map[k][2] == b) return k;
        if (map.size() >= 256) return 0;
        map.push_back({r, g, b});
        f.palette.push_back((uint8_t)r);
        f.palette.push_back((uint8_t)g);
        f.palette.push_back((uint8_t)b);
        return (int)map.size() - 1;
    };
    for (int i = 0; i < n; i++) {
        int r = rgb[i * 3] & 0xF0, g = rgb[i * 3 + 1] & 0xF0, b = rgb[i * 3 + 2] & 0xF0;
        f.indices[i] = (uint8_t)findOrAdd(r, g, b);
    }
    f.paletteSize = (int)map.size();
}

// GIF LZW compressor. `startBits` is the pixel code size (>=2). Emits the
// packed code stream (without sub-block framing) into `out`.
void gifLZWCompress(const uint8_t* data, int count, int startBits,
                    std::vector<uint8_t>& out) {
    const int clear = 1 << startBits;
    const int end = clear + 1;
    const int maxCode = 4096;

    auto writeCode = [&](int code, int& nbits, uint32_t& acc, int& nacc) {
        acc |= ((uint32_t)code) << nacc;
        nacc += nbits;
        while (nacc >= 8) { out.push_back((uint8_t)(acc & 0xFF)); acc >>= 8; nacc -= 8; }
    };
    auto flush = [&](uint32_t acc, int nacc) {
        if (nacc > 0) out.push_back((uint8_t)(acc & 0xFF));
    };

    int nbits = startBits + 1;
    uint32_t acc = 0; int nacc = 0;
    int nextCode = end + 1;

    std::unordered_map<uint32_t, int> dict;

    writeCode(clear, nbits, acc, nacc);
    int prev = data[0];
    if (count == 1) {
        writeCode(prev, nbits, acc, nacc);
        writeCode(end, nbits, acc, nacc);
        flush(acc, nacc);
        return;
    }
    for (int i = 1; i < count; i++) {
        int c = data[i];
        uint32_t key = ((uint32_t)prev << 8) | (uint32_t)c;
        auto it = dict.find(key);
        if (it != dict.end()) {
            prev = it->second;
        } else {
            writeCode(prev, nbits, acc, nacc);
            prev = c;
            if (nextCode < maxCode) {
                dict[key] = nextCode;
                nextCode++;
                if (nextCode > (1 << nbits) && nbits < 12) nbits++;
            } else {
                // dictionary full: emit clear code and reset
                writeCode(clear, nbits, acc, nacc);
                nbits = startBits + 1;
                dict.clear();
                nextCode = end + 1;
            }
        }
    }
    writeCode(prev, nbits, acc, nacc);
    writeCode(end, nbits, acc, nacc);
    flush(acc, nacc);
}

bool writeGifFile(const std::string& path, const std::vector<std::vector<uint8_t>>& rgbFrames,
                  const std::vector<int>& delaysCs, int w, int h) {
    if (rgbFrames.empty() || w <= 0 || h <= 0) return false;
    const int nTotal = w * h;
    std::vector<GifFrame> frames;
    frames.reserve(rgbFrames.size());
    for (const auto& rgb : rgbFrames) {
        GifFrame f;
        buildLocalPalette(rgb.data(), nTotal, f);
        frames.push_back(std::move(f));
    }

    std::vector<uint8_t> out;
    const uint8_t hdr[] = {'G', 'I', 'F', '8', '9', 'a'};
    out.insert(out.end(), hdr, hdr + 6);
    // Logical screen descriptor (no global table)
    out.push_back((uint8_t)(w & 0xFF)); out.push_back((uint8_t)((w >> 8) & 0xFF));
    out.push_back((uint8_t)(h & 0xFF)); out.push_back((uint8_t)((h >> 8) & 0xFF));
    out.push_back(0x00);
    out.push_back(0x00);
    out.push_back(0x00);
    // NETSCAPE looping extension
    const uint8_t loop[] = {0x21, 0xFF, 0x0B, 'N','E','T','S','C','A','P','E','2','.','0',
                            0x03, 0x01, 0x00, 0x00, 0x00};
    out.insert(out.end(), loop, loop + sizeof(loop));

    for (int fi = 0; fi < (int)frames.size(); fi++) {
        const GifFrame& f = frames[fi];
        // Graphic control extension: delay in 1/100s, no transparency
        int d = std::max(1, delaysCs[fi]);
        out.push_back(0x21); out.push_back(0xF9); out.push_back(0x04);
        out.push_back(0x00);
        out.push_back((uint8_t)(d & 0xFF)); out.push_back((uint8_t)((d >> 8) & 0xFF));
        out.push_back(0x00);
        out.push_back(0x00);
        // Image descriptor
        out.push_back(0x2C);
        out.push_back(0x00); out.push_back(0x00); out.push_back(0x00); out.push_back(0x00);
        out.push_back((uint8_t)(w & 0xFF)); out.push_back((uint8_t)((w >> 8) & 0xFF));
        out.push_back((uint8_t)(h & 0xFF)); out.push_back((uint8_t)((h >> 8) & 0xFF));
        int psize = 1;
        while ((1 << psize) < f.paletteSize) psize++;
        if (psize < 2) psize = 2;
        int palCount = 1 << psize;
        out.push_back((uint8_t)(0x80 | (psize - 1)));
        for (int k = 0; k < palCount; k++) {
            if (k < f.paletteSize) {
                out.push_back(f.palette[k * 3 + 0]);
                out.push_back(f.palette[k * 3 + 1]);
                out.push_back(f.palette[k * 3 + 2]);
            } else {
                out.push_back(0); out.push_back(0); out.push_back(0);
            }
        }
        out.push_back((uint8_t)psize); // LZW min code size
        std::vector<uint8_t> packed;
        gifLZWCompress(f.indices.data(), nTotal, psize, packed);
        for (size_t i = 0; i < packed.size(); i += 255) {
            size_t cnt = std::min((size_t)255, packed.size() - i);
            out.push_back((uint8_t)cnt);
            out.insert(out.end(), packed.begin() + (ptrdiff_t)i,
                       packed.begin() + (ptrdiff_t)i + (ptrdiff_t)cnt);
        }
        out.push_back(0);
    }
    out.push_back(0x3B); // trailer

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write((const char*)out.data(), (std::streamsize)out.size());
    return (bool)ofs;
}

} // namespace

bool exportFramePNG(const DrawingDocument& doc, int frameIndex, const std::string& path) {
    std::vector<uint8_t> rgba;
    int w, h;
    if (!frameCompositeRGBA(doc, frameIndex, rgba, w, h)) { DebugLog::log("[Export] PNG: bad frame"); return false; }
    if (w <= 0 || h <= 0) { DebugLog::log("[Export] PNG: bad size"); return false; }
    int ok = stbi_write_png(path.c_str(), w, h, 4, rgba.data(), w * 4);
    if (!ok) { DebugLog::log("[Export] PNG: write failed"); return false; }
    DebugLog::log("[Export] Wrote PNG %dx%d frame %d", w, h, frameIndex);
    return true;
}

bool exportFrameJPG(const DrawingDocument& doc, int frameIndex, const std::string& path, int quality) {
    std::vector<uint8_t> rgba;
    int w, h;
    if (!frameOpaqueRGBA(doc, frameIndex, rgba, w, h)) { DebugLog::log("[Export] JPG: bad frame"); return false; }
    int q = std::clamp(quality, 1, 100);
    int ok = stbi_write_jpg(path.c_str(), w, h, 4, rgba.data(), q);
    if (!ok) { DebugLog::log("[Export] JPG: write failed"); return false; }
    DebugLog::log("[Export] Wrote JPG %dx%d frame %d", w, h, frameIndex);
    return true;
}

bool exportAnimationGIF(const DrawingDocument& doc, const std::string& path) {
    const int n = doc.frameCount();
    if (n <= 0) { DebugLog::log("[Export] GIF: no frames"); return false; }
    const int w = doc.width(), h = doc.height();
    if (w <= 0 || h <= 0) { DebugLog::log("[Export] GIF: bad size"); return false; }
    std::vector<std::vector<uint8_t>> frames;
    std::vector<int> delays;
    frames.reserve(n);
    delays.reserve(n);
    for (int i = 0; i < n; i++) {
        std::vector<uint8_t> rgba;
        int fw, fh;
        if (!frameOpaqueRGBA(doc, i, rgba, fw, fh)) { DebugLog::log("[Export] GIF: frame %d bad", i); return false; }
        // strip alpha -> rgb
        std::vector<uint8_t> rgb((size_t)fw * fh * 3);
        for (int p = 0; p < fw * fh; p++) {
            rgb[p * 3 + 0] = rgba[p * 4 + 0];
            rgb[p * 3 + 1] = rgba[p * 4 + 1];
            rgb[p * 3 + 2] = rgba[p * 4 + 2];
        }
        frames.push_back(std::move(rgb));
        const Frame* f = doc.getFrame(i);
        delays.push_back(f ? (int)(f->duration() * 100.0f + 0.5f) : 10);
    }
    bool ok = writeGifFile(path, frames, delays, w, h);
    if (!ok) { DebugLog::log("[Export] GIF: write failed"); return false; }
    DebugLog::log("[Export] Wrote GIF %dx%d %d frames", w, h, n);
    return true;
}

bool frameOpaqueRGB(const DrawingDocument& doc, int frameIndex,
                    std::vector<uint8_t>& rgb, int& w, int& h) {
    std::vector<uint8_t> rgba;
    if (!frameOpaqueRGBA(doc, frameIndex, rgba, w, h)) return false;
    rgb.assign((size_t)w * (size_t)h * 3, 0);
    for (int p = 0; p < w * h; p++) {
        rgb[p * 3 + 0] = rgba[p * 4 + 0];
        rgb[p * 3 + 1] = rgba[p * 4 + 1];
        rgb[p * 3 + 2] = rgba[p * 4 + 2];
    }
    return true;
}

} // namespace ImageExport
