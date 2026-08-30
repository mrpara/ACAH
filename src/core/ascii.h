// ascii.h - turns the rasterizer's linear-RGB sample buffer into a grid of
// coloured characters, and provides the text/HUD drawing helpers that write
// straight into that grid.
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "math3d.h"
#include "raster.h"

namespace sb {

struct Cell {
    char ch = ' ';
    uint8_t r = 0, g = 0, b = 0;      // glyph colour
    uint8_t br = 0, bg = 0, bb = 0;   // cell background colour
};

struct AsciiFrame {
    int w = 0, h = 0;
    std::vector<Cell> cells;
    // Per-cell scratch used by the conversion: the downsampled luminance and the
    // nearest depth in each cell, which the edge pass needs a second look at.
    std::vector<float> scratchLum;
    std::vector<float> scratchDepth;

    void resize(int width, int height) {
        w = width; h = height;
        cells.assign(static_cast<size_t>(w) * h, Cell{});
        scratchLum.assign(static_cast<size_t>(w) * h, 0.0f);
        scratchDepth.assign(static_cast<size_t>(w) * h, 0.0f);
    }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    Cell& at(int x, int y) { return cells[static_cast<size_t>(y) * w + x]; }
    const Cell& at(int x, int y) const { return cells[static_cast<size_t>(y) * w + x]; }
};

enum class Palette : int {
    PhosphorGreen = 0,
    Amber,
    IceBlue,
    Monochrome,
    FullColor,
    Count
};

const char* paletteName(Palette p);

enum class Ramp : int {
    Classic = 0,   // " .:-=+*#%@"
    Fine,          // finer 15-step gradient
    Minimal,       // chunky 7-step
    Count
};

const char* rampName(Ramp r);
const char* rampChars(Ramp r);

// How much colour each cell paints behind its glyph. Solid mode turns the grid
// into something closer to a low-resolution image with character texture on top,
// which reads far more cleanly at a distance than glyphs on black.
enum class Background : int {
    None = 0,
    Dim,
    Solid,
    Count
};

const char* backgroundName(Background b);

struct AsciiSettings {
    Palette palette = Palette::PhosphorGreen;
    // The finest ramp and solid backgrounds by default. Together with the
    // smallest font they turn the grid into a low-resolution image with
    // character texture on top, which is far more legible than sparse glyphs
    // on black - and legibility is what this whole renderer is for.
    Ramp ramp = Ramp::Fine;
    Background background = Background::Solid;

    // Depth-discontinuity outlining. Cells that sit in front of a much more
    // distant neighbour get pushed up the ramp, which gives objects a crisp
    // silhouette instead of letting them dissolve into the background.
    float edgeStrength = 0.42f;
    float edgeThreshold = 0.16f;
    // Deliberately below the usual 2.2. A display gamma of 2.2 compresses the
    // scene's luminance ratios into two or three glyphs; 1.3 keeps enough of
    // the range to spend the whole ramp on.
    float gamma = 1.32f;
    float brightness = 1.0f;
    // Levels, applied after gamma. With only ~10 ramp steps to spend, stretching
    // the scene's actual range across all of them is what makes the image read.
    float blackPoint = 0.105f;
    // Ground cover is close to a single luminance, so the white point mostly
    // decides which glyph the whole landscape lands on rather than how much
    // contrast the landscape has. Raising it to leave headroom at the top
    // sounds right and is a trap: it drops the terrain onto ':' and the world
    // reads as empty black. 0.315 puts the ground around mid-ramp with seven
    // glyphs still above it, which is enough headroom for the machines now
    // that structures are held down the ramp and mechs carry by far the
    // strongest rim in the scene. The black point has to stay here too: any
    // lower and the sky stops being blank and fills with punctuation.
    float whitePoint = 0.315f;
    float contrast = 1.10f;     // S-curve strength around mid grey, 1 = off
    // Ordered dither trades the clean character bands for a stippled texture.
    // Off by default: the bands are the look, and stipple reads as static.
    bool dither = false;
};

// Downsamples the rasterizer's supersampled buffer and quantises it to glyphs.
void convertToAscii(const Rasterizer& raster, const AsciiSettings& settings, AsciiFrame& out);

// ------------------------------------------------------------------- HUD text

struct TextStyle {
    Vec3 color{0.55f, 1.0f, 0.6f};
    bool transparentSpaces = true;   // spaces leave the scene visible underneath
};

void drawText(AsciiFrame& frame, int x, int y, const std::string& text, const TextStyle& style);
void drawTextRight(AsciiFrame& frame, int xRight, int y, const std::string& text, const TextStyle& style);
void drawRect(AsciiFrame& frame, int x, int y, int w, int h, const TextStyle& style);
void drawHorizontalBar(AsciiFrame& frame, int x, int y, int width, float fill01,
                       const TextStyle& on, const TextStyle& off);
void putCell(AsciiFrame& frame, int x, int y, char ch, const Vec3& color);

// Opaque dark backing over a rectangle of cells. Panels sit under every block
// of HUD text: a glyph drawn straight over a dense character field is
// unreadable no matter what colour it is, and this is the fix.
void fillPanel(AsciiFrame& frame, int x, int y, int w, int h,
               const Vec3& bg = Vec3(0.016f, 0.030f, 0.022f));

// Writes the frame as plain text (no colour) - used by the headless test tool.
std::string frameToText(const AsciiFrame& frame);

} // namespace sb
