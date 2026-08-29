#include "PsdReader.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace {

struct LayerRecord {
    int top = 0, left = 0, bottom = 0, right = 0;
    std::string name;
    uint8_t opacity = 255;
    bool visible = true;
    int divider = 0;
    struct Channel { int16_t id; uint32_t length; };
    std::vector<Channel> channels;
    std::vector<uint8_t> planes[4];
};

// An in-frame layer group recovered from the PSD lsct group markers that
// PsdWriter emits (closer + members + named opener per group). Both
// `frame` and `members` are resolved during the read pass below.
struct RestoredGroup {
    std::string name;
    bool collapsed = false;
    int frame = -1;
    std::vector<size_t> members;   // global raster record indices (0 = background)
};

// Mirrors kLayerTagPalette in ui/MainWindow.cpp (A<<24 | B<<16 | G<<8 | R).
// Group colors are persisted in the Mammoth meta since format support was
// added; this palette is only the fallback for files written before that.
static uint32_t restoredGroupColor(int index) {
    static const uint32_t palette[9] = {
        0xFFFFFFFFu, 0xFF3440EBu, 0xFF23A6F5u, 0xFF0AD6FFu, 0xFF71CC2Eu,
        0xFF9CBC1Au, 0xFFDB9834u, 0xFFB6599Bu, 0xFF8C2CE9u,
    };
    return palette[((index % 9) + 9) % 9];
}

static std::string readPascal4(Psd::ByteReader& r) {
    size_t start = r.i;
    uint8_t len = r.u8();
    if (!r.need(len)) return {};
    std::string s((const char*)r.ptr(), len);
    r.skip(len);
    size_t used = r.i - start;
    r.skip((4 - (used % 4)) % 4);
    return s;
}

static std::string readUnicode(const uint8_t* d, size_t n) {
    Psd::ByteReader r{d, n, 0};
    uint32_t chars = r.u32();
    std::string out;
    for (uint32_t i = 0; i < chars && r.need(2); ++i) {
        uint16_t c = r.u16();
        if (c < 0x80) out.push_back((char)c);
        else if (c < 0x800) { out.push_back((char)(0xc0 | (c >> 6))); out.push_back((char)(0x80 | (c & 63))); }
        else { out.push_back((char)(0xe0 | (c >> 12))); out.push_back((char)(0x80 | ((c >> 6) & 63))); out.push_back((char)(0x80 | (c & 63))); }
    }
    return out;
}

static bool parseExtra(Psd::ByteReader& r, LayerRecord& lr) {
    uint32_t mask = r.u32(); r.skip(mask);
    uint32_t blend = r.u32(); r.skip(blend);
    lr.name = readPascal4(r);
    while (r.ok() && r.remain() >= 12) {
        char sig[5] = {}, key[5] = {};
        for (int i = 0; i < 4; ++i) sig[i] = (char)r.u8();
        for (int i = 0; i < 4; ++i) key[i] = (char)r.u8();
        uint32_t len = r.u32();
        if (!r.need(len)) return false;
        const uint8_t* data = r.ptr();
        if ((std::memcmp(sig, "8BIM", 4) == 0 || std::memcmp(sig, "8B64", 4) == 0)) {
            if (std::memcmp(key, "luni", 4) == 0) lr.name = readUnicode(data, len);
            else if (std::memcmp(key, "lsct", 4) == 0 && len >= 4) {
                Psd::ByteReader q{data, len, 0}; lr.divider = (int)q.u32();
            }
        }
        r.skip(len);
        if (len & 1) r.skip(1);
    }
    return r.ok();
}

static bool readResources(Psd::ByteReader& r, Psd::MammothMeta* meta) {
    uint32_t size = r.u32();
    if (!r.need(size)) return false;
    Psd::ByteReader res{r.ptr(), size, 0}; r.skip(size);
    while (res.ok() && res.remain() >= 12) {
        res.skip(4); // 8BIM
        uint16_t id = res.u16();
        uint8_t nameLen = res.u8(); res.skip(nameLen);
        if ((1 + nameLen) & 1) res.skip(1);
        uint32_t len = res.u32();
        if (!res.need(len)) return false;
        if (id == Psd::RESOURCE_MAMMOTH && meta) Psd::decodeMeta(res.ptr(), len, *meta);
        res.skip(len); if (len & 1) res.skip(1);
    }
    return r.ok();
}

}

bool PsdReader::read(const std::string& path, DrawingDocument& out, Psd::MammothMeta* meta) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    Psd::ByteReader r{bytes.data(), bytes.size(), 0};
    char signature[5] = {}; for (int i=0;i<4;i++) signature[i]=(char)r.u8();
    if (std::memcmp(signature, "8BPS", 4) != 0 || r.u16() != 1) return false;
    r.skip(6); uint16_t channels = r.u16(); int height = (int)r.u32(); int width = (int)r.u32();
    if (channels < 3 || width <= 0 || height <= 0 || r.u16() != 8 || r.u16() != 3) return false;
    uint32_t modeSize = r.u32(); r.skip(modeSize);
    Psd::MammothMeta localMeta;
    if (!readResources(r, &localMeta)) return false;
    if (meta) *meta = localMeta;
    uint32_t lmSize = r.u32();
    if (!r.need(lmSize)) return false;
    Psd::ByteReader lm{r.ptr(), lmSize, 0}; r.skip(lmSize);
    uint32_t layerInfoSize = lm.u32();
    if (!lm.need(layerInfoSize) || layerInfoSize < 2) return false;
    Psd::ByteReader li{lm.ptr(), layerInfoSize, 0};
    int count = li.i16(); if (count < 0) count = -count;
    if (count < 0 || count > 30000) return false;
    std::vector<LayerRecord> records((size_t)count);
    for (auto& lr : records) {
        lr.top=li.i32(); lr.left=li.i32(); lr.bottom=li.i32(); lr.right=li.i32();
        uint16_t nch=li.u16();
        for (uint16_t i=0;i<nch;i++) lr.channels.push_back({li.i16(),li.u32()});
        li.skip(4); li.skip(4); lr.opacity=li.u8(); li.skip(1); lr.visible=(li.u8() & 0x02)==0; li.skip(1);
        uint32_t extra=li.u32(); if (!li.need(extra)) return false;
        Psd::ByteReader ex{li.ptr(),extra,0}; if (!parseExtra(ex,lr)) return false; li.skip(extra);
    }
    // Channel payloads follow every record, in record order.
    for (auto& lr : records) {
        int w=std::max(0,lr.right-lr.left), h=std::max(0,lr.bottom-lr.top);
        for (const auto& ch : lr.channels) {
            if (!li.need(ch.length)) return false;
            if ((ch.id >= 0 && ch.id <= 2) || ch.id == -1) {
                int dst = ch.id == -1 ? 3 : ch.id;
                if (!Psd::decompressChannel(li.ptr(), (int)ch.length, w, h, lr.planes[dst])) return false;
            }
            li.skip(ch.length);
        }
    }
    std::vector<const LayerRecord*> raster;
    for (const auto& lr : records) {
        if (lr.divider == 0 && lr.right > lr.left && lr.bottom > lr.top) raster.push_back(&lr);
    }
    DrawingDocument doc(width, height, localMeta.docName.empty() ? "Untitled Project" : localMeta.docName.c_str());
    auto copyRaster = [&](Frame* frame, Layer* l, const LayerRecord& lr) {
        int w=lr.right-lr.left, h=lr.bottom-lr.top;
        l->setName(lr.name.c_str()); l->setOpacity(lr.opacity / 255.0f); l->setVisible(lr.visible);
        uint8_t* dst=l->mutableData();
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            int dx = lr.left + x, dy = lr.top + y;
            if (dx < 0 || dy < 0 || dx >= width || dy >= height) continue;
            size_t sourceOffset = (size_t)y * w + x;
            size_t destinationOffset = ((size_t)dy * width + dx) * 4;
            dst[destinationOffset] = lr.planes[0].empty() ? 0 : lr.planes[0][sourceOffset];
            dst[destinationOffset + 1] = lr.planes[1].empty() ? 0 : lr.planes[1][sourceOffset];
            dst[destinationOffset + 2] = lr.planes[2].empty() ? 0 : lr.planes[2][sourceOffset];
            dst[destinationOffset + 3] = lr.planes[3].empty() ? 255 : lr.planes[3][sourceOffset];
        }
    };

    // Files written by Mammoth carry exact frame/layer counts in private data.
    // Use those counts to restore animation frames independently of PSD's group
    // ordering; standard PSDs fall through to a single-frame layer import.
    if (!localMeta.frames.empty()) {
        size_t rasterIndex = raster.empty() ? 0 : 1; // writer's white background
        // Frame fi's rasters occupy [frameRasterStart[fi],
        // frameRasterStart[fi] + layers.size()) in global raster-record order.
        std::vector<size_t> frameRasterStart(localMeta.frames.size(), 1);
        for (size_t i = 1; i < localMeta.frames.size(); ++i)
            frameRasterStart[i] = frameRasterStart[i-1] + localMeta.frames[i-1].layers.size();

        // In-frame layer groups are written as PSD lsct group markers but are
        // not part of the flat raster stream. Rebuild them from the marker
        // layout PsdWriter::emitFrame produces: per frame a leading
        // "</Layer group>" closer, then one block per top-level stack item
        // (a bare layer, or a group as closer + members + named opener), then
        // the frame-folder opener.
        std::vector<RestoredGroup> restored;
        {
            int phase = 0;   // 0 = expect frame-prefix closer, 1 = items,
                             // 2 = collecting a group's members
            std::vector<int> openGroups;
            size_t ri = 0;   // raster record index (0 = background)
            for (const auto& lr : records) {
                bool isRaster = lr.divider == 0 && lr.right > lr.left && lr.bottom > lr.top;
                if (isRaster) {
                    if (ri > 0 && phase == 2)
                        restored[(size_t)openGroups.back()].members.push_back(ri);
                    ++ri;
                    continue;
                }
                if (lr.divider == 3) {                 // "</Layer group>" closer
                    if (phase == 0) phase = 1;         // the frame's prefix closer
                    else {                             // starts a group block
                        restored.push_back(RestoredGroup());
                        openGroups.push_back((int)restored.size() - 1);
                        phase = 2;
                    }
                } else if (lr.divider == 1 || lr.divider == 2) {   // group/frame opener
                    if (phase == 2) {
                        RestoredGroup& g = restored[(size_t)openGroups.back()];
                        g.name = lr.name;
                        g.collapsed = lr.divider == 2;
                        if (!g.members.empty()) {
                            for (size_t f = 0; f < localMeta.frames.size(); ++f) {
                                if (g.members.back() < frameRasterStart[f] +
                                                       localMeta.frames[f].layers.size())
                                    { g.frame = (int)f; break; }
                            }
                        }
                        // Empty groups take the frame that owns the rasters
                        // still to come.
                        if (g.frame < 0)
                            g.frame = (int)localMeta.frames.size() - 1;
                        openGroups.pop_back();
                        phase = 1;
                    } else {
                        phase = 0;                     // frame-folder opener
                    }
                }
            }
        }

        for (size_t fi = 0; fi < localMeta.frames.size(); ++fi) {
            Frame* frame = fi == 0 ? doc.getFrame(0) : doc.addFrame();
            const auto& extra = localMeta.frames[fi];
            frame->setName(extra.name.c_str()); frame->setDuration(extra.duration);
            frame->setOpacity(extra.opacity); frame->setVisible(extra.visible);
            for (size_t li = 0; li < extra.layers.size() && rasterIndex < raster.size(); ++li, ++rasterIndex) {
                Layer* l = li == 0 ? frame->getLayer(0) : frame->addLayer(raster[rasterIndex]->name.c_str());
                copyRaster(frame, l, *raster[rasterIndex]);
                const auto& layerExtra = extra.layers[li];
                l->setColor(layerExtra.color);
                if (layerExtra.isAttribute) l->setAttributeLayer(true, layerExtra.attrSource);
                l->setAttrOpacity(layerExtra.attrOpacity); l->setAttrTint(layerExtra.attrTint);
            }
            size_t applied = 0;
            for (const auto& g : restored) {
                if (g.frame != (int)fi || g.name.empty()) continue;
                uint32_t color = restoredGroupColor((int)applied);
                if (fi < localMeta.frameLayerGroupColors.size() &&
                    applied < localMeta.frameLayerGroupColors[fi].size())
                    color = localMeta.frameLayerGroupColors[fi][applied];
                int gi = frame->groupCount();
                frame->addGroup(g.name.c_str(), color);
                if (g.collapsed) frame->setGroupCollapsed(gi, true);
                for (size_t m : g.members) {
                    if (m < frameRasterStart[fi]) continue;
                    size_t li = m - frameRasterStart[fi];
                    if (li < extra.layers.size() && frame->getLayer((int)li))
                        frame->addLayerToGroup((int)li, gi);
                }
                ++applied;
            }
        }
        for (const auto& group : localMeta.frameGroups) {
            doc.addFrameGroup(group.name.c_str(), group.color);
            int gi = doc.frameGroupCount() - 1;
            doc.setFrameGroupCollapsed(gi, group.collapsed);
            for (int frameIndex : group.frameIndices) doc.addFrameToGroup(frameIndex, gi);
        }
    } else {
        Frame* frame = doc.getFrame(0);
        bool firstRaster = true;
        // Standard PSD records are top-to-bottom; Frame's layers are bottom-to-top.
        for (auto it = raster.rbegin(); it != raster.rend(); ++it) {
            Layer* l = firstRaster ? frame->getLayer(0) : frame->addLayer((*it)->name.c_str());
            firstRaster = false;
            copyRaster(frame, l, **it);
        }
    }
    out = std::move(doc);
    return true;
}
