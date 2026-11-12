/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Vector fonts (FONT_KIND_VECTOR, see VectorFontData): the text is shaped with
// HarfBuzz and the glyphs are rendered with FreeType, from the font file in the
// assets. The display text functions (display.h: measureStr, drawStr, ...) use
// this; other code uses those functions.

#pragma once

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI && EEZ_OPTION_GUI_VECTOR_FONTS

#include <stdint.h>

#include <eez/core/assets.h>

namespace eez {
namespace gui {
namespace vector_font {

// The 8-bit coverage image of a glyph.
struct GlyphImage {
    int width;
    int height;
    // from the pen position to the first column
    int left;
    // from the baseline up to the first row
    int top;
    // width * height bytes
    const uint8_t *pixels;
};

// A glyph of shaped text, in visual order.
struct ShapedGlyph {
    uint32_t glyphIndex;
    // the byte offset in the text of the first character of the glyph
    uint32_t cluster;
    // in 1/64 px
    int32_t xOffset;
    int32_t yOffset;
    // in 1/64 px, the letter spacing included
    int32_t advance;
};

// The caches are shared by the threads that draw and measure text: what shape
// and getGlyphImage return is valid while the lock is held.
struct Lock {
    Lock();
    ~Lock();
};

// The glyphs of the text (UTF-8, textLength bytes) in the font. Without
// ligatures (editable text) every glyph is one character, so a cursor can be
// put between any two characters. Returns nullptr if the font can't be used
// (no font file, a file that isn't a font).
const ShapedGlyph *shape(const VectorFontData *font, const char *text, int textLength, bool ligatures, uint32_t &numGlyphs);

// nullptr if the glyph has no image (a space) or the font can't be used
const GlyphImage *getGlyphImage(const VectorFontData *font, uint32_t glyphIndex);

// Frees everything that is cached (call it when the assets are replaced).
void reset();

} // namespace vector_font
} // namespace gui
} // namespace eez

#endif
