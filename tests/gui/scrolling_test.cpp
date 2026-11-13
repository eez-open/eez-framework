/*
 * eez-framework tests: gui/scrolling.cpp, touch scrolling of the List and the
 * Grid (gui_lists page: a list of 20 items of 60 px in 300 px at 0, 0, 400x300,
 * a grid of 60 items 90x60, 4 in a row, at 420, 0, 360x300).
 */

#include "gui_test.h"

using namespace gui_test;

static void showLists() {
    app()->showPage(PAGE_ID_GUI_LISTS);
    harness::frame(2);
    harness::advance(400);
}

// index of the list (or grid) item under the point, -1 if none
static int itemAt(int x, int y) {
    auto widgetCursor = findWidget(x, y, false);
    if (!widgetCursor.widget || widgetCursor.widget->type != WIDGET_TYPE_TEXT) {
        return -1;
    }
    return widgetCursor.iterators[0];
}

// first item fully visible at the top of the list
static int listTop() {
    return itemAt(200, 30);
}

static int gridTopLeft() {
    return itemAt(465, 30);
}

// drags the finger from y1 to y2 in steps, holds it still (no fling) and lifts it
static void drag(int x, int y1, int y2, bool fling = false) {
    press(x, y1);
    int steps = 6;
    for (int i = 1; i <= steps; i++) {
        moveTo(x, y1 + (y2 - y1) * i / steps);
    }
    if (!fling) {
        // still for more than the velocity window (100 ms)
        harness::frame(10);
    }
    releaseTouch();
}

static void settle() {
    harness::frame(100);
}

TEST("gui/scrolling: the list shows its first items") {
    showLists();
    CHECK_EQ(listTop(), 0);
    CHECK_EQ(itemAt(200, 270), 4);
    CHECK_EQ(gridTopLeft(), 0);
    CHECK_EQ(itemAt(735, 30), 3);
    CHECK_EQ(itemAt(465, 90), 4);
}

TEST("gui/scrolling: a tap on a list item executes its action with the item index") {
    showLists();
    tap(200, 90);
    CHECK_EQ(g_actions.listItem, 1);
    CHECK_EQ(g_actions.listItemIndex, 1);
}

TEST("gui/scrolling: a tap on a grid item executes its action with the item index") {
    showLists();
    tap(555, 90);
    CHECK_EQ(g_actions.gridItem, 1);
    CHECK_EQ(g_actions.gridItemIndex, 5);
}

TEST("gui/scrolling: the touch down of a list item is delayed, a button outside of lists is pressed at once") {
    showLists();
    harness::touch(600, 430);
    harness::frame(4);
    CHECK(g_activeWidget.widget != nullptr);
    releaseTouch();
    harness::advance(400);

    harness::touch(200, 90);
    harness::frame(4);
    // the list can still take over the touch
    CHECK(g_activeWidget.widget == nullptr);
    harness::frame(5);
    CHECK(g_activeWidget.widget != nullptr);
    releaseTouch();
    CHECK_EQ(g_actions.listItem, 1);
}

TEST("gui/scrolling: dragging the list scrolls it and doesn't click the item") {
    showLists();
    drag(200, 250, 70);
    settle();
    CHECK_EQ(listTop(), 3);
    CHECK_EQ(g_actions.listItem, 0);
}

TEST("gui/scrolling: the list follows the finger while it is dragged") {
    showLists();
    press(200, 250);
    // beyond the drag threshold (12 px): the list takes over the touch, it
    // moves with the finger from here
    moveTo(200, 230);
    CHECK_EQ(listTop(), 0);
    moveTo(200, 170);
    // 60 px up: item 1 is at the top
    CHECK_EQ(itemAt(200, 5), 1);
    moveTo(200, 140);
    // 90 px up: item 1 is half visible at the top, item 2 below it
    CHECK_EQ(itemAt(200, 40), 2);
    harness::frame(10);
    releaseTouch();
}

TEST("gui/scrolling: a finger move below the drag threshold is a tap") {
    showLists();
    press(200, 90);
    moveTo(200, 98);
    releaseTouch();
    CHECK_EQ(g_actions.listItem, 1);
    CHECK_EQ(listTop(), 0);
}

TEST("gui/scrolling: the list settles on an item boundary") {
    showLists();
    // 120 px, 100 px after the list took over the touch (the first step of
    // 20 px): nearer to 2 items than to 1
    drag(200, 250, 130);
    settle();
    CHECK_EQ(listTop(), 2);
    CHECK_EQ(itemAt(200, 1), 2);
}

TEST("gui/scrolling: a fling moves the list further after the finger is lifted") {
    showLists();
    drag(200, 280, 160, true);
    int atRelease = itemAt(200, 1);
    settle();
    CHECK(listTop() > 2);
    CHECK(listTop() > atRelease);
}

TEST("gui/scrolling: the list doesn't scroll beyond its end") {
    showLists();
    for (int i = 0; i < 6; i++) {
        drag(200, 290, 10, true);
        settle();
        harness::advance(400);
    }
    // 20 items, 5 visible
    CHECK_EQ(listTop(), 15);
    CHECK_EQ(itemAt(200, 270), 19);
}

TEST("gui/scrolling: the list pulled beyond its start returns to the start") {
    showLists();
    drag(200, 50, 250);
    settle();
    CHECK_EQ(listTop(), 0);
    CHECK_EQ(g_actions.listItem, 0);
}

TEST("gui/scrolling: a tap on a moving list stops it and doesn't click") {
    showLists();
    drag(200, 290, 50, true);
    harness::frame(3);
    int moving = itemAt(200, 150);
    press(200, 150);
    releaseTouch();
    CHECK_EQ(g_actions.listItem, 0);
    int stopped = listTop();
    settle();
    CHECK_EQ(listTop(), stopped);
    CHECK(moving >= 0);
}

TEST("gui/scrolling: the grid scrolls by rows") {
    showLists();
    drag(600, 250, 130);
    settle();
    // two rows of 4 items
    CHECK_EQ(gridTopLeft(), 8);
    CHECK_EQ(listTop(), 0);
}

TEST("gui/scrolling: a horizontal finger move on a vertical list is a tap of the item") {
    // pinned: no list scrolls in that direction, the item gets the touch
    showLists();
    press(100, 90);
    moveTo(200, 92);
    releaseTouch();
    CHECK_EQ(listTop(), 0);
    CHECK_EQ(g_actions.listItem, 1);
}

TEST("gui/scrolling: the list scroll stops when the page is changed") {
    showLists();
    drag(200, 290, 50, true);
    app()->showPage(PAGE_ID_GUI_PAGE_A);
    harness::frame(10);
    app()->showPage(PAGE_ID_GUI_LISTS);
    harness::frame(100);
    CHECK_EQ(g_actions.listItem, 0);
    CHECK(listTop() >= 0);
}

////////////////////////////////////////////////////////////////////////////////
// page dots of the horizontal list (0, 320, 400x80, 20 items of 100 px): 17
// positions, 8x8 dots 8 px apart in a row of 264 px at x = 68, 16 px above the
// bottom

static const int DOTS_X = 68;
static const int DOTS_Y = 320 + 80 - 16;

// the pixels of the dot that are not of the color (0xBBGGRR)
static int dotOther(int i, uint32_t color) {
    int n = 0;
    for (int y = DOTS_Y; y < DOTS_Y + 8; y++) {
        for (int x = DOTS_X + 16 * i; x < DOTS_X + 16 * i + 8; x++) {
            if ((harness::pixel(x, y) & 0xFFFFFF) != color) {
                n++;
            }
        }
    }
    return n;
}

static const uint32_t PAGE_DOT = 0xFFFFFF;
static const uint32_t PAGE_DOT_SELECTED = 0xC06020;

static void dragX(int y, int x1, int x2) {
    press(x1, y);
    int steps = 6;
    for (int i = 1; i <= steps; i++) {
        moveTo(x1 + (x2 - x1) * i / steps, y);
    }
    harness::frame(10);
    releaseTouch();
}

TEST("gui/scrolling: a horizontal list shows a page dot for every position, the current one selected") {
    showLists();
    CHECK_EQ(dotOther(0, PAGE_DOT_SELECTED), 0);
    for (int i = 1; i < 17; i++) {
        CHECK_EQ(dotOther(i, PAGE_DOT), 0);
    }
    // no dot after the last one
    CHECK(dotOther(17, PAGE_DOT) > 0);
}

TEST("gui/scrolling: the page dot follows the position of the horizontal list") {
    showLists();
    dragX(350, 300, 100);
    settle();
    CHECK_EQ(dotOther(0, PAGE_DOT), 0);
    CHECK_EQ(dotOther(2, PAGE_DOT_SELECTED), 0);
    CHECK_EQ(dotOther(3, PAGE_DOT), 0);
}

////////////////////////////////////////////////////////////////////////////////
// a container that scrolls vertically (gui_scroll page: 10 rows of 60 px in a
// container 400x300 at 0, 0), scrolled by pixels

static void showScroll() {
    app()->showPage(PAGE_ID_GUI_SCROLL);
    harness::frame(2);
    harness::advance(400);
}

// the row under the point (its design y / 60), -1 if none
static int rowAt(int x, int y) {
    auto widgetCursor = findWidget(x, y, false);
    if (!widgetCursor.widget || widgetCursor.widget->type != WIDGET_TYPE_TEXT) {
        return -1;
    }
    return widgetCursor.widget->y / 60;
}

TEST("gui/scrolling: a container that scrolls vertically shows its first rows") {
    showScroll();
    CHECK_EQ(rowAt(200, 30), 0);
    CHECK_EQ(rowAt(200, 270), 4);
    // the rows below the container are not found
    CHECK_EQ(rowAt(200, 330), -1);
}

TEST("gui/scrolling: dragging the container scrolls it by pixels") {
    showScroll();
    // 120 px, 100 px after the container took over the touch (the first
    // step of 20 px), not moved to a row boundary
    drag(200, 250, 130);
    settle();
    // row 1 from y = -40 to 19, row 2 from 20
    CHECK_EQ(rowAt(200, 19), 1);
    CHECK_EQ(rowAt(200, 20), 2);
    CHECK_EQ(rowAt(200, 295), 6);
}

TEST("gui/scrolling: the container doesn't scroll beyond the end of its content") {
    showScroll();
    drag(200, 290, 10);
    drag(200, 290, 10);
    settle();
    // 600 px of content in 300 px: the last row at the bottom
    CHECK_EQ(rowAt(200, 270), 9);
    CHECK_EQ(rowAt(200, 5), 5);
}

TEST("gui/scrolling: a tap on a row of the scrolled container is a tap of that row") {
    showScroll();
    drag(200, 250, 130);
    settle();
    int clicks = 0;
    g_onAction = [&clicks](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_A) {
            clicks++;
        }
    };
    tap(200, 30);
    CHECK_EQ(clicks, 1);
    g_onAction = nullptr;
}
