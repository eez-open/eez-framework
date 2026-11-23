/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * eez-framework tests: widgets with their own touch handling (WidgetTouchKind)
 * in a container that scrolls vertically, page gui_scroll_widgets: rows of
 * 60 px in a container 400x300 at 0, 0: a switch (y 0), a drop down list
 * (y 60), a checkbox (y 120), a button group of 3 buttons (y 180), an up/down
 * (y 240, left half down, right half up), then text rows.
 */

#include "gui_test.h"

#include <eez/gui/scrolling.h>

using namespace gui_test;

static const int SWITCH_Y = 30;
static const int DROPDOWN_Y = 90;
static const int CHECK_Y = 150;
static const int GROUP_Y = 210;
static const int UPDOWN_Y = 270;

// outside of the container and of the press cancel margin
static const int OUT_X = 600, OUT_Y = 400;

static void showWidgets() {
    app()->showPage(PAGE_ID_GUI_SCROLL_WIDGETS);
    harness::frame(2);
    harness::advance(400);
}

// the design y of the widget under the point, -1 if none
static int designYAt(int x, int y) {
    auto widgetCursor = findWidget(x, y, false);
    return widgetCursor.widget ? widgetCursor.widget->y : -1;
}

static bool isSelectPageShown() {
    return activePageId() == INTERNAL_PAGE_ID_SELECT_FROM_ENUM;
}

// press, move outside of the widget and release there
static void releaseOutside(int x, int y) {
    press(x, y);
    moveTo(OUT_X, OUT_Y);
    releaseTouch();
    harness::advance(400);
}

// drags the finger from y1 to y2 in steps and lifts it; the container must
// have taken over the touch. quick: the first move right after the touch down
// (before the widget gets the postponed touch down)
static void dragContainer(int x, int y1, int y2, bool quick = false) {
    if (quick) {
        harness::touch(x, y1);
        harness::frame(1);
    } else {
        press(x, y1);
    }
    int steps = 6;
    for (int i = 1; i <= steps; i++) {
        moveTo(x, y1 + (y2 - y1) * i / steps);
    }
    CHECK(isTouchScrollDragging());
    harness::frame(10);
    releaseTouch();
    harness::frame(100);
}

////////////////////////////////////////////////////////////////////////////////
// switch

TEST("gui/touch_widgets: a switch is toggled by a tap in a scrolling container") {
    showWidgets();
    tap(200, SWITCH_Y);
    CHECK(g_switch);
}

TEST("gui/touch_widgets: a switch released outside of it is not toggled and is not drawn pressed outside") {
    showWidgets();
    press(200, SWITCH_Y);
    // the touch down is postponed in a scrolling container
    harness::frame(5);
    CHECK(g_activeWidget.widget != nullptr);
    moveTo(OUT_X, OUT_Y);
    CHECK(g_activeWidget.widget == nullptr);
    releaseTouch();
    CHECK(!g_switch);
}

TEST("gui/touch_widgets: a switch the finger left and came back to is toggled") {
    showWidgets();
    press(200, SWITCH_Y);
    moveTo(OUT_X, OUT_Y);
    moveTo(210, SWITCH_Y + 5);
    CHECK(g_activeWidget.widget != nullptr);
    releaseTouch();
    CHECK(g_switch);
}

TEST("gui/touch_widgets: a drag that starts on a switch scrolls the container, the switch is not toggled") {
    showWidgets();
    // pulled beyond the start: the container returns to it
    dragContainer(200, SWITCH_Y, 250);
    CHECK(!g_switch);
    CHECK_EQ(designYAt(200, SWITCH_Y), 0);
}

////////////////////////////////////////////////////////////////////////////////
// drop down list

TEST("gui/touch_widgets: a tap on a drop down list opens the select page") {
    showWidgets();
    tap(200, DROPDOWN_Y);
    CHECK(isSelectPageShown());
}

TEST("gui/touch_widgets: a drop down list released outside of it doesn't open") {
    showWidgets();
    releaseOutside(200, DROPDOWN_Y);
    CHECK(!isSelectPageShown());
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_SCROLL_WIDGETS);
}

TEST("gui/touch_widgets: a drag that starts on a drop down list scrolls the container, it doesn't open") {
    showWidgets();
    dragContainer(200, DROPDOWN_Y, 10);
    CHECK(!isSelectPageShown());
    // the switch row is above the container
    CHECK(designYAt(200, 10) > 0);
}

////////////////////////////////////////////////////////////////////////////////
// checkbox

TEST("gui/touch_widgets: a checkbox released outside of it is not checked") {
    showWidgets();
    releaseOutside(200, CHECK_Y);
    CHECK(!g_check);
    tap(200, CHECK_Y);
    CHECK(g_check);
}

TEST("gui/touch_widgets: a drag that starts on a checkbox scrolls the container, it is not checked") {
    showWidgets();
    dragContainer(200, CHECK_Y, 20);
    CHECK(!g_check);
    CHECK(designYAt(200, 20) >= 120);
}

////////////////////////////////////////////////////////////////////////////////
// button group

TEST("gui/touch_widgets: a button group selects the button at release") {
    showWidgets();
    press(333, GROUP_Y);
    CHECK_EQ(g_choice, 0);
    releaseTouch();
    CHECK_EQ(g_choice, 2);
}

TEST("gui/touch_widgets: a button group released outside of it selects nothing") {
    showWidgets();
    releaseOutside(333, GROUP_Y);
    CHECK_EQ(g_choice, 0);
}

TEST("gui/touch_widgets: a release just outside of a button group, within the margin, selects the nearest button") {
    showWidgets();
    // 10 px right of the container (the button group is 400 px wide)
    press(380, GROUP_Y);
    moveTo(409, GROUP_Y);
    releaseTouch();
    CHECK_EQ(g_choice, 2);
}

TEST("gui/touch_widgets: a drag that starts on a button group scrolls the container, nothing is selected") {
    showWidgets();
    dragContainer(333, GROUP_Y, 30);
    CHECK_EQ(g_choice, 0);
    CHECK(designYAt(200, 30) >= 120);
}

////////////////////////////////////////////////////////////////////////////////
// up/down: acts at touch down

TEST("gui/touch_widgets: an up/down acts at touch down and repeats while pressed") {
    showWidgets();
    press(300, UPDOWN_Y);
    harness::frame(10);
    CHECK_EQ(g_choice, 1);
    // first repeat after 300 ms, then every 50 ms
    harness::frame(30);
    CHECK(g_choice > 2);
    releaseTouch();
}

TEST("gui/touch_widgets: an up/down doesn't repeat while the finger is outside of it") {
    showWidgets();
    press(300, UPDOWN_Y);
    harness::frame(10);
    CHECK_EQ(g_choice, 1);
    moveTo(OUT_X, OUT_Y);
    harness::frame(60);
    CHECK_EQ(g_choice, 1);
    releaseTouch();
    CHECK_EQ(g_choice, 1);
}

TEST("gui/touch_widgets: a quick drag that starts on an up/down scrolls the container, the value doesn't change") {
    showWidgets();
    dragContainer(300, UPDOWN_Y, 30, true);
    CHECK_EQ(g_choice, 0);
    CHECK(designYAt(200, 30) >= 180);
}
