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
};
