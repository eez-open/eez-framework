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

#include <stdio.h>

#include <eez/core/os.h>
#include <eez/core/hmi.h>

#if OPTION_MOUSE
#include <eez/core/mouse.h>
#endif

#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>
#include <eez/gui/scrolling.h>

#define CONF_GUI_LONG_TOUCH_TIMEOUT_MS 1000
#define CONF_GUI_KEYPAD_FIRST_AUTO_REPEAT_DELAY_MS 300
#define CONF_GUI_KEYPAD_NEXT_AUTO_REPEAT_DELAY_MS 50
#define CONF_GUI_EXTRA_LONG_TOUCH_TIMEOUT_MS 15000

// While a scrollable list under the finger can still take over the touch,
// touch down is sent to the pressed widget with this delay. If the list
// takes over the touch before that, the widget doesn't get the touch at all.
#define CONF_GUI_TOUCH_SCROLL_TOUCH_DOWN_DELAY_MS 100

// Touch that starts this soon after the page has changed, near the position
// where the previous touch ended, is ignored. It is an accidental double tap
// (or a touch screen bounce), the user didn't have time to see the new page.
// Set to 0 to disable.
#ifndef CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_MS
#define CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_MS 300
#endif

// Max. distance in pixels from the previous touch for the touch to be ignored,
// 0 means that touch is ignored at any position.
#ifndef CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_RADIUS
#define CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_RADIUS 50
#endif

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

WidgetCursor g_activeWidget;
bool g_isLongTouch;

static WidgetCursor g_foundWidgetAtDown;

static bool g_touchActionExecuted;
static bool g_touchActionExecutedAtDown;

static OnTouchFunctionType g_onTouchFunction;

static bool g_longTouchGenerated;
static bool g_extraLongTouchGenerated;

static bool g_touchScrollCaptured;

static bool g_touchDownPending;
static Event g_pendingTouchDownEvent;
static uint32_t g_pendingTouchDownTimeMs;

// After postponed touch down is sent on touch up, touch up is postponed
// for one GUI tick, so that the pressed widget is rendered at least once.
static bool g_touchUpPending;
static Event g_pendingTouchUpEvent;

// whole touch, from touch down to touch up, is ignored
static bool g_touchIgnored;

static bool g_lastTouchUpValid;
static Event g_lastTouchUpEvent;

// processTouchEvent is reentered when the page touched outside is closed
static int g_processTouchEventDepth;

////////////////////////////////////////////////////////////////////////////////

static void processTouchEvent(EventType type, Event &touchEvent);
static void doProcessTouchEvent(Event &touchEvent);
static bool isAccidentalTouchDown(const Event &touchEvent);
static void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent);
static void onWidgetDefaultTouch(const WidgetCursor &widgetCursor, Event &touchEvent);
static void onWidgetTouch(const WidgetCursor &widgetCursor, Event &touchEvent);

////////////////////////////////////////////////////////////////////////////////

void onTouchEvent(bool pressed, int x, int y, Event &lastEvent) {
	auto eventType = lastEvent.type;
	if (pressed) {
		if (lastEvent.type == EVENT_TYPE_TOUCH_NONE || lastEvent.type == EVENT_TYPE_TOUCH_UP) {
			eventType = EVENT_TYPE_TOUCH_DOWN;
		} else {
			if (lastEvent.type == EVENT_TYPE_TOUCH_DOWN) {
				eventType = EVENT_TYPE_TOUCH_MOVE;
			}
		}
	} else {
		if (lastEvent.type == EVENT_TYPE_TOUCH_DOWN || lastEvent.type == EVENT_TYPE_TOUCH_MOVE) {
			eventType = EVENT_TYPE_TOUCH_UP;
		} else if (lastEvent.type == EVENT_TYPE_TOUCH_UP) {
			eventType = EVENT_TYPE_TOUCH_NONE;
		}
	}

	if (eventType != lastEvent.type || x != lastEvent.x || y != lastEvent.y) {
		lastEvent.type = eventType;
		lastEvent.x = x;
		lastEvent.y = y;
		if (lastEvent.type != EVENT_TYPE_TOUCH_NONE) {
			sendTouchEventToGuiThread(lastEvent);
		}
	} else {
		static const uint32_t SEND_TOUC_MOVE_EVENT_EVERY_MS = 20;
		if (lastEvent.type == EVENT_TYPE_TOUCH_MOVE) {
			auto time = millis();
			if (time - lastEvent.time > SEND_TOUC_MOVE_EVENT_EVERY_MS) {
				sendTouchEventToGuiThread(lastEvent);
			}
		}
	}
}

static void doProcessTouchEvent(Event &touchEvent) {
	if (g_hooks.isEventHandlingDisabled()) {
		return;
	}

    if (touchEvent.type != EVENT_TYPE_TOUCH_NONE) {
        // finish the previous touch first
        processPostponedTouchEvents();
    }

    static uint32_t m_lastAutoRepeatEventTimeMs;
    static uint32_t m_touchDownTimeMs;

    if (touchEvent.type != EVENT_TYPE_TOUCH_NONE) {
        uint32_t tickCountMs = millis();

        eez::hmi::noteActivity();

        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
            m_touchDownTimeMs = tickCountMs;
            m_lastAutoRepeatEventTimeMs = tickCountMs;
            g_longTouchGenerated = false;
            g_extraLongTouchGenerated = false;
            processTouchEvent(EVENT_TYPE_TOUCH_DOWN, touchEvent);
        } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
            processTouchEvent(EVENT_TYPE_TOUCH_MOVE, touchEvent);

            if (!g_longTouchGenerated && int32_t(tickCountMs - m_touchDownTimeMs) >= CONF_GUI_LONG_TOUCH_TIMEOUT_MS) {
                g_longTouchGenerated = true;
                processTouchEvent(EVENT_TYPE_LONG_TOUCH, touchEvent);
            }

            if (g_longTouchGenerated && !g_extraLongTouchGenerated && int32_t(tickCountMs - m_touchDownTimeMs) >= CONF_GUI_EXTRA_LONG_TOUCH_TIMEOUT_MS) {
                g_extraLongTouchGenerated = true;
				g_touchActionExecuted = true;
				int action = g_hooks.getExtraLongTouchAction();
				if (action != EEZ_ACTION_ID_NONE) {
					executeAction(WidgetCursor(), action);
				}
            }

            if (int32_t(tickCountMs - m_lastAutoRepeatEventTimeMs) >= (m_lastAutoRepeatEventTimeMs == m_touchDownTimeMs ? CONF_GUI_KEYPAD_FIRST_AUTO_REPEAT_DELAY_MS : CONF_GUI_KEYPAD_NEXT_AUTO_REPEAT_DELAY_MS)) {
                processTouchEvent(EVENT_TYPE_AUTO_REPEAT, touchEvent);
                m_lastAutoRepeatEventTimeMs = tickCountMs;
            }
        } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
            processTouchEvent(EVENT_TYPE_TOUCH_UP, touchEvent);
            g_lastTouchUpValid = true;
            g_lastTouchUpEvent = touchEvent;
        }
    }
}

void processTouchEvent(Event &touchEvent) {
    g_processTouchEventDepth++;
    doProcessTouchEvent(touchEvent);
    g_processTouchEventDepth--;
}

static bool isPageChangedRecently(AppContext *appContext, uint32_t timeMs) {
    // also true if the page has changed after the touch has started
    return appContext && int32_t(timeMs - appContext->getShowPageTime()) < CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_MS;
}

static bool isAccidentalTouchDown(const Event &touchEvent) {
    if (CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_MS == 0) {
        return false;
    }

    // touch down sent again after the page touched outside is closed
    if (g_processTouchEventDepth > 1) {
        return false;
    }

    if (!g_lastTouchUpValid || int32_t(touchEvent.time - g_lastTouchUpEvent.time) >= CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_MS) {
        return false;
    }

    if (CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_RADIUS > 0) {
        int dx = touchEvent.x - g_lastTouchUpEvent.x;
        int dy = touchEvent.y - g_lastTouchUpEvent.y;
        if (dx * dx + dy * dy > CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_RADIUS * CONF_GUI_IGNORE_TOUCH_AFTER_PAGE_CHANGE_RADIUS) {
            return false;
        }
    }

    return isPageChangedRecently(getRootAppContext(), touchEvent.time) ||
        (g_foundWidgetAtDown.appContext && isPageChangedRecently(g_foundWidgetAtDown.appContext, touchEvent.time));
}

// List can take over the touch from the widget pressed at touch down only
// if that widget has not done anything yet that can't be undone.
static bool canTouchScrollCapture(bool atTouchDown) {
    if (g_longTouchGenerated) {
        return false;
    }

    if (g_onTouchFunction == onPageTouch) {
        return true;
    }

    if (g_onTouchFunction == onWidgetDefaultTouch) {
        // at touch down the widget hasn't received the event yet
        if (!atTouchDown && (g_touchActionExecutedAtDown || g_touchActionExecuted)) {
            return false;
        }

        // widget is gone if the action executed at touch down changed the page
        if (!g_foundWidgetAtDown) {
            return false;
        }

        return getWidgetAction(g_foundWidgetAtDown) != EEZ_ACTION_ID_DRAG_OVERLAY;
    }

    // widgets with their own touch handling (slider, scroll bar, ...) keep the touch
    return false;
}

static void captureTouchScroll() {
    g_touchScrollCaptured = true;

    // cancel the press of the widget found at touch down
    g_activeWidget = 0;
    g_touchActionExecuted = true;

    g_foundWidgetAtDown = getTouchScrollList();
    g_onTouchFunction = touchScrollOnTouch;
}

static void sendPendingTouchDown() {
    g_touchDownPending = false;
    if (g_onTouchFunction) {
        Event event = g_pendingTouchDownEvent;
        event.type = EVENT_TYPE_TOUCH_DOWN;
        g_onTouchFunction(g_foundWidgetAtDown, event);
    }
}

static void processTouchEvent(EventType type, Event &touchEvent) {
    if (type != EVENT_TYPE_TOUCH_DOWN && g_touchIgnored) {
        return;
    }

    if (type == EVENT_TYPE_TOUCH_DOWN) {
        g_foundWidgetAtDown = findWidget(touchEvent.x, touchEvent.y);

        g_touchIgnored = isAccidentalTouchDown(touchEvent);
        if (g_touchIgnored) {
            g_foundWidgetAtDown = 0;
            g_onTouchFunction = nullptr;
            g_activeWidget = 0;
            g_touchDownPending = false;
            g_touchScrollCaptured = false;
            return;
        }

        g_onTouchFunction = getWidgetTouchFunction(g_foundWidgetAtDown);
        if (!g_onTouchFunction) {
            g_onTouchFunction = onPageTouch;
        }

        // widget hasn't received this touch yet
        g_touchActionExecuted = false;
        g_touchActionExecutedAtDown = false;
        g_touchDownPending = false;

        g_touchScrollCaptured = false;
        if (touchScrollBegin(canTouchScrollCapture(true) ? getFoundWidgetScrollLists() : ScrollLists(), touchEvent)) {
            // touch stopped the moving list, it doesn't go to the widget under the finger
            captureTouchScroll();
            return;
        }

        if (isTouchScrollPending()) {
            // postpone touch down, the list can still take over the touch
            g_touchDownPending = true;
            g_pendingTouchDownEvent = touchEvent;
            g_pendingTouchDownTimeMs = millis();
            return;
        }
    } else if (type == EVENT_TYPE_TOUCH_MOVE) {
        if (!g_touchScrollCaptured && canTouchScrollCapture(false) && touchScrollShouldCapture(touchEvent)) {
            // widget never gets the postponed touch down
            g_touchDownPending = false;
            captureTouchScroll();
        } else if (
            g_touchDownPending &&
            (!isTouchScrollPending() || int32_t(millis() - g_pendingTouchDownTimeMs) >= CONF_GUI_TOUCH_SCROLL_TOUCH_DOWN_DELAY_MS)
        ) {
            sendPendingTouchDown();
        }
    } else if (type == EVENT_TYPE_TOUCH_UP) {
        if (g_touchDownPending) {
            // tap: widget gets touch down now and touch up in the next GUI tick
            sendPendingTouchDown();
            g_touchUpPending = true;
            g_pendingTouchUpEvent = touchEvent;
            return;
        }
        g_activeWidget = 0;
        if (!g_touchScrollCaptured) {
            touchScrollEnd();
        }
    }

    if (g_touchDownPending) {
        // nothing is sent to the widget before touch down
        return;
    }

    if (g_onTouchFunction) {
        Event event = touchEvent;
        event.type = type;
        g_onTouchFunction(g_foundWidgetAtDown, event);
    }
}

void processPostponedTouchEvents() {
    if (g_touchUpPending) {
        g_touchUpPending = false;
        processTouchEvent(EVENT_TYPE_TOUCH_UP, g_pendingTouchUpEvent);
    }
}

OnTouchFunctionType getWidgetTouchFunction(const WidgetCursor &widgetCursor) {
    if (widgetCursor) {
        auto action = getWidgetAction(widgetCursor);
        if (action && !widgetCursor.appContext->isWidgetActionEnabled(widgetCursor)) {
            return nullptr;
        }

        if (widgetCursor.currentState->hasOnTouch()) {
			return onWidgetTouch;
		}

        if (widgetCursor.appContext->isWidgetActionEnabled(widgetCursor)) {
            return onWidgetDefaultTouch;
        }

        return g_hooks.getWidgetTouchFunction(widgetCursor);
    }

    return nullptr;
}

static void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
	if (foundWidget.appContext) {
		foundWidget.appContext->onPageTouch(foundWidget, touchEvent);
	} else {
		getRootAppContext()->onPageTouch(foundWidget, touchEvent);
	}
}

static void onWidgetDefaultTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (!widgetCursor.widget) {
        return;
    }

    auto action = getWidgetAction(widgetCursor);

    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        g_touchActionExecuted = false;
        g_touchActionExecutedAtDown = false;

        if (action == EEZ_ACTION_ID_DRAG_OVERLAY) {
            dragOverlay(touchEvent);
            g_activeWidget = widgetCursor;
        } else if (widgetCursor.appContext->testExecuteActionOnTouchDown(action)) {
            executeAction(widgetCursor, action);
            g_touchActionExecutedAtDown = true;
            if (widgetCursor.appContext->isAutoRepeatAction(action)) {
                g_activeWidget = widgetCursor;
            }
        } else {
            g_activeWidget = widgetCursor;
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        if (action == EEZ_ACTION_ID_DRAG_OVERLAY) {
            dragOverlay(touchEvent);
        }
    } else if (touchEvent.type == EVENT_TYPE_AUTO_REPEAT) {
        if (widgetCursor.appContext->isWidgetActionEnabled(widgetCursor) && widgetCursor.appContext->isAutoRepeatAction(action)) {
            g_touchActionExecuted = true;
            executeAction(widgetCursor, action);
        }
    } else if (touchEvent.type == EVENT_TYPE_LONG_TOUCH) {
        g_touchActionExecuted = true;
        int action = widgetCursor.appContext->getLongTouchActionHook(widgetCursor);
        if (action != EEZ_ACTION_ID_NONE) {
            g_isLongTouch = true;
            executeAction(widgetCursor, action);
            g_isLongTouch = false;
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        if (!g_touchActionExecutedAtDown) {
            if (!g_touchActionExecuted) {
                if (action == EEZ_ACTION_ID_DRAG_OVERLAY) {
                    dragOverlay(touchEvent);
                } else {
                    executeAction(widgetCursor, action);
                }
            }
        }
    }
}

static void onWidgetTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
	if (widgetCursor) {
        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
			g_activeWidget = g_foundWidgetAtDown;
        }
		widgetCursor.currentState->onTouch(widgetCursor, touchEvent);
	}
}


////////////////////////////////////////////////////////////////////////////////

WidgetCursor &getFoundWidgetAtDown() {
    return g_foundWidgetAtDown;
}

void setFoundWidgetAtDown(WidgetCursor &widgetCursor) {
    g_foundWidgetAtDown = widgetCursor;
}

void clearFoundWidgetAtDown() {
    g_foundWidgetAtDown = 0;
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
