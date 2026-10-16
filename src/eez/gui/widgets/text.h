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

namespace eez {
namespace gui {

struct TextWidget : public Widget {
    AssetsPtr<const char> text;
    uint8_t flags;
};

struct TextWidgetState : public WidgetState {
	WidgetStateFlags flags;
	Value data;

    // widget size when "Fit to content" is enabled
    int16_t fitW = 0;
    int16_t fitH = 0;

    // last rendered rectangle, used to clear the area left over when the
    // widget shrinks because of "Fit to content"
    int16_t renderedX = 0;
    int16_t renderedY = 0;
    int16_t renderedW = 0;
    int16_t renderedH = 0;

    bool updateState() override;
    void render() override;

    // sets g_widgetCursor size to the size calculated in updateState
    void applyFitToContent();
};

void TextWidget_autoSize(TextWidget& widget);

} // namespace gui
} // namespace eez
