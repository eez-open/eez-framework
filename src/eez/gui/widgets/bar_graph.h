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

struct BarGraphWidget : public Widget {
    int16_t textStyle;
    int16_t line1Data;
    int16_t line1Style;
    int16_t line2Data;
    int16_t line2Style;
    int16_t min;
    int16_t max;
    int16_t refreshRate;
    uint8_t orientation; // BAR_GRAPH_ORIENTATION_...
    uint8_t flags; // BAR_GRAPH_FLAG_...
    // ticks and their numbers beside the bar (left of a vertical bar, below a
    // horizontal one) in this style, 0: no scale
    int16_t scaleStyle;
    // the width of a vertical bar, the height of a horizontal one, 0: all the
    // room left by the scale and the marker
    int16_t thickness;
    // text beside the marker of line 1 (flow expression, 0: none)
    int16_t line1Label;
};

// The bar is drawn as a track with rounded corners (the radius of the widget
// style, inside its padding), when the widget has a scale, a thickness or one
// of these flags; else it fills the whole widget as before.
#define BAR_GRAPH_FLAG_LINE1_MARKER (1 << 0) // a triangle beside the bar points at line 1
#define BAR_GRAPH_FLAG_ZONE (1 << 1) // the part between the bar and line 1 in the bar color, dimmed

struct BarGraphWidgetState : public WidgetState {
	WidgetStateFlags flags;
	Value data;
    // style after overrideStyle
    int styleId;
    uint16_t color;
    uint16_t backgroundColor;
    // g_blinkOpacity while the widget blinks, else 255 (see BlinkOpacity)
    uint8_t blinkOpacity;
    Value line1Data;
    Value line2Data;
    Value textData;
    Value min;
    Value max;
    Value refreshRate;
    Value line1Label;

    uint32_t textDataRefreshLastTime;

    bool updateState() override;
    void render() override;

private:
    void renderTrack(const BarGraphWidget *widget, const Style *style, float min, float max);
};

} // namespace gui
} // namespace eez
