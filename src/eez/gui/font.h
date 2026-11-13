/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <stdint.h>

#include <eez/core/assets.h>

namespace eez {
namespace gui {
namespace font {

struct Font {
    const FontData *fontData;

    // extra space between the characters of the text, in 1/64 px (the letter
    // spacing of the style, see styleGetFont), also used in the fonts the
    // font tags switch to
    int letterSpacing;

    Font();
    Font(const FontData *fontData_);

	explicit operator bool() const {
		return fontData != nullptr;
	}

    // A vector font (FONT_KIND_VECTOR) is drawn from a font file: it has no
    // glyphs in the assets, its text is measured and drawn by the display text
    // functions (display.h).
    bool isVector() const {
        return fontData != nullptr && fontData->kind == FONT_KIND_VECTOR;
    }

    const VectorFontData *getVectorFontData() const {
        return (const VectorFontData *)fontData;
    }

    // the glyph of a bitmap font, nullptr for a vector font (use
    // display::measureGlyph and the other text functions)
    const GlyphData *getGlyph(int32_t encoding);

    uint8_t getAscent() const;
    uint8_t getDescent() const;
    uint8_t getHeight() const;
};

} // namespace font
} // namespace gui
} // namespace eez
