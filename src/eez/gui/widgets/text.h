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

// TextWidget::flags
// the label is left of the value (ValueParts::labelLeft)
static const uint8_t TEXT_WIDGET_LABEL_LEFT_FLAG = 0x01;
// "Fit to content": the widget has the width / the height of its content
static const uint8_t TEXT_WIDGET_FIT_TO_CONTENT_WIDTH_FLAG = 0x02;
static const uint8_t TEXT_WIDGET_FIT_TO_CONTENT_HEIGHT_FLAG = 0x04;

// The Text, DisplayData and Button widgets: three names of one widget that
// shows one line, a fixed text or the value of its data. The name says what a
// widget is for (and gives its defaults in Studio); every property works in
// all three.
struct TextWidget : public Widget {
    // a fixed text; when it is empty the widget shows its data
    AssetsPtr<const char> text;
    uint8_t flags = 0; // TEXT_WIDGET_..._FLAG
    // the part of a number the widget shows (DISPLAY_OPTION_...)
    uint8_t displayOption = 0;
    // the widget can be used (a flow expression, 0: always); not enabled:
    // drawn in the disabled state, its action doesn't run
    int16_t enabled = 0;
    // the shortest time between two changes of the shown value, in ms (a flow
    // expression, 0: none)
    int16_t refreshRate = 0;
    int16_t unitAndFractionFont = 0; // only for DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION
    // a number shown in a unit (a string such as "mA") with the decimals of a
    // precision (the smallest step, in that unit), see getValueInUnit; 0 if not set
    int16_t displayUnit = 0;
    int16_t precision = 0;
    // a text above the value, or left of it with TEXT_WIDGET_LABEL_LEFT_FLAG
    // (a flow expression, 0 if not set), in labelStyle (0: the widget style),
    // see drawValueParts
    int16_t label = 0;
    int16_t labelStyle = 0;
    // the style of the unit with DISPLAY_OPTION_VALUE_AND_UNIT (0: the widget
    // style), see drawValueParts
    int16_t unitStyle = 0;
    // an icon at the left, before the label, and one at the right, after the
    // value and the unit (a row of a settings group): a text in the font of
    // its style (flow expressions, 0 if not set; the styles 0: the widget
    // style), see ValueParts
    int16_t icon = 0;
    int16_t iconStyle = 0;
    int16_t trailingIcon = 0;
    int16_t trailingIconStyle = 0;
    int16_t reserved = 0;
};

typedef TextWidget DisplayDataWidget;
typedef TextWidget ButtonWidget;

// the names of the flag before the three widgets had one structure
static const uint8_t DISPLAY_DATA_WIDGET_LABEL_LEFT_FLAG = TEXT_WIDGET_LABEL_LEFT_FLAG;

struct TextWidgetState : public WidgetState {
	WidgetStateFlags flags;
	Value data;
    Value label;
    Value icon;
    Value trailingIcon;

    // style after overrideStyle and the colors of the text after the hooks: a
    // change of the hook results redraws the widget
    int styleId = 0;
    uint16_t color = 0;
    uint16_t backgroundColor = 0;
    // g_blinkOpacity while the widget blinks, else 255 (see BlinkOpacity)
    uint8_t blinkOpacity = 255;
    uint32_t dataRefreshLastTime = 0;
    int16_t cursorPosition = -1;
    // in pixels
    int xScroll = 0;

    // widget size when "Fit to content" is enabled
    int16_t fitW = 0;
    int16_t fitH = 0;

    // last rendered rectangle, used to clear the area left over when the
    // widget shrinks because of "Fit to content"
    int16_t renderedX = 0;
    int16_t renderedY = 0;
    int16_t renderedW = 0;
    int16_t renderedH = 0;

    // The internal pages (page.cpp) draw a state they made themselves, without
    // updateState: it is enabled, not pressed, not focused and has no data.
    TextWidgetState() {
        flags.active = 0;
        flags.focused = 0;
        flags.disabled = 0;
        flags.enabled = 1;
    }

    bool updateState() override;
    void render() override;

    // sets g_widgetCursor size to the size calculated in updateState
    void applyFitToContent();

private:
    void renderText(const TextWidget *widget, const Style *style, bool active);
    void renderParts(const TextWidget *widget, const Style *style, bool active);
    void renderIntegerUnitAndFraction(const TextWidget *widget, const Style *style, bool active);
};

struct DisplayDataWidgetState : public TextWidgetState {};
struct ButtonWidgetState : public TextWidgetState {};

// true for the three widget types of the TextWidget
inline bool isTextWidgetType(uint16_t type) {
    return type == WIDGET_TYPE_TEXT || type == WIDGET_TYPE_DISPLAY_DATA || type == WIDGET_TYPE_BUTTON;
}

// the size of a widget with a fixed text in the font of its style with the
// padding and the borders
void TextWidget_autoSize(TextWidget& widget);

// The size of what the widget widgetCursor.widget shows (its text, or its data
// now, also during the refresh rate, with the label and the unit) with the
// padding and the borders. False for the integer, unit and fraction option, it
// keeps its own size.
bool TextWidget_getContentSize(const WidgetCursor &widgetCursor, int &w, int &h);

// The y of the baseline of the text or the value the widget widgetCursor.widget
// shows in a box of the height h, from the top of the box; -1 if its style has
// no font.
int TextWidget_getBaseline(const WidgetCursor &widgetCursor, int h);

int DISPLAY_DATA_getCharIndexAtPosition(int xPos, const WidgetCursor &widgetCursor);
int DISPLAY_DATA_getCursorXPosition(int cursorPosition, const WidgetCursor &widgetCursor);

inline bool DisplayDataWidget_getContentSize(const WidgetCursor &widgetCursor, int &w, int &h) {
    return TextWidget_getContentSize(widgetCursor, w, h);
}

inline int DisplayDataWidget_getBaseline(const WidgetCursor &widgetCursor, int h) {
    return TextWidget_getBaseline(widgetCursor, h);
}

} // namespace gui
} // namespace eez
