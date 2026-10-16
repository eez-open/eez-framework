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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/grid.h>

namespace eez {
namespace gui {

#define GRID_FLOW_ROW 1
#define GRID_FLOW_COLUMN 2

// Grid is scrolled by rows (vertically) if items flow in rows,
// or by columns (horizontally) if items flow in columns.
bool GridWidgetState::updateState() {
    WIDGET_STATE_START(GridWidget);

    auto newCount = eez::gui::count(widgetCursor, widget->data);
    if (newCount != count) {
        count = newCount;
        hasPreviousState = false;
    }

    auto itemWidget = static_cast<const Widget *>(widget->itemWidget);

    isVertical = widget->gridFlow != GRID_FLOW_COLUMN;

    int extent = isVertical ? widgetCursor.h : widgetCursor.w;
    int cross = isVertical ? widgetCursor.w : widgetCursor.h;
    int itemCross = isVertical ? itemWidget->width : itemWidget->height;

    lineExtent = isVertical ? itemWidget->height : itemWidget->width;
    pitch = lineExtent;

    itemsPerLine = itemCross > 0 ? cross / itemCross : 0;
    if (itemsPerLine < 1) {
        itemsPerLine = 1;
    }

    // number of lines that completely fit inside the grid
    int visibleLines = pitch > 0 ? extent / pitch : 0;

    if (updateScrollState(widgetCursor, widget->data, visibleLines)) {
        hasPreviousState = false;
    }

    WIDGET_STATE_END()
}

void GridWidgetState::enumChildren() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const GridWidget *)widgetCursor.widget;

    EnumChildrenContext context;
    beginEnumChildren(context);

    const Widget *childWidget = static_cast<const Widget *>(widget->itemWidget);

    int itemW = childWidget->width;
    int itemH = childWidget->height;

    int extent = isVertical ? context.savedH : context.savedW;
    int crossPitch = isVertical ? itemW : itemH;

    widgetCursor.widget = childWidget;

    // While scrolling, pixel offset is relative to the start of the line, and
    // one line more is enumerated, so that the number of enumerated items
    // doesn't change on every line boundary. Partially visible line is shown
    // only if the grid is scrollable, otherwise only lines that completely fit.
    int firstIndex = context.scrolling ? startPosition / itemsPerLine * itemsPerLine : startPosition;
    int limit;
    if (context.scrolling) {
        limit = extent + pitch;
    } else if (touchScrollable) {
        limit = extent;
    } else {
        limit = MAX(extent - pitch + 1, 1);
    }

    int offset = -context.renderPixelOffset;

    Value oldValue;

    for (int index = firstIndex; index >= 0 && index < count && offset < limit; offset += pitch) {
        for (int i = 0; i < itemsPerLine && index < count; ++i, ++index) {
            if (offset + pitch <= 0) {
                // line is completely outside of the grid
                continue;
            }

            select(widgetCursor, widget->data, index, oldValue);

            widgetCursor.x = isVertical ? context.savedX + i * crossPitch : context.savedX + offset;
            widgetCursor.y = isVertical ? context.savedY + offset : context.savedY + i * crossPitch;
            widgetCursor.w = itemW;
            widgetCursor.h = itemH;

            widgetCursor.pushIterator(index);
            enumWidget();
            widgetCursor.popIterator();
        }
    }

    deselect(widgetCursor, widget->data, oldValue);

    endEnumChildren(context);
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
