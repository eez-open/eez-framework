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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// TODO remove: temporary touch scroll tracing
#define TOUCH_SCROLL_TRACE 0

#include <eez/core/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/scrolling.h>

namespace eez {
namespace gui {

ScrollLists g_findEnclosingScrollLists;

static int getTouchScrollPixelOffset(const WidgetCursor &widgetCursor, int startPosition);
static int getTouchScrollLocalPosition(const WidgetCursor &widgetCursor, int startPosition);

static bool isYtDataSupported(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_SIZE, widgetCursor, value);
    return value.getType() != VALUE_TYPE_UNDEFINED;
}

bool ScrollableWidgetState::updateScrollState(const WidgetCursor &widgetCursor, int16_t data, int visibleLines) {
    bool changed = false;

    localPosition = !isYtDataSupported(widgetCursor, data);

    int newStartPosition;
    if (localPosition) {
        int lines = (count + itemsPerLine - 1) / itemsPerLine;
        maxPosition = lines > visibleLines ? lines - visibleLines : 0;
        newStartPosition = getTouchScrollLocalPosition(widgetCursor, startPosition);
        if (newStartPosition > maxPosition * itemsPerLine) {
            newStartPosition = maxPosition * itemsPerLine;
        }
    } else {
        int size = ytDataGetSize(widgetCursor, data);
        int pageSize = ytDataGetPageSize(widgetCursor, data);
        // last line can be only partially filled
        maxPosition = size > pageSize ? (size - pageSize + itemsPerLine - 1) / itemsPerLine : 0;
        newStartPosition = ytDataGetPosition(widgetCursor, data);
    }
    touchScrollable = maxPosition > 0 && pitch > 0;

    if (newStartPosition != startPosition) {
        startPosition = newStartPosition;
        changed = true;
    }

    auto newPixelOffset = getTouchScrollPixelOffset(widgetCursor, startPosition / itemsPerLine);
    if (newPixelOffset != pixelOffset) {
        pixelOffset = newPixelOffset;
        changed = true;
    }

    return changed;
}

// Thin non interactive indicator of the scroll position, on the right side of
// the vertical list or on the bottom of the horizontal list (in style color).
static const int SCROLL_INDICATOR_THICKNESS = 3;
static const int SCROLL_INDICATOR_MARGIN = 2;
static const int SCROLL_INDICATOR_MIN_LENGTH = 16;

static Rect getScrollIndicatorRect(bool isVertical, const Style *style, int x, int y, int w, int h, int pitch, int maxPosition, int scroll) {
    Rect rect = {};

    if (maxPosition <= 0 || pitch <= 0 || style->color == TRANSPARENT_COLOR_INDEX) {
        return rect;
    }

    int extent = isVertical ? h : w;
    int track = extent - 2 * SCROLL_INDICATOR_MARGIN;
    if (track <= 0) {
        return rect;
    }

    int maxScroll = maxPosition * pitch;
    if (scroll < 0) {
        scroll = 0;
    } else if (scroll > maxScroll) {
        scroll = maxScroll;
    }

    int length = track * extent / (extent + maxScroll);
    if (length < SCROLL_INDICATOR_MIN_LENGTH) {
        length = MIN(SCROLL_INDICATOR_MIN_LENGTH, track);
    }

    int position = SCROLL_INDICATOR_MARGIN + (track - length) * scroll / maxScroll;

    if (isVertical) {
        rect.x = x + w - SCROLL_INDICATOR_MARGIN - SCROLL_INDICATOR_THICKNESS;
        rect.y = y + position;
        rect.w = SCROLL_INDICATOR_THICKNESS;
        rect.h = length;
    } else {
        rect.x = x + position;
        rect.y = y + h - SCROLL_INDICATOR_MARGIN - SCROLL_INDICATOR_THICKNESS;
        rect.w = length;
        rect.h = SCROLL_INDICATOR_THICKNESS;
    }

    return rect;
}

void ScrollableWidgetState::beginEnumChildren(EnumChildrenContext &context) {
    WidgetCursor &widgetCursor = g_widgetCursor;

    const Style *style = getStyle(widgetCursor.widget->style);

    context.isFindScrollList =
        g_findCallback && touchScrollable && widgetCursor.appContext->isTouchScrollEnabled(widgetCursor) &&
        g_findEnclosingScrollLists.count < MAX_NESTED_SCROLL_LISTS;
    if (context.isFindScrollList) {
        auto &list = g_findEnclosingScrollLists.lists[g_findEnclosingScrollLists.count++];
        list = widgetCursor;
        // widgetCursor.currentState already points to the first child
        list.currentState = this;
    }

    auto savedX = context.savedX = widgetCursor.x;
    auto savedY = context.savedY = widgetCursor.y;
    auto savedW = context.savedW = widgetCursor.w;
    auto savedH = context.savedH = widgetCursor.h;

    context.savedCursor = widgetCursor.cursor;
    context.savedRefreshed = widgetCursor.refreshed;

    context.renderPixelOffset = 0;
    context.scrolling = false;
    context.scrollIndicator = Rect();
    context.drawScrollIndicator = false;
    context.savedDirty = false;

    if (!g_findCallback) {
        context.savedClipRect = display::intersectClipRect(savedX, savedY, savedW, savedH);

        if (pixelOffset != 0) {
            context.scrolling = true;
            context.renderPixelOffset = pixelOffset;
        }

        if (touchScrollable && widgetCursor.appContext->isTouchScrollEnabled(widgetCursor)) {
            context.scrollIndicator = getScrollIndicatorRect(isVertical, style, savedX, savedY, savedW, savedH,
                pitch, maxPosition, startPosition / itemsPerLine * pitch + context.renderPixelOffset);
        }
        bool scrollIndicatorMoved = context.scrollIndicator != renderedScrollIndicator;
        renderedScrollIndicator = context.scrollIndicator;

        if (context.scrolling || renderedPixelOffset != 0 || scrollIndicatorMoved) {
            // items (or scroll indicator) moved since the previous frame, redraw everything
            drawRectangle(savedX, savedY, savedW, savedH, style, false, false, true);
            widgetCursor.refreshed = true;
        }

        // Scroll indicator is drawn over the items, so it must be redrawn
        // whenever something under it could have been redrawn.
        context.drawScrollIndicator = context.scrollIndicator.w > 0 && (scrollIndicatorMoved || rendered || widgetCursor.refreshed);
        rendered = false;
        context.savedDirty = display::isRenderBufferDirty();
        display::clearRenderBufferDirty();

#if TOUCH_SCROLL_TRACE
        if (pixelOffset != 0 || renderedPixelOffset != 0) {
            printf("TS render t=%u start=%d count=%d pixelOffset=%d prevOffset=%d y=%d h=%d\n",
                (unsigned)millis(), startPosition, count, pixelOffset, renderedPixelOffset, savedY, savedH);
        }
#endif
        renderedPixelOffset = context.renderPixelOffset;
    } else {
        // forEachWidget walks the widget states of the last render,
        // so it must enumerate exactly the same items as the last render
        context.renderPixelOffset = renderedPixelOffset;
        context.scrolling = renderedPixelOffset != 0;

        // parts of the items outside of the list are not visible, so they can't be touched
        context.savedFindClipRect = g_findClipRect;
        int x1 = savedX;
        int y1 = savedY;
        int x2 = savedX + savedW;
        int y2 = savedY + savedH;
        if (g_findClipRect.w > 0) {
            // list inside another clipped list
            x1 = MAX(x1, g_findClipRect.x);
            y1 = MAX(y1, g_findClipRect.y);
            x2 = MIN(x2, g_findClipRect.x + g_findClipRect.w);
            y2 = MIN(y2, g_findClipRect.y + g_findClipRect.h);
        }
        if (x2 > x1 && y2 > y1) {
            g_findClipRect.x = x1;
            g_findClipRect.y = y1;
            g_findClipRect.w = x2 - x1;
            g_findClipRect.h = y2 - y1;
        } else {
            // nothing is visible (w == 0 would mean no clipping)
            g_findClipRect.x = -32768;
            g_findClipRect.y = -32768;
            g_findClipRect.w = 1;
            g_findClipRect.h = 1;
        }
    }
}

void ScrollableWidgetState::endEnumChildren(EnumChildrenContext &context) {
    WidgetCursor &widgetCursor = g_widgetCursor;

    widgetCursor.widget = widget;

    widgetCursor.x = context.savedX;
    widgetCursor.y = context.savedY;
    widgetCursor.w = context.savedW;
    widgetCursor.h = context.savedH;

    widgetCursor.cursor = context.savedCursor;
    widgetCursor.refreshed = context.savedRefreshed;

    const Style *style = getStyle(widget->style);

    if (!g_findCallback) {
        auto &scrollIndicator = context.scrollIndicator;
        if (scrollIndicator.w > 0 && (context.drawScrollIndicator || display::isRenderBufferDirty())) {
            display::setColor(style->color);
            display::fillRect(scrollIndicator.x, scrollIndicator.y, scrollIndicator.x + scrollIndicator.w - 1, scrollIndicator.y + scrollIndicator.h - 1);
        }
        if (context.savedDirty) {
            display::markRenderBufferDirty();
        }

        display::restoreClipRect(context.savedClipRect);
    } else {
        g_findClipRect = context.savedFindClipRect;

        if (isRepaintingOverlappingWidgets() && renderedScrollIndicator.w > 0) {
            // items are repainted, scroll indicator is drawn over them
            auto savedClipRect = display::intersectClipRect(context.savedX, context.savedY, context.savedW, context.savedH);
            display::setColor(style->color);
            display::fillRect(
                renderedScrollIndicator.x, renderedScrollIndicator.y,
                renderedScrollIndicator.x + renderedScrollIndicator.w - 1, renderedScrollIndicator.y + renderedScrollIndicator.h - 1
            );
            display::restoreClipRect(savedClipRect);
        }
    }

    if (context.isFindScrollList) {
        g_findEnclosingScrollLists.count--;
    }
}

void ScrollableWidgetState::render() {
    rendered = true;
}

////////////////////////////////////////////////////////////////////////////////
// Touch scrolling

// finger travel (px) along the list before the list takes over the touch
static const int TOUCH_SCROLL_DRAG_THRESHOLD = 12;

// After a flick the list travels velocity * TOUCH_SCROLL_FLING_DISTANCE_FACTOR (ms)
// before it stops (then it is moved to the nearest item).
static const float TOUCH_SCROLL_FLING_DISTANCE_FACTOR = 325.0f;

// After the finger is lifted the list moves to its rest position along a
// cubic Hermite curve, which starts with the finger velocity (so the list
// doesn't jump at release) and ends with zero velocity at the given duration
// (unlike an exponential curve, it doesn't creep pixel by pixel at the end).
// Durations are in ms.
static const float TOUCH_SCROLL_MIN_FLING_DURATION = 300.0f;
static const float TOUCH_SCROLL_MAX_FLING_DURATION = 1200.0f;
static const float TOUCH_SCROLL_MIN_SETTLE_DURATION = 200.0f;
static const float TOUCH_SCROLL_MAX_SETTLE_DURATION = 400.0f;

// px/ms
static const float TOUCH_SCROLL_MIN_FLING_VELOCITY = 0.25f;
static const float TOUCH_SCROLL_MAX_FLING_VELOCITY = 8.0f;

// fling velocity is measured from the finger movement within this time before lift off
static const uint32_t TOUCH_SCROLL_VELOCITY_WINDOW_MS = 100;

// resistance when the list is pulled beyond its start or end
static const float TOUCH_SCROLL_RUBBER_BAND_COEFFICIENT = 0.55f;

static const int TOUCH_SCROLL_MAX_SAMPLES = 16;

enum TouchScrollPhase {
    TOUCH_SCROLL_IDLE,
    TOUCH_SCROLL_PENDING,   // finger is down, but hasn't moved far enough yet
    TOUCH_SCROLL_DRAGGING,
    TOUCH_SCROLL_ANIMATING  // finger is lifted, list is moving to its rest position
};

struct TouchScrollSample {
    uint32_t time;
    int position;
};

// list under the finger, captured at touch down
struct TouchScrollList {
    WidgetCursor list;
    bool isVertical;
    int pitch;          // distance between two items in px
    bool isPager;       // item is as big as the list, flick moves by one item only
    int extent;         // list height (or width)
    bool localPosition; // position is kept in the widget state, not in the list data
    int itemsPerLine;   // 1 for the List, items in one row (or column) for the Grid
    int maxPosition;    // in lines
    int dataPosition;   // in lines
};

struct TouchScroll {
    TouchScrollPhase phase;

    // While pending, it is not known yet which of the nested lists will scroll,
    // it depends on the direction in which the finger moves.
    TouchScrollList candidates[MAX_NESTED_SCROLL_LISTS]; // from outermost to innermost
    int numCandidates;

    WidgetCursor list;
    bool isVertical;
    int pitch;          // distance between two items in px
    bool isPager;       // item is as big as the list, flick moves by one item only
    int pagerStartPosition; // item at touch start (in pager mode)
    int extent;         // list height (or width)
    bool localPosition; // position is kept in the widget state, not in the list data
    int itemsPerLine;   // 1 for the List, items in one row (or column) for the Grid
    int maxPosition;    // in lines
    float maxScroll;    // in px

    float scroll;       // in px, outside of [0, maxScroll] while rubber banding
    int dataPosition;   // last position (in lines) set to the list data

    int touchStart;     // along the list
    int touchStartX;
    int touchStartY;
    float rawScrollStart;

    TouchScrollSample samples[TOUCH_SCROLL_MAX_SAMPLES];
    int numSamples;

    uint32_t animationStartTime;
    float animationStart;
    float animationTarget;
    float animationDuration;
    float animationVelocity; // initial velocity in px per animation duration

    bool seen;          // list was rendered since the last tick
};

static TouchScroll g_touchScroll;

// Position (in items) set by touch scroll to the list with local position, it is
// picked up by ScrollableWidgetState::updateScrollState (also after touch scroll stops).
static WidgetCursor g_localPositionList;
static int g_localPosition;

static int getAxisPosition(const Event &touchEvent) {
    return g_touchScroll.isVertical ? touchEvent.y : touchEvent.x;
}


static float rubberBand(float overscroll) {
    // the further it is pulled the harder it gets, never beyond extent
    float d = (float)g_touchScroll.extent;
    float c = TOUCH_SCROLL_RUBBER_BAND_COEFFICIENT;
    return d * c * overscroll / (c * overscroll + d);
}

static float rubberBandInverse(float overscroll) {
    float d = (float)g_touchScroll.extent;
    float c = TOUCH_SCROLL_RUBBER_BAND_COEFFICIENT;
    if (overscroll > 0.99f * d) {
        overscroll = 0.99f * d;
    }
    return d * overscroll / (c * (d - overscroll));
}

// raw scroll follows the finger, scroll is what is displayed
static float scrollFromRawScroll(float rawScroll) {
    if (rawScroll < 0) {
        return -rubberBand(-rawScroll);
    }
    if (rawScroll > g_touchScroll.maxScroll) {
        return g_touchScroll.maxScroll + rubberBand(rawScroll - g_touchScroll.maxScroll);
    }
    return rawScroll;
}

static float rawScrollFromScroll(float scroll) {
    if (scroll < 0) {
        return -rubberBandInverse(-scroll);
    }
    if (scroll > g_touchScroll.maxScroll) {
        return g_touchScroll.maxScroll + rubberBandInverse(scroll - g_touchScroll.maxScroll);
    }
    return scroll;
}

static void setTouchScroll(float scroll) {
    auto &ts = g_touchScroll;

    ts.scroll = scroll;

    int position = (int)floorf(scroll / ts.pitch);
    if (position < 0) {
        position = 0;
    } else if (position > ts.maxPosition) {
        position = ts.maxPosition;
    }

    if (position != ts.dataPosition) {
        ts.dataPosition = position;
        if (ts.localPosition) {
            g_localPositionList = ts.list;
            g_localPosition = position * ts.itemsPerLine;
        } else {
            ytDataSetPosition(ts.list, ts.list.widget->data, position * ts.itemsPerLine);
        }
    }

#if TOUCH_SCROLL_TRACE
    printf("TS scroll t=%u phase=%d scroll=%.1f position=%d dataPosition=%d max=%d pitch=%d\n",
        (unsigned)millis(), (int)ts.phase, scroll, position,
        ts.dataPosition, ts.maxPosition, ts.pitch);
#endif
}

static void stopTouchScroll() {
#if TOUCH_SCROLL_TRACE
    printf("TS stop t=%u phase=%d scroll=%.1f\n", (unsigned)millis(), (int)g_touchScroll.phase, g_touchScroll.scroll);
#endif
    g_touchScroll.phase = TOUCH_SCROLL_IDLE;
    g_touchScroll.list = WidgetCursor();
}

static void finishTouchScrollAnimation() {
    setTouchScroll(g_touchScroll.animationTarget);
    stopTouchScroll();
}

static void resetTouchScrollSamples() {
    g_touchScroll.numSamples = 0;
}

static void addTouchScrollSample(const Event &touchEvent) {
    auto &ts = g_touchScroll;
    if (ts.numSamples > 0 && ts.samples[ts.numSamples - 1].position == getAxisPosition(touchEvent)) {
        // Touch move event with unchanged position is resent periodically while
        // the finger is held still, keep the time when the finger stopped.
        return;
    }
    if (ts.numSamples == TOUCH_SCROLL_MAX_SAMPLES) {
        for (int i = 1; i < TOUCH_SCROLL_MAX_SAMPLES; i++) {
            ts.samples[i - 1] = ts.samples[i];
        }
        ts.numSamples--;
    }
    ts.samples[ts.numSamples].time = touchEvent.time;
    ts.samples[ts.numSamples].position = getAxisPosition(touchEvent);
    ts.numSamples++;
}

// returns scroll velocity in px/ms
static float getTouchScrollVelocity(uint32_t liftOffTime) {
    auto &ts = g_touchScroll;

    if (ts.numSamples < 2) {
        return 0;
    }

    auto &last = ts.samples[ts.numSamples - 1];
    if (int32_t(liftOffTime - last.time) > (int32_t)TOUCH_SCROLL_VELOCITY_WINDOW_MS) {
        // finger was held still before lift off
        return 0;
    }

    int first = ts.numSamples - 1;
    while (first > 0 && int32_t(last.time - ts.samples[first - 1].time) <= (int32_t)TOUCH_SCROLL_VELOCITY_WINDOW_MS) {
        first--;
    }

    int32_t dt = int32_t(last.time - ts.samples[first].time);
    if (dt <= 0) {
        return 0;
    }

    // scroll moves in the opposite direction of the finger
    return -1.0f * (last.position - ts.samples[first].position) / dt;
}

static void startTouchScrollAnimation(float velocity) {
    auto &ts = g_touchScroll;

#if TOUCH_SCROLL_TRACE
    printf("TS release t=%u scroll=%.1f velocity=%.3f\n", (unsigned)millis(), ts.scroll, velocity);
#endif

    float destination = ts.scroll;

    bool isOverscrolled = ts.scroll < 0 || ts.scroll > ts.maxScroll;
    bool isFling = !isOverscrolled && fabsf(velocity) >= TOUCH_SCROLL_MIN_FLING_VELOCITY;
    if (isFling) {
        if (velocity > TOUCH_SCROLL_MAX_FLING_VELOCITY) {
            velocity = TOUCH_SCROLL_MAX_FLING_VELOCITY;
        } else if (velocity < -TOUCH_SCROLL_MAX_FLING_VELOCITY) {
            velocity = -TOUCH_SCROLL_MAX_FLING_VELOCITY;
        }
        destination += velocity * TOUCH_SCROLL_FLING_DISTANCE_FACTOR;
    }

    // rest position is aligned to an item
    float target;
    if (ts.isPager) {
        // move at most one item from the item at touch start
        float position;
        if (isFling) {
            // fling moves to the next item in the fling direction
            position = velocity > 0 ? floorf(ts.scroll / ts.pitch) + 1 : ceilf(ts.scroll / ts.pitch) - 1;
        } else {
            position = roundf(ts.scroll / ts.pitch);
        }
        if (position < ts.pagerStartPosition - 1) {
            position = ts.pagerStartPosition - 1.0f;
        } else if (position > ts.pagerStartPosition + 1) {
            position = ts.pagerStartPosition + 1.0f;
        }
        target = position * ts.pitch;
    } else {
        target = roundf(destination / ts.pitch) * ts.pitch;
    }
    if (target < 0) {
        target = 0;
    } else if (target > ts.maxScroll) {
        target = ts.maxScroll;
    }

    float distance = target - ts.scroll;

    float duration;
    if (isFling && distance * velocity > 0) {
        // with initial velocity 3 * distance / duration the curve is ease-out cubic,
        // choose duration so that it continues with the finger velocity
        duration = 3.0f * distance / velocity;
        if (duration < TOUCH_SCROLL_MIN_FLING_DURATION) {
            duration = TOUCH_SCROLL_MIN_FLING_DURATION;
        } else if (duration > TOUCH_SCROLL_MAX_FLING_DURATION) {
            duration = TOUCH_SCROLL_MAX_FLING_DURATION;
        }
    } else {
        duration = TOUCH_SCROLL_MIN_SETTLE_DURATION + fabsf(distance);
        if (duration > TOUCH_SCROLL_MAX_SETTLE_DURATION) {
            duration = TOUCH_SCROLL_MAX_SETTLE_DURATION;
        }
    }

#if TOUCH_SCROLL_TRACE
    printf("TS target=%.1f duration=%.0f\n", target, duration);
#endif

    ts.animationStartTime = millis();
    ts.animationStart = ts.scroll;
    ts.animationTarget = target;
    ts.animationDuration = duration;
    // Continue with the finger velocity, but not faster than ease-out cubic,
    // otherwise the list would overshoot the target. If the finger moved away
    // from the target (e.g. while rubber banding), start from zero velocity.
    float animationVelocity = velocity * duration;
    if (distance * animationVelocity <= 0) {
        animationVelocity = 0;
    } else if (fabsf(animationVelocity) > 3.0f * fabsf(distance)) {
        animationVelocity = 3.0f * distance;
    }
    ts.animationVelocity = animationVelocity;
    ts.phase = TOUCH_SCROLL_ANIMATING;

    if (fabsf(distance) < 0.5f) {
        finishTouchScrollAnimation();
    }
}

static int getTouchScrollPixelOffset(const WidgetCursor &widgetCursor, int startPosition) {
    auto &ts = g_touchScroll;

    if (ts.phase == TOUCH_SCROLL_IDLE || widgetCursor != ts.list) {
        return 0;
    }

    ts.seen = true;

    if (ts.phase == TOUCH_SCROLL_PENDING) {
        // not yet known if this is a tap or a drag
        return 0;
    }

    int offset = (int)roundf(ts.scroll) - startPosition * ts.pitch;

    if (offset < -ts.extent) {
        offset = -ts.extent;
    } else if (offset > ts.extent + ts.pitch) {
        offset = ts.extent + ts.pitch;
    }

    return offset;
}

static int getTouchScrollLocalPosition(const WidgetCursor &widgetCursor, int startPosition) {
    if (g_localPositionList && widgetCursor == g_localPositionList) {
        g_localPositionList = WidgetCursor();
        return g_localPosition;
    }
    return startPosition;
}

bool isTouchScrollableList(const WidgetCursor &widgetCursor) {
    auto type = widgetCursor.widget->type;
    if ((type != WIDGET_TYPE_LIST && type != WIDGET_TYPE_GRID) || widgetCursor.currentState->type != type) {
        return false;
    }
    auto state = (ScrollableWidgetState *)widgetCursor.currentState;
    return state->touchScrollable && widgetCursor.appContext->isTouchScrollEnabled(widgetCursor);
}

static bool initTouchScrollList(const WidgetCursor &listWidgetCursor, TouchScrollList &c) {
    // listWidgetCursor comes from findWidget, its currentState is the list state
    auto state = (ScrollableWidgetState *)listWidgetCursor.currentState;

    c.list = listWidgetCursor;
    c.isVertical = state->isVertical;
    c.extent = c.isVertical ? listWidgetCursor.h : listWidgetCursor.w;
    c.pitch = state->pitch;
    c.isPager = state->lineExtent == c.extent;

    if (c.pitch <= 0 || c.extent <= 0) {
        return false;
    }

    c.localPosition = state->localPosition;
    c.itemsPerLine = state->itemsPerLine;
    c.maxPosition = state->maxPosition;
    c.dataPosition = state->startPosition / state->itemsPerLine;

    return true;
}

static void selectTouchScrollList(const TouchScrollList &c) {
    auto &ts = g_touchScroll;

    ts.list = c.list;
    ts.isVertical = c.isVertical;
    ts.pitch = c.pitch;
    ts.isPager = c.isPager;
    ts.extent = c.extent;
    ts.localPosition = c.localPosition;
    ts.itemsPerLine = c.itemsPerLine;
    ts.maxPosition = c.maxPosition;
    ts.maxScroll = 1.0f * ts.maxPosition * ts.pitch;
    ts.dataPosition = c.dataPosition;
    ts.scroll = 1.0f * MIN(ts.dataPosition, ts.maxPosition) * ts.pitch;
}

bool touchScrollBegin(const ScrollLists &lists, const Event &touchEvent) {
    auto &ts = g_touchScroll;

    bool caught = false;

    if (ts.phase == TOUCH_SCROLL_ANIMATING) {
        for (int i = 0; i < lists.count; i++) {
            if (lists.lists[i] == ts.list) {
                // finger stops the moving list and continues dragging it from there
                caught = true;
                break;
            }
        }
        if (!caught) {
            finishTouchScrollAnimation();
        }
    } else if (ts.phase != TOUCH_SCROLL_IDLE) {
        stopTouchScroll();
    }

    ts.touchStartX = touchEvent.x;
    ts.touchStartY = touchEvent.y;

    resetTouchScrollSamples();

    if (caught) {
        // if the list was caught while moving, count from the item it was moving to,
        // so that each flick moves by one more item
        ts.pagerStartPosition = (int)roundf(ts.animationTarget / ts.pitch);

        ts.touchStart = getAxisPosition(touchEvent);
        ts.rawScrollStart = rawScrollFromScroll(ts.scroll);
        addTouchScrollSample(touchEvent);

        ts.phase = TOUCH_SCROLL_DRAGGING;
        ts.seen = true;
        return true;
    }

    ts.numCandidates = 0;
    for (int i = 0; i < lists.count; i++) {
        if (initTouchScrollList(lists.lists[i], ts.candidates[ts.numCandidates])) {
            ts.numCandidates++;
        }
    }

    if (ts.numCandidates == 0) {
        stopTouchScroll();
        return false;
    }

    // until it is known which list will scroll, watch the innermost one
    ts.list = ts.candidates[ts.numCandidates - 1].list;

    ts.phase = TOUCH_SCROLL_PENDING;
    ts.seen = true;

    return false;
}

bool touchScrollShouldCapture(const Event &touchEvent) {
    auto &ts = g_touchScroll;

    if (ts.phase != TOUCH_SCROLL_PENDING) {
        return false;
    }

    int dx = abs(touchEvent.x - ts.touchStartX);
    int dy = abs(touchEvent.y - ts.touchStartY);

    if (dx < TOUCH_SCROLL_DRAG_THRESHOLD && dy < TOUCH_SCROLL_DRAG_THRESHOLD) {
        return false;
    }

    // innermost list that scrolls in the direction of the finger movement
    bool isVertical = dy > dx;
    int i;
    for (i = ts.numCandidates - 1; i >= 0; i--) {
        if (ts.candidates[i].isVertical == isVertical) {
            break;
        }
    }

    if (i < 0) {
        // finger moves across the list(s), leave it to the pressed widget
        stopTouchScroll();
        return false;
    }

    selectTouchScrollList(ts.candidates[i]);

    ts.phase = TOUCH_SCROLL_DRAGGING;

    ts.pagerStartPosition = (int)roundf(ts.scroll / ts.pitch);

    // start dragging from here, so that the list doesn't jump by the threshold
    ts.touchStart = getAxisPosition(touchEvent);
    ts.rawScrollStart = rawScrollFromScroll(ts.scroll);

    resetTouchScrollSamples();
    addTouchScrollSample(touchEvent);

    return true;
}

void touchScrollOnTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    auto &ts = g_touchScroll;

    if (ts.phase != TOUCH_SCROLL_DRAGGING) {
        return;
    }

    if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        addTouchScrollSample(touchEvent);
        float rawScroll = ts.rawScrollStart + (ts.touchStart - getAxisPosition(touchEvent));
        setTouchScroll(scrollFromRawScroll(rawScroll));
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        startTouchScrollAnimation(getTouchScrollVelocity(touchEvent.time));
    }
}

void touchScrollEnd() {
    if (g_touchScroll.phase == TOUCH_SCROLL_PENDING) {
        stopTouchScroll();
    }
}

bool isTouchScrollPending() {
    return g_touchScroll.phase == TOUCH_SCROLL_PENDING;
}

bool isTouchScrollDragging() {
    return g_touchScroll.phase == TOUCH_SCROLL_DRAGGING;
}

const WidgetCursor &getTouchScrollList() {
    return g_touchScroll.list;
}

void touchScrollTick() {
    auto &ts = g_touchScroll;

    if (ts.phase == TOUCH_SCROLL_IDLE) {
        return;
    }

    if (!ts.seen) {
        // list is not displayed anymore
        stopTouchScroll();
        return;
    }
    ts.seen = false;

    if (ts.phase == TOUCH_SCROLL_ANIMATING) {
        float t = (float)(uint32_t)(millis() - ts.animationStartTime) / ts.animationDuration;
        if (t >= 1.0f) {
            finishTouchScrollAnimation();
        } else {
            // cubic Hermite with start velocity animationVelocity and end velocity 0
            float t2 = t * t;
            float t3 = t2 * t;
            float h10 = t3 - 2.0f * t2 + t;
            float h01 = 3.0f * t2 - 2.0f * t3;
            setTouchScroll(ts.animationStart + ts.animationVelocity * h10 + (ts.animationTarget - ts.animationStart) * h01);
        }
    }
}


} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
