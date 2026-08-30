// raw_renderer.h - draws the software rasterizer's colour buffer straight to the
// screen as pixels, bypassing the ASCII stage entirely.
//
// This is the "what is actually being rendered" view: same geometry, same
// lighting, no character quantisation. Useful for judging the 3D on its own
// terms, and for telling apart a rendering bug from an ASCII-stage bug.
#pragma once

#include <cstdint>
#include <vector>

#include "../core/raster.h"
#include "gl.h"

namespace sb {

class RawRenderer {
public:
    bool init(const char** errorOut);
    void shutdown();

    void draw(const Rasterizer& raster, int viewportW, int viewportH, float gamma = 2.2f);

private:
    gl::GLuint program_ = 0;
    gl::GLuint vao_ = 0;
    gl::GLuint texture_ = 0;
    int texW_ = 0, texH_ = 0;
    gl::GLint uTex_ = -1;
    std::vector<uint8_t> pixels_;
};

} // namespace sb
