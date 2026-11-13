/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <string.h>

#include <eez/core/os.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/display_data.h>

namespace eez {
namespace gui {

enum {
    DISPLAY_OPTION_ALL = 0,
    DISPLAY_OPTION_INTEGER = 1,
    DISPLAY_OPTION_FRACTION = 2,
    DISPLAY_OPTION_FRACTION_AND_UNIT = 3,
    DISPLAY_OPTION_UNIT = 4,
    DISPLAY_OPTION_INTEGER_AND_FRACTION = 5,
    DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION = 6
};

// for the text of the value, font tags make the text longer than what is drawn
// (9 bytes for one icon from an icon font)
static const size_t TEXT_BUFFER_SIZE = 128;

int findStartOfFraction(char *text) {
    int i;
    for (i = 0; text[i] && (text[i] == '<' || text[i] == ' ' || text[i] == '-' || (text[i] >= '0' && text[i] <= '9')); i++);
    return i;
}

int findStartOfUnit(char *text, int i) {
    for (i = 0; text[i] && (text[i] == '<' || text[i] == ' ' || text[i] == '-' || (text[i] >= '0' && text[i] <= '9') || text[i] == '.'); i++);
    return i;
}

// A text with a number before the unit (e.g. the preview "12.451 V" Studio's
// page editor shows) is split into integer, fraction and unit like a float.
static bool isNumberText(const char *text) {
    int k = findStartOfUnit((char *)text, 0);
    for (int i = 0; i < k; i++) {
        if (text[i] >= '0' && text[i] <= '9') {
            return true;
        }
    }
    return false;
}

static void trimText(const char *&text, int &length) {
    while (length > 0 && *text == ' ') {
        text++;
        length--;
    }
    while (length > 0 && text[length - 1] == ' ') {
        length--;
    }
}

static void renderIntegerUnitAndFraction(DisplayDataWidgetState *widgetState, const DisplayDataWidget *widget, const Style *style, bool active) {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    char text[TEXT_BUFFER_SIZE];
    widgetState->data.toText(text, sizeof(text));

    const char *integer = text;
    int integerLength = (int)strlen(text);
    const char *unit = "";
    int unitLength = 0;
    const char *fraction = "";
    int fractionLength = 0;

    // value that is not a float (or a text with a number) is displayed whole
    // as the integer part
    if (
        widgetState->data.getType() == VALUE_TYPE_FLOAT ||
        (widgetState->data.isString() && isNumberText(text))
    ) {
        int i = findStartOfFraction(text);
        int k = findStartOfUnit(text, i);

        integerLength = i;

        if (i < k) {
            fraction = text + i;
            fractionLength = k - i;
        } else {
            fraction = ".0";
            fractionLength = 2;
        }

        unit = text + k;
        unitLength = (int)strlen(unit);
    }

    trimText(integer, integerLength);
    trimText(unit, unitLength);
    trimText(fraction, fractionLength);

    drawIntegerUnitAndFraction(
        integer, integerLength,
        unit, unitLength,
        fraction, fractionLength,
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        style, font::Font(getFontData(widget->unitAndFractionFont)),
        active,
        &widgetState->color, &widgetState->backgroundColor
    );
}

bool DisplayDataWidgetState::updateState() {
    WIDGET_STATE_START(DisplayDataWidget);

    WIDGET_STATE(styleId, overrideStyle(widgetCursor, widget->style));

    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(flags.focused, isFocusWidget(widgetCursor));
    WIDGET_STATE(flags.disabled, isWidgetActionDisabled(widgetCursor));

    auto stateStyle = getWidgetStateStyle(getStyle(styleId), flags.active, flags.focused, getDisabledStateFlag(flags.disabled));
    const Style *style = stateStyle.style;

    WIDGET_STATE(blinkOpacity, styleIsBlink(style) || isBlinking(widgetCursor, widget->data) ? g_blinkOpacity : 255);

    uint32_t refreshRate;
    if (widgetCursor.flowState) {
        refreshRate = get(widgetCursor, widget->refreshRate).toInt32(nullptr);
    } else {
        refreshRate = getTextRefreshRate(widgetCursor, widget->data);
    }

    bool refreshData = true;
    auto newData = get(widgetCursor, widget->data);
    auto currentTime = millis();
    if (hasPreviousState && data != newData) {
        if (refreshRate != 0 && currentTime - dataRefreshLastTime < refreshRate) {
            refreshData = false;
        }
    }
    if (refreshData) {
        WIDGET_STATE(data, newData);
        dataRefreshLastTime = currentTime;
    }

    WIDGET_STATE(color, getColor(widgetCursor, widget->data, style));
    WIDGET_STATE(backgroundColor, getBackgroundColor(widgetCursor, widget->data, style));

    bool cursorVisible = millis() % (2 * CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS) < CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS;
    WIDGET_STATE(cursorPosition, cursorVisible ? getTextCursorPosition(widgetCursor, widget->data) : -1);

    WIDGET_STATE(xScroll, getXScroll(widgetCursor));

    WIDGET_STATE_END()
}

void DisplayDataWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const DisplayDataWidget *)widgetCursor.widget;
    auto stateStyle = getWidgetStateStyle(getStyle(styleId), flags.active, flags.focused, getDisabledStateFlag(flags.disabled));
    const Style *style = stateStyle.style;

    BlinkOpacity blinkOpacityScope(blinkOpacity);

    if (widget->displayOption == DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION) {
        renderIntegerUnitAndFraction(this, widget, style, stateStyle.active);
    } else {
        renderText(widget, style, stateStyle.active);
    }

    if (flags.focused) {
        drawFocusRing(widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style);
    }
}

void DisplayDataWidgetState::renderText(const DisplayDataWidget *widget, const Style *style, bool active) {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    char text[TEXT_BUFFER_SIZE];
    data.toText(text, sizeof(text));

    char *start = text;

    int length = -1;

    if (widget->displayOption != DISPLAY_OPTION_ALL) {
        // a text with a number is cut like the text of a float
        if (
            data.getType() == VALUE_TYPE_FLOAT ||
            (data.isString() && isNumberText(text))
        ) {
            if (widget->displayOption == DISPLAY_OPTION_INTEGER) {
                int i = findStartOfFraction(text);
                text[i] = 0;
            } else if (widget->displayOption == DISPLAY_OPTION_FRACTION) {
                int i = findStartOfFraction(text);
                start = text + i;
            } else if (widget->displayOption == DISPLAY_OPTION_FRACTION_AND_UNIT) {
                int i = findStartOfFraction(text);
                int k = findStartOfUnit(text, i);
                if (i < k) {
                    start = text + i;
                    text[k] = 0;
                }
                else {
                    stringCopy(text, sizeof(text), ".0");
                }
            } else if (widget->displayOption == DISPLAY_OPTION_UNIT) {
                int i = findStartOfUnit(text, 0);
                start = text + i;
            } else if (widget->displayOption == DISPLAY_OPTION_INTEGER_AND_FRACTION) {
                int i = findStartOfUnit(text, 0);
                text[i] = 0;
            }

            // trim left
            while (*start && *start == ' ') {
                start++;
            }

            // trim right
            length = (int)strlen(start);
            if (length > 0 && start[length - 1] == ' ') {
                length--;
            }
        } else {
            if (
                widget->displayOption != DISPLAY_OPTION_INTEGER &&
                widget->displayOption != DISPLAY_OPTION_INTEGER_AND_FRACTION
            ) {
                *text = 0;
            }
        }
    }

    drawText(
        start, length,
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        style,
        active, false,
        &color, &backgroundColor,
        data.getType() == VALUE_TYPE_FLOAT
#if OPTION_KEYPAD
        || widget->data == EEZ_DATA_ID_KEYPAD_EDIT_UNIT
#endif
        , // useSmallerFontIfDoesNotFit
        cursorPosition,
        xScroll
    );
}

int DISPLAY_DATA_getCharIndexAtPosition(int xPos, const WidgetCursor &widgetCursor) {
    auto widget = (const DisplayDataWidget *)widgetCursor.widget;

	const Style *style = getStyle(overrideStyle(widgetCursor, widget->style));

    char text[TEXT_BUFFER_SIZE];
    Value data = get(widgetCursor, widget->data);
    data.toText(text, sizeof(text));

    char *start = text;

    if (widget->displayOption == DISPLAY_OPTION_INTEGER) {
        int i = findStartOfFraction(text);
        text[i] = 0;
    } else if (widget->displayOption == DISPLAY_OPTION_FRACTION) {
        int i = findStartOfFraction(text);
        start = text + i;
    } else if (widget->displayOption == DISPLAY_OPTION_FRACTION_AND_UNIT) {
        int i = findStartOfFraction(text);
        int k = findStartOfUnit(text, i);
        if (i < k) {
            start = text + i;
            text[k] = 0;
        } else {
            stringCopy(text, sizeof(text), ".0");
        }
    } else if (widget->displayOption == DISPLAY_OPTION_UNIT) {
        int i = findStartOfUnit(text, 0);
        start = text + i;
    } else if (widget->displayOption == DISPLAY_OPTION_INTEGER_AND_FRACTION) {
        int i = findStartOfUnit(text, 0);
        text[i] = 0;
    }

    // trim left
    while (*start && *start == ' ') {
        start++;
    }

    // trim right
    int length = (int)strlen(start);
    while (length > 0 && start[length - 1] == ' ') {
        length--;
    }

    int xScroll = getXScroll(widgetCursor);

    return getCharIndexAtPosition(xPos, start, length, widgetCursor.x - xScroll, widgetCursor.y, widgetCursor.w, widgetCursor.h, style);
}

int DISPLAY_DATA_getCursorXPosition(int cursorPosition, const WidgetCursor &widgetCursor) {
    auto widget = (const DisplayDataWidget *)widgetCursor.widget;

	const Style *style = getStyle(overrideStyle(widgetCursor, widget->style));

    char text[TEXT_BUFFER_SIZE];
    Value data = get(widgetCursor, widget->data);
    data.toText(text, sizeof(text));

    char *start = text;

    if (widget->displayOption == DISPLAY_OPTION_INTEGER) {
        int i = findStartOfFraction(text);
        text[i] = 0;
    } else if (widget->displayOption == DISPLAY_OPTION_FRACTION) {
        int i = findStartOfFraction(text);
        start = text + i;
    } else if (widget->displayOption == DISPLAY_OPTION_FRACTION_AND_UNIT) {
        int i = findStartOfFraction(text);
        int k = findStartOfUnit(text, i);
        if (i < k) {
            start = text + i;
            text[k] = 0;
        } else {
            stringCopy(text, sizeof(text), ".0");
        }
    } else if (widget->displayOption == DISPLAY_OPTION_UNIT) {
        int i = findStartOfUnit(text, 0);
        start = text + i;
    } else if (widget->displayOption == DISPLAY_OPTION_INTEGER_AND_FRACTION) {
        int i = findStartOfUnit(text, 0);
        text[i] = 0;
    }

    // trim left
    while (*start && *start == ' ') {
        start++;
    }

    // trim right
    int length = (int)strlen(start);
    while (length > 0 && start[length - 1] == ' ') {
        length--;
    }

    return getCursorXPosition(cursorPosition, start, length, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style);
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
