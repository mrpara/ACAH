// glyph_renderer.h - draws an AsciiFrame with OpenGL.
//
// The whole character grid goes out as a single instanced draw call: one quad
// instance per cell, reading its glyph and colour from a per-frame vertex
// buffer, sampling a baked font atlas. Even a 300x120 grid is one draw and
// ~290 KB of streamed instance data, so the GPU is never the bottleneck.
#pragma once

#include <cstdint>
#include <vector>

#include "../core/ascii.h"
#include "../core/font.h"
#include "gl.h"

namespace sb {

class GlyphRenderer {
public:
    bool init(const char** errorOut);
    void shutdown();

    // Rebuilds the atlas for a different font. Safe to call between frames.
    void setFont(const FontDef& font);
    const FontDef& font() const { return font_; }

    // `scale` is an integer magnification of the glyph cell, for high-DPI
    // displays where an 8x16 cell would otherwise be uncomfortably small.
    void draw(const AsciiFrame& frame, int viewportW, int viewportH,
              int originX, int originY, int scale,
              float scanlineStrength, float glowStrength);

private:
    bool buildProgram(const char** errorOut);
    void buildAtlas();

    struct Instance {
        uint16_t cx = 0, cy = 0;
        uint8_t glyph = 0;
        uint8_t r = 0, g = 0, b = 0;      // glyph colour
        uint8_t br = 0, bg = 0, bb = 0;   // background colour
        uint8_t pad = 0;
    };
    static_assert(sizeof(Instance) == 12, "instance layout must stay tightly packed");

    FontDef font_{nullptr, 0, 0, ""};
    gl::GLuint program_ = 0;
    gl::GLuint vao_ = 0;
    gl::GLuint vbo_ = 0;
    gl::GLuint atlas_ = 0;
    size_t vboCapacity_ = 0;

    int atlasCols_ = 16, atlasRows_ = 6;
    int slotW_ = 0, slotH_ = 0, atlasW_ = 0, atlasH_ = 0;

    gl::GLint uViewport_ = -1;
    gl::GLint uCellSize_ = -1;
    gl::GLint uOrigin_ = -1;
    gl::GLint uAtlasGrid_ = -1;
    gl::GLint uGlyphUV_ = -1;
    gl::GLint uAtlas_ = -1;
    gl::GLint uScanline_ = -1;
    gl::GLint uGlow_ = -1;

    std::vector<Instance> instances_;
};

} // namespace sb
