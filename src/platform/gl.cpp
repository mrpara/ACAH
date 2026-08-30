#include "gl.h"

#include <SDL3/SDL.h>

namespace sb {
namespace gl {

#define SB_GL_DEFINE(ret, name, args) ret (*name) args = nullptr;
SB_GL_FUNCTIONS(SB_GL_DEFINE)
#undef SB_GL_DEFINE

const char* load() {
#define SB_GL_LOAD(ret, name, args)                                                    \
    name = reinterpret_cast<ret (*) args>(SDL_GL_GetProcAddress("gl" #name));          \
    if (!name) return "gl" #name;
    SB_GL_FUNCTIONS(SB_GL_LOAD)
#undef SB_GL_LOAD
    return nullptr;
}

} // namespace gl
} // namespace sb
