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

#include <stdio.h>
#include <string.h>

#include <eez/core/os.h>
#include <eez/core/util.h>
#include <eez/core/utf8.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/text.h>

static const size_t MAX_TEXT_LEN = 128;

namespace eez {
namespace gui {

// The character (code point) of the UTF-8 text at the byte offset i, the
// character ends before the byte offset end. Returns its length in bytes, an
// invalid or cut sequence is taken as a one byte character.
static int getCharAt(const char *text, int i, int end, utf8_int32_t &encoding) {
#if UTF8_SUPPORT
    uint8_t c = (uint8_t)text[i];
    int length = (c & 0xF8) == 0xF0 ? 4 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xE0) == 0xC0 ? 2 : 1;
    if (length > 1 && i + length <= end) {
        int k;
        for (k = 1; k < length && ((uint8_t)text[i + k] & 0xC0) == 0x80; k++) {
        }
        if (k == length) {
            utf8codepoint(text + i, &encoding);
            return length;
        }
    }
#else
    EEZ_UNUSED(end);
#endif
    encoding = (uint8_t)text[i];
    return 1;
}

// The character of the UTF-8 text that ends before the byte offset end and
// starts at or after the byte offset begin. Returns the byte offset of its start.
static int getCharBefore(const char *text, int begin, int end, utf8_int32_t &encoding) {
    int start = end - 1;
#if UTF8_SUPPORT
    while (start > begin && end - start < 4 && ((uint8_t)text[start] & 0xC0) == 0x80) {
        start--;
    }
    if (getCharAt(text, start, end, encoding) == end - start) {
        return start;
    }
    start = end - 1;
#else
    EEZ_UNUSED(begin);
#endif
    encoding = (uint8_t)text[start];
    return start;
}

enum {
    DISPLAY_OPTION_ALL = 0,
    DISPLAY_OPTION_INTEGER = 1,
    DISPLAY_OPTION_FRACTION = 2,
    DISPLAY_OPTION_FRACTION_AND_UNIT = 3,
    DISPLAY_OPTION_UNIT = 4,
    DISPLAY_OPTION_INTEGER_AND_FRACTION = 5,
    DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION = 6,
    // the number in the widget style and the unit in the unit style on its
    // baseline (see drawValueParts)
    DISPLAY_OPTION_VALUE_AND_UNIT = 7
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
    for (i = 0; text[i] && (text[i] == '<' || text[i] == ' ' || text[i] == '-' || (text[i] >= '0' && text[i] <= '9') || text[i] == '.' || text[i] == display::DIGIT_GROUP_GAP_CHAR); i++);
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

// The color of the text: the overrideStyleColor hook, then the color the data
// gives (DATA_OPERATION_GET_COLOR).
static uint16_t getTextColor(const WidgetCursor &widgetCursor, const TextWidget *widget, bool hasData, const Style *style) {
    uint16_t color = g_hooks.overrideStyleColor(widgetCursor, style);
    if (hasData) {
        uint16_t dataColor = getColor(widgetCursor, widget->data, style);
        if (dataColor != style->color) {
            color = dataColor;
        }
    }
    return color;
}

static uint16_t getTextBackgroundColor(const WidgetCursor &widgetCursor, const TextWidget *widget, bool hasData, const Style *style) {
    return hasData ? getBackgroundColor(widgetCursor, widget->data, style) : style->backgroundColor;
}

// the fixed text of the widget, null if it has none
static const char *getFixedText(const TextWidget *widget) {
    const char *text = widget->text ? static_cast<const char *>(widget->text) : nullptr;
    return text && text[0] ? text : nullptr;
}

// the widget shows its data: it has no fixed text
static bool showsData(const TextWidget *widget) {
    return !getFixedText(widget) && widget->data;
}

// The data as the widget shows it: a number in the unit with the decimals of
// the precision of the widget (getValueInUnit), when the widget has them. A
// value with its own decimals (a module field with a resolution) shows them.
static Value getTextWidgetValue(const WidgetCursor &widgetCursor, const TextWidget *widget) {
    // the edit value that isn't set yet (AppContext::getFocusEditValue), else the data
    Value data;
    if (!widgetCursor.appContext->getFocusEditValue(widgetCursor, widget->data, data)) {
        data = get(widgetCursor, widget->data);
    }
    if (!widget->displayUnit && !widget->precision) {
        return data;
    }

    Unit unit = UNIT_NONE;
    if (widget->displayUnit) {
        Value unitValue = get(widgetCursor, widget->displayUnit);
        if (unitValue.isString()) {
            unit = getUnitFromName(unitValue.getString());
        }
    }

    double precision = widget->precision ? get(widgetCursor, widget->precision).toDouble() : 0;

    return getValueInUnit(data, unit, precision);
}

// The text of the data as it is shown: the decimals of a number are in groups
// of three (display::groupDecimals).
static void getDataText(const Value &data, char *text, int count) {
    data.toText(text, count);
    if (data.isFloat() || data.isDouble()) {
        display::groupDecimals(text, count);
    }
}

// a label, an icon or the unit in its own style: drawn by drawValueParts
static bool usesValueParts(const TextWidget *widget) {
    return widget->label || widget->icon || widget->trailingIcon || widget->displayOption == DISPLAY_OPTION_VALUE_AND_UNIT;
}

// the texts of the parts drawValueParts draws
struct ValuePartsTexts {
    char text[TEXT_BUFFER_SIZE];
    char label[TEXT_BUFFER_SIZE];
    // an icon: one character, with a font tag at most
    char icon[32];
    char trailingIcon[32];
};

// The text of the data as the display option shows it: from start, length
// characters (-1: to the end). text has room for TEXT_BUFFER_SIZE characters.
static void getDisplayText(const TextWidget *widget, const Value &data, char *text, char *&start, int &length) {
    getDataText(data, text, TEXT_BUFFER_SIZE);

    start = text;

    length = -1;

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
                    stringCopy(text, TEXT_BUFFER_SIZE, ".0");
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
}

// The parts drawValueParts draws (usesValueParts): the label, the text of the
// data cut as the display option says, the unit apart with the value and unit
// option, the icons.
static void getValueParts(const TextWidget *widget, const Value &data, const Value &label, const Value &icon, const Value &trailingIcon, ValuePartsTexts &texts, ValueParts &parts) {
    char *text = texts.text;
    char *labelText = texts.label;
    if (widget->displayOption == DISPLAY_OPTION_VALUE_AND_UNIT) {
        getDataText(data, text, TEXT_BUFFER_SIZE);
        const char *value = text;
        int valueLength = (int)strlen(text);
        // a value that is not a number has no unit
        if (
            data.getType() == VALUE_TYPE_FLOAT ||
            (data.isString() && isNumberText(text))
        ) {
            int k = findStartOfUnit(text, 0);
            const char *unit = text + k;
            int unitLength = (int)strlen(unit);
            trimText(unit, unitLength);
            parts.unit = unit;
            parts.unitLength = unitLength;
            valueLength = k;
        }
        trimText(value, valueLength);
        parts.value = value;
        parts.valueLength = valueLength;
    } else {
        char *start;
        int length;
        getDisplayText(widget, data, text, start, length);
        parts.value = start;
        parts.valueLength = length;
    }

    if (widget->label) {
        label.toText(labelText, TEXT_BUFFER_SIZE);
        parts.label = labelText;
    }

    parts.labelStyle = widget->labelStyle ? getStyle(widget->labelStyle) : nullptr;
    parts.labelLeft = (widget->flags & DISPLAY_DATA_WIDGET_LABEL_LEFT_FLAG) != 0;
    parts.unitStyle = widget->unitStyle ? getStyle(widget->unitStyle) : nullptr;

    if (widget->icon && icon.getType() != VALUE_TYPE_UNDEFINED) {
        icon.toText(texts.icon, sizeof(texts.icon));
        parts.icon = texts.icon;
    }
    parts.iconStyle = widget->iconStyle ? getStyle(widget->iconStyle) : nullptr;
    if (widget->trailingIcon && trailingIcon.getType() != VALUE_TYPE_UNDEFINED) {
        trailingIcon.toText(texts.trailingIcon, sizeof(texts.trailingIcon));
        parts.trailingIcon = texts.trailingIcon;
    }
    parts.trailingIconStyle = widget->trailingIconStyle ? getStyle(widget->trailingIconStyle) : nullptr;
}

// the parts of the widget widgetCursor.widget (usesValueParts) with its data
// and label now
static void getCurrentValueParts(const WidgetCursor &widgetCursor, ValuePartsTexts &texts, ValueParts &parts) {
    auto widget = (const TextWidget *)widgetCursor.widget;
    Value label = widget->label ? get(widgetCursor, widget->label) : Value();
    Value icon = widget->icon ? get(widgetCursor, widget->icon) : Value();
    Value trailingIcon = widget->trailingIcon ? get(widgetCursor, widget->trailingIcon) : Value();
    Value data = showsData(widget) ? getTextWidgetValue(widgetCursor, widget) : Value();
    getValueParts(widget, data, label, icon, trailingIcon, texts, parts);
    const char *fixedText = getFixedText(widget);
    if (fixedText) {
        parts.value = fixedText;
        parts.valueLength = -1;
        parts.unit = nullptr;
        parts.unitLength = 0;
    }
}

void TextWidget_autoSize(TextWidget& widget) {
    const Style *style = getStyle(widget.style);
    font::Font font = styleGetFont(style);
    widget.width = style->borderSizeLeft + style->paddingLeft + display::measureStr(static_cast<const char *>(widget.text), -1, font, 0) + style->borderSizeRight + style->paddingRight;
    widget.height = style->borderSizeTop + style->paddingTop + font.getHeight() + style->borderSizeBottom + style->paddingBottom;
}

// the width of the fixed text or of the data as the widget shows it, in the font
static int measureTextContent(const TextWidget *widget, const char *fixedText, bool hasData, const Value &data, font::Font &font) {
    if (!font) {
        return 0;
    }
    if (fixedText) {
        return display::measureStr(fixedText, -1, font, 0);
    }
    if (hasData) {
        if (data.isString() && widget->displayOption == DISPLAY_OPTION_ALL) {
            const char *str = data.getString();
            return str ? display::measureStr(str, -1, font, 0) : 0;
        }
        char text[TEXT_BUFFER_SIZE];
        char *start;
        int length;
        getDisplayText(widget, data, text, start, length);
        return display::measureStr(start, length, font, 0);
    }
    return 0;
}

bool TextWidget_getContentSize(const WidgetCursor &widgetCursor, int &w, int &h) {
    auto widget = (const TextWidget *)widgetCursor.widget;
    if (widget->displayOption == DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION) {
        return false;
    }

    const Style *style = getStyle(overrideStyle(widgetCursor, widget->style));

    if (usesValueParts(widget)) {
        ValuePartsTexts texts;
        ValueParts parts;
        getCurrentValueParts(widgetCursor, texts, parts);
        if (measureValueParts(parts, style, w, h)) {
            return true;
        }
    }

    font::Font font = styleGetFont(style);

    const char *fixedText = getFixedText(widget);
    bool hasData = showsData(widget);
    Value data = hasData ? getTextWidgetValue(widgetCursor, widget) : Value();

    // must be kept in sync with TextWidgetState::updateState
    w = style->borderSizeLeft + style->paddingLeft + measureTextContent(widget, fixedText, hasData, data, font) + style->paddingRight + style->borderSizeRight;
    h = style->borderSizeTop + style->paddingTop + (font ? font.getHeight() : 0) + style->paddingBottom + style->borderSizeBottom;
    return true;
}

int TextWidget_getBaseline(const WidgetCursor &widgetCursor, int h) {
    auto widget = (const TextWidget *)widgetCursor.widget;
    const Style *style = getStyle(overrideStyle(widgetCursor, widget->style));

    if (usesValueParts(widget) && widget->displayOption != DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION) {
        ValuePartsTexts texts;
        ValueParts parts;
        getCurrentValueParts(widgetCursor, texts, parts);
        return getValuePartsBaseline(parts, style, h);
    }

    return getTextBaseline(style, h);
}

bool TextWidgetState::updateState() {
    WIDGET_STATE_START_EVEN_IF_HIDDEN(TextWidget);

    // "Fit to content" sets the size also while hidden: the area of a hidden
    // widget is cleared (see RENDER_WIDGET)
    bool fitToContent = (widget->flags & (TEXT_WIDGET_FIT_TO_CONTENT_WIDTH_FLAG | TEXT_WIDGET_FIT_TO_CONTENT_HEIGHT_FLAG)) != 0;
    if (!fitToContent) {
        WIDGET_STATE_RETURN_IF_HIDDEN();
    }

    WIDGET_STATE(styleId, overrideStyle(widgetCursor, widget->style));

    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(flags.focused, isFocusWidget(widgetCursor));

    bool isEnabled = true;
    if (widget->enabled) {
        auto enabled = get(widgetCursor, widget->enabled);
        isEnabled = enabled.getType() == VALUE_TYPE_UNDEFINED || enabled.getInt();
    }
    WIDGET_STATE(flags.enabled, isEnabled ? 1 : 0);

    // disabled: the enabled data, also the action disabled by the app context
    WIDGET_STATE(flags.disabled, isWidgetActionDisabled(widgetCursor));

    // the size (fit to content) is from the style, the same in all states
    const Style *style = getStyle(styleId);
    auto stateStyle = getWidgetStateStyle(style, flags.active, flags.focused, getDisabledStateFlag(!flags.enabled || flags.disabled));

    const char *fixedText = getFixedText(widget);
    bool hasData = showsData(widget);

    WIDGET_STATE(blinkOpacity, styleIsBlink(stateStyle.style) || (hasData && isBlinking(widgetCursor, widget->data)) ? g_blinkOpacity : 255);

    if (hasData) {
        uint32_t refreshRate;
        if (widgetCursor.flowState) {
            refreshRate = widget->refreshRate ? get(widgetCursor, widget->refreshRate).toInt32(nullptr) : 0;
        } else {
            refreshRate = getTextRefreshRate(widgetCursor, widget->data);
        }

        bool refreshData = true;
        auto newData = getTextWidgetValue(widgetCursor, widget);
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
    } else {
        WIDGET_STATE(data, 0);
    }

    if (widget->label) {
        WIDGET_STATE(label, get(widgetCursor, widget->label));
    }
    if (widget->icon) {
        WIDGET_STATE(icon, get(widgetCursor, widget->icon));
    }
    if (widget->trailingIcon) {
        WIDGET_STATE(trailingIcon, get(widgetCursor, widget->trailingIcon));
    }

    WIDGET_STATE(color, getTextColor(widgetCursor, widget, hasData, stateStyle.style));
    WIDGET_STATE(backgroundColor, getTextBackgroundColor(widgetCursor, widget, hasData, stateStyle.style));

    if (hasData) {
        bool cursorVisible = millis() % (2 * CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS) < CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS;
        WIDGET_STATE(cursorPosition, cursorVisible ? getTextCursorPosition(widgetCursor, widget->data) : -1);
        WIDGET_STATE(xScroll, getXScroll(widgetCursor));
    }

    if (fitToContent) {
        font::Font font = styleGetFont(style);

        // must be kept in sync with TextWidget_autoSize and TextWidget_getContentSize
        if (widget->flags & TEXT_WIDGET_FIT_TO_CONTENT_WIDTH_FLAG) {
            // the width depends only on the text, the data and the style, so
            // it is measured only for a new state or after a change above
            if (!hasPreviousState) {
                int contentW = widgetCursor.w;
                int contentH = widgetCursor.h;
                if (usesValueParts(widget) && TextWidget_getContentSize(widgetCursor, contentW, contentH)) {
                    fitW = (int16_t)contentW;
                } else {
                    int textWidth = measureTextContent(widget, fixedText, hasData, data, font);
                    fitW = (int16_t)(style->borderSizeLeft + style->paddingLeft + textWidth + style->paddingRight + style->borderSizeRight);
                }
            }
        } else {
            fitW = widgetCursor.w;
        }

        if (widget->flags & TEXT_WIDGET_FIT_TO_CONTENT_HEIGHT_FLAG) {
            int textHeight = font ? font.getHeight() : 0;
            WIDGET_STATE(fitH, (int16_t)(style->borderSizeTop + style->paddingTop + textHeight + style->paddingBottom + style->borderSizeBottom));
        } else {
            fitH = widgetCursor.h;
        }

        applyFitToContent();
    }

    WIDGET_STATE_END()
}

void TextWidgetState::applyFitToContent() {
    auto widget = (const TextWidget *)g_widgetCursor.widget;
    if (g_layoutPlacedWidget == widget) {
        // the size is set by the row or column (see gui/layout.h)
        return;
    }
    if (widget->flags & TEXT_WIDGET_FIT_TO_CONTENT_WIDTH_FLAG) {
        if (g_isRTL) {
            // keep the right edge, see doStaticLayout
            g_widgetCursor.x += g_widgetCursor.w - fitW;
        }
        g_widgetCursor.w = fitW;
    }
    if (widget->flags & TEXT_WIDGET_FIT_TO_CONTENT_HEIGHT_FLAG) {
        g_widgetCursor.h = fitH;
    }
}

void TextWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const TextWidget *)widgetCursor.widget;
    // resolved again, the same as in updateState: the internal pages
    // (page.cpp) render a state without calling updateState
    auto stateStyle = getWidgetStateStyle(
        getStyle(overrideStyle(widgetCursor, widget->style)),
        flags.active, flags.focused, getDisabledStateFlag(!flags.enabled || flags.disabled)
    );
    const Style *style = stateStyle.style;

    if (widget->flags & (TEXT_WIDGET_FIT_TO_CONTENT_WIDTH_FLAG | TEXT_WIDGET_FIT_TO_CONTENT_HEIGHT_FLAG)) {
        // previously rendered area is not covered by the new one
        // (i.e. widget shrunk), clear it
        if (
            renderedW > 0 && renderedH > 0 &&
            (
                renderedX < widgetCursor.x ||
                renderedY < widgetCursor.y ||
                renderedX + renderedW > widgetCursor.x + widgetCursor.w ||
                renderedY + renderedH > widgetCursor.y + widgetCursor.h
            )
        ) {
            int x1 = renderedX;
            int y1 = renderedY;
            int x2 = renderedX + renderedW - 1;
            int y2 = renderedY + renderedH - 1;
            drawBorderAndBackground(x1, y1, x2, y2, nullptr, TRANSPARENT_COLOR_INDEX);
        }

        renderedX = widgetCursor.x;
        renderedY = widgetCursor.y;
        renderedW = widgetCursor.w;
        renderedH = widgetCursor.h;
    }

    // in a row only the value and the unit blink (see renderParts)
    bool isRow = usesValueParts(widget) && widget->displayOption != DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION &&
        isValuePartsRow(widget->label && (widget->flags & TEXT_WIDGET_LABEL_LEFT_FLAG), widget->icon != 0, widget->trailingIcon != 0);
    BlinkOpacity blinkOpacityScope(isRow ? 255 : blinkOpacity);

    // the colors now (not the ones of updateState: see above)
    bool hasData = showsData(widget);
    color = getTextColor(widgetCursor, widget, hasData, style);
    backgroundColor = getTextBackgroundColor(widgetCursor, widget, hasData, style);

    if (widget->displayOption == DISPLAY_OPTION_INTEGER_UNIT_AND_FRACTION && hasData) {
        renderIntegerUnitAndFraction(widget, style, stateStyle.active);
    } else if (usesValueParts(widget)) {
        renderParts(widget, style, stateStyle.active);
    } else {
        renderText(widget, style, stateStyle.active);
    }

    if (flags.focused) {
        drawFocusRing(widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style);
    }
}

void TextWidgetState::renderIntegerUnitAndFraction(const TextWidget *widget, const Style *style, bool active) {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    char text[TEXT_BUFFER_SIZE];
    getDataText(data, text, sizeof(text));

    const char *integer = text;
    int integerLength = (int)strlen(text);
    const char *unit = "";
    int unitLength = 0;
    const char *fraction = "";
    int fractionLength = 0;

    // value that is not a float (or a text with a number) is displayed whole
    // as the integer part
    if (
        data.getType() == VALUE_TYPE_FLOAT ||
        (data.isString() && isNumberText(text))
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
        &color, &backgroundColor
    );
}

void TextWidgetState::renderParts(const TextWidget *widget, const Style *style, bool active) {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    ValuePartsTexts texts;
    ValueParts parts;
    getValueParts(widget, data, label, icon, trailingIcon, texts, parts);
    const char *fixedText = getFixedText(widget);
    if (fixedText) {
        parts.value = fixedText;
        parts.valueLength = -1;
        parts.unit = nullptr;
        parts.unitLength = 0;
    }

    // the label and the unit in the states of the widget too
    unsigned otherStates = getDisabledStateFlag(!flags.enabled || flags.disabled);
    if (parts.labelStyle) {
        parts.labelStyle = getWidgetStateStyle(parts.labelStyle, flags.active, flags.focused, otherStates).style;
    }
    if (parts.unitStyle) {
        parts.unitStyle = getWidgetStateStyle(parts.unitStyle, flags.active, flags.focused, otherStates).style;
    }
    if (parts.iconStyle) {
        parts.iconStyle = getWidgetStateStyle(parts.iconStyle, flags.active, flags.focused, otherStates).style;
    }
    if (parts.trailingIconStyle) {
        parts.trailingIconStyle = getWidgetStateStyle(parts.trailingIconStyle, flags.active, flags.focused, otherStates).style;
    }

    if (flags.focused && parts.label) {
        parts.labelIcon = widgetCursor.appContext->getFocusIcon(widgetCursor, parts.labelIconStyle);
    }

    if (isValuePartsRow(widget->label && (widget->flags & TEXT_WIDGET_LABEL_LEFT_FLAG), widget->icon != 0, widget->trailingIcon != 0)) {
        parts.blinkOpacity = blinkOpacity;
    }

    drawValueParts(
        parts,
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        style,
        active,
        &color, &backgroundColor,
        data.getType() == VALUE_TYPE_FLOAT // useSmallerFontIfDoesNotFit
    );
}

void TextWidgetState::renderText(const TextWidget *widget, const Style *style, bool active) {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    uint16_t overrideColor = color;
    uint16_t overrideBackgroundColor = backgroundColor;

    const char *fixedText = getFixedText(widget);
    if (fixedText) {
        drawText(
            fixedText, -1,
            widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
            style,
            active,
            &overrideColor, &overrideBackgroundColor
        );
        return;
    }

    if (!widget->data) {
        return;
    }

    if (data.isString() && widget->displayOption == DISPLAY_OPTION_ALL) {
        if (data.getOptions() & STRING_OPTIONS_FILE_ELLIPSIS) {
                const char *fullText = data.getString();
                if (!fullText) {
                    fullText = "";
                }
                int fullTextLength = (int)strlen(fullText);
                font::Font font = styleGetFont(style);
                if (!font || display::measureStr(fullText, -1, font) <= widgetCursor.w) {
                    drawText(
                        fullText, -1,
                        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
                        style,
                        active,
                        &overrideColor, &overrideBackgroundColor
                    );
                } else {
                    // the start and the end of the text with "..." between:
                    // characters are taken alternately from the left and from the
                    // right while they fit, fullText[0 .. iLeft) + "..." +
                    // fullText[iRight .. fullTextLength) (byte offsets)
                    char text[MAX_TEXT_LEN + 1];
                    int width = display::measureStr("...", 3, font);
                    int textLength = 3;
                    int iLeft = 0;
                    int iRight = fullTextLength;
                    while (iLeft < iRight) {
                        utf8_int32_t encoding;
                        int length = getCharAt(fullText, iLeft, iRight, encoding);
                        int widthLeft = display::measureGlyph(encoding, font);
                        if (width + widthLeft > widgetCursor.w || textLength + length > (int)MAX_TEXT_LEN) {
                            break;
                        }
                        width += widthLeft;
                        iLeft += length;
                        textLength += length;

                        if (iLeft >= iRight) {
                            break;
                        }

                        int start = getCharBefore(fullText, iLeft, iRight, encoding);
                        length = iRight - start;
                        int widthRight = display::measureGlyph(encoding, font);
                        if (width + widthRight > widgetCursor.w || textLength + length > (int)MAX_TEXT_LEN) {
                            break;
                        }
                        width += widthRight;
                        iRight = start;
                        textLength += length;
                    }

                    memcpy(text, fullText, iLeft);
                    memcpy(text + iLeft, "...", 3);
                    memcpy(text + iLeft + 3, fullText + iRight, fullTextLength - iRight);
                    text[textLength] = 0;

                    drawText(
                        text, -1,
                        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
                        style,
                        active,
                        &overrideColor, &overrideBackgroundColor
                    );
                }

        } else {
            // the whole string, also when it is longer than the text buffer
            const char *str = data.getString();
            drawText(
                str ? str : "", -1,
                widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
                style,
                active,
                &overrideColor, &overrideBackgroundColor,
#if OPTION_KEYPAD
                widget->data == EEZ_DATA_ID_KEYPAD_EDIT_UNIT,
#else
                false,
#endif
                cursorPosition,
                xScroll
            );
        }
        return;
    }

    char text[TEXT_BUFFER_SIZE];
    char *start;
    int length;
    getDisplayText(widget, data, text, start, length);

    drawText(
        start, length,
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        style,
        active,
        &overrideColor, &overrideBackgroundColor,
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
    auto widget = (const TextWidget *)widgetCursor.widget;

	const Style *style = getStyle(overrideStyle(widgetCursor, widget->style));

    char text[TEXT_BUFFER_SIZE];
    Value data = getTextWidgetValue(widgetCursor, widget);
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
    auto widget = (const TextWidget *)widgetCursor.widget;

	const Style *style = getStyle(overrideStyle(widgetCursor, widget->style));

    char text[TEXT_BUFFER_SIZE];
    Value data = getTextWidgetValue(widgetCursor, widget);
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
