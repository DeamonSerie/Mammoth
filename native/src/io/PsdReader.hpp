#pragma once
#include "PsdCodec.hpp"
#include "../document/DrawingDocument.hpp"
#include <string>

class PsdReader {
public:
    // Load a PSD into `out`. Pixels, layer groups, and frame groups come from
    // the layer stack; Mammoth private data (when present) restores extras.
    static bool read(const std::string& path, DrawingDocument& out,
                      Psd::MammothMeta* meta = nullptr);
};
