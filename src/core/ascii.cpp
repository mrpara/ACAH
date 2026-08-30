#include "ascii.h"

#include <cstring>
#include "font_data.h"

namespace sb {

namespace {

// Ramps chosen so measured ink coverage rises in even steps - see
// buildRampLut() below for why that matters more than the character choice.
const char* kRampClassic = " .:;=ix%#@";
const char* kRampFine    = " ',:;=c<kYm0O8B@";
const char* kRampMinimal = " ,=rm#@";

// Standard 4x4 Bayer matrix, scaled to [-0.5, 0.5) in convertToAscii.
const float kBayer4[16] = {
     0.0f,  8.0f,  2.0f, 10.0f,
    12.0f,  4.0f, 14.0f,  6.0f,
     3.0f, 11.0f,  1.0f,  9.0f,
    15.0f,  7.0f, 13.0f,  5.0f
};

struct PaletteDef {
    const char* name;
    Vec3 tint;
    float floorLevel;   // how much colour a dim cell keeps
};

const PaletteDef kPalettes[] = {
    {"PHOSPHOR", Vec3(0.24f, 1.00f, 0.38f), 0.42f},
    {"AMBER",    Vec3(1.00f, 0.66f, 0.16f), 0.42f},
    {"ICE",      Vec3(0.38f, 0.86f, 1.00f), 0.42f},
    {"MONO",     Vec3(0.90f, 0.94f, 0.92f), 0.40f},
    {"FULL RGB", Vec3(1.00f, 1.00f, 1.00f), 0.36f},
};

inline uint8_t toByte(float v) {
    const int i = static_cast<int>(clampf(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    return static_cast<uint8_t>(i);
}

// --------------------------------------------------------- ramp calibration
//
// Picking a glyph by its position in the ramp string assumes the characters are
// evenly spaced in perceived density, and they are not: '-' inks about 4% of its
// cell while '@' inks nearly 40%. Mapping by *measured* ink coverage instead is
// the single biggest legibility win in the whole ASCII stage - without it the
// midtones collapse into what looks like empty space.

struct RampLut {
    const char* ramp = nullptr;
    int len = 0;
    uint8_t index[256] = {};    // luminance byte -> position in `ramp`
};

inline int popcount8(uint8_t v) {
    int n = 0;
    while (v) { n += (v & 1u); v >>= 1; }
    return n;
}

RampLut buildRampLut(const char* ramp) {
    RampLut lut;
    lut.ramp = ramp;
    lut.len = static_cast<int>(std::strlen(ramp));
    if (lut.len <= 0) return lut;

    float coverage[64] = {};
    const int n = std::min(lut.len, 64);
    const float cellArea = static_cast<float>(kFont8x16Width * kFont8x16Height);
    float maxCov = 0.0f;
    for (int i = 0; i < n; ++i) {
        int code = static_cast<unsigned char>(ramp[i]);
        if (code < kFontFirstGlyph || code >= kFontFirstGlyph + kFontGlyphCount) code = ' ';
        const uint8_t* rows = kFont8x16Bits
                            + static_cast<size_t>(code - kFontFirstGlyph) * kFont8x16Height;
        int bits = 0;
        for (int y = 0; y < kFont8x16Height; ++y) bits += popcount8(rows[y]);
        coverage[i] = bits / cellArea;
        maxCov = std::max(maxCov, coverage[i]);
    }
    if (maxCov <= 0.0f) maxCov = 1.0f;

    for (int l = 0; l < 256; ++l) {
        const float want = (l / 255.0f) * maxCov;
        int best = 0;
        float bestErr = 1e9f;
        for (int i = 0; i < n; ++i) {
            const float err = std::fabs(coverage[i] - want);
            if (err < bestErr) { bestErr = err; best = i; }
        }
        lut.index[l] = static_cast<uint8_t>(best);
    }
    return lut;
}

const RampLut& rampLut(Ramp r) {
    static const RampLut classic = buildRampLut(kRampClassic);
    static const RampLut fine    = buildRampLut(kRampFine);
    static const RampLut minimal = buildRampLut(kRampMinimal);
    switch (r) {
        case Ramp::Fine:    return fine;
        case Ramp::Minimal: return minimal;
        case Ramp::Classic:
        default:            return classic;
    }
}

} // namespace

const char* paletteName(Palette p) {
    const int i = static_cast<int>(p);
    return (i >= 0 && i < static_cast<int>(Palette::Count)) ? kPalettes[i].name : "?";
}

const char* backgroundName(Background b) {
    switch (b) {
        case Background::None:  return "NONE";
        case Background::Dim:   return "DIM";
        case Background::Solid: return "SOLID";
        default:                return "?";
    }
}

const char* rampName(Ramp r) {
    switch (r) {
        case Ramp::Classic: return "CLASSIC";
        case Ramp::Fine:    return "FINE";
        case Ramp::Minimal: return "MINIMAL";
        default:            return "?";
    }
}

const char* rampChars(Ramp r) {
    switch (r) {
        case Ramp::Fine:    return kRampFine;
        case Ramp::Minimal: return kRampMinimal;
        case Ramp::Classic:
        default:            return kRampClassic;
    }
}

void convertToAscii(const Rasterizer& raster, const AsciiSettings& settings, AsciiFrame& out) {
    const int cx = raster.cellsX();
    const int cy = raster.cellsY();
    const int ss = raster.supersample();
    const int sw = raster.sampleWidth();
    if (out.w != cx || out.h != cy) out.resize(cx, cy);

    const std::vector<Vec3>& src = raster.colorBuffer();
    const RampLut& lut = rampLut(settings.ramp);
    const char* ramp = lut.ramp;
    const float rampMax = static_cast<float>(std::max(lut.len - 1, 1));
    const float invSamples = 1.0f / static_cast<float>(ss * ss);
    const float invGamma = 1.0f / std::max(settings.gamma, 0.05f);
    const PaletteDef& pal = kPalettes[static_cast<int>(settings.palette)];
    const bool fullColor = (settings.palette == Palette::FullColor);
    const float invRange = 1.0f / std::max(settings.whitePoint - settings.blackPoint, 1e-3f);
    // One dither step is half a ramp step, which is exactly enough to break the
    // flat bands without making the image look noisy.
    const float ditherScale = settings.dither ? (1.0f / rampMax) : 0.0f;

    const std::vector<float>& depthSrc = raster.depthBuffer();

    // ---- pass 1: downsample colour and depth, apply the tone curve ----------
    for (int y = 0; y < cy; ++y) {
        for (int x = 0; x < cx; ++x) {
            Vec3 sum(0.0f);
            float nearest = 0.0f;      // depth is 1/w, so larger is closer
            for (int sy = 0; sy < ss; ++sy) {
                const size_t base = static_cast<size_t>(y * ss + sy) * sw + x * ss;
                const Vec3* row = src.data() + base;
                const float* drow = depthSrc.data() + base;
                for (int sx = 0; sx < ss; ++sx) {
                    sum += row[sx];
                    nearest = std::max(nearest, drow[sx]);
                }
            }
            Vec3 rgb = sum * invSamples * settings.brightness;

            // No clamp here any more: the tone curve below needs to see how
            // far over the white point a surface actually is.
            const float linearLum = std::max(
                0.2126f * rgb.x + 0.7152f * rgb.y + 0.0722f * rgb.z, 0.0f);
            float lum = std::pow(linearLum, invGamma);

            // Levels, then a highlight shoulder, then a symmetric S-curve.
            //
            // The shoulder is the important one and it is new. The levels
            // stretch maps a narrow band - about 0.105 to 0.315 - across the
            // whole ramp, which is what gives the LANDSCAPE its contrast; but
            // it also means every lit surface of a machine, a vehicle or a
            // near building lands above 1.0 and used to be clamped flat. The
            // result was a game whose foreground objects were solid blocks of
            // the brightest glyph: all silhouette, no volume, none of the
            // plate work and limb shaping the meshes actually carry visible at
            // all. Rolling the overflow off with a tanh keeps the landscape
            // band untouched and spreads roughly 0.9 to 2.5 - which is where
            // machine surfaces live - across the top four or five ramp steps.
            lum = (lum - settings.blackPoint) * invRange;
            lum = std::max(lum, 0.0f);
            {
                constexpr float knee = 0.62f;
                constexpr float span = 1.0f - knee;
                constexpr float soft = 2.6f;
                if (lum > knee)
                    lum = knee + span * std::tanh((lum - knee) / (span * soft));
            }
            lum = clampf(lum, 0.0f, 1.0f);
            if (settings.contrast != 1.0f) {
                lum = (lum < 0.5f)
                    ? 0.5f * std::pow(lum * 2.0f, settings.contrast)
                    : 1.0f - 0.5f * std::pow((1.0f - lum) * 2.0f, settings.contrast);
            }

            const size_t i = static_cast<size_t>(y) * cx + x;
            out.scratchLum[i] = lum;
            out.scratchDepth[i] = nearest;

            // Stash the hue now; pass 2 only adjusts brightness.
            Vec3 tint;
            if (fullColor) {
                const float peak = std::max(rgb.x, std::max(rgb.y, rgb.z));
                tint = (peak > 1e-4f) ? rgb * (1.0f / peak) : Vec3(1.0f);
            } else {
                tint = pal.tint;
            }
            Cell& cell = out.cells[i];
            cell.r = toByte(tint.x);
            cell.g = toByte(tint.y);
            cell.b = toByte(tint.z);
        }
    }

    // ---- pass 2: silhouette edges, dither, glyph and colour ----------------
    const float edgeStrength = std::max(settings.edgeStrength, 0.0f);
    const float edgeThreshold = clampf(settings.edgeThreshold, 0.0f, 0.95f);
    const float invEdgeRange = 1.0f / std::max(1.0f - edgeThreshold, 1e-3f);
    const float bgLevel = (settings.background == Background::Solid) ? 0.55f
                        : (settings.background == Background::Dim)   ? 0.20f : 0.0f;

    for (int y = 0; y < cy; ++y) {
        for (int x = 0; x < cx; ++x) {
            const size_t i = static_cast<size_t>(y) * cx + x;
            float lum = out.scratchLum[i];

            if (edgeStrength > 0.0f) {
                const float dc = out.scratchDepth[i];
                if (dc > 1e-6f) {
                    // Only brighten the near side of a discontinuity, so objects
                    // gain an outline rather than casting a halo onto whatever
                    // is behind them.
                    float step = 0.0f;
                    const int nx[4] = {x - 1, x + 1, x, x};
                    const int ny[4] = {y, y, y - 1, y + 1};
                    for (int k = 0; k < 4; ++k) {
                        if (nx[k] < 0 || ny[k] < 0 || nx[k] >= cx || ny[k] >= cy) continue;
                        const float dn = out.scratchDepth[static_cast<size_t>(ny[k]) * cx + nx[k]];
                        step = std::max(step, 1.0f - dn / dc);
                    }
                    if (step > edgeThreshold)
                        lum += smoothstep01((step - edgeThreshold) * invEdgeRange) * edgeStrength;
                }
            }

            if (ditherScale > 0.0f) {
                const float d = (kBayer4[(y & 3) * 4 + (x & 3)] * (1.0f / 16.0f)) - 0.46875f;
                lum += d * ditherScale;
            }
            lum = clampf(lum, 0.0f, 1.0f);

            Cell& cell = out.cells[i];
            cell.ch = ramp[lut.index[static_cast<int>(lum * 255.0f + 0.5f)]];

            const Vec3 tint(cell.r / 255.0f, cell.g / 255.0f, cell.b / 255.0f);
            // The glyph already encodes brightness; the colour carries a softer
            // version of it so the grid keeps its CRT-phosphor feel.
            const Vec3 fg = tint * (pal.floorLevel + (1.0f - pal.floorLevel) * lum);
            cell.r = toByte(fg.x);
            cell.g = toByte(fg.y);
            cell.b = toByte(fg.z);

            if (bgLevel > 0.0f) {
                // The background carries the cell's tone at reduced level, so the
                // glyph still reads as the brighter mark on top of it.
                const Vec3 bgc = tint * (lum * lum * bgLevel);
                cell.br = toByte(bgc.x);
                cell.bg = toByte(bgc.y);
                cell.bb = toByte(bgc.z);
            } else {
                cell.br = cell.bg = cell.bb = 0;
            }
        }
    }
}

// ------------------------------------------------------------------- HUD text

void putCell(AsciiFrame& frame, int x, int y, char ch, const Vec3& color) {
    if (!frame.inside(x, y)) return;
    Cell& c = frame.at(x, y);
    c.ch = ch;
    c.r = toByte(color.x);
    c.g = toByte(color.y);
    c.b = toByte(color.z);
    // HUD cells always sit on black, whatever background mode the scene uses.
    c.br = c.bg = c.bb = 0;
}

void fillPanel(AsciiFrame& frame, int x, int y, int w, int h, const Vec3& bg) {
    const uint8_t br = static_cast<uint8_t>(clampf(bg.x, 0.0f, 1.0f) * 255.0f);
    const uint8_t bgg = static_cast<uint8_t>(clampf(bg.y, 0.0f, 1.0f) * 255.0f);
    const uint8_t bb = static_cast<uint8_t>(clampf(bg.z, 0.0f, 1.0f) * 255.0f);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            if (!frame.inside(x + i, y + j)) continue;
            Cell& c = frame.at(x + i, y + j);
            c.ch = ' ';
            c.r = c.g = c.b = 0;
            // Never fully zero, or the compositor treats the cell as untouched
            // and the scene shows through the "panel".
            c.br = std::max<uint8_t>(br, 1);
            c.bg = bgg;
            c.bb = bb;
        }
}

void drawText(AsciiFrame& frame, int x, int y, const std::string& text, const TextStyle& style) {
    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch < 32 || ch > 126) continue;
        if (ch == ' ') {
            // An opaque space blanks the scene behind it. Padding HUD strings
            // with spaces is what keeps them readable over a dense character
            // field - without it the text drowns in the terrain.
            if (style.transparentSpaces) continue;
            putCell(frame, x + static_cast<int>(i), y, ' ', Vec3(0.0f));
            continue;
        }
        putCell(frame, x + static_cast<int>(i), y, ch, style.color);
        // The backing: a near-black cell behind the glyph. Without it, HUD
        // text drawn over a dense character field is soup - the screenshot
        // that prompted this had mission text braided through a skyline.
        if (frame.inside(x + static_cast<int>(i), y)) {
            Cell& c = frame.at(x + static_cast<int>(i), y);
            if ((c.br | c.bg | c.bb) == 0) { c.br = 4; c.bg = 8; c.bb = 6; }
        }
    }
}

void drawTextRight(AsciiFrame& frame, int xRight, int y, const std::string& text, const TextStyle& style) {
    drawText(frame, xRight - static_cast<int>(text.size()) + 1, y, text, style);
}

void drawRect(AsciiFrame& frame, int x, int y, int w, int h, const TextStyle& style) {
    if (w < 2 || h < 2) return;
    for (int i = 1; i < w - 1; ++i) {
        putCell(frame, x + i, y, '-', style.color);
        putCell(frame, x + i, y + h - 1, '-', style.color);
    }
    for (int j = 1; j < h - 1; ++j) {
        putCell(frame, x, y + j, '|', style.color);
        putCell(frame, x + w - 1, y + j, '|', style.color);
    }
    putCell(frame, x, y, '+', style.color);
    putCell(frame, x + w - 1, y, '+', style.color);
    putCell(frame, x, y + h - 1, '+', style.color);
    putCell(frame, x + w - 1, y + h - 1, '+', style.color);
}

void drawHorizontalBar(AsciiFrame& frame, int x, int y, int width, float fill01,
                       const TextStyle& on, const TextStyle& off) {
    const int filled = static_cast<int>(clampf(fill01, 0.0f, 1.0f) * width + 0.5f);
    for (int i = 0; i < width; ++i) {
        const bool lit = i < filled;
        putCell(frame, x + i, y, lit ? '#' : '.', lit ? on.color : off.color);
    }
}

std::string frameToText(const AsciiFrame& frame) {
    std::string s;
    s.reserve(static_cast<size_t>(frame.w + 1) * frame.h);
    for (int y = 0; y < frame.h; ++y) {
        for (int x = 0; x < frame.w; ++x) s.push_back(frame.at(x, y).ch);
        s.push_back('\n');
    }
    return s;
}

} // namespace sb
