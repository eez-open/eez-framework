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

#include <eez/core/sound.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/checkbox.h>

namespace eez {
namespace gui {

static const size_t MAX_TEXT_LEN = 128;

bool CheckboxWidgetState::updateState() {
    WIDGET_STATE_START(CheckboxWidget);

    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(data, widget->data ? get(widgetCursor, widget->data) : 0);
    WIDGET_STATE(text, widget->text ? get(widgetCursor, widget->text) : Value());

    WIDGET_STATE_END()
}

void CheckboxWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const CheckboxWidget *)widgetCursor.widget;
    const Style *style = getStyle(overrideStyle(widgetCursor, widget->style));

    char label[MAX_TEXT_LEN + 1];
    label[0] = 0;
    if (text.getType() != VALUE_TYPE_UNDEFINED) {
        text.toText(label, sizeof(label));
    }

    drawCheckboxOrRadio(
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        style,
        false,
        data.toBool(),
        label,
        flags.active
    );
}

bool CheckboxWidgetState::hasOnTouch() {
    return widget->data || widget->action != EEZ_ACTION_ID_NONE;
}

void CheckboxWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        auto widget = widgetCursor.widget;

        // data that can't be set (e.g. a native variable without set) doesn't
        // change, then only the action is executed
        bool changed = false;
        if (widget->data) {
            bool checked = get(widgetCursor, widget->data).toBool();
            set(widgetCursor, widget->data, Value(!checked, VALUE_TYPE_BOOLEAN));
            changed = get(widgetCursor, widget->data).toBool() != checked;
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
