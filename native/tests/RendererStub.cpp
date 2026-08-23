// Headless stub for Renderer methods referenced by tool code under test.
// The real Renderer.cpp links against sokol/GL and is excluded from the
// headless test build.

#include "../src/rendering/Renderer.hpp"

void Renderer::queueSolidRect(float x, float y, float w, float h, Color color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}
