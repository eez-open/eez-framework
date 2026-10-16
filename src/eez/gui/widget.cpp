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

#include <assert.h>
#include <cstddef>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <eez/core/debug.h>
#include <eez/core/os.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/core/assets.h>

#include <eez/gui/draw.h>

#include <eez/gui/widgets/containers/app_view.h>
#include <eez/gui/widgets/containers/container.h>
#include <eez/gui/widgets/containers/grid.h>
#include <eez/gui/widgets/containers/user_widget.h>
#include <eez/gui/widgets/containers/list.h>
#include <eez/gui/widgets/containers/select.h>

#include <eez/gui/widgets/bar_graph.h>
#include <eez/gui/widgets/bitmap.h>
#include <eez/gui/widgets/button_group.h>
#include <eez/gui/widgets/button.h>
#include <eez/gui/widgets/canvas.h>
#include <eez/gui/widgets/display_data.h>
#include <eez/gui/widgets/drop_down_list.h>
#include <eez/gui/widgets/gauge.h>
#include <eez/gui/widgets/input.h>
#include <eez/gui/widgets/list_graph.h>
#include <eez/gui/widgets/multiline_text.h>
#include <eez/gui/widgets/progress.h>
#include <eez/gui/widgets/rectangle.h>
#include <eez/gui/widgets/roller.h>
#include <eez/gui/widgets/scroll_bar.h>
#include <eez/gui/widgets/slider.h>
#include <eez/gui/widgets/switch.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/toggle_button.h>
#include <eez/gui/widgets/up_down.h>
#include <eez/gui/widgets/yt_graph.h>
#include <eez/gui/widgets/line_chart.h>
#include <eez/gui/widgets/qr_code.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

bool g_isActiveWidget;
EnumWidgetsCallback g_findCallback;
bool g_foundWidgetAtDownInvalid;

bool g_isRTL = false;

// set while overlapping widgets are repainted (see doStaticLayoutWithOverlapping)
static bool g_repaintingOverlappingWidgets;

bool isRepaintingOverlappingWidgets() {
    return g_repaintingOverlappingWidgets;
}

static bool isRenderedIntoOwnBuffer(WidgetState *widgetState) {
    if (widgetState->type == WIDGET_TYPE_APP_VIEW) {
        // each page is rendered into its own buffer
        return true;
    }
    if (widgetState->type == WIDGET_TYPE_CONTAINER) {
        // overlay
        return ((ContainerWidgetState *)widgetState)->overlay != nullptr;
    }
    return false;
}

// Some widgets (Text widget with "Fit to content") calculate their size in
// updateState. Applies that size to g_widgetCursor when the widget is visited
// without calling updateState (find, repaint of overlapping widgets).
static void applyContentSize(WidgetState *widgetState) {
    if (g_widgetCursor.widget->type == WIDGET_TYPE_TEXT && widgetState->type == WIDGET_TYPE_TEXT) {
        ((TextWidgetState *)widgetState)->applyFitToContent();
    }
}

////////////////////////////////////////////////////////////////////////////////

struct NoneWidgetState : public WidgetState {
    bool updateState() {
        WIDGET_STATE_START(Widget);
        WIDGET_STATE_END()
    }
};

static Widget g_noneWidget = { WIDGET_TYPE_NONE };

struct ReservedWidgetState : public WidgetState {};

// widget placementNew functions
#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) \
void NAME##_placementNew(void *ptr) { new (ptr) NAME_PASCAL_CASE##WidgetState(); }
WIDGET_TYPES
#undef WIDGET_TYPE

typedef void (*WidgetStatePlacementNewFunctionType)(void *ptr);

#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) NAME##_placementNew,
static WidgetStatePlacementNewFunctionType g_widgetStatePlacementNewFunctions[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

// widget state sizes
#define WIDGET_TYPE(NAME_PASCAL_CASE, NAME, ID) sizeof(NAME_PASCAL_CASE##WidgetState),
static size_t g_widgetStateSizes[] = {
    WIDGET_TYPES
};
#undef WIDGET_TYPE

////////////////////////////////////////////////////////////////////////////////

bool WidgetState::updateState() {
	return false;
}

void WidgetState::render() {
}

void WidgetState::enumChildren() {
}

bool WidgetState::hasOnTouch() {
    return false;
}

void WidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
}

bool WidgetState::hasOnKeyboard() {
    return false;
}

bool WidgetState::onKeyboard(const WidgetCursor &widgetCursor, uint8_t key, uint8_t mod) {
    return false;
}

////////////////////////////////////////////////////////////////////////////////

bool WidgetCursor::isPage() const {
    return widget->type == WIDGET_TYPE_CONTAINER && ((((ContainerWidget *)widget)->flags & PAGE_CONTAINER) != 0);
}

void WidgetCursor::pushBackground(int x, int y, const Style *style, bool active) {
    if (backgroundStyleStackPointer == BACKGROUND_STYLE_STACK_SIZE) {
        // make space: remove item at the bottom of stack
        for (size_t i = 1; i < BACKGROUND_STYLE_STACK_SIZE; i++) {
            backgroundStyleStack[i - 1] = backgroundStyleStack[i];
        }
        backgroundStyleStackPointer--;
    }

    backgroundStyleStack[backgroundStyleStackPointer].x = x;
    backgroundStyleStack[backgroundStyleStackPointer].y = y;
    backgroundStyleStack[backgroundStyleStackPointer].style = style;
    backgroundStyleStack[backgroundStyleStackPointer].active = active;
    backgroundStyleStackPointer++;
}

void WidgetCursor::popBackground() {
	if (backgroundStyleStackPointer > 0) {
		backgroundStyleStackPointer--;
	}
}

////////////////////////////////////////////////////////////////////////////////

#define RENDER_WIDGET() \
    if ((!widget->visible || widgetState->isVisible.toBool()) && widgetCursor.opacity > 0) { \
        auto savedOpacity = display::setOpacity(widgetCursor.opacity); \
        widgetState->render(); \
        display::setOpacity(savedOpacity); \
    } else { \
        int x1 = g_widgetCursor.x; \
        int y1 = g_widgetCursor.y; \
        int x2 = g_widgetCursor.x + g_widgetCursor.w - 1; \
        int y2 = g_widgetCursor.y + g_widgetCursor.h - 1; \
        drawBorderAndBackground(x1, y1, x2, y2, nullptr, TRANSPARENT_COLOR_INDEX); \
    } \

////////////////////////////////////////////////////////////////////////////////
// Widget states of the previous frame
//
// New frame states are written in place over the previous frame states as long
// as the widget structure is the same (states don't move, so the pointers to
// them stay valid). On the first difference, the rest of the previous frame
// states (the tail) is copied to the tail buffer and the states are read from
// there. Widget whose previous state is not found gets a new state (and so do
// all its descendants), but the states of the widgets after it are still
// reused: the read position is realigned at the end of every reused widget.

alignas(8) EXTRAM_DATA static uint8_t g_previousStatesTailBuffer[GUI_STATE_BUFFER_SIZE];

// read position and end of the previous frame states, as offsets from g_widgetStateStart
static uint32_t g_previousStateOffset;
static uint32_t g_previousStatesEnd;

// previous frame states from this offset are in the tail buffer
static uint32_t g_previousStatesTailOffset;
static bool g_previousStatesTailCopied;

// false while enumerating the descendants of a widget with a new state
static bool g_readPreviousStates;

static bool g_widgetStatesStructureChanged;

static WidgetState *getPreviousState(uint32_t offset) {
    if (offset >= g_previousStatesEnd) {
        return nullptr;
    }
    if (g_previousStatesTailCopied) {
        if (offset < g_previousStatesTailOffset) {
            // already overwritten by the new frame states
            return nullptr;
        }
        return (WidgetState *)(g_previousStatesTailBuffer + offset - g_previousStatesTailOffset);
    }
    // in place, offset is the same as the write position
    return (WidgetState *)((uint8_t *)g_widgetStateStart + offset);
}

// must be called before the new frame states diverge from the previous frame states
static void copyPreviousStatesTail() {
    if (g_previousStatesTailCopied) {
        return;
    }

    g_previousStatesTailCopied = true;
    g_previousStatesTailOffset = g_previousStateOffset;
    g_widgetStatesStructureChanged = true;

    if (g_previousStatesTailOffset < g_previousStatesEnd) {
        auto tailStart = (uint8_t *)g_widgetStateStart + g_previousStatesTailOffset;
        memcpy(g_previousStatesTailBuffer, tailStart, g_previousStatesEnd - g_previousStatesTailOffset);

        // g_foundWidgetAtDown state will be moved or destroyed
        auto &foundWidgetAtDown = getFoundWidgetAtDown();
        if ((uint8_t *)foundWidgetAtDown.currentState >= tailStart && (uint8_t *)foundWidgetAtDown.currentState < (uint8_t *)g_widgetStateStart + g_previousStatesEnd) {
            g_foundWidgetAtDownInvalid = true;
        }
    }
}

void beginWidgetStatesUpdate(bool hasPreviousState) {
    g_previousStateOffset = 0;
    g_previousStatesEnd = hasPreviousState ? (uint8_t *)g_widgetStateEnd - (uint8_t *)g_widgetStateStart : 0;
    g_previousStatesTailCopied = false;
    g_readPreviousStates = hasPreviousState;
    g_widgetStatesStructureChanged = !hasPreviousState;
}

static void destroyStates(uint8_t *start, uint8_t *end) {
    while (start < end) {
        auto widgetState = (WidgetState *)start;
        start += g_widgetStateSizes[widgetState->type];
        if (!widgetState->moved) {
            widgetState->~WidgetState();
        }
    }
}

// returns true if widget states structure is changed in this frame
bool endWidgetStatesUpdate() {
    // destroy the previous frame states that are not reused
    if (g_previousStatesTailCopied) {
        if (g_previousStatesTailOffset < g_previousStatesEnd) {
            destroyStates(g_previousStatesTailBuffer, g_previousStatesTailBuffer + g_previousStatesEnd - g_previousStatesTailOffset);
        }
    } else {
        uint32_t newStatesEnd = (uint8_t *)g_widgetStateEnd - (uint8_t *)g_widgetStateStart;
        if (newStatesEnd < g_previousStatesEnd) {
            destroyStates((uint8_t *)g_widgetStateEnd, (uint8_t *)g_widgetStateStart + g_previousStatesEnd);
            g_widgetStatesStructureChanged = true;
        }
    }

    g_previousStatesEnd = 0;
    g_readPreviousStates = false;

    return g_widgetStatesStructureChanged;
}

void enumWidget() {
    WidgetCursor &widgetCursor = g_widgetCursor;
    const Widget *widget = widgetCursor.widget;

	auto widgetState = widgetCursor.currentState;

    // The state of this widget must fit in the state buffer, check it before
    // it is created.
    if ((uint8_t *)widgetState + g_widgetStateSizes[widget->type] > (uint8_t *)g_widgetStateStart + GUI_STATE_BUFFER_SIZE) {
        static bool g_stateBufferOverflowReported = false;
        if (!g_stateBufferOverflowReported) {
            g_stateBufferOverflowReported = true;
            printf("GUI state buffer overflow (size %u)\n", (unsigned)GUI_STATE_BUFFER_SIZE);
        }
        return;
    }

	bool savedIsActiveWidget = g_isActiveWidget;
	g_isActiveWidget = g_isActiveWidget || widgetCursor == g_activeWidget;

    if (g_findCallback) {
        if (g_repaintingOverlappingWidgets && isRenderedIntoOwnBuffer(widgetState)) {
            // not affected by the repaint of the current buffer, skip the whole subtree
            widgetCursor.currentState = (WidgetState *)((uint8_t *)widgetState + widgetState->subtreeSize);
            g_isActiveWidget = savedIsActiveWidget;
            return;
        }

        if (!widget->visible || widgetState->isVisible.toBool()) {
            applyContentSize(widgetState);
    		g_findCallback();
        }

        widgetCursor.currentState = (WidgetState *)((uint8_t *)widgetCursor.currentState + g_widgetStateSizes[widget->type]);

        widgetState->enumChildren();

        g_isActiveWidget = savedIsActiveWidget;
        return;
    }

    bool savedHasPreviousState = widgetCursor.hasPreviousState;
    bool savedReadPreviousStates = g_readPreviousStates;

    uint32_t previousStateOffset = g_previousStateOffset;
    WidgetState *previousState = g_readPreviousStates ? getPreviousState(previousStateOffset) : nullptr;
    uint32_t previousSubtreeSize = previousState ? previousState->subtreeSize : 0;

    bool reuse =
        widgetCursor.hasPreviousState &&
        previousState &&
        !previousState->moved &&
        previousState->type == widget->type &&
        previousState->widget == widget;

    if (reuse) {
        if (previousState != widgetState) {
            // move from the tail buffer
            memcpy((void *)widgetState, (void *)previousState, g_widgetStateSizes[widget->type]);
            previousState->moved = true;
        }

        // previous states of the children follow
        g_previousStateOffset += g_widgetStateSizes[widget->type];

        bool refresh = widgetState->updateState();
        if (refresh || widgetCursor.refreshed) {
            RENDER_WIDGET();
        }
    } else {
        if (g_readPreviousStates) {
            // new state is written over the previous frame states
            copyPreviousStatesTail();

            if (previousState) {
                // previous state (with its descendants) is replaced by this widget
                g_previousStateOffset = previousStateOffset + previousSubtreeSize;
            }

            // descendants don't have previous states
            g_readPreviousStates = false;
        }

        widgetCursor.hasPreviousState = false;

        // create new widget state
        g_widgetStatePlacementNewFunctions[widget->type](widgetState);
        widgetState->type = widget->type;
        widgetState->moved = false;
        widgetState->widget = widget;

        widgetState->updateState();

        RENDER_WIDGET();
    }

    if (g_foundWidgetAtDownInvalid) {
        // find new cursor for g_foundWidgetAtDown
        auto &foundWidgetAtDown = getFoundWidgetAtDown();
        if (foundWidgetAtDown == widgetCursor) {
            foundWidgetAtDown = widgetCursor;
            g_foundWidgetAtDownInvalid = false;
        }
    }

	widgetCursor.currentState = (WidgetState *)((uint8_t *)widgetCursor.currentState + g_widgetStateSizes[widget->type]);

	uint32_t stateSize = (uint8_t *)widgetCursor.currentState - (uint8_t *)g_widgetStateStart;
    if (stateSize >= g_maxGuiStateSize) {
        g_maxGuiStateSize = stateSize;
    }

	widgetState->enumChildren();

    widgetState->subtreeSize = (uint8_t *)widgetCursor.currentState - (uint8_t *)widgetState;

    if (reuse) {
        // continue with the previous state that follows this widget subtree
        // (children structure could be changed)
        uint32_t previousSubtreeEnd = previousStateOffset + previousSubtreeSize;
        if (g_previousStateOffset != previousSubtreeEnd) {
            copyPreviousStatesTail();
            g_previousStateOffset = previousSubtreeEnd;
        }
    }

    widgetCursor.hasPreviousState = savedHasPreviousState;
    g_readPreviousStates = savedReadPreviousStates;

	g_isActiveWidget = savedIsActiveWidget;
}

void enumNoneWidget() {
    WidgetCursor &widgetCursor = g_widgetCursor;
    auto savedWidget = widgetCursor.widget;
    widgetCursor.widget = &g_noneWidget;
    auto savedX = g_widgetCursor.x;
    auto savedY = g_widgetCursor.y;
    g_widgetCursor.x += g_noneWidget.x;
    g_widgetCursor.y += g_noneWidget.y;
    g_widgetCursor.w = g_noneWidget.width;
    g_widgetCursor.h = g_noneWidget.height;
	enumWidget();
    g_widgetCursor.x = savedX;
    g_widgetCursor.y = savedY;
    widgetCursor.widget = savedWidget;
}

////////////////////////////////////////////////////////////////////////////////

void freeWidgetStates(WidgetState *widgetStateStart) {
    WidgetState *widgetState = widgetStateStart;
    while (widgetState < g_widgetStateEnd) {
        auto nextWidgetState = (WidgetState *)((uint8_t*)widgetState + g_widgetStateSizes[widgetState->type]);
        widgetState->~WidgetState();
        widgetState = nextWidgetState;
    }

    // invalidate g_foundWidgetAtDown if it was among freed widgets
	auto &widgetCursor = getFoundWidgetAtDown();
	if (widgetCursor.currentState >= widgetStateStart) {
		g_foundWidgetAtDownInvalid = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

void forEachWidget(EnumWidgetsCallback callback) {
	g_findCallback = callback;
    enumRootWidget();
	g_findCallback = nullptr;
}

////////////////////////////////////////////////////////////////////////////////

static int g_findWidgetAtX;
static int g_findWidgetAtY;
static bool g_clicked;

static bool g_found;
static WidgetCursor g_foundWidget;
static int g_distanceToFoundWidget;
static ScrollLists g_foundWidgetScrollLists;
Rect g_findClipRect;

int g_xOverlayOffset = 0;
int g_yOverlayOffset = 0;

static AppContext *g_popPageAppContext;

static void findWidgetStep() {
	if (g_found) {
		return;
	}

    WidgetCursor &widgetCursor = g_widgetCursor;

	if (widgetCursor.appContext->isActivePageInternal()) {
		auto internalPage = (InternalPage *)widgetCursor.appContext->getActivePage();

		WidgetCursor foundWidget = internalPage->findWidgetInternalPage(g_findWidgetAtX, g_findWidgetAtY, g_clicked);
		if (foundWidget) {
			g_foundWidget = foundWidget;
			g_foundWidgetScrollLists.count = 0;
			g_found = true;
			return;
        }

        if (g_clicked) {
            if (internalPage->closeIfTouchedOutside()) {
                // clicked outside internal page, close internal page (if not toast)
                g_popPageAppContext = widgetCursor.appContext;
            }
        }

        bool passThrough = internalPage->canClickPassThrough();
        if (!passThrough) {
			g_foundWidget = 0;
			g_foundWidgetScrollLists.count = 0;
			g_found = true;
			return;
		}
	}

    if (widgetCursor.isPage()) {
		if (g_foundWidget && g_foundWidget.appContext == widgetCursor.appContext) {
			g_foundWidget = widgetCursor;
			g_distanceToFoundWidget = INT_MAX;
			g_foundWidgetScrollLists.count = 0;
		}
    }

    const Widget *widget = widgetCursor.widget;

    Overlay *overlay = getOverlay(widgetCursor);
	if (overlay) {
		getOverlayOffset(widgetCursor, g_xOverlayOffset, g_yOverlayOffset);
	}

	int x = widgetCursor.x + g_xOverlayOffset;
	int y = widgetCursor.y + g_yOverlayOffset;

	if (overlay) {
		g_xOverlayOffset = 0;
		g_yOverlayOffset = 0;
	}

#ifndef MIN_TOUCH_DETECTION_SIZE
#define MIN_TOUCH_DETECTION_SIZE 50
#endif

    int w = overlay ? overlay->width : widgetCursor.w;
    if (w < MIN_TOUCH_DETECTION_SIZE) {
        x = x - (MIN_TOUCH_DETECTION_SIZE - w) / 2;
        w = MIN_TOUCH_DETECTION_SIZE;
    }

    int h = overlay ? overlay->height : widgetCursor.h;
    if (h < MIN_TOUCH_DETECTION_SIZE) {
        y = y - (MIN_TOUCH_DETECTION_SIZE - h) / 2;
        h = MIN_TOUCH_DETECTION_SIZE;
    }

    bool inside =
        g_findWidgetAtX >= x && g_findWidgetAtX < x + w &&
        g_findWidgetAtY >= y && g_findWidgetAtY < y + h;

    if (inside && g_findClipRect.w > 0) {
        inside =
            g_findWidgetAtX >= g_findClipRect.x && g_findWidgetAtX < g_findClipRect.x + g_findClipRect.w &&
            g_findWidgetAtY >= g_findClipRect.y && g_findWidgetAtY < g_findClipRect.y + g_findClipRect.h;
    }

    // scrollable list is found only if touch is not on some other widget inside the list
    bool isScrollList = g_clicked && isTouchScrollableList(widgetCursor);

    if (inside && (widget->type == WIDGET_TYPE_APP_VIEW || getWidgetTouchFunction(widgetCursor) || isScrollList)) {
        int dx = g_findWidgetAtX - (x + w / 2);
        int dy = g_findWidgetAtY - (y + h / 2);
        int distance = dx * dx + dy * dy;

        auto action = getWidgetAction(widgetCursor);
        if (action == EEZ_ACTION_ID_DRAG_OVERLAY) {
            if (overlay && !overlay->state) {
                return;
            }
            g_foundWidget = widgetCursor;
            g_distanceToFoundWidget = INT_MAX;
            g_foundWidgetScrollLists.count = 0;
        } else {
            if (
				!g_foundWidget ||
				distance <= g_distanceToFoundWidget ||
				g_foundWidget.widget->type == WIDGET_TYPE_APP_VIEW ||
				g_foundWidget.widget->type == WIDGET_TYPE_LIST ||
				g_foundWidget.widget->type == WIDGET_TYPE_GRID
			) {
                g_foundWidget = widgetCursor;
                g_distanceToFoundWidget = distance;
                g_foundWidgetScrollLists = g_findEnclosingScrollLists;
                if (isScrollList && g_foundWidgetScrollLists.count < MAX_NESTED_SCROLL_LISTS) {
                    g_foundWidgetScrollLists.lists[g_foundWidgetScrollLists.count++] = widgetCursor;
                }

                // if found widget is AppView, make sure we set right AppContext
                if (widget->type == WIDGET_TYPE_APP_VIEW) {
                    if (widget->data != EEZ_DATA_ID_NONE) {
                        Value appContextValue = get(widgetCursor, widget->data);
                        g_foundWidget.appContext = (AppContext *)appContextValue.getVoidPointer();
                    }
                }
            }
        }
    }
}

WidgetCursor findWidget(int16_t x, int16_t y, bool clicked) {
	g_found = false;
    g_foundWidget = 0;
    g_foundWidgetScrollLists.count = 0;
    g_findEnclosingScrollLists.count = 0;
    g_findClipRect = Rect();
    g_clicked = clicked;

    g_findWidgetAtX = x;
    g_findWidgetAtY = y;

    g_xOverlayOffset = 0;
    g_yOverlayOffset = 0;

    g_popPageAppContext = nullptr;

    forEachWidget(findWidgetStep);

    if (g_popPageAppContext) {
        g_popPageAppContext->popPage();
        g_popPageAppContext = nullptr;
    }

    return g_foundWidget;
}

const ScrollLists &getFoundWidgetScrollLists() {
    return g_foundWidgetScrollLists;
}

////////////////////////////////////////////////////////////////////////////////

void resizeWidget(
    const Widget *widget,
    Rect &widgetRect,
    int containerOriginalWidth,
    int containerOriginalHeight,
    int containerWidth,
    int containerHeight
) {
    auto flags = widget->flags;

    auto pinToLeft = flags & WIDGET_FLAG_PIN_TO_LEFT;
    auto pinToRight = flags & WIDGET_FLAG_PIN_TO_RIGHT;
    auto pinToTop = flags & WIDGET_FLAG_PIN_TO_TOP;
    auto pinToBottom = flags & WIDGET_FLAG_PIN_TO_BOTTOM;

    auto fixWidth = flags & WIDGET_FLAG_FIX_WIDTH;
    auto fixHeight = flags & WIDGET_FLAG_FIX_HEIGHT;

    auto left = widgetRect.x;
    auto right = widgetRect.x + widgetRect.w;

    if (pinToLeft) {
        // left = left;
    } else {
        if (!fixWidth) {
            left =
                (widgetRect.x * containerWidth) /
                containerOriginalWidth;
        }
    }

    if (pinToRight) {
        right = containerWidth - (containerOriginalWidth - right);
    } else {
        if (!fixWidth) {
            right = (right * containerWidth) / containerOriginalWidth;
        }
    }

    if (fixWidth) {
        if (pinToLeft && !pinToRight) {
            right = left + widgetRect.w;
        } else if (pinToRight && !pinToLeft) {
            left = right - widgetRect.w;
        } else if (!pinToLeft && !pinToRight) {
            auto center =
                ((widgetRect.x + widgetRect.w / 2) *
                    containerWidth) /
                containerOriginalWidth;
            left = center - widgetRect.w / 2;
            right = left + widgetRect.w;
        }
    }

    auto top = widgetRect.y;
    auto bottom = widgetRect.y + widgetRect.h;

    if (pinToTop) {
        //top = top;
    } else {
        if (!fixHeight) {
            top =
                (widgetRect.y * containerHeight) /
                containerOriginalHeight;
        }
    }

    if (pinToBottom) {
        bottom = containerHeight - (containerOriginalHeight - bottom);
    } else {
        if (!fixHeight) {
            bottom =
                (bottom * containerHeight) / containerOriginalHeight;
        }
    }

    if (fixHeight) {
        if (pinToTop && !pinToBottom) {
            bottom = top + widgetRect.h;
        } else if (pinToBottom && !pinToTop) {
            top = bottom - widgetRect.h;
        } else if (!pinToTop && !pinToBottom) {
            auto center =
                ((widgetRect.y + widgetRect.h / 2) *
                    containerHeight) /
                containerOriginalHeight;
            top = center - widgetRect.h / 2;
            bottom = top + widgetRect.h;
        }
    }


    widgetRect.x = left;
    widgetRect.y = top;
    widgetRect.w = right - left;
    widgetRect.h = bottom - top;
}

void applyTimeline(WidgetCursor& widgetCursor, Rect &widgetRect) {
    if (widgetCursor.widget->timeline.count > 0) {
        auto x = widgetCursor.widget->x;
        auto y = widgetCursor.widget->y;
        auto w = widgetCursor.widget->width;
        auto h = widgetCursor.widget->height;
        float opacity = 1.0f;

        auto timelinePosition = widgetCursor.flowState->timelinePosition;
        for (uint32_t i = 0; i < widgetCursor.widget->timeline.count; i++) {
            auto keyframe = widgetCursor.widget->timeline[i];

            if (timelinePosition < keyframe->start) {
                continue;
            }

            if (
                timelinePosition >= keyframe->start &&
                timelinePosition <= keyframe->end
            ) {
                auto t =
                    keyframe->start == keyframe->end
                        ? 1
                        : (timelinePosition - keyframe->start) /
                        (keyframe->end - keyframe->start);

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_X) {
                    auto t2 = g_easingFuncs[keyframe->xEasingFunc](t);

                    if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_CP2) {
                        auto p1 = x;
                        auto p2 = keyframe->cp1x;
                        auto p3 = keyframe->cp2x;
                        auto p4 = keyframe->x;
                        x =
                            (1 - t2) * (1 - t2) * (1 - t2) * p1 +
                            3 * (1 - t2) * (1 - t2) * t2 * p2 +
                            3 * (1 - t2) * t2 * t2 * p3 +
                            t2 * t2 * t2 * p4;
                    } else if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_CP1) {
                        auto p1 = x;
                        auto p2 = keyframe->cp1x;
                        auto p3 = keyframe->x;
                        x =
                            (1 - t2) * (1 - t2) * p1 +
                            2 * (1 - t2) * t2 * p2 +
                            t2 * t2 * p3;
                    } else {
                        auto p1 = x;
                        auto p2 = keyframe->x;
                        x = (1 - t2) * p1 + t2 * p2;
                    }
                }

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_WIDTH) {
                    w += g_easingFuncs[keyframe->widthEasingFunc](t) * (keyframe->width - w);
                }

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_Y) {
                    auto t2 = g_easingFuncs[keyframe->yEasingFunc](t);

                    if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_CP2) {
                        auto p1 = y;
                        auto p2 = keyframe->cp1y;
                        auto p3 = keyframe->cp2y;
                        auto p4 = keyframe->y;
                        y =
                            (1 - t2) * (1 - t2) * (1 - t2) * p1 +
                            3 * (1 - t2) * (1 - t2) * t2 * p2 +
                            3 * (1 - t2) * t2 * t2 * p3 +
                            t2 * t2 * t2 * p4;
                    } else if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_CP1) {
                        auto p1 = y;
                        auto p2 = keyframe->cp1y;
                        auto p3 = keyframe->y;
                        y =
                            (1 - t2) * (1 - t2) * p1 +
                            2 * (1 - t2) * t2 * p2 +
                            t2 * t2 * p3;
                    } else {
                        auto p1 = y;
                        auto p2 = keyframe->y;
                        y = (1 - t2) * p1 + t2 * p2;
                    }
                }

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_HEIGHT) {
                    h += g_easingFuncs[keyframe->heightEasingFunc](t) * (keyframe->height - h);
                }

                if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_OPACITY) {
                    opacity += g_easingFuncs[keyframe->opacityEasingFunc](t) * (keyframe->opacity - opacity);
                }

                break;
            }

            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_X) {
                x = keyframe->x;
            }
            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_Y) {
                y = keyframe->y;
            }
            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_WIDTH) {
                w = keyframe->width;
            }
            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_HEIGHT) {
                h = keyframe->height;
            }

            if (keyframe->enabledProperties & WIDGET_TIMELINE_PROPERTY_OPACITY) {
                opacity = keyframe->opacity;
            }
        }

        widgetRect.x = x;
        widgetRect.y = y;

        widgetRect.w = w;
        widgetRect.h = h;

        widgetCursor.opacity = (uint8_t)roundf(255.0f * opacity);
    } else {
        widgetRect.x = widgetCursor.widget->x;
        widgetRect.y = widgetCursor.widget->y;

        widgetRect.w = widgetCursor.widget->width;
        widgetRect.h = widgetCursor.widget->height;
    }
}

// sets the position, size and opacity of widgetCursor.widget inside the container with static layout
static void setStaticLayoutChildPosition(
    WidgetCursor& widgetCursor,
    int containerOriginalWidth,
    int containerOriginalHeight,
    int containerWidth,
    int containerHeight
) {
    auto savedX = widgetCursor.x;

    Rect widgetRect;

    applyTimeline(widgetCursor, widgetRect);

    if (containerOriginalWidth != containerWidth || containerOriginalHeight != containerHeight) {
        resizeWidget(widgetCursor.widget, widgetRect, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);
    }

    widgetCursor.x += widgetRect.x;
    widgetCursor.y += widgetRect.y;

    widgetCursor.w = widgetRect.w;
    widgetCursor.h = widgetRect.h;

    if (g_isRTL) {
        widgetCursor.x = savedX + containerWidth - ((widgetCursor.x - savedX) + widgetCursor.w);
    }
}

void doStaticLayout(
    WidgetCursor& widgetCursor,
    const ListOfAssetsPtr<Widget> &widgets,
    int containerOriginalWidth,
    int containerOriginalHeight,
    int containerWidth,
    int containerHeight
) {
    for (uint32_t index = 0; index < widgets.count; ++index) {
        widgetCursor.widget = widgets[index];

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;
        auto savedOpacity = widgetCursor.opacity;

        setStaticLayoutChildPosition(widgetCursor, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);

        enumWidget();

        widgetCursor.x = savedX;
        widgetCursor.y = savedY;
        widgetCursor.opacity = savedOpacity;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Overlapping widgets
//
// Widgets are rendered in order (the next widget is drawn over the previous
// one), but only the widgets that are changed are rendered. If widget A is
// changed and widget B after it overlaps it, A is drawn over B. Also, if B is
// changed and it has transparent background, the container background is
// drawn under B, i.e. over A.
//
// So, while children are rendered as usual, it is collected (with the display
// damage probe) what is drawn by each child. If that area is shared by more
// than one child, the area is repainted afterwards, clipped to that area:
// container background and then all the children that intersect with it.
// Children are repainted by walking their states from the first pass (like
// forEachWidget), so updateState is not called again.

static const int MAX_OVERLAPPING_DAMAGE_RECTS = 8;

struct OverlappingDamageRects {
    Rect rects[MAX_OVERLAPPING_DAMAGE_RECTS];
    int count = 0;

    void add(const display::DamageProbe &probe) {
        Rect rect;
        rect.x = probe.x1;
        rect.y = probe.y1;
        rect.w = probe.x2 - probe.x1 + 1;
        rect.h = probe.y2 - probe.y1 + 1;

        // already included
        for (int i = 0; i < count; i++) {
            if (isInside(rect, rects[i])) {
                return;
            }
        }

        if (count < MAX_OVERLAPPING_DAMAGE_RECTS) {
            rects[count++] = rect;
        } else {
            // no more space, merge with the last one
            unite(rects[count - 1], rect);
        }
    }

    static bool isInside(const Rect &rect, const Rect &outer) {
        return rect.x >= outer.x && rect.y >= outer.y &&
            rect.x + rect.w <= outer.x + outer.w && rect.y + rect.h <= outer.y + outer.h;
    }

    static void unite(Rect &rect, const Rect &other) {
        int x1 = MIN(rect.x, other.x);
        int y1 = MIN(rect.y, other.y);
        int x2 = MAX(rect.x + rect.w, other.x + other.w);
        int y2 = MAX(rect.y + rect.h, other.y + other.h);
        rect.x = x1;
        rect.y = y1;
        rect.w = x2 - x1;
        rect.h = y2 - y1;
    }
};

static bool isIntersecting(const Rect &rect, int x, int y, int w, int h) {
    return rect.x < x + w && x < rect.x + rect.w && rect.y < y + h && y < rect.y + rect.h;
}

static void repaintOverlappingWidgetStep() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    if (widgetCursor.opacity == 0) {
        return;
    }

    display::ClipRect savedClipRect;
    if (g_findClipRect.w > 0) {
        // widget is inside of the list
        savedClipRect = display::intersectClipRect(g_findClipRect.x, g_findClipRect.y, g_findClipRect.w, g_findClipRect.h);
    } else {
        savedClipRect = display::getClipRect();
    }

    auto widgetState = widgetCursor.currentState;

    // Containers remember in render that their children must be rendered as
    // refreshed (it is used in enumChildren right after render). When repainting,
    // enumChildren doesn't use it, so it must not stay set for the next frame.
    bool savedRepainted = false;
    if (widgetState->type == WIDGET_TYPE_CONTAINER) {
        savedRepainted = ((ContainerWidgetState *)widgetState)->repainted;
    } else if (widgetState->type == WIDGET_TYPE_SELECT) {
        savedRepainted = ((SelectWidgetState *)widgetState)->repainted;
    } else if (widgetState->type == WIDGET_TYPE_USER_WIDGET) {
        savedRepainted = ((UserWidgetWidgetState *)widgetState)->repainted;
    } else if (widgetState->type == WIDGET_TYPE_LIST || widgetState->type == WIDGET_TYPE_GRID) {
        savedRepainted = ((ScrollableWidgetState *)widgetState)->rendered;
    }

    auto savedOpacity = display::setOpacity(widgetCursor.opacity);
    widgetState->render();
    display::setOpacity(savedOpacity);

    if (widgetState->type == WIDGET_TYPE_CONTAINER) {
        ((ContainerWidgetState *)widgetState)->repainted = savedRepainted;
    } else if (widgetState->type == WIDGET_TYPE_SELECT) {
        ((SelectWidgetState *)widgetState)->repainted = savedRepainted;
    } else if (widgetState->type == WIDGET_TYPE_USER_WIDGET) {
        ((UserWidgetWidgetState *)widgetState)->repainted = savedRepainted;
    } else if (widgetState->type == WIDGET_TYPE_LIST || widgetState->type == WIDGET_TYPE_GRID) {
        ((ScrollableWidgetState *)widgetState)->rendered = savedRepainted;
    }

    display::restoreClipRect(savedClipRect);
}

static void repaintOverlappingWidgets(
    WidgetCursor& widgetCursor,
    const ListOfAssetsPtr<Widget> &widgets,
    int containerOriginalWidth,
    int containerOriginalHeight,
    int containerWidth,
    int containerHeight,
    const Style *containerStyle,
    bool containerActive,
    WidgetState *firstChildState,
    WidgetState *endChildState,
    const Rect &rect
) {
    // count children in the repainted area
    int count = 0;
    auto childState = firstChildState;
    for (uint32_t index = 0; index < widgets.count && childState < endChildState && count < 2; ++index) {
        widgetCursor.widget = widgets[index];

        if (childState->widget != widgetCursor.widget) {
            // state was not created (state buffer overflow)
            break;
        }

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;
        auto savedOpacity = widgetCursor.opacity;

        setStaticLayoutChildPosition(widgetCursor, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);
        applyContentSize(childState);

        if (isIntersecting(rect, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h)) {
            count++;
        }

        childState = (WidgetState *)((uint8_t *)childState + childState->subtreeSize);

        widgetCursor.x = savedX;
        widgetCursor.y = savedY;
        widgetCursor.opacity = savedOpacity;
    }

    if (count < 2) {
        // nothing is overlapped
        return;
    }

    auto savedClipRect = display::intersectClipRect(rect.x, rect.y, rect.w, rect.h);
    auto &clipRect = display::getClipRect();
    if (clipRect.x1 > clipRect.x2 || clipRect.y1 > clipRect.y2) {
        display::restoreClipRect(savedClipRect);
        return;
    }

    // container background, the container itself is on the top of the background
    // stack, it is removed so that only the backgrounds under the container are
    // drawn (if container background is transparent)
    BackgroundStyle containerBackground = {};
    bool containerBackgroundRemoved = false;
    if (widgetCursor.backgroundStyleStackPointer > 0) {
        containerBackground = widgetCursor.backgroundStyleStack[widgetCursor.backgroundStyleStackPointer - 1];
        widgetCursor.backgroundStyleStackPointer--;
        containerBackgroundRemoved = true;
    }

    auto savedOpacity = display::setOpacity(widgetCursor.opacity);
    drawRectangle(widgetCursor.x, widgetCursor.y, containerWidth, containerHeight, containerStyle, containerActive);
    display::setOpacity(savedOpacity);

    if (containerBackgroundRemoved) {
        widgetCursor.backgroundStyleStack[widgetCursor.backgroundStyleStackPointer++] = containerBackground;
    }

    // children, background is already drawn so they are rendered as refreshed
    auto savedRefreshed = widgetCursor.refreshed;
    auto savedFindCallback = g_findCallback;
    auto savedFindClipRect = g_findClipRect;
    auto savedRepaintingOverlappingWidgets = g_repaintingOverlappingWidgets;

    widgetCursor.refreshed = true;
    g_findCallback = repaintOverlappingWidgetStep;
    g_findClipRect = Rect();
    g_repaintingOverlappingWidgets = true;

    childState = firstChildState;
    for (uint32_t index = 0; index < widgets.count && childState < endChildState; ++index) {
        widgetCursor.widget = widgets[index];

        if (childState->widget != widgetCursor.widget) {
            // state was not created (state buffer overflow)
            break;
        }

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;
        auto savedOpacity = widgetCursor.opacity;

        setStaticLayoutChildPosition(widgetCursor, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);

        applyContentSize(childState);

        auto nextChildState = (WidgetState *)((uint8_t *)childState + childState->subtreeSize);

        if (isIntersecting(rect, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h)) {
            widgetCursor.currentState = childState;
            enumWidget();
        }

        childState = nextChildState;

        widgetCursor.x = savedX;
        widgetCursor.y = savedY;
        widgetCursor.opacity = savedOpacity;
    }

    widgetCursor.currentState = endChildState;

    widgetCursor.refreshed = savedRefreshed;
    g_findCallback = savedFindCallback;
    g_findClipRect = savedFindClipRect;
    g_repaintingOverlappingWidgets = savedRepaintingOverlappingWidgets;

    display::restoreClipRect(savedClipRect);
}

void doStaticLayoutWithOverlapping(
    WidgetCursor& widgetCursor,
    const ListOfAssetsPtr<Widget> &widgets,
    int containerOriginalWidth,
    int containerOriginalHeight,
    int containerWidth,
    int containerHeight,
    const Style *containerStyle,
    bool containerActive
) {
    if (g_findCallback || widgetCursor.refreshed) {
        // all the children are rendered (in order), so overlapping is not a problem
        doStaticLayout(widgetCursor, widgets, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);
        return;
    }

    auto firstChildState = widgetCursor.currentState;

    OverlappingDamageRects damageRects;

    for (uint32_t index = 0; index < widgets.count; ++index) {
        widgetCursor.widget = widgets[index];

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;
        auto savedOpacity = widgetCursor.opacity;

        setStaticLayoutChildPosition(widgetCursor, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);

        display::DamageProbe probe;
        display::beginDamageProbe(probe);

        enumWidget();

        display::endDamageProbe(probe);

        if (!probe.isEmpty()) {
            damageRects.add(probe);
        }

        widgetCursor.x = savedX;
        widgetCursor.y = savedY;
        widgetCursor.opacity = savedOpacity;
    }

    auto endChildState = widgetCursor.currentState;

    for (int i = 0; i < damageRects.count; i++) {
        repaintOverlappingWidgets(
            widgetCursor, widgets,
            containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight,
            containerStyle, containerActive,
            firstChildState, endChildState,
            damageRects.rects[i]
        );
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
