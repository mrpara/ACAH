#include "raw_renderer.h"

#include <cmath>
#include <cstdio>

namespace sb {

namespace {

const char* kVertexShader = R"(#version 330 core
out vec2 vUV;
void main() {
    // Fullscreen triangle generated from gl_VertexID - no vertex buffer needed.
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUV = vec2(p.x, 1.0 - p.y);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* kFragmentShader = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 oColor;
void main() {
    oColor = vec4(texture(uTex, vUV).rgb, 1.0);
}
)";

bool compile(gl::GLenum type, const char* src, gl::GLuint& out, char* log, int logSize) {
    out = gl::CreateShader(type);
    gl::ShaderSource(out, 1, &src, nullptr);
    gl::CompileShader(out);
    gl::GLint ok = 0;
    gl::GetShaderiv(out, gl::COMPILE_STATUS, &ok);
    if (!ok) { gl::GetShaderInfoLog(out, logSize, nullptr, log); return false; }
    return true;
}

} // namespace

bool RawRenderer::init(const char** errorOut) {
    static char log[1024];
    gl::GLuint vs = 0, fs = 0;
    if (!compile(gl::VERTEX_SHADER, kVertexShader, vs, log, sizeof(log)) ||
        !compile(gl::FRAGMENT_SHADER, kFragmentShader, fs, log, sizeof(log))) {
        std::fprintf(stderr, "raw renderer shader: %s\n", log);
        if (errorOut) *errorOut = "raw renderer shader failed to compile";
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
        std::fprintf(stderr, "raw renderer link: %s\n", log);
        if (errorOut) *errorOut = "raw renderer program failed to link";
        return false;
    }
    uTex_ = gl::GetUniformLocation(program_, "uTex");

    gl::GenVertexArrays(1, &vao_);
    gl::GenTextures(1, &texture_);
    return true;
}

void RawRenderer::shutdown() {
    if (texture_) { gl::DeleteTextures(1, &texture_); texture_ = 0; }
    if (vao_) { gl::DeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (program_) { gl::DeleteProgram(program_); program_ = 0; }
}

void RawRenderer::draw(const Rasterizer& raster, int viewportW, int viewportH, float gamma) {
    const int w = raster.sampleWidth();
    const int h = raster.sampleHeight();
    if (w <= 0 || h <= 0 || !program_) return;

    const std::vector<Vec3>& src = raster.colorBuffer();
    pixels_.resize(static_cast<size_t>(w) * h * 3);

    // The rasterizer works in linear light; encode for display on the way out.
    const float invGamma = 1.0f / std::max(gamma, 0.05f);
    for (size_t i = 0; i < src.size(); ++i) {
        const Vec3& c = src[i];
        const float ch[3] = {c.x, c.y, c.z};
        for (int k = 0; k < 3; ++k) {
            const float v = std::pow(clampf(ch[k], 0.0f, 1.0f), invGamma);
            pixels_[i * 3 + static_cast<size_t>(k)] =
                static_cast<uint8_t>(clampf(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }

    gl::BindTexture(gl::TEXTURE_2D, texture_);
    gl::PixelStorei(gl::UNPACK_ALIGNMENT, 1);
    if (w != texW_ || h != texH_) {
        gl::TexImage2D(gl::TEXTURE_2D, 0, gl::RGB, w, h, 0, gl::RGB, gl::UNSIGNED_BYTE, pixels_.data());
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR);
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR);
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE);
        gl::TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE);
        texW_ = w; texH_ = h;
    } else {
        gl::TexImage2D(gl::TEXTURE_2D, 0, gl::RGB, w, h, 0, gl::RGB, gl::UNSIGNED_BYTE, pixels_.data());
    }

    gl::Viewport(0, 0, viewportW, viewportH);
    gl::UseProgram(program_);
    gl::ActiveTexture(gl::TEXTURE0);
    gl::BindTexture(gl::TEXTURE_2D, texture_);
    gl::Uniform1i(uTex_, 0);
    gl::Disable(gl::BLEND);
    gl::Disable(gl::DEPTH_TEST);
    gl::BindVertexArray(vao_);
    gl::DrawArraysInstanced(gl::TRIANGLE_STRIP, 0, 3, 1);
    gl::BindVertexArray(0);
}

} // namespace sb
