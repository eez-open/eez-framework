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

#include <eez/flow/private.h>

namespace eez {
namespace gui {

struct InputWidget : public Widget {
	uint16_t flags;
	int16_t min;
	int16_t max;
    int16_t precision;
	int16_t unit;
	uint16_t componentIndex;
	int16_t info;
	int16_t defaultValue;
	// a number Input is shown in this unit (a string such as "mA", 0 if not
	// set: in unit) with the decimals of its precision, see getValueInUnit
	int16_t displayUnit;
	// a text above the value, or left of it with INPUT_WIDGET_LABEL_LEFT_FLAG
	// (a flow expression, 0 if not set), in labelStyle (0: the widget style),
	// see drawValueParts
	int16_t label;
	int16_t labelStyle;
	int16_t reserved1;
};

struct InputWidgetState : public WidgetState {
	WidgetStateFlags flags;
	Value data;
	// a number: data as it is shown (getInputWidgetShownValue), it changes
	// also with the display unit and the precision
	Value shownData;
	Value label;
	// style after overrideStyle and the text color after the
	// overrideStyleColor hook
	int styleId;
	uint16_t color;
	// g_blinkOpacity while the widget blinks, else 255 (see BlinkOpacity)
	uint8_t blinkOpacity;

    bool updateState() override;
	void render() override;
};

static const uint16_t INPUT_WIDGET_TYPE_TEXT = 0x0001;
static const uint16_t INPUT_WIDGET_TYPE_NUMBER = 0x0002;
static const uint16_t INPUT_WIDGET_PASSWORD_FLAG = 0x0100;
// the label is left of the value (ValueParts::labelLeft)
static const uint16_t INPUT_WIDGET_LABEL_LEFT_FLAG = 0x0200;

bool getInputWidgetIsPassword(const gui::WidgetCursor &widgetCursor);

Value getInputWidgetMin(const gui::WidgetCursor &widgetCursor);
Value getInputWidgetMax(const gui::WidgetCursor &widgetCursor);

Value getInputWidgetDefaultValue(const gui::WidgetCursor &widgetCursor);

Value getInputWidgetPrecision(const gui::WidgetCursor &widgetCursor);

Unit getInputWidgetUnit(const gui::WidgetCursor &widgetCursor);

// the display unit, or the unit when it is not set
Unit getInputWidgetDisplayUnit(const gui::WidgetCursor &widgetCursor);

// The number data (in the base unit, see getInputWidgetData) as the Input shows
// it: in the display unit with the decimals of the precision (getValueInUnit).
Value getInputWidgetShownValue(const gui::WidgetCursor &widgetCursor, const Value &data);

Value getInputWidgetInfo(const gui::WidgetCursor &widgetCursor);

Value getInputWidgetData(const gui::WidgetCursor &widgetCursor, const Value &dataValue);

// The y of the baseline of the value the Input widget widgetCursor.widget shows
// in a box of the height h, from the top of the box; -1 if its style has no
// font.
int InputWidget_getBaseline(const WidgetCursor &widgetCursor, int h);

} // namespace gui
} // namespace eez
