/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/progress.h>

namespace eez {
namespace gui {

bool ProgressWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    bool hasPreviousState = widgetCursor.hasPreviousState;
    auto widget = (const ProgressWidget *)widgetCursor.widget;
    
    WIDGET_STATE(data, get(widgetCursor, widget->data));

    return !hasPreviousState;
}

void ProgressWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const ProgressWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);

    drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style);

    int percentFrom;
    int percentTo;
    if (data.getType() == VALUE_TYPE_RANGE) {
        percentFrom = data.getRangeFrom();
        percentTo = data.getRangeTo();
    } else {
        percentFrom = 0;
        percentTo = data.getInt();
    }

    percentFrom = clamp(percentFrom, 0, 100.0f);
    percentTo = clamp(percentTo, 0, 100.0f);
    if (percentFrom > percentTo) {
        percentFrom = percentTo;
    }

    auto isHorizontal = widget->w > widget->h;
    if (isHorizontal) {
        auto xFrom = percentFrom * widget->w / 100;
        auto xTo = percentTo * widget->w / 100;
        drawRectangle(widgetCursor.x + xFrom, widgetCursor.y, xTo - xFrom, (int)widget->h, style, true);
    } else {
        auto yFrom = percentFrom * widget->h / 100;
        auto yTo = percentTo * widget->h / 100;
        drawRectangle(widgetCursor.x, widgetCursor.y - yFrom, yTo - yFrom, (int)widget->h, getStyle(widget->style), true);
    }
}

} // namespace gui
} // namespace eez