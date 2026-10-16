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
#include <eez/gui/widgets/containers/list.h>

namespace eez {
namespace gui {

#define LIST_TYPE_VERTICAL 1
#define LIST_TYPE_HORIZONTAL 2

// List can be stretched by the container layout (w and h are the list size
// after the layout). Item is stretched along with the list only in the
// dimension in which it has the same size as the list in the design.
static int getItemWidth(const ListWidget *widget, int w) {
    auto itemWidget = static_cast<const Widget *>(widget->itemWidget);
    return itemWidget->width == widget->width ? w : itemWidget->width;
}

static int getItemHeight(const ListWidget *widget, int h) {
    auto itemWidget = static_cast<const Widget *>(widget->itemWidget);
    return itemWidget->height == widget->height ? h : itemWidget->height;
}

bool ListWidgetState::updateState() {
    WIDGET_STATE_START(ListWidget);

    auto newCount = eez::gui::count(widgetCursor, widget->data);
    if (newCount != count) {
        count = newCount;
        hasPreviousState = false;
    }

    isVertical = widget->listType == LIST_TYPE_VERTICAL;
    lineExtent = isVertical ? getItemHeight(widget, widgetCursor.h) : getItemWidth(widget, widgetCursor.w);
    pitch = lineExtent + widget->gap;
    itemsPerLine = 1;

    // number of items that completely fit inside the list
    int extent = isVertical ? widgetCursor.h : widgetCursor.w;
    int visibleLines = pitch > 0 ? (extent + widget->gap) / pitch : 0;

    if (updateScrollState(widgetCursor, widget->data, visibleLines)) {
        hasPreviousState = false;
    }

    WIDGET_STATE_END()
}

void ListWidgetState::enumChildren() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const ListWidget *)widgetCursor.widget;

    EnumChildrenContext context;
    beginEnumChildren(context);

    int itemW = getItemWidth(widget, context.savedW);
    int itemH = getItemHeight(widget, context.savedH);

    int extent = isVertical ? context.savedH : context.savedW;

    widgetCursor.widget = static_cast<const Widget *>(widget->itemWidget);

    // While scrolling, enumerate one item more, so that the number of
    // enumerated items doesn't change on every item boundary.
    int limit = context.scrolling ? extent + pitch : extent;

    int offset = -context.renderPixelOffset;

    Value oldValue;

    for (int index = startPosition; index >= 0 && index < count && offset < limit; ++index, offset += pitch) {
        if (offset + lineExtent <= 0) {
            // item is completely outside of the list
            continue;
        }

        select(widgetCursor, widget->data, index, oldValue);

        widgetCursor.x = isVertical ? context.savedX : context.savedX + offset;
        widgetCursor.y = isVertical ? context.savedY + offset : context.savedY;
        widgetCursor.w = itemW;
        widgetCursor.h = itemH;

        widgetCursor.pushIterator(index);
        enumWidget();
        widgetCursor.popIterator();
    }

    deselect(widgetCursor, widget->data, oldValue);

    endEnumChildren(context);
}

void ListWidgetState::render() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    ScrollableWidgetState::render();

    auto widget = (const ListWidget *)widgetCursor.widget;
    const Style *style = getStyle(widget->style);

    auto n = count - startPosition;

    if (widget->listType == LIST_TYPE_VERTICAL) {
        auto offset = n * getItemHeight(widget, widgetCursor.h) + (n - 1) * widget->gap;
        auto height = widgetCursor.h;
        if (offset < height) {
            drawRectangle(widgetCursor.x, widgetCursor.y + offset, widgetCursor.w, MIN(widgetCursor.h, height - offset), style, false, false, true);
        }
    } else {
        auto offset = n * getItemWidth(widget, widgetCursor.w) + (n - 1) * widget->gap;
        auto width = widgetCursor.w;
        if (offset < width) {
            drawRectangle(widgetCursor.x + offset, widgetCursor.y, MIN(widgetCursor.w, width - offset), widgetCursor.h, style, false, false, true);
        }
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
