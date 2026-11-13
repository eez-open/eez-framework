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

#include <math.h>
#include <string.h>

#include <eez/core/alloc.h>
#include <eez/core/util.h>
#include <eez/core/utf8.h>

#include <eez/gui/gui.h>
#include <eez/gui/data.h>

#include <eez/gui/widgets/input.h>

#include <eez/flow/private.h>

namespace eez {
namespace gui {

static const size_t MAX_TEXT_LEN = 128;

// The parameters are evaluated with the widget cursor every time (not cached
// per component), because the same Input widget can be rendered several times
// inside a List or Grid with different parameters for each item.

Unit getInputWidgetUnit(const gui::WidgetCursor &widgetCursor) {
	auto widget = (const InputWidget*)widgetCursor.widget;

	auto unitValue = get(widgetCursor, widget->unit);
	if (unitValue.isString()) {
		return getUnitFromName(unitValue.getString());
	} else {
		return UNIT_NONE;
	}
}

static Value getInputWidgetNumberParam(const gui::WidgetCursor &widgetCursor, int16_t param) {
	auto unit = getInputWidgetUnit(widgetCursor);
	auto value = get(widgetCursor, param);
	return Value(value.toFloat() * getUnitFactor(unit), getBaseUnit(unit));
}

bool getInputWidgetIsPassword(const gui::WidgetCursor &widgetCursor) {
	auto widget = (const InputWidget*)widgetCursor.widget;
	return (widget->flags & INPUT_WIDGET_PASSWORD_FLAG) != 0;
}

Value getInputWidgetMin(const gui::WidgetCursor &widgetCursor) {
	return getInputWidgetNumberParam(widgetCursor, ((const InputWidget*)widgetCursor.widget)->min);
}

Value getInputWidgetMax(const gui::WidgetCursor &widgetCursor) {
	return getInputWidgetNumberParam(widgetCursor, ((const InputWidget*)widgetCursor.widget)->max);
}

Value getInputWidgetDefaultValue(const gui::WidgetCursor &widgetCursor) {
	return getInputWidgetNumberParam(widgetCursor, ((const InputWidget*)widgetCursor.widget)->defaultValue);
}

Value getInputWidgetPrecision(const gui::WidgetCursor &widgetCursor) {
	return getInputWidgetNumberParam(widgetCursor, ((const InputWidget*)widgetCursor.widget)->precision);
}

Unit getInputWidgetDisplayUnit(const gui::WidgetCursor &widgetCursor) {
	auto widget = (const InputWidget*)widgetCursor.widget;
	if (widget->displayUnit) {
		auto unitValue = get(widgetCursor, widget->displayUnit);
		if (unitValue.isString()) {
			Unit unit = getUnitFromName(unitValue.getString());
			if (unit != UNIT_UNKNOWN) {
				return unit;
			}
		}
	}
	return getInputWidgetUnit(widgetCursor);
}

static bool hasUnit(Unit unit) {
	return unit != UNIT_NONE && unit != UNIT_UNKNOWN;
}

Value getInputWidgetShownValue(const gui::WidgetCursor &widgetCursor, const Value &data) {
	Unit unit = getInputWidgetDisplayUnit(widgetCursor);

	// the unit getValueInUnit shows the data in
	Unit dataUnit = data.getUnit();
	Unit shownUnit = hasUnit(unit) && (!hasUnit(dataUnit) || getBaseUnit(dataUnit) == getBaseUnit(unit)) ? unit : dataUnit;

	// the precision (in the base unit) in that unit
	double precision = getInputWidgetPrecision(widgetCursor).toDouble();
	if (precision > 0 && hasUnit(shownUnit)) {
		precision /= getUnitFactor(shownUnit);
	}

	return getValueInUnit(data, unit, precision);
}

Value getInputWidgetInfo(const gui::WidgetCursor &widgetCursor) {
	return get(widgetCursor, ((const InputWidget*)widgetCursor.widget)->info);
}

// set while InputWidgetState::updateState reads the data: the string is
// returned without a copy, the state copies it only when it is changed
static bool g_readStringInPlace;

Value getInputWidgetData(const gui::WidgetCursor &widgetCursor, const Value &dataValue) {
	auto widget = (const InputWidget*)widgetCursor.widget;

	if (widget->flags & INPUT_WIDGET_TYPE_NUMBER) {
		auto unit = getInputWidgetUnit(widgetCursor);
		float factor = getUnitFactor(unit);
		auto baseUnit = getBaseUnit(unit);
		// the decimals of the data (a module field with a resolution) are kept
		// when the number isn't converted
		uint16_t options = 0;
		if (factor == 1.0f && (dataValue.isFloat() || dataValue.isDouble())) {
			options = dataValue.getOptions() & (FLOAT_OPTIONS_FIXED_DECIMALS | FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(7));
		}
    	return Value(dataValue.toFloat() * factor, baseUnit, options);
	} else if (dataValue.isString()) {
		if (g_readStringInPlace) {
			return dataValue;
		}
		// a copy: the string of the data can be in a buffer that is reused
		const char *str = dataValue.getString();
        return Value::makeStringRef(str ? str : "", -1, 0xa3d4f1d4);
	} else {
		// a number is shown as its text, undefined and null as an empty text
		return dataValue.toString(0xa3d4f1d4);
	}
}

bool InputWidgetState::updateState() {
    WIDGET_STATE_START(InputWidget);

	WIDGET_STATE(styleId, overrideStyle(widgetCursor, widget->style));

	WIDGET_STATE(flags.active, g_isActiveWidget);
	WIDGET_STATE(flags.focused, isFocusWidget(widgetCursor));
	WIDGET_STATE(flags.disabled, isWidgetActionDisabled(widgetCursor));

	auto stateStyle = getWidgetStateStyle(getStyle(styleId), flags.active, flags.focused, getDisabledStateFlag(flags.disabled));
	const Style *style = stateStyle.style;

	WIDGET_STATE(blinkOpacity, styleIsBlink(style) ? g_blinkOpacity : 255);
	WIDGET_STATE(color, g_hooks.overrideStyleColor(widgetCursor, style));

	// A string is compared where it is (no copy in every frame), WIDGET_STATE
	// copies it only when it is changed
	g_readStringInPlace = true;
	Value newData = get(widgetCursor, widget->data);
	g_readStringInPlace = false;
	WIDGET_STATE(data, newData);

	if (widget->flags & INPUT_WIDGET_TYPE_NUMBER) {
		WIDGET_STATE(shownData, getInputWidgetShownValue(widgetCursor, data));
	}

    WIDGET_STATE_END()
}

void InputWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

	auto widget = (const InputWidget*)widgetCursor.widget;

	auto stateStyle = getWidgetStateStyle(getStyle(styleId), flags.active, flags.focused, getDisabledStateFlag(flags.disabled));
	const Style *style = stateStyle.style;

	BlinkOpacity blinkOpacityScope(blinkOpacity);

	uint16_t overrideColor = color;
	uint16_t overrideBackgroundColor = style->backgroundColor;

	char text[MAX_TEXT_LEN + 1];
	if (widget->flags & INPUT_WIDGET_TYPE_NUMBER) {
		shownData.toText(text, sizeof(text));
	} else {
		data.toText(text, sizeof(text));
	}

	if ((widget->flags & INPUT_WIDGET_TYPE_TEXT) && (widget->flags & INPUT_WIDGET_PASSWORD_FLAG)) {
		// one * for each character (UTF-8 continuation bytes are skipped)
		int n = 0;
		for (int i = 0; text[i]; i++) {
#if UTF8_SUPPORT
			if (((uint8_t)text[i] & 0xC0) == 0x80) {
				continue;
			}
#endif
			text[n++] = '*';
		}
		text[n] = 0;
	}

	drawText(
		text, -1,
		widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
		style,
		stateStyle.active, false,
		&overrideColor, &overrideBackgroundColor,
		true // useSmallerFontIfDoesNotFit
	);

	if (flags.focused) {
		drawFocusRing(widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style);
	}
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
