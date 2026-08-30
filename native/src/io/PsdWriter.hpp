#pragma once
#include "PsdCodec.hpp"
#include "../document/DrawingDocument.hpp"
#include <string>

class PsdWriter {
public:
    // Write a Photoshop-compatible PSD. Animation frames become top-level
    // layer groups. Mammoth extras (brush, attribute layers, frame groups,
    // format version) go in Image Resource 4000.
    static bool write(const DrawingDocument& doc, const std::string& path,
                       const Psd::MammothMeta& meta);

    // Write a portable PSD for importing into other programs: identical
    // structure to write(), but no Mammoth private Image Resource is embedded,
    // so no proprietary data rides along. Attribute layers and frame-group
    // colours cannot be represented in plain PSD, so they are dropped (only
    // their rasterised appearance survives via the layer compositing).
    static bool writePortable(const DrawingDocument& doc, const std::string& path);
};
