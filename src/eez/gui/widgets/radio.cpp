/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <string.h>

#include <eez/core/sound.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/radio.h>

namespace eez {
namespace gui {

static const size_t MAX_TEXT_LEN = 128;

// strings are compared as strings, everything else as numbers (an enum member,
// an integer and a float with the same value are equal)
static bool isRadioChecked(const Value &dataValue, const Value &radioValue) {
    Value a = dataValue.getValue();
    Value b = radioValue.getValue();

    if (a.isUndefinedOrNull() || b.isUndefinedOrNull()) {
        return false;
    }

    if (a.isString() || b.isString()) {
        if (!a.isString() || !b.isString()) {
            return false;
        }
        const char *aStr = a.getString();
        const char *bStr = b.getString();
        return aStr && bStr && strcmp(aStr, bStr) == 0;
    }

    return a.toDouble() == b.toDouble();
}

bool RadioWidgetState::updateState() {
    WIDGET_STATE_START(RadioWidget);

    WIDGET_STATE(styleId, overrideStyle(widgetCursor, widget->style));
    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(flags.disabled, isWidgetActionDisabled(widgetCursor));
    WIDGET_STATE(data, widget->data ? get(widgetCursor, widget->data) : Value());
    WIDGET_STATE(value, widget->value ? get(widgetCursor, widget->value) : Value());
    WIDGET_STATE(text, widget->text ? get(widgetCursor, widget->text) : Value());

    WIDGET_STATE_END()
}

void RadioWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    bool checked = isRadioChecked(data, value);
    auto stateStyle = getWidgetStateStyle(getStyle(styleId), flags.active, false, (checked ? STYLE_STATE_FLAG(STYLE_STATE_CHECKED) : 0) | getDisabledStateFlag(flags.disabled));

    char label[MAX_TEXT_LEN + 1];
    label[0] = 0;
    if (text.getType() != VALUE_TYPE_UNDEFINED) {
        text.toText(label, sizeof(label));
    }

    drawCheckboxOrRadio(
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        stateStyle.style,
        true,
        checked,
        label,
        stateStyle.active
    );
}

bool RadioWidgetState::hasOnTouch() {
    return widget->data || widget->action != EEZ_ACTION_ID_NONE;
}

void RadioWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        auto widget = (const RadioWidget *)widgetCursor.widget;

        // data that can't be set (e.g. a native variable without set) doesn't
        // change, then only the action is executed
        bool changed = false;
        if (widget->data && widget->value) {
            Value radioValue = get(widgetCursor, widget->value);
            bool checked = isRadioChecked(get(widgetCursor, widget->data), radioValue);
            set(widgetCursor, widget->data, radioValue);
            changed = !checked && isRadioChecked(get(widgetCursor, widget->data), radioValue);
        }

        if (widget->action != EEZ_ACTION_ID_NONE) {
            executeAction(widgetCursor, widget->action);
        } else if (changed) {
            sound::playClick();
        }
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
