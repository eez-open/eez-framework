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
#include <stdio.h>

#include <eez/core/alloc.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/data.h>
#include <eez/gui/widgets/gauge.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

static const float PI_F = 3.14159265358979f;

static float deg2Rad(float angleDeg) {
	return angleDeg * PI_F / 180.0f;
}

// The band between the radii from the left (180 degrees) over the top to the
// angle (degrees, counterclockwise from the right).
static void arcBar(display::Path &path, float xCenter, float yCenter, float radOuter, float radInner, float angleDeg) {
	// on the screen y goes down, so the angle goes clockwise
	float angle = -deg2Rad(angleDeg);
	path.arc(xCenter, yCenter, radOuter, radOuter, -PI_F, angle);
	path.arc(xCenter, yCenter, radInner, radInner, angle, -PI_F);
	path.close();
}

float firstTick(float n) {
	auto p = powf(10, floorf(log10f(n / 6.0f)));
	auto f = n / 6.0f / p;
	int i;
	if (f > 5) {
		i = 10;
	} else if (f > 2) {
		i = 5;
	} else {
		i = 2;
	}
	return i * p;
}

////////////////////////////////////////////////////////////////////////////////

bool GaugeWidgetState::updateState() {
    WIDGET_STATE_START(GaugeWidget);

	WIDGET_STATE(flags.active, g_isActiveWidget);
	WIDGET_STATE(data, get(widgetCursor, widget->data));
	WIDGET_STATE(minValue, get(widgetCursor, widget->min));
	WIDGET_STATE(maxValue, get(widgetCursor, widget->max));
	WIDGET_STATE(thresholdValue, get(widgetCursor, widget->threshold));
	WIDGET_STATE(unitValue, get(widgetCursor, widget->unit));

    WIDGET_STATE_END()
}

void GaugeWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

	auto widget = (const GaugeWidget*)widgetCursor.widget;

	using namespace display;

	float min = minValue.toFloat();
	float max = maxValue.toFloat();
	float value = data.toFloat();
	float threshold = thresholdValue.toFloat();

	// the string is kept in unitString while it is used
	Value unitString = unitValue.toString(0xa9ddede3);
	const char *unit = unitString.getString();

	if (isNaN(min) || isNaN(max) || isNaN(value) || isinf(min) || isinf(max) || isinf(value) || min >= max) {
		min = 0.0;
		max = 1.0f;
		value = 0.0f;
	} else {
		if (value < min) {
			value = min;
		} else if (value > max) {
			value = max;
		}
	}

	const Style* style = getStyle(widget->style);
	const Style* barStyle = getStyle(widget->barStyle);
	const Style* valueStyle = getStyle(widget->valueStyle);
	const Style* ticksStyle = getStyle(widget->ticksStyle);
	const Style* thresholdStyle = getStyle(widget->thresholdStyle);

	auto isActive = flags.active;

	Color colorBorder = getColorFromIndex(isActive ? style->activeColor : style->color);
	Color colorBar = getColorFromIndex(isActive ? barStyle->activeColor : barStyle->color);

	auto xCenter = widgetCursor.w / 2;
	auto yCenter = widgetCursor.h - 8;

	// clear background
	setColor(isActive ? style->activeBackgroundColor : style->backgroundColor);
	fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w - 1, widgetCursor.y + widgetCursor.h - 1);

	auto savedClipRect = intersectClipRect(widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h);

	// the drawing below is relative to the widget
	float x0 = widgetCursor.x;
	float y0 = widgetCursor.y;

	// draw frame
	if (style->borderSizeLeft > 0) {
		setColorByValue(colorBorder);
		drawRoundedRect(
			widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w - 1, widgetCursor.y + widgetCursor.h - 1,
			style->borderSizeLeft,
			style->borderRadiusTLX, style->borderRadiusTLY, style->borderRadiusTRX, style->borderRadiusTRY,
			style->borderRadiusBRX, style->borderRadiusBRY, style->borderRadiusBLX, style->borderRadiusBLY
		);
	}

	static const int PADDING_HORZ = 56;
	static const int TICK_LINE_LENGTH = 5;
	static const int TICK_LINE_WIDTH = 1;
	static const int TICK_TEXT_GAP = 1;
	static const int THRESHOLD_LINE_WIDTH = 2;

	// draw border
	auto radBorderOuter = (widgetCursor.w - PADDING_HORZ) / 2;

	auto BORDER_WIDTH = radBorderOuter / 3;
	auto BAR_WIDTH = BORDER_WIDTH / 2;

	auto radBorderInner = radBorderOuter - BORDER_WIDTH;

	// reused, the path keeps its memory between the draws
	static Path path;

	{
		path.clear();
		arcBar(path, x0 + xCenter, y0 + yCenter, radBorderOuter, radBorderInner, 0);
		setColorByValue(colorBorder);
		strokePath(path, 1.5f);
	}

	// draw bar
	auto radBarOuter = (widgetCursor.w - PADDING_HORZ) / 2 - (BORDER_WIDTH - BAR_WIDTH) / 2;
	auto radBarInner = radBarOuter - BAR_WIDTH;
	auto angle = remap(value, min, 180.0f, max, 0.0f);
	{
		path.clear();
		arcBar(path, x0 + xCenter, y0 + yCenter, radBarOuter, radBarInner, angle);
		setColorByValue(colorBar);
		fillPath(path);
	}

	// draw threshold
	auto thresholdAngleDeg = remap(threshold, min, 180.0f, max, 0);
	if (thresholdAngleDeg >= 0 && thresholdAngleDeg <= 180.0f) {
		auto thresholdAngle = deg2Rad(thresholdAngleDeg);
		float acos = cosf(thresholdAngle);
		float asin = sinf(thresholdAngle);
		int x1 = floorf(xCenter + radBarInner * acos);
		int y1 = floorf(yCenter - radBarInner * asin);
		int x2 = floorf(xCenter + radBarOuter * acos);
		int y2 = floorf(yCenter - radBarOuter * asin);

		float points[] = { x0 + x1, y0 + y1, x0 + x2, y0 + y2 };
		setColorByValue(getColorFromIndex(isActive ? thresholdStyle->activeColor : thresholdStyle->color));
		drawPolyline(points, 2, THRESHOLD_LINE_WIDTH);
	}

	// draw ticks: at min + i * ft (not accumulated, ft can be below the float
	// resolution at min), at most MAX_TICKS
	static const int MAX_TICKS = 100;
	font::Font ticksFont = styleGetFont(ticksStyle);
	auto ft = firstTick(max - min);
	auto ticksRad = radBorderOuter + 1;
	int numTicks = ft > 0 && !isinf(ft) ? MAX_TICKS : 0;
	for (int tickIndex = 0; tickIndex < numTicks; tickIndex++) {
		float tickValue = min + tickIndex * ft;
		if (tickValue > max) {
			break;
		}
		auto tickAngleDeg = remap(tickValue, min, 180.0f, max, 0);
		if (tickAngleDeg <= 180.0) {
			auto tickAngle = deg2Rad(tickAngleDeg);
			float acos = cosf(tickAngle);
			float asin = sinf(tickAngle);
			int x1 = floorf(xCenter + ticksRad * acos);
			int y1 = floorf(yCenter - ticksRad * asin);
			int x2 = floorf(xCenter + (ticksRad + TICK_LINE_LENGTH) * acos);
			int y2 = floorf(yCenter - (ticksRad + TICK_LINE_LENGTH) * asin);

			float points[] = { x0 + x1, y0 + y1, x0 + x2, y0 + y2 };
			setColorByValue(getColorFromIndex(isActive ? ticksStyle->activeColor : ticksStyle->color));
			drawPolyline(points, 2, TICK_LINE_WIDTH);

			char tickText[50];
			snprintf(tickText, sizeof(tickText), "%g", tickValue);
			if (unit && *unit) {
				stringAppendString(tickText, sizeof(tickText), " ");
				stringAppendString(tickText, sizeof(tickText), unit);
			}

			auto tickTextWidth = display::measureStr(tickText, -1, ticksFont);
			if (tickAngleDeg == 180.0) {
				drawText(
					tickText,
					-1,
					widgetCursor.x + xCenter -
						radBorderOuter -
						TICK_TEXT_GAP -
						tickTextWidth,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			} else if (tickAngleDeg > 90.0) {
				drawText(
					tickText,
					-1,
					widgetCursor.x + x2 - TICK_TEXT_GAP - tickTextWidth,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			} else if (tickAngleDeg == 90.0) {
				drawText(
					tickText,
					-1,
					widgetCursor.x + x2 - tickTextWidth / 2,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			} else if (tickAngleDeg > 0) {
				drawText(
					tickText,
					-1,
					widgetCursor.x + x2 + TICK_TEXT_GAP,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			} else {
				drawText(
					tickText,
					-1,
					widgetCursor.x + xCenter + radBorderOuter + TICK_TEXT_GAP,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			}
		}
	}

	// draw value
	font::Font valueFont = styleGetFont(valueStyle);
	char valueText[50];
	snprintf(valueText, sizeof(valueText), "%g", value);
	if (unit && *unit) {
        stringAppendString(valueText, sizeof(valueText), " ");
        stringAppendString(valueText, sizeof(valueText), unit);
	}
	auto valueTextWidth = display::measureStr(valueText, -1, valueFont);
	drawText(
		valueText,
		-1,
		widgetCursor.x + xCenter - valueTextWidth / 2,
		widgetCursor.y + yCenter - valueFont.getHeight(),
		valueTextWidth,
		valueFont.getHeight(),
		valueStyle,
		isActive
	);

	restoreClipRect(savedClipRect);
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
