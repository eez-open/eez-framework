/*
 * eez-framework tests: the items of a List and a Grid that the layout resized
 * (getListItemSize). gui_resize shows the page gui_resize_view (300x320: a
 * list of 8 items 100x40 at 0, 0, a grid of 8 items 50x80, 2 in a row, at 100,
 * 0, a list pinned to the top and the bottom at 200, 0) in a user widget of
 * 300x240 at 0, 0 (smaller) and of 300x480 at 400, 0 (bigger).
 */

#include "gui_test.h"

using namespace gui_test;

static void showResize() {
    app()->showPage(PAGE_ID_GUI_RESIZE);
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

TEST("gui/list_resize: the items of a list shown smaller are smaller, all of them fit") {
    showResize();
    // 240 / 320: items of 30 px
    CHECK_EQ(itemAt(50, 15), 0);
    CHECK_EQ(itemAt(50, 45), 1);
    CHECK_EQ(itemAt(50, 225), 7);
}

TEST("gui/list_resize: the items of a list shown bigger are bigger") {
    showResize();
    // 480 / 320: items of 60 px
    CHECK_EQ(itemAt(450, 30), 0);
    CHECK_EQ(itemAt(450, 90), 1);
    CHECK_EQ(itemAt(450, 450), 7);
}

TEST("gui/list_resize: the items of a grid are resized with the grid") {
    showResize();
    // smaller: items 50x60, 2 in a row, 4 rows in 240 px
    CHECK_EQ(itemAt(125, 30), 0);
    CHECK_EQ(itemAt(175, 30), 1);
    CHECK_EQ(itemAt(125, 210), 6);
    CHECK_EQ(itemAt(175, 210), 7);
    // bigger: items 50x120
    CHECK_EQ(itemAt(525, 60), 0);
    CHECK_EQ(itemAt(575, 420), 7);
}

TEST("gui/list_resize: a list pinned to both edges keeps the item size, it shows fewer or more items") {
    showResize();
    // smaller: 6 items of 40 px in 240 px
    CHECK_EQ(itemAt(250, 20), 0);
    CHECK_EQ(itemAt(250, 220), 5);
    // bigger: the 8 items end at 320, the rest of the 480 px list is empty
    CHECK_EQ(itemAt(650, 300), 7);
    CHECK_EQ(itemAt(650, 400), -1);
}

