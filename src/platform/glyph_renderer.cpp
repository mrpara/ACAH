#include "glyph_renderer.h"

#include <cstdio>
#include <cstring>

namespace sb {

namespace {

const char* kVertexShader = R"(#version 330 core
layout(location = 0) in uvec2 aCell;
layout(location = 1) in uint  aGlyph;
layout(location = 2) in vec3  aColor;
layout(location = 3) in vec3  aBackground;

uniform vec2 uViewport;    // framebuffer size in pixels
uniform vec2 uCellSize;    // character cell size in pixels
uniform vec2 uOrigin;      // pixel offset of the grid's top-left corner
uniform vec4 uAtlasGrid;   // cols, rows, slot width in UV, slot height in UV
uniform vec4 uGlyphUV;     // glyph width/height in UV, padding in UV

out vec3 vColor;
out vec3 vBackground;
out vec2 vUV;

void main() {
    // One quad per instance, corners generated from gl_VertexID as a strip.
    vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));

    vec2 pixel = uOrigin + (vec2(aCell) + corner) * uCellSize;
    gl_Position = vec4(pixel.x / uViewport.x * 2.0 - 1.0,
                       1.0 - pixel.y / uViewport.y * 2.0,
                       0.0, 1.0);

    float g = float(aGlyph);
    float col = mod(g, uAtlasGrid.x);
    float row = floor(g / uAtlasGrid.x);
    vUV = vec2(col, row) * uAtlasGrid.zw + uGlyphUV.zw + corner * uGlyphUV.xy;
    vColor = aColor;
    vBackground = aBackground;
}
)";

const char* kFragmentShader = R"(#version 330 core
in vec3 vColor;
in vec3 vBackground;
in vec2 vUV;

uniform sampler2D uAtlas;
uniform float uScanline;   // 0 = off
uniform float uGlow;       // extra brightness, fakes phosphor bloom

out vec4 oColor;

void main() {
    float a = texture(uAtlas, vUV).r;

    // Blend the glyph over the cell's own background rather than discarding, so
    // background modes can paint the cell solid. With a black background this is
    // identical to the old behaviour.
    vec3 c = mix(vBackground, vColor * (1.0 + uGlow), a);

    // Darken alternate physical scanlines a touch. Subtle on purpose: at high
    // resolutions a strong scanline just looks like the image is broken.
    float line = 1.0 - uScanline * step(1.0, mod(gl_FragCoord.y, 2.0));
    oColor = vec4(c * line, 1.0);
}
)";

bool compile(gl::GLenum type, const char* src, gl::GLuint& out, char* log, int logSize) {
    out = gl::CreateShader(type);
    gl::ShaderSource(out, 1, &src, nullptr);
    gl::CompileShader(out);
    gl::GLint ok = 0;
    gl::GetShaderiv(out, gl::COMPILE_STATUS, &ok);
    if (!ok) {
        gl::GetShaderInfoLog(out, logSize, nullptr, log);
        return false;
    }
    return true;
}

} // namespace

bool GlyphRenderer::buildProgram(const char** errorOut) {
    static char log[2048];
    gl::GLuint vs = 0, fs = 0;
    if (!compile(gl::VERTEX_SHADER, kVertexShader, vs, log, sizeof(log))) {
        std::fprintf(stderr, "vertex shader: %s\n", log);
        if (errorOut) *errorOut = "vertex shader failed to compile";
        return false;
    }
    if (!compile(gl::FRAGMENT_SHADER, kFragmentShader, fs, log, sizeof(log))) {
        std::fprintf(stderr, "fragment shader: %s\n", log);
        if (errorOut) *errorOut = "fragment shader failed to compile";
        return false;
    }
    program_ = gl::CreateProgram();
    gl::AttachShader(program_, vs);
    gl::AttachShader(program_, fs);
    gl::LinkProgram(program_);
    gl::GLint ok = 0;
    gl::GetProgramiv(program_, gl::LINK_STATUS, &ok);
    gl::DeleteShader(vs);
    gl::DeleteShader(fs);
    if (!ok) {
        gl::GetProgramInfoLog(program_, sizeof(log), nullptr, log);
        std::fprintf(stderr, "link: %s\n", log);
        if (errorOut) *errorOut = "shader program failed to link";
        return false;
    }

    uViewport_   = gl::GetUniformLocation(program_, "uViewport");
    uCellSize_   = gl::GetUniformLocation(program_, "uCellSize");
    uOrigin_     = gl::GetUniformLocation(program_, "uOrigin");
    uAtlasGrid_  = gl::GetUniformLocation(program_, "uAtlasGrid");
    uGlyphUV_    = gl::GetUniformLocation(program_, "uGlyphUV");
    uAtlas_      = gl::GetUniformLocation(program_, "uAtlas");
    uScanline_   = gl::GetUniformLocation(program_, "uScanline");
    uGlow_       = gl::GetUniformLocation(program_, "uGlow");
    return true;
}

bool GlyphRenderer::init(const char** errorOut) {
    if (!buildProgram(errorOut)) return false;

    gl::GenVertexArrays(1, &vao_);
    gl::BindVertexArray(vao_);
    gl::GenBuffers(1, &vbo_);
    gl::BindBuffer(gl::ARRAY_BUFFER, vbo_);

    const gl::GLsizei stride = sizeof(Instance);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribIPointer(0, 2, gl::UNSIGNED_SHORT, stride,
                             reinterpret_cast<const void*>(offsetof(Instance, cx)));
    gl::VertexAttribDivisor(0, 1);

    gl::EnableVertexAttribArray(1);
    gl::VertexAttribIPointer(1, 1, gl::UNSIGNED_BYTE, stride,
                             reinterpret_cast<const void*>(offsetof(Instance, glyph)));
    gl::VertexAttribDivisor(1, 1);

    gl::EnableVertexAttribArray(2);
    gl::VertexAttribPointer(2, 3, gl::UNSIGNED_BYTE, 1 /*normalized*/, stride,
                            reinterpret_cast<const void*>(offsetof(Instance, r)));
    gl::VertexAttribDivisor(2, 1);

    gl::EnableVertexAttribArray(3);
    gl::VertexAttribPointer(3, 3, gl::UNSIGNED_BYTE, 1 /*normalized*/, stride,
                            reinterpret_cast<const void*>(offsetof(Instance, br)));
    gl::VertexAttribDivisor(3, 1);

    gl::BindVertexArray(0);

    gl::GenTextures(1, &atlas_);
    setFont(fontLarge());
    return true;
}

void GlyphRenderer::shutdown() {
    if (vbo_) { gl::DeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { gl::DeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (atlas_) { gl::DeleteTextures(1, &atlas_); atlas_ = 0; }
    if (program_) { gl::DeleteProgram(program_); program_ = 0; }
}

void GlyphRenderer::setFont(const FontDef& font) {
    font_ = font;
    buildAtlas();
}

void GlyphRenderer::buildAtlas() {
    if (!font_.bits) return;

    // One pixel of padding around every glyph, so bilinear sampling at the edge
    // of a cell fades to nothing instead of bleeding in the neighbouring glyph.
    slotW_ = font_.cellW + 2;
    slotH_ = font_.cellH + 2;
    atlasW_ = atlasCols_ * slotW_;
    atlasH_ = atlasRows_ * slotH_;

    std::vector<uint8_t> pixels(static_cast<size_t>(atlasW_) * atlasH_, 0);
    for (int g = 0; g < kFontGlyphCount; ++g) {
        const int col = g % atlasCols_;
        const int row = g / atlasCols_;
        const uint8_t* rows = font_.bits + static_cast<size_t>(g) * font_.cellH;
        for (int y = 0; y < font_.cellH; ++y) {
            const uint8_t bits = rows[y];
            if (!bits) continue;
            uint8_t* dst = pixels.data()
                         + static_cast<size_t>(row * slotH_ + 1 + y) * atlasW_
                         + col * slotW_ + 1;
            for (int x = 0; x < font_.cellW; ++x)
                if (bits & (0x80u >> x)) dst[x] = 255;
        }
    }

    gl::BindTexture(gl::TEXTURE_2D, atlas_);
    gl::PixelStorei(gl::UNPACK_ALIGNMENT, 1);
    gl::TexImage2D(gl::TEXTURE_2D, 0, gl::R8, atlasW_, atlasH_, 0,
                   gl::RED, gl::UNSIGNED_BYTE, pixels.data());
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR);
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR);
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE);
    gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE);
    gl::BindTexture(gl::TEXTURE_2D, 0);
}

void GlyphRenderer::draw(const AsciiFrame& frame, int viewportW, int viewportH,
                         int originX, int originY, int scale,
                         float scanlineStrength, float glowStrength) {
    if (scale < 1) scale = 1;
    if (!program_ || frame.cells.empty()) return;

    // Blank cells are the majority of a typical frame; skipping them here is
    // free and cuts the instance count roughly in half.
    instances_.clear();
    instances_.reserve(frame.cells.size());
    for (int y = 0; y < frame.h; ++y) {
        for (int x = 0; x < frame.w; ++x) {
            const Cell& c = frame.at(x, y);
            const bool blankGlyph = (c.ch == ' ') || (c.r | c.g | c.b) == 0;
            const bool blankBg = (c.br | c.bg | c.bb) == 0;
            if (blankGlyph && blankBg) continue;   // nothing to paint at all
            int code = static_cast<unsigned char>(c.ch);
            if (code < kFontFirstGlyph || code >= kFontFirstGlyph + kFontGlyphCount) continue;
            Instance inst;
            inst.cx = static_cast<uint16_t>(x);
            inst.cy = static_cast<uint16_t>(y);
            inst.glyph = static_cast<uint8_t>(code - kFontFirstGlyph);
            inst.r = c.r; inst.g = c.g; inst.b = c.b;
            inst.br = c.br; inst.bg = c.bg; inst.bb = c.bb;
            if (blankGlyph) inst.glyph = 0;        // background only
            instances_.push_back(inst);
        }
    }
    if (instances_.empty()) return;

    gl::BindVertexArray(vao_);
    gl::BindBuffer(gl::ARRAY_BUFFER, vbo_);
    const size_t bytes = instances_.size() * sizeof(Instance);
    if (bytes > vboCapacity_) {
        vboCapacity_ = bytes + bytes / 2;
        gl::BufferData(gl::ARRAY_BUFFER, static_cast<gl::GLsizeiptr>(vboCapacity_),
                       nullptr, gl::STREAM_DRAW);
    }
    // Orphan then refill: lets the driver hand us fresh storage instead of
    // stalling until last frame's draw has finished reading the old one.
    gl::BufferData(gl::ARRAY_BUFFER, static_cast<gl::GLsizeiptr>(vboCapacity_),
                   nullptr, gl::STREAM_DRAW);
    gl::BufferSubData(gl::ARRAY_BUFFER, 0, static_cast<gl::GLsizeiptr>(bytes), instances_.data());

    gl::UseProgram(program_);
    gl::Uniform2f(uViewport_, static_cast<float>(viewportW), static_cast<float>(viewportH));
    gl::Uniform2f(uCellSize_, static_cast<float>(font_.cellW * scale),
                  static_cast<float>(font_.cellH * scale));
    gl::Uniform2f(uOrigin_, static_cast<float>(originX), static_cast<float>(originY));
    gl::Uniform4f(uAtlasGrid_, static_cast<float>(atlasCols_), static_cast<float>(atlasRows_),
                  static_cast<float>(slotW_) / atlasW_, static_cast<float>(slotH_) / atlasH_);
    gl::Uniform4f(uGlyphUV_, static_cast<float>(font_.cellW) / atlasW_,
                  static_cast<float>(font_.cellH) / atlasH_,
                  1.0f / atlasW_, 1.0f / atlasH_);
    gl::Uniform1i(uAtlas_, 0);
    gl::Uniform1f(uScanline_, scanlineStrength);
    gl::Uniform1f(uGlow_, glowStrength);

    gl::ActiveTexture(gl::TEXTURE0);
    gl::BindTexture(gl::TEXTURE_2D, atlas_);

    gl::Disable(gl::BLEND);
    gl::Disable(gl::DEPTH_TEST);

    gl::DrawArraysInstanced(gl::TRIANGLE_STRIP, 0, 4, static_cast<gl::GLsizei>(instances_.size()));

    gl::BindVertexArray(0);
}

} // namespace sb
