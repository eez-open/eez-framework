/*
 * EEZ Modular Firmware
 * Copyright (C) 2026-present, Envox d.o.o.
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

#pragma once

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////
// Touch (kinetic) scrolling of the List and the Grid widget.
//
// Items are arranged in lines: in the List each item is one line, in the Grid
// a line is one row (if items flow in rows, it scrolls vertically) or one
// column (if items flow in columns, it scrolls horizontally).
//
// If widget data supports DATA_OPERATION_YT_DATA_GET_SIZE, _GET_PAGE_SIZE,
// _GET_POSITION and _SET_POSITION (same as for the ScrollBar widget), the
// widget is scrollable when size > page size and the position (in items) is
// kept in the data. Otherwise (YT_DATA_GET_SIZE returns undefined value), the
// widget is scrollable when its lines don't fit inside it and the position is
// kept in the widget state. The widget is dragged with a finger, keeps moving
// after a flick, and comes to rest aligned to a line. If the line is exactly
// as big as the widget, a single flick moves by one line only. If scrollable
// widgets are nested, the innermost one that scrolls in the direction in
// which the finger moved takes the touch.
//
// Below, "list" is any scrollable widget (List or Grid).

struct ScrollableWidgetState : public WidgetState {
    int startPosition = 0; // in items
    int count;
    int pixelOffset;
    int renderedPixelOffset = 0;
    bool touchScrollable;
    // widget data doesn't support DATA_OPERATION_YT_DATA_GET_SIZE, ..., so
    // startPosition is not taken from the data but kept here (set by touch scroll)
    bool localPosition;
    bool isVertical;
    int pitch;          // distance between two lines in px
    int lineExtent;     // line size along the scroll direction in px (without gap)
    int itemsPerLine;   // 1 for the List
    int maxPosition;    // in lines
    // scroll indicator drawn in the last frame (w == 0 if not drawn)
    Rect renderedScrollIndicator = {};
    // render() was called in this frame
    bool rendered = false;

    void render() override;

protected:
    // Called from updateState after count, isVertical, pitch, lineExtent and
    // itemsPerLine are set. visibleLines is the number of lines that
    // completely fit inside the widget. Returns true if items are moved.
    bool updateScrollState(const WidgetCursor &widgetCursor, int16_t data, int visibleLines);

    // Saved between beginEnumChildren and endEnumChildren.
    struct EnumChildrenContext {
        bool isFindScrollList;

        int16_t savedX;
        int16_t savedY;
        int16_t savedW;
        int16_t savedH;
        Cursor savedCursor;
        bool savedRefreshed;

        // items are moved by renderPixelOffset along the scroll direction
        int renderPixelOffset;
        bool scrolling;

        display::ClipRect savedClipRect;
        Rect savedFindClipRect;
        Rect scrollIndicator;
        bool drawScrollIndicator;
        bool savedDirty;
    };

    // Called at the start and at the end of enumChildren, items are enumerated
    // in between. Items are clipped to the widget (item can be partially outside
    // of the widget, e.g. the last item or, while touch scrolling, items moved
    // by pixelOffset).
    void beginEnumChildren(EnumChildrenContext &context);
    void endEnumChildren(EnumChildrenContext &context);
};

// scrollable lists that enclose the widget currently visited by forEachWidget
extern ScrollLists g_findEnclosingScrollLists;

bool isTouchScrollableList(const WidgetCursor &widgetCursor);

// Called on touch down with the scrollable lists under the finger (can be empty).
// Returns true if the touch caught the list while it was still moving: the
// touch then belongs to the list from the start.
bool touchScrollBegin(const ScrollLists &lists, const Event &touchEvent);

// Called on touch move while the touch still belongs to the widget pressed at touch down.
// Returns true once the finger moved far enough along one of the lists to start the scroll.
bool touchScrollShouldCapture(const Event &touchEvent);

// Touch function of the list after it captured the touch.
void touchScrollOnTouch(const WidgetCursor &widgetCursor, Event &touchEvent);

// Called on touch up if the list did not capture the touch.
void touchScrollEnd();

// Finger is down on a scrollable list, but hasn't moved far enough yet to start the scroll.
bool isTouchScrollPending();

// List is dragged with a finger. Touch driver can report the release without
// debounce delay, so the list continues to move as soon as the finger is lifted.
bool isTouchScrollDragging();

const WidgetCursor &getTouchScrollList();

// Called once per frame.
void touchScrollTick();

} // namespace gui
} // namespace eez
