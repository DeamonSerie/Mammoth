#include "PsdWriter.hpp"
#include <fstream>
#include <algorithm>

namespace {

using Psd::ByteWriter;

struct PreparedLayer {
    int top = 0, left = 0, bottom = 0, right = 0;
    std::string name;
    uint8_t opacity = 255;
    bool visible = true;
    bool isDivider = false;
    int dividerType = 0; // 1 open, 2 closed, 3 bounding
    std::vector<uint8_t> channelPayload[4];
};

static void pascalNamePadded(ByteWriter& w, const std::string& name) {
    std::string n = name.substr(0, 255);
    uint8_t len = (uint8_t)n.size();
    w.u8(len);
    w.raw(n.data(), n.size());
    size_t total = 1 + n.size();
    while (total % 4) {
        w.u8(0);
        total++;
    }
}

static void writeLuni(ByteWriter& extra, const std::string& name) {
    ByteWriter data;
    data.u32((uint32_t)name.size());
    for (unsigned char c : name) {
        data.u8(0);
        data.u8(c);
    }
    data.padEven();
    extra.str("8BIM", 4);
    extra.str("luni", 4);
    extra.u32((uint32_t)data.buf.size());
    extra.raw(data.buf.data(), data.buf.size());
}

static void writeLyid(ByteWriter& extra, uint32_t id) {
    extra.str("8BIM", 4);
    extra.str("lyid", 4);
    extra.u32(4);
    extra.u32(id);
}

static void writeLsct(ByteWriter& extra, int type) {
    extra.str("8BIM", 4);
    extra.str("lsct", 4);
    extra.u32(16);
    extra.u32((uint32_t)type);
    extra.str("8BIM", 4);
    extra.str("norm", 4);
    extra.u32(0);
}

static void emitDivider(std::vector<PreparedLayer>& layers, int type, const std::string& name) {
    PreparedLayer p;
    p.isDivider = true;
    p.dividerType = type;
    p.name = name;
    p.visible = true;
    p.opacity = 255;
    for (int c = 0; c < 4; c++)
        p.channelPayload[c] = Psd::compressChannelRLE(nullptr, 0, 0);
    layers.push_back(std::move(p));
}

static void emitRaster(std::vector<PreparedLayer>& layers, const Layer& L) {
    PreparedLayer p;
    p.name = L.name();
    p.opacity = (uint8_t)std::clamp((int)(L.opacity() * 255.0f + 0.5f), 0, 255);
    p.visible = L.visible();
    int left, top, right, bottom;
    Psd::findOpaqueBounds(L.data(), L.width(), L.height(), left, top, right, bottom);
    p.left = left;
    p.top = top;
    p.right = right;
    p.bottom = bottom;
    int w = right - left;
    int h = bottom - top;
    std::vector<uint8_t> r, g, b, a;
    Psd::splitPlanes(L.data(), L.width(), L.height(), left, top, w, h, r, g, b, a);
    const uint8_t* planes[4] = {
        w > 0 ? r.data() : nullptr,
        w > 0 ? g.data() : nullptr,
        w > 0 ? b.data() : nullptr,
        w > 0 ? a.data() : nullptr
    };
    for (int c = 0; c < 4; c++)
        p.channelPayload[c] = Psd::compressChannelRLE(planes[c], w, h);
    layers.push_back(std::move(p));
}

static std::string frameFolderName(const Frame& f, int index) {
    std::string n = f.name();
    if (n.empty()) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Frame %d", index + 1);
        n = buf;
    }
    int ms = (int)(f.duration() * 1000.0f + 0.5f);
    char suf[32];
    snprintf(suf, sizeof(suf), "_%dms", ms);
    return n + suf;
}

static void emitFrame(std::vector<PreparedLayer>& layers, const Frame& f, int index) {
    emitDivider(layers, 3, "</Layer group>");
    for (int s = 0; s < f.stackCount(); s++) {
        const Frame::StackNode& nd = f.stackNode(s);
        if (nd.isGroup) {
            const Frame::Group& g = f.getGroup(nd.index);
            emitDivider(layers, 3, "</Layer group>");
            for (int li : g.layerIndices) {
                const Layer* L = f.getLayer(li);
                if (L) emitRaster(layers, *L);
            }
            emitDivider(layers, g.collapsed ? 2 : 1, g.name);
        } else {
            const Layer* L = f.getLayer(nd.index);
            if (L) emitRaster(layers, *L);
        }
    }
    emitDivider(layers, 1, frameFolderName(f, index));
}

static void writeLayerRecord(ByteWriter& w, const PreparedLayer& p, uint32_t layerId) {
    w.i32(p.top);
    w.i32(p.left);
    w.i32(p.bottom);
    w.i32(p.right);
    w.u16(4);
    const int16_t ids[4] = {0, 1, 2, -1};
    for (int c = 0; c < 4; c++) {
        w.i16(ids[c]);
        w.u32((uint32_t)p.channelPayload[c].size());
    }
    w.str("8BIM", 4);
    w.str("norm", 4);
    w.u8(p.opacity);
    w.u8(0); // clipping
    uint8_t flags = 0x08; // useful info in bit 4 unused
    if (!p.visible) flags |= 0x02;
    w.u8(flags);
    w.u8(0);

    ByteWriter extra;
    extra.u32(0); // mask
    extra.u32(0); // blending ranges
    pascalNamePadded(extra, p.name);
    writeLuni(extra, p.name);
    writeLyid(extra, layerId);
    if (p.isDivider)
        writeLsct(extra, p.dividerType);

    w.u32((uint32_t)extra.buf.size());
    w.raw(extra.buf.data(), extra.buf.size());
}

static void writeResource(ByteWriter& w, uint16_t id, const char* name,
                           const uint8_t* data, uint32_t len) {
    w.str("8BIM", 4);
    w.u16(id);
    uint8_t nlen = (uint8_t)std::min((size_t)255, strlen(name));
    w.u8(nlen);
    w.raw(name, nlen);
    if ((1 + nlen) & 1) w.u8(0);
    w.u32(len);
    w.raw(data, len);
    if (len & 1) w.u8(0);
}

} // namespace

static bool writePsd(const DrawingDocument& doc, const std::string& path,
                     const Psd::MammothMeta* metaPtr);

bool PsdWriter::writePortable(const DrawingDocument& doc, const std::string& path) {
    return writePsd(doc, path, nullptr);
}

bool PsdWriter::write(const DrawingDocument& doc, const std::string& path,
                       const Psd::MammothMeta& meta) {
    return writePsd(doc, path, &meta);
}

static bool writePsd(const DrawingDocument& doc, const std::string& path,
                     const Psd::MammothMeta* metaPtr) {
    const int width = doc.width();
    const int height = doc.height();
    if (width <= 0 || height <= 0) return false;

    std::vector<PreparedLayer> layers;

    // White background (bottom-most)
    {
        Layer bg(width, height);
        bg.setName("Background");
        uint8_t* d = bg.mutableData();
        for (int i = 0; i < width * height; i++) {
            d[i * 4 + 0] = 255;
            d[i * 4 + 1] = 255;
            d[i * 4 + 2] = 255;
            d[i * 4 + 3] = 255;
        }
        emitRaster(layers, bg);
    }

    for (int i = 0; i < doc.frameCount(); i++) {
        const Frame* f = doc.getFrame(i);
        if (f) emitFrame(layers, *f, i);
    }

    ByteWriter file;
    file.str("8BPS", 4);
    file.u16(1);
    for (int i = 0; i < 6; i++) file.u8(0);
    file.u16(4); // RGBA
    file.u32((uint32_t)height);
    file.u32((uint32_t)width);
    file.u16(8);
    file.u16(3); // RGB

    // Color mode data
    file.u32(0);

    // Image resources: resolution + Mammoth private data
    ByteWriter res;
    uint8_t reso[16] = {};
    // 72 DPI as 16.16 fixed
    reso[0] = 0x00; reso[1] = 0x48; reso[2] = 0x00; reso[3] = 0x00;
    reso[4] = 0x00; reso[5] = 0x01;
    reso[6] = 0x00; reso[7] = 0x01;
    reso[8] = 0x00; reso[9] = 0x48; reso[10] = 0x00; reso[11] = 0x00;
    reso[12] = 0x00; reso[13] = 0x01;
    reso[14] = 0x00; reso[15] = 0x01;
    writeResource(res, 1005, "", reso, 16);

    if (metaPtr) {
        Psd::MammothMeta m = *metaPtr;
        m.width = width;
        m.height = height;
        m.docName = doc.name();
        if (m.formatVersion < 1) m.formatVersion = 1;
        std::vector<uint8_t> blob = Psd::encodeMeta(m);
        writeResource(res, Psd::RESOURCE_MAMMOTH, Psd::RESOURCE_NAME,
                      blob.data(), (uint32_t)blob.size());
    }

    file.u32((uint32_t)res.buf.size());
    file.raw(res.buf.data(), res.buf.size());

    // Layer and mask info
    ByteWriter layerSec;
    ByteWriter layerInfo;
    int count = (int)layers.size();
    layerInfo.i16((int16_t)(-count)); // negative → transparency in merged
    uint32_t layerId = 1;
    for (const auto& p : layers)
        writeLayerRecord(layerInfo, p, layerId++);
    for (const auto& p : layers) {
        for (int c = 0; c < 4; c++)
            layerInfo.raw(p.channelPayload[c].data(), p.channelPayload[c].size());
    }
    layerSec.u32((uint32_t)layerInfo.buf.size());
    layerSec.raw(layerInfo.buf.data(), layerInfo.buf.size());
    layerSec.u32(0); // global layer mask none

    file.u32((uint32_t)layerSec.buf.size());
    file.raw(layerSec.buf.data(), layerSec.buf.size());

    // Merged image: first frame composite over white, RLE
    std::vector<uint8_t> comp;
    int cw = width, ch = height;
    if (doc.frameCount() > 0 && doc.getFrame(0)) {
        doc.getFrame(0)->compositeToBuffer(comp, cw, ch, 1.0f);
    } else {
        comp.assign((size_t)width * height * 4, 255);
    }
    // Composite over white
    std::vector<uint8_t> r(width * height), g(width * height), b(width * height), a(width * height);
    for (int i = 0; i < width * height; i++) {
        uint8_t sr = (i * 4 + 3 < (int)comp.size()) ? comp[i * 4] : 255;
        uint8_t sg = (i * 4 + 3 < (int)comp.size()) ? comp[i * 4 + 1] : 255;
        uint8_t sb = (i * 4 + 3 < (int)comp.size()) ? comp[i * 4 + 2] : 255;
        uint8_t sa = (i * 4 + 3 < (int)comp.size()) ? comp[i * 4 + 3] : 255;
        // over white
        r[i] = (uint8_t)(sr + (255 - sa) * 255 / 255);
        g[i] = (uint8_t)(sg + (255 - sa) * 255 / 255);
        b[i] = (uint8_t)(sb + (255 - sa) * 255 / 255);
        a[i] = 255;
        // actually src-over white: out = src + (1-sa)*white
        r[i] = (uint8_t)std::min(255, (int)sr + ((255 - sa) * 255) / 255);
        g[i] = (uint8_t)std::min(255, (int)sg + ((255 - sa) * 255) / 255);
        b[i] = (uint8_t)std::min(255, (int)sb + ((255 - sa) * 255) / 255);
        a[i] = 255;
    }
    const uint8_t* planes[4] = {r.data(), g.data(), b.data(), a.data()};
    std::vector<std::vector<uint8_t>> rowPack[4];
    for (int c = 0; c < 4; c++) {
        rowPack[c].resize(height);
        for (int y = 0; y < height; y++)
            rowPack[c][y] = Psd::packBits(planes[c] + y * width, width);
    }
    file.u16(1); // RLE
    for (int c = 0; c < 4; c++)
        for (int y = 0; y < height; y++)
            file.u16((uint16_t)std::min(rowPack[c][y].size(), (size_t)65535));
    for (int c = 0; c < 4; c++)
        for (int y = 0; y < height; y++)
            file.raw(rowPack[c][y].data(), rowPack[c][y].size());

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write((const char*)file.buf.data(), (std::streamsize)file.buf.size());
    return (bool)out;
}
