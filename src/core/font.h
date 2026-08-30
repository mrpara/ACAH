// font.h - access to the baked bitmap fonts plus a CPU blitter that turns an
// AsciiFrame into RGB pixels. The GL layer uploads the same bits as a texture;
// the headless tool uses the blitter to write image files.
#pragma once

#include <cstdint>
#include <vector>
#include "ascii.h"
#include "font_data.h"

namespace sb {

struct FontDef {
    const uint8_t* bits;
    int cellW;
    int cellH;
    const char* name;
};

inline FontDef fontLarge() { return {kFont8x16Bits, kFont8x16Width, kFont8x16Height, "8x16"}; }
inline FontDef fontSmall() { return {kFont6x12Bits, kFont6x12Width, kFont6x12Height, "6x12"}; }
inline FontDef fontTiny()  { return {kFont4x8Bits,  kFont4x8Width,  kFont4x8Height,  "4x8"}; }

inline const uint8_t* glyphRows(const FontDef& font, char ch) {
    int code = static_cast<unsigned char>(ch);
    if (code < kFontFirstGlyph || code >= kFontFirstGlyph + kFontGlyphCount) code = ' ';
    return font.bits + static_cast<size_t>(code - kFontFirstGlyph) * font.cellH;
}

// Renders the character grid into a tightly packed 24-bit RGB image.
inline void blitFrameToRGB(const AsciiFrame& frame, const FontDef& font,
                           std::vector<uint8_t>& outRGB, int& outW, int& outH) {
    outW = frame.w * font.cellW;
    outH = frame.h * font.cellH;
    outRGB.assign(static_cast<size_t>(outW) * outH * 3, 0);

    for (int cy = 0; cy < frame.h; ++cy) {
        for (int cx = 0; cx < frame.w; ++cx) {
            const Cell& cell = frame.at(cx, cy);
            const uint8_t* rows = glyphRows(font, cell.ch);
            const bool hasBg = (cell.br | cell.bg | cell.bb) != 0;
            for (int y = 0; y < font.cellH; ++y) {
                const uint8_t bitsRow = rows[y];
                if (!bitsRow && !hasBg) continue;
                uint8_t* dst = outRGB.data()
                             + (static_cast<size_t>(cy * font.cellH + y) * outW + cx * font.cellW) * 3;
                for (int x = 0; x < font.cellW; ++x) {
                    const bool ink = (bitsRow & (0x80u >> x)) != 0;
                    dst[x * 3 + 0] = ink ? cell.r : cell.br;
                    dst[x * 3 + 1] = ink ? cell.g : cell.bg;
                    dst[x * 3 + 2] = ink ? cell.b : cell.bb;
                }
            }
        }
    }
}

} // namespace sb
