#include "PsdCodec.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>

namespace Psd {

std::vector<uint8_t> packBits(const uint8_t* data, int len) {
    std::vector<uint8_t> out;
    if (len <= 0) return out;
    out.reserve((size_t)len + (size_t)len / 8 + 8);
    int i = 0;
    while (i < len) {
        int run = 1;
        while (i + run < len && data[i] == data[i + run] && run < 128)
            run++;
        if (run >= 3) {
            out.push_back((uint8_t)(257 - run));
            out.push_back(data[i]);
            i += run;
            continue;
        }
        int start = i;
        int lit = 0;
        while (i < len && lit < 128) {
            int r = 1;
            while (i + r < len && data[i] == data[i + r] && r < 128) r++;
            if (r >= 3) break;
            i++;
            lit++;
        }
        if (lit == 0) continue;
        out.push_back((uint8_t)(lit - 1));
        out.insert(out.end(), data + start, data + start + lit);
    }
    return out;
}

bool unpackBits(const uint8_t* src, int srcLen, uint8_t* dst, int dstLen) {
    int si = 0, di = 0;
    while (di < dstLen) {
        if (si >= srcLen) return false;
        int n = src[si++];
        if (n < 128) {
            int count = n + 1;
            if (si + count > srcLen || di + count > dstLen) return false;
            std::memcpy(dst + di, src + si, (size_t)count);
            si += count;
            di += count;
        } else if (n > 128) {
            int count = 257 - n;
            if (si >= srcLen || di + count > dstLen) return false;
            uint8_t v = src[si++];
            std::memset(dst + di, v, (size_t)count);
            di += count;
        }
        // 128 = nop
    }
    return di == dstLen;
}

void splitPlanes(const uint8_t* rgba, int canvasW, int canvasH,
                  int left, int top, int width, int height,
                  std::vector<uint8_t>& r, std::vector<uint8_t>& g,
                  std::vector<uint8_t>& b, std::vector<uint8_t>& a) {
    const int n = width * height;
    r.assign(n, 0);
    g.assign(n, 0);
    b.assign(n, 0);
    a.assign(n, 0);
    if (n <= 0) return;
    for (int y = 0; y < height; y++) {
        int cy = top + y;
        if (cy < 0 || cy >= canvasH) continue;
        for (int x = 0; x < width; x++) {
            int cx = left + x;
            if (cx < 0 || cx >= canvasW) continue;
            size_t off = ((size_t)cy * (size_t)canvasW + (size_t)cx) * 4;
            int di = y * width + x;
            r[di] = rgba[off + 0];
            g[di] = rgba[off + 1];
            b[di] = rgba[off + 2];
            a[di] = rgba[off + 3];
        }
    }
}

void findOpaqueBounds(const uint8_t* rgba, int w, int h,
                       int& left, int& top, int& right, int& bottom) {
    left = w;
    top = h;
    right = 0;
    bottom = 0;
    bool any = false;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (rgba[((size_t)y * w + x) * 4 + 3] == 0) continue;
            any = true;
            if (x < left) left = x;
            if (y < top) top = y;
            if (x + 1 > right) right = x + 1;
            if (y + 1 > bottom) bottom = y + 1;
        }
    }
    if (!any) {
        left = top = right = bottom = 0;
    }
}

std::vector<uint8_t> compressChannelRLE(const uint8_t* plane, int width, int height) {
    ByteWriter w;
    if (width <= 0 || height <= 0) {
        w.u16(1); // RLE, zero rows
        return w.buf;
    }
    std::vector<std::vector<uint8_t>> rows((size_t)height);
    std::vector<uint16_t> counts((size_t)height);
    bool tooBig = false;
    for (int y = 0; y < height; y++) {
        rows[y] = packBits(plane + y * width, width);
        if (rows[y].size() > 65535) { tooBig = true; break; }
        counts[y] = (uint16_t)rows[y].size();
    }
    if (tooBig) {
        // Raw fallback
        w.u16(0);
        w.raw(plane, (size_t)width * (size_t)height);
        return w.buf;
    }
    w.u16(1);
    for (int y = 0; y < height; y++) w.u16(counts[y]);
    for (int y = 0; y < height; y++)
        w.raw(rows[y].data(), rows[y].size());
    return w.buf;
}

bool decompressChannel(const uint8_t* src, int srcLen, int width, int height,
                       std::vector<uint8_t>& out) {
    const int n = width * height;
    out.assign(n > 0 ? n : 0, 0);
    if (srcLen < 2) return n == 0;
    ByteReader r{src, (size_t)srcLen, 0};
    uint16_t comp = r.u16();
    if (n == 0) return true;
    if (comp == 0) {
        if (!r.need((size_t)n)) return false;
        std::memcpy(out.data(), r.ptr(), (size_t)n);
        return true;
    }
    if (comp != 1) return false; // zip not supported
    if (!r.need((size_t)height * 2)) return false;
    std::vector<uint16_t> counts((size_t)height);
    size_t total = 0;
    for (int y = 0; y < height; y++) {
        counts[y] = r.u16();
        total += counts[y];
    }
    if (!r.need(total)) return false;
    const uint8_t* packed = r.ptr();
    int off = 0;
    for (int y = 0; y < height; y++) {
        if (!unpackBits(packed + off, counts[y], out.data() + y * width, width))
            return false;
        off += counts[y];
    }
    return true;
}

static void writeStr(ByteWriter& w, const std::string& s) {
    w.u32((uint32_t)s.size());
    w.raw(s.data(), s.size());
}
static std::string readStr(ByteReader& r) {
    uint32_t n = r.u32();
    if (!r.need(n)) return {};
    std::string s((const char*)r.ptr(), n);
    r.skip(n);
    return s;
}

std::vector<uint8_t> encodeMeta(const MammothMeta& m) {
    ByteWriter w;
    w.str("Manm", 4);
    w.u32((uint32_t)m.formatVersion);
    w.u32(1); // blob layout version
    writeStr(w, m.docName);
    w.i32(m.width);
    w.i32(m.height);

    w.i32(m.brushType);
    w.f32(m.brushSize);
    w.f32(m.brushOpacity);
    w.f32(m.brushHardness);
    w.u8(m.customEnabled ? 1 : 0);
    writeStr(w, m.customBrush);
    w.f32(m.pressureSize);
    w.f32(m.pressureOpacity);
    w.f32(m.pressureEase);
    w.u8(m.pressureEnabled ? 1 : 0);

    w.u32((uint32_t)m.frames.size());
    for (const auto& f : m.frames) {
        writeStr(w, f.name);
        w.f32(f.duration);
        w.f32(f.opacity);
        w.u8(f.visible ? 1 : 0);
        w.u32((uint32_t)f.layers.size());
        for (const auto& l : f.layers) {
            w.u8(l.isAttribute ? 1 : 0);
            w.i32(l.attrSource);
            w.f32(l.attrOpacity);
            w.u32(l.attrTint);
            w.u32(l.color);
        }
    }

    w.u32((uint32_t)m.frameGroups.size());
    for (const auto& g : m.frameGroups) {
        writeStr(w, g.name);
        w.u32(g.color);
        w.u8(g.collapsed ? 1 : 0);
        w.u32((uint32_t)g.frameIndices.size());
        for (int idx : g.frameIndices) w.i32(idx);
    }
    return w.buf;
}

bool decodeMeta(const uint8_t* data, size_t n, MammothMeta& out) {
    if (n < 12) return false;
    ByteReader r{data, n, 0};
    if (r.u8() != 'M' || r.u8() != 'a' || r.u8() != 'n' || r.u8() != 'm')
        return false;
    out.formatVersion = (int)r.u32();
    uint32_t blob = r.u32();
    if (blob != 1) return false;
    out.docName = readStr(r);
    out.width = r.i32();
    out.height = r.i32();
    out.brushType = r.i32();
    out.brushSize = r.f32();
    out.brushOpacity = r.f32();
    out.brushHardness = r.f32();
    out.customEnabled = r.u8() != 0;
    out.customBrush = readStr(r);
    out.pressureSize = r.f32();
    out.pressureOpacity = r.f32();
    out.pressureEase = r.f32();
    out.pressureEnabled = r.u8() != 0;

    uint32_t nf = r.u32();
    out.frames.clear();
    out.frames.resize(nf);
    for (auto& f : out.frames) {
        f.name = readStr(r);
        f.duration = r.f32();
        f.opacity = r.f32();
        f.visible = r.u8() != 0;
        uint32_t nl = r.u32();
        f.layers.resize(nl);
        for (auto& l : f.layers) {
            l.isAttribute = r.u8() != 0;
            l.attrSource = r.i32();
            l.attrOpacity = r.f32();
            l.attrTint = r.u32();
            l.color = r.u32();
        }
    }
    uint32_t ng = r.u32();
    out.frameGroups.resize(ng);
    for (auto& g : out.frameGroups) {
        g.name = readStr(r);
        g.color = r.u32();
        g.collapsed = r.u8() != 0;
        uint32_t ni = r.u32();
        g.frameIndices.resize(ni);
        for (uint32_t i = 0; i < ni; i++)
            g.frameIndices[i] = r.i32();
    }
    return r.ok();
}

static uint32_t crc32(const uint8_t* data, size_t n) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++)
        c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static uint32_t adler32(const uint8_t* data, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

static void pngChunk(std::vector<uint8_t>& f, const char* type, const uint8_t* data, uint32_t len) {
    auto be32 = [&](uint32_t v) {
        f.push_back((uint8_t)(v >> 24));
        f.push_back((uint8_t)(v >> 16));
        f.push_back((uint8_t)(v >> 8));
        f.push_back((uint8_t)v);
    };
    be32(len);
    size_t typeAt = f.size();
    f.push_back((uint8_t)type[0]);
    f.push_back((uint8_t)type[1]);
    f.push_back((uint8_t)type[2]);
    f.push_back((uint8_t)type[3]);
    f.insert(f.end(), data, data + len);
    uint32_t c = crc32(f.data() + typeAt, 4 + len);
    be32(c);
}

bool writePngRGBA(const std::string& path, const uint8_t* rgba, int w, int h) {
    if (w <= 0 || h <= 0 || !rgba) return false;
    // Uncompressed scanlines: filter 0 + RGBA
    std::vector<uint8_t> raw((size_t)h * (1 + (size_t)w * 4));
    for (int y = 0; y < h; y++) {
        raw[(size_t)y * (1 + (size_t)w * 4)] = 0;
        std::memcpy(raw.data() + (size_t)y * (1 + (size_t)w * 4) + 1,
                    rgba + (size_t)y * w * 4, (size_t)w * 4);
    }
    // zlib stored blocks
    std::vector<uint8_t> z;
    z.push_back(0x78);
    z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t chunk = std::min((size_t)65535, raw.size() - pos);
        bool last = (pos + chunk >= raw.size());
        z.push_back(last ? 1 : 0);
        uint16_t len = (uint16_t)chunk;
        uint16_t nlen = (uint16_t)~len;
        z.push_back((uint8_t)len);
        z.push_back((uint8_t)(len >> 8));
        z.push_back((uint8_t)nlen);
        z.push_back((uint8_t)(nlen >> 8));
        z.insert(z.end(), raw.begin() + (ptrdiff_t)pos,
                 raw.begin() + (ptrdiff_t)pos + (ptrdiff_t)chunk);
        pos += chunk;
    }
    uint32_t ad = adler32(raw.data(), raw.size());
    z.push_back((uint8_t)(ad >> 24));
    z.push_back((uint8_t)(ad >> 16));
    z.push_back((uint8_t)(ad >> 8));
    z.push_back((uint8_t)ad);

    std::vector<uint8_t> file;
    const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    file.insert(file.end(), sig, sig + 8);

    uint8_t ihdr[13] = {};
    auto putbe = [](uint8_t* d, uint32_t v) {
        d[0] = (uint8_t)(v >> 24); d[1] = (uint8_t)(v >> 16);
        d[2] = (uint8_t)(v >> 8); d[3] = (uint8_t)v;
    };
    putbe(ihdr, (uint32_t)w);
    putbe(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;
    ihdr[9] = 6; // RGBA
    pngChunk(file, "IHDR", ihdr, 13);
    pngChunk(file, "IDAT", z.data(), (uint32_t)z.size());
    pngChunk(file, "IEND", ihdr, 0);

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write((const char*)file.data(), (std::streamsize)file.size());
    return (bool)out;
}

void resizeNearest(const uint8_t* src, int sw, int sh,
                    uint8_t* dst, int dw, int dh) {
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            const uint8_t* s = src + ((size_t)sy * sw + sx) * 4;
            uint8_t* d = dst + ((size_t)y * dw + x) * 4;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        }
    }
}

} // namespace Psd
