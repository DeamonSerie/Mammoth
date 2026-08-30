#pragma once
#include <cstdint>
#include <string>
#include <vector>

class DrawingDocument;

// Frame/project image export for sharing with other programs. Single frames
// become flat raster images (PNG keeps transparency; JPEG composites over
// white). Whole projects export as a portable PSD (no Mammoth private data)
// or as an animated GIF (all frames composited over white, opaque).
namespace ImageExport {

// Export one frame as a PNG (RGBA transparency preserved). Returns false on
// invalid frame index, zero document, or write failure.
bool exportFramePNG(const DrawingDocument& doc, int frameIndex, const std::string& path);

// Export one frame as a baseline JPEG composited over white (opaque).
// quality is in [1,100]. Returns false on invalid input or write failure.
bool exportFrameJPG(const DrawingDocument& doc, int frameIndex, const std::string& path,
                    int quality = 90);

// Export every frame as an animated GIF89a. Frames are composited over white
// (opaque) with a 256-colour local palette each; all frames use the animation
// loop extension so the result plays continuously. Returns false on empty
// document or write failure.
bool exportAnimationGIF(const DrawingDocument& doc, const std::string& path);

// Composite a single frame over an opaque white background into `rgb`
// (width*height*3, RGB order). Returns false for an invalid frame index.
bool frameOpaqueRGB(const DrawingDocument& doc, int frameIndex,
                    std::vector<uint8_t>& rgb, int& w, int& h);

} // namespace ImageExport
