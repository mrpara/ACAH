cmake_minimum_required(VERSION 3.16)
project(SpiderBot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Everything lands in bin/ so the batch script knows where to find the exe.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY        ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG  ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

option(SPIDERBOT_BUILD_HEADLESS "Build the headless render/benchmark tool" ON)

# ---------------------------------------------------------------------------
# Core engine: pure C++17, no dependencies. Deliberately kept free of SDL and
# OpenGL so it can be compiled and tested without a display (see tools/).
# ---------------------------------------------------------------------------
add_library(spiderbot_core STATIC
    src/core/mesh.cpp
    src/core/raster.cpp
    src/core/ascii.cpp
    src/core/terrain.cpp
    src/core/obstacle.cpp
    src/core/buildings.cpp
    src/core/arena.cpp
    src/core/world.cpp
    src/core/parts.cpp
    src/core/mech.cpp
    src/core/units.cpp
    src/core/props.cpp
    src/core/mech_visual.cpp
    src/core/combat.cpp
    src/core/ai.cpp
    src/core/campaign.cpp
    src/core/store.cpp
    src/core/cabin.cpp
    src/core/game.cpp
)
target_include_directories(spiderbot_core PUBLIC src)

find_package(Threads REQUIRED)
target_link_libraries(spiderbot_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(spiderbot_core PRIVATE /W4 /permissive- /fp:fast)
    target_compile_definitions(spiderbot_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    # /GL + /LTCG on the rasterizer is worth a few percent.
    target_compile_options(spiderbot_core PRIVATE $<$<CONFIG:Release>:/O2 /Oi /GL>)
else()
    target_compile_options(spiderbot_core PRIVATE -Wall -Wextra -ffast-math)
endif()

# ---------------------------------------------------------------------------
# SDL3. Uses an installed copy if one is present, otherwise fetches and builds
# a pinned release. Static by default so the finished exe has no loose DLLs.
# ---------------------------------------------------------------------------
find_package(SDL3 QUIET CONFIG)

if(NOT SDL3_FOUND)
    message(STATUS "SDL3 not found locally - fetching release-3.4.14 (first build only)")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    set(SDL_TEST   OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL  OFF CACHE BOOL "" FORCE)
    # Overridable so an offline machine can point at a local SDL clone:
    #   cmake -B build -S . -DSPIDERBOT_SDL_REPOSITORY=/path/to/SDL
    set(SPIDERBOT_SDL_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        CACHE STRING "Git repository to fetch SDL3 from")
    FetchContent_Declare(SDL3
        GIT_REPOSITORY ${SPIDERBOT_SDL_REPOSITORY}
        GIT_TAG        release-3.4.14
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

if(TARGET SDL3::SDL3-static)
    set(SPIDERBOT_SDL_TARGET SDL3::SDL3-static)
else()
    set(SPIDERBOT_SDL_TARGET SDL3::SDL3)
endif()

# ---------------------------------------------------------------------------
# The game executable.
# ---------------------------------------------------------------------------
add_executable(spiderbot
    src/platform/main.cpp
    src/platform/gl.cpp
    src/platform/glyph_renderer.cpp
    src/platform/audio.cpp
    src/platform/raw_renderer.cpp
)
target_link_libraries(spiderbot PRIVATE spiderbot_core ${SPIDERBOT_SDL_TARGET})

if(MSVC)
    target_compile_options(spiderbot PRIVATE /W4 /permissive-)
    target_compile_definitions(spiderbot PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Console subsystem: the game prints its control list on startup.
    set_target_properties(spiderbot PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
else()
    target_compile_options(spiderbot PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# A SENDABLE executable.
#
# The point of this block: with a GCC toolchain on Windows - MinGW, MSYS2,
# UCRT64 - the default link leaves the exe importing libstdc++-6.dll,
# libgcc_s_seh-1.dll and libwinpthread-1.dll. Those live in the toolchain's
# own bin directory, which is on the PATH of the machine that BUILT the game
# and nowhere else. The result runs perfectly for the developer and dies on
# every friend's machine with "libstdc++-6.dll was not found", which is a
# miserable thing to send someone.
#
# SDL is already linked statically (SDL_STATIC above), so this is the last
# piece: link the C++ and GCC runtimes in too and the finished exe imports
# nothing but Windows itself. One file, no folder, no installer.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # Probed rather than assumed. A full `-static` is what also swallows
    # libwinpthread, but on an unusual toolchain it can fail to link, and a
    # packaging improvement that breaks the build is not an improvement. If
    # the probe fails we fall back to the two flags that always work, and
    # package.bat's import check will bundle whatever is left over.
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LINK_OPTIONS -static-libgcc -static-libstdc++ -static)
    check_cxx_source_compiles("#include <thread>\nint main(){ std::thread t([]{}); t.join(); return 0; }"
                              SPIDERBOT_FULL_STATIC_LINK)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(SPIDERBOT_FULL_STATIC_LINK)
        target_link_options(spiderbot PRIVATE
            -static-libgcc -static-libstdc++ -static)
        message(STATUS "spiderbot: fully static link - the exe is sendable on its own")
    else()
        target_link_options(spiderbot PRIVATE -static-libgcc -static-libstdc++)
        message(STATUS "spiderbot: partial static link - package.bat will bundle any remaining DLLs")
    endif()
endif()

# If SDL ended up as a shared library, drop the DLL next to the exe.
if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET spiderbot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:spiderbot>)
endif()

# ---------------------------------------------------------------------------
# Headless tool: renders scripted frames to PPM/text and benchmarks the
# rasterizer. No window, no GPU, no SDL.
# ---------------------------------------------------------------------------
if(SPIDERBOT_BUILD_HEADLESS)
    add_executable(spiderbot_headless tools/headless.cpp)
    target_link_libraries(spiderbot_headless PRIVATE spiderbot_core)
    if(MSVC)
        target_compile_definitions(spiderbot_headless PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    # The campaign regression runner. Plays missions with a scripted pilot and
    # reports what happened - the only way to test combat, AI and the economy,
    # none of which can be judged from a screenshot.
    add_executable(spiderbot_sim tools/sim.cpp)
    target_link_libraries(spiderbot_sim PRIVATE spiderbot_core)

    # A visual probe for one machine at a time, used when tuning proportions.
    add_executable(spiderbot_probe tools/probe_mech.cpp)
    target_link_libraries(spiderbot_probe PRIVATE spiderbot_core)

    # The controls harness: drives the real Game with synthetic input and
    # reports what the machine did. Movement, camera and jump bugs are close to
    # impossible to judge from a screenshot and miserable to test by hand.
    add_executable(spiderbot_ctrl tools/ctrl.cpp)
    target_link_libraries(spiderbot_ctrl PRIVATE spiderbot_core)
    if(MSVC)
        target_compile_definitions(spiderbot_sim PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_definitions(spiderbot_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_definitions(spiderbot_ctrl PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()
