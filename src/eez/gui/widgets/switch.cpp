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

#include <eez/core/sound.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/switch.h>

static const uint32_t ANIMATION_DURATION = 150;
static const size_t MAX_TEXT_LEN = 128;

namespace eez {
namespace gui {

bool SwitchWidgetState::updateState() {
    WIDGET_STATE_START(SwitchWidget);

	if (changeStartTime != 0) {
		if (millis() - changeStartTime > ANIMATION_DURATION) {
			changeStartTime = 0;
		}
		hasPreviousState = false;
	}

    WIDGET_STATE(data, widget->data ? get(widgetCursor, widget->data) : 0);

    auto enabled = widget->enabled ? get(widgetCursor, widget->enabled) : Value();
    WIDGET_STATE(flags.enabled, enabled.getType() == VALUE_TYPE_UNDEFINED || enabled.getInt() ? 1 : 0);

    WIDGET_STATE(label, widget->label ? get(widgetCursor, widget->label) : Value());

    WIDGET_STATE_END()
}

void SwitchWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const SwitchWidget *)widgetCursor.widget;

    unsigned disabledState = getDisabledStateFlag(!flags.enabled);
    const Style *widgetStyle = getStyle(widget->style);
    const Style *style = getStateStyle(widgetStyle, disabledState);

	drawRectangle(
		widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
		nullptr,
		false,
		false
	);

    auto on = data.getInt() ? true : false;

	float t;
	if (changeStartTime) {
		t = 1.0f * (millis() - changeStartTime) / ANIMATION_DURATION;
		if (t > 1.0f) {
			t = 1.0f;
		}
		if (!on) {
			t = 1.0f - t;
		}
	} else {
		t = on ? 1.0f : 0;
	}

    auto x = widgetCursor.x + style->paddingLeft;
	auto y = widgetCursor.y + style->paddingTop;
	auto w =
        widgetCursor.w -
        style->paddingLeft -
        style->paddingRight;
	auto h =
        widgetCursor.h -
        style->paddingTop -
        style->paddingBottom;

    // a row: the label, the state text, then the track at the right
    if (widget->label || (widget->flags & SWITCH_WIDGET_FLAG_STATE_TEXT)) {
        int trackW = MIN(2 * h, w);
        int textLeft = x;
        int textRight = x + w - trackW;
        x = textRight;
        w = trackW;

        // the state text, in the width of the longer one (the label doesn't
        // move when the state changes)
        if (widget->flags & SWITCH_WIDGET_FLAG_STATE_TEXT) {
            const Style *base = widget->stateTextStyle ? getStyle(widget->stateTextStyle) : widgetStyle;
            const Style *stateTextStyle = getStateStyle(base, (on ? STYLE_STATE_FLAG(STYLE_STATE_CHECKED) : 0) | disabledState);
            font::Font font = styleGetFont(base);
            int textW = 0;
            if (font) {
                textW = MAX(display::measureStr("On", -1, font), display::measureStr("Off", -1, font));
            }
            int boxW = textW + base->paddingLeft + base->paddingRight;
            textRight -= boxW;
            drawText(
                on ? "On" : "Off", -1,
                textRight, widgetCursor.y, boxW, widgetCursor.h,
                stateTextStyle,
                false, false, nullptr, nullptr, false, -1, 0, true
            );
        }

        if (widget->label && textRight > textLeft) {
            const Style *base = widget->labelStyle ? getStyle(widget->labelStyle) : widgetStyle;
            char text[MAX_TEXT_LEN + 1];
            text[0] = 0;
            if (label.getType() != VALUE_TYPE_UNDEFINED) {
                label.toText(text, sizeof(text));
            }
            drawText(
                text, -1,
                textLeft, widgetCursor.y, textRight - textLeft, widgetCursor.h,
                getStateStyle(base, disabledState),
                false, false, nullptr, nullptr, false, -1, 0, true
            );
        }
    }

    // the on part: the background of the checked state style (without a
    // checked state style the same as the off part, the knob shows the state)
    const Style *onStyle = getStateStyle(widgetStyle, STYLE_STATE_FLAG(STYLE_STATE_CHECKED) | disabledState);

    display::setColor(style->borderColor);

    int pos = (int)floorf(x + t * (w - 1));

	display::setBackColor(style->backgroundColor);
    display::fillRoundedRect(
		x, y, x + w - 1, y + h - 1,
		style->borderSizeLeft, h, true, true,
        pos, y, x + w - 1, y + h - 1
	);

	display::setBackColor(onStyle->backgroundColor);
	display::fillRoundedRect(
		x, y, x + w - 1, y + h - 1,
		style->borderSizeLeft, h, true, true,
		x, y, pos - 1, y + h - 1
	);

    // draw knob: the same gap to the edge of the track on all sides
    int inset = 3 + style->borderSizeLeft;
	y += inset;
	h -= 2 * inset;

	auto xDisabled = x + inset;
	auto xEnabled = x + w - h - inset;
    x = (int)round(xDisabled + t * (xEnabled - xDisabled));

    w = h;

    display::setBackColor(style->color);
    display::fillRoundedRect(x, y, x + w - 1, y + h - 1, 1, h, false, true);
}

bool SwitchWidgetState::hasOnTouch() {
    return true;
}

void SwitchWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
	if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        if (!flags.enabled) {
            return;
        }

        // data that can't be set (a native variable that refuses the value)
        // doesn't change: no click and no animation, only the action is
        // executed
        auto widget = widgetCursor.widget;
        bool on = get(widgetCursor, widget->data).getInt() ? true : false;
        set(widgetCursor, widget->data, on ? 0 : 1);
        bool changed = (get(widgetCursor, widget->data).getInt() ? true : false) != on;

        if (widget->action != EEZ_ACTION_ID_NONE) {
            executeAction(widgetCursor, widget->action);
        } else if (changed) {
            sound::playClick();
        }

		if (changed) {
			changeStartTime = millis();
			if (changeStartTime == 0) {
				changeStartTime = 1;
			}
		}
	}
}


} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
