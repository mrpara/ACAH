// gl.h - the small slice of OpenGL 3.3 core this project actually uses,
// loaded through SDL_GL_GetProcAddress. Pulling in glad or GLEW for ~30
// entry points would be more build friction than it is worth.
#pragma once

#include <cstddef>
#include <cstdint>

namespace sb {
namespace gl {

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef float GLfloat;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef void GLvoid;

// ------------------------------------------------------------------ constants
constexpr GLenum COLOR_BUFFER_BIT      = 0x00004000;
constexpr GLenum DEPTH_BUFFER_BIT      = 0x00000100;
constexpr GLenum TRIANGLE_STRIP        = 0x0005;
constexpr GLenum ARRAY_BUFFER          = 0x8892;
constexpr GLenum STREAM_DRAW           = 0x88E0;
constexpr GLenum FLOAT                 = 0x1406;
constexpr GLenum UNSIGNED_BYTE         = 0x1401;
constexpr GLenum UNSIGNED_SHORT        = 0x1403;
constexpr GLenum FRAGMENT_SHADER       = 0x8B30;
constexpr GLenum VERTEX_SHADER         = 0x8B31;
constexpr GLenum COMPILE_STATUS        = 0x8B81;
constexpr GLenum LINK_STATUS           = 0x8B82;
constexpr GLenum TEXTURE_2D            = 0x0DE1;
constexpr GLenum TEXTURE0              = 0x84C0;
constexpr GLenum R8                    = 0x8229;
constexpr GLenum RED                   = 0x1903;
constexpr GLenum TEXTURE_MIN_FILTER    = 0x2801;
constexpr GLenum TEXTURE_MAG_FILTER    = 0x2800;
constexpr GLenum TEXTURE_WRAP_S        = 0x2802;
constexpr GLenum TEXTURE_WRAP_T        = 0x2803;
constexpr GLenum NEAREST               = 0x2600;
constexpr GLenum LINEAR                = 0x2601;
constexpr GLenum CLAMP_TO_EDGE         = 0x812F;
constexpr GLenum UNPACK_ALIGNMENT      = 0x0CF5;
constexpr GLenum PACK_ALIGNMENT        = 0x0D05;
constexpr GLenum RGB                   = 0x1907;
constexpr GLenum BLEND                 = 0x0BE2;
constexpr GLenum SRC_ALPHA             = 0x0302;
constexpr GLenum ONE_MINUS_SRC_ALPHA   = 0x0303;
constexpr GLenum DEPTH_TEST            = 0x0B71;
constexpr GLboolean FALSE_             = 0;

// ------------------------------------------------------------------ functions
// Loaded once by gl::load(); each is a plain function pointer.
#define SB_GL_FUNCTIONS(X)                                                                        \
    X(void,   Viewport,          (GLint, GLint, GLsizei, GLsizei))                                \
    X(void,   ClearColor,        (GLfloat, GLfloat, GLfloat, GLfloat))                            \
    X(void,   Clear,             (GLbitfield))                                                    \
    X(void,   Enable,            (GLenum))                                                        \
    X(void,   Disable,           (GLenum))                                                        \
    X(void,   BlendFunc,         (GLenum, GLenum))                                                \
    X(void,   PixelStorei,       (GLenum, GLint))                                                 \
    X(GLuint, CreateShader,      (GLenum))                                                        \
    X(void,   ShaderSource,      (GLuint, GLsizei, const GLchar* const*, const GLint*))           \
    X(void,   CompileShader,     (GLuint))                                                        \
    X(void,   GetShaderiv,       (GLuint, GLenum, GLint*))                                        \
    X(void,   GetShaderInfoLog,  (GLuint, GLsizei, GLsizei*, GLchar*))                            \
    X(void,   DeleteShader,      (GLuint))                                                        \
    X(GLuint, CreateProgram,     (void))                                                          \
    X(void,   AttachShader,      (GLuint, GLuint))                                                \
    X(void,   LinkProgram,       (GLuint))                                                        \
    X(void,   GetProgramiv,      (GLuint, GLenum, GLint*))                                        \
    X(void,   GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))                            \
    X(void,   UseProgram,        (GLuint))                                                        \
    X(void,   DeleteProgram,     (GLuint))                                                        \
    X(GLint,  GetUniformLocation,(GLuint, const GLchar*))                                         \
    X(void,   Uniform1i,         (GLint, GLint))                                                  \
    X(void,   Uniform1f,         (GLint, GLfloat))                                                \
    X(void,   Uniform2f,         (GLint, GLfloat, GLfloat))                                       \
    X(void,   Uniform4f,         (GLint, GLfloat, GLfloat, GLfloat, GLfloat))                     \
    X(void,   GenVertexArrays,   (GLsizei, GLuint*))                                              \
    X(void,   BindVertexArray,   (GLuint))                                                        \
    X(void,   DeleteVertexArrays,(GLsizei, const GLuint*))                                        \
    X(void,   GenBuffers,        (GLsizei, GLuint*))                                              \
    X(void,   BindBuffer,        (GLenum, GLuint))                                                \
    X(void,   BufferData,        (GLenum, GLsizeiptr, const void*, GLenum))                       \
    X(void,   BufferSubData,     (GLenum, GLintptr, GLsizeiptr, const void*))                     \
    X(void,   DeleteBuffers,     (GLsizei, const GLuint*))                                        \
    X(void,   EnableVertexAttribArray, (GLuint))                                                  \
    X(void,   VertexAttribPointer,  (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))     \
    X(void,   VertexAttribIPointer, (GLuint, GLint, GLenum, GLsizei, const void*))                \
    X(void,   VertexAttribDivisor,  (GLuint, GLuint))                                             \
    X(void,   DrawArraysInstanced,  (GLenum, GLint, GLsizei, GLsizei))                            \
    X(void,   GenTextures,       (GLsizei, GLuint*))                                              \
    X(void,   BindTexture,       (GLenum, GLuint))                                                \
    X(void,   TexImage2D,        (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void,   TexParameteri,     (GLenum, GLenum, GLint))                                         \
    X(void,   ActiveTexture,     (GLenum))                                                        \
    X(void,   DeleteTextures,    (GLsizei, const GLuint*))                                     \
    X(void,   ReadPixels,        (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))

#define SB_GL_DECLARE(ret, name, args) extern ret (*name) args;
SB_GL_FUNCTIONS(SB_GL_DECLARE)
#undef SB_GL_DECLARE

// Resolves every entry point. Returns the name of the first one that failed,
// or nullptr on success.
const char* load();

} // namespace gl
} // namespace sb
