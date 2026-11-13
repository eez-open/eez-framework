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

#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/button.h>

static const size_t MAX_TEXT_LEN = 128;

namespace eez {
namespace gui {

bool ButtonWidgetState::updateState() {
    WIDGET_STATE_START(ButtonWidget);

    WIDGET_STATE(flags.active, g_isActiveWidget);

    auto enabled = get(widgetCursor, widget->enabled);
    WIDGET_STATE(flags.enabled, enabled.getType() == VALUE_TYPE_UNDEFINED || enabled.getInt() ? 1 : 0);

    // disabled: the enabled data, also the action disabled by the app context
    WIDGET_STATE(flags.disabled, isWidgetActionDisabled(widgetCursor));

    WIDGET_STATE(styleId, overrideStyle(widgetCursor, widget->style));
    const Style *style = getWidgetStateStyle(getStyle(styleId), flags.active, false, getDisabledStateFlag(!flags.enabled || flags.disabled)).style;
    WIDGET_STATE(blinkOpacity, isBlinking(widgetCursor, widget->data) || styleIsBlink(style) ? g_blinkOpacity : 255);

    WIDGET_STATE(data, widget->data ? get(widgetCursor, widget->data) : 0);

    WIDGET_STATE_END()
}

void ButtonWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const ButtonWidget *)widgetCursor.widget;
    // resolved again, the same as in updateState: the internal pages
    // (page.cpp) render a ButtonWidgetState without calling updateState
    auto stateStyle = getWidgetStateStyle(
        getStyle(overrideStyle(widgetCursor, widget->style)),
        flags.active, false, getDisabledStateFlag(!flags.enabled || flags.disabled)
    );
    const Style *style = stateStyle.style;
    bool active = stateStyle.active;

    BlinkOpacity blinkOpacityScope(blinkOpacity);

    if (widget->data) {
        if (data.isString()) {
            drawText(data.getString(), -1, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, active);
        } else {
            char text[MAX_TEXT_LEN + 1];
            data.toText(text, sizeof(text));
            drawText(text, -1, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, active);
        }
    } else if (widget->text) {
        drawText(static_cast<const char *>(widget->text), -1, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, active);
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
