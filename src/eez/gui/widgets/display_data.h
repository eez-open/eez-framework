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

#define CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS 500

namespace eez {
namespace gui {

struct DisplayDataWidget : public Widget {
    int16_t refreshRate;
    uint8_t displayOption;
    // DISPLAY_DATA_WIDGET_LABEL_LEFT_FLAG
    uint8_t flags;
    int16_t unitAndFractionFont; // only for DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION
    // a number shown in a unit (a string such as "mA") with the decimals of a
    // precision (the smallest step, in that unit), see getValueInUnit; 0 if not set
    int16_t displayUnit;
    int16_t precision;
    // a text above the value, or left of it with
    // DISPLAY_DATA_WIDGET_LABEL_LEFT_FLAG (a flow expression, 0 if not set), in
    // labelStyle (0: the widget style), see drawValueParts
    int16_t label;
    int16_t labelStyle;
    // the style of the unit with DISPLAY_OPTION_VALUE_AND_UNIT (0: the widget
    // style), see drawValueParts
    int16_t unitStyle;
};

// the label is left of the value (ValueParts::labelLeft)
static const uint8_t DISPLAY_DATA_WIDGET_LABEL_LEFT_FLAG = 0x01;

struct DisplayDataWidgetState : public WidgetState {
	WidgetStateFlags flags;
	Value data;
    Value label;
    // style after overrideStyle
    int styleId;
    uint16_t color;
    uint16_t backgroundColor;
    // g_blinkOpacity while the widget blinks, else 255 (see BlinkOpacity)
    uint8_t blinkOpacity;
    uint32_t dataRefreshLastTime;
    int16_t cursorPosition;
    // in pixels
    int xScroll;

    bool updateState() override;
    void render() override;

private:
    void renderText(const DisplayDataWidget *widget, const Style *style, bool active);
    void renderParts(const DisplayDataWidget *widget, const Style *style, bool active);
};

// The size of the text the DisplayData widget widgetCursor.widget shows (its
// data now, also during the refresh rate, with the label and the unit) with the
// padding and the borders. False for the integer, unit and fraction option, it
// keeps its own size.
bool DisplayDataWidget_getContentSize(const WidgetCursor &widgetCursor, int &w, int &h);

// The y of the baseline of the value the DisplayData widget widgetCursor.widget
// shows in a box of the height h, from the top of the box; -1 if its style has
// no font.
int DisplayDataWidget_getBaseline(const WidgetCursor &widgetCursor, int h);

int DISPLAY_DATA_getCharIndexAtPosition(int xPos, const WidgetCursor &widgetCursor);
int DISPLAY_DATA_getCursorXPosition(int cursorPosition, const WidgetCursor &widgetCursor);

} // namespace gui
} // namespace eez
