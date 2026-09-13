// led.h - the 3x5 dot font the cockpit's instrument legends are built from.
//
// Nothing in the cabin is allowed to be text drawn over the picture. The
// console's legends and digits are LAMPS: little emissive squares bolted to
// an instrument face, arranged in a 3-wide, 5-tall matrix per character.
// They are geometry, so they are projected, swayed, tilted and occluded
// with the panel they are screwed to, exactly like the gauges beside them.
//
// The letterforms are deliberately crude - three dots across is the width
// of a real seven-segment-era matrix module - because at the size a cabin
// panel affords, a finer face would just be mud on the character grid.
#pragma once

#include <cstdint>

namespace sb {

constexpr int kLedGlyphW = 3;
constexpr int kLedGlyphH = 5;
constexpr int kLedAdvance = 4;   // glyph columns plus the gap to the next

// Rows top to bottom; bit 2 is the leftmost dot. ASCII 32..95; anything
// outside is drawn as a space, and lowercase is folded to uppercase.
constexpr uint8_t kLedFont[64][5] = {
    {0, 0, 0, 0, 0},                    // 32 space
    {2, 2, 2, 0, 2},                    // !
    {5, 5, 0, 0, 0},                    // "
    {5, 7, 5, 7, 5},                    // #
    {3, 6, 7, 3, 6},                    // $
    {5, 1, 2, 4, 5},                    // %
    {2, 5, 2, 5, 3},                    // &
    {2, 2, 0, 0, 0},                    // '
    {1, 2, 2, 2, 1},                    // (
    {4, 2, 2, 2, 4},                    // )
    {0, 5, 2, 5, 0},                    // *
    {0, 2, 7, 2, 0},                    // +
    {0, 0, 0, 2, 4},                    // ,
    {0, 0, 7, 0, 0},                    // -
    {0, 0, 0, 0, 2},                    // .
    {1, 1, 2, 4, 4},                    // /
    {7, 5, 5, 5, 7},                    // 0
    {2, 6, 2, 2, 7},                    // 1
    {7, 1, 7, 4, 7},                    // 2
    {7, 1, 7, 1, 7},                    // 3
    {5, 5, 7, 1, 1},                    // 4
    {7, 4, 7, 1, 7},                    // 5
    {7, 4, 7, 5, 7},                    // 6
    {7, 1, 1, 1, 1},                    // 7
    {7, 5, 7, 5, 7},                    // 8
    {7, 5, 7, 1, 7},                    // 9
    {0, 2, 0, 2, 0},                    // :
    {0, 2, 0, 2, 4},                    // ;
    {1, 2, 4, 2, 1},                    // <
    {0, 7, 0, 7, 0},                    // =
    {4, 2, 1, 2, 4},                    // >
    {7, 1, 3, 0, 2},                    // ?
    {7, 5, 7, 4, 3},                    // @
    {2, 5, 7, 5, 5},                    // A
    {6, 5, 6, 5, 6},                    // B
    {3, 4, 4, 4, 3},                    // C
    {6, 5, 5, 5, 6},                    // D
    {7, 4, 6, 4, 7},                    // E
    {7, 4, 6, 4, 4},                    // F
    {3, 4, 5, 5, 3},                    // G
    {5, 5, 7, 5, 5},                    // H
    {7, 2, 2, 2, 7},                    // I
    {1, 1, 1, 5, 2},                    // J
    {5, 5, 6, 5, 5},                    // K
    {4, 4, 4, 4, 7},                    // L
    {5, 7, 7, 5, 5},                    // M
    {5, 7, 7, 7, 5},                    // N - the full diagonal, because
                                        // the narrow form is an R at this size
    {7, 5, 5, 5, 7},                    // O
    {6, 5, 6, 4, 4},                    // P
    {7, 5, 5, 7, 1},                    // Q
    {6, 5, 6, 5, 5},                    // R
    {3, 4, 2, 1, 6},                    // S
    {7, 2, 2, 2, 2},                    // T
    {5, 5, 5, 5, 7},                    // U
    {5, 5, 5, 5, 2},                    // V
    {5, 5, 7, 7, 5},                    // W
    {5, 5, 2, 5, 5},                    // X
    {5, 5, 2, 2, 2},                    // Y
    {7, 1, 2, 4, 7},                    // Z
    {3, 2, 2, 2, 3},                    // [
    {4, 4, 2, 1, 1},                    // backslash
    {6, 2, 2, 2, 6},                    // ]
    {2, 5, 0, 0, 0},                    // ^
    {0, 0, 0, 0, 7},                    // _
};

// The five dot rows for one character, or the space glyph for anything the
// matrix cannot draw.
inline const uint8_t* ledGlyph(char c) {
    int i = static_cast<unsigned char>(c);
    if (i >= 'a' && i <= 'z') i -= 32;
    if (i < 32 || i > 95) i = 32;
    return kLedFont[i - 32];
}

} // namespace sb
