/*
 * eez-framework tests: BarGraph, YTGraph (native data), ListGraph, Progress
 * widgets (pages w_bar, w_yt, w_lgraph, w_progress).
 */

#include <math.h>

#include "wtest.h"

#include <eez/gui/widgets/bar_graph.h>
#include <eez/gui/widgets/yt_graph.h>

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

// w_bar: green bar on blue (style w_bar), line 1 red, line 2 yellow
const int BAR_LR[] = { 10, 10, 200, 30 };
const int BAR_BT[] = { 10, 60, 40, 200 };
const int BAR_TEXT[] = { 300, 10, 200, 30 };

// w_yt: 400x200, background black, grid gray, trace red
const int YT[] = { 10, 10, 400, 200 };

// w_lgraph: background black, y1 red, y2 yellow, cursor gray
const int LG[] = { 10, 10, 300, 150 };

// w_progress: blue, green part
const int PR_H[] = { 10, 10, 200, 30 };
const int PR_V[] = { 10, 60, 30, 200 };

#define RECT(R) R[0], R[1], R[2], R[3]

void noLines() {
    set(V_B, intValue(-1));
    set(V_C, intValue(-1));
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// BarGraph

TEST("widgets/bargraph: the bar fills the part of the value") {
    set(V_A, intValue(50));
    noLines();
    show(PAGE_ID_W_BAR);
    // 50 of 0..100 in 200 px
    CHECK_EQ(countOther(BAR_LR[0], BAR_LR[1], 100, BAR_LR[3], GREEN), 0);
    CHECK_EQ(countOther(BAR_LR[0] + 100, BAR_LR[1], 100, BAR_LR[3], BLUE), 0);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/bargraph: redrawn when the value changes") {
    set(V_A, intValue(50));
    noLines();
    show(PAGE_ID_W_BAR);
    set(V_A, intValue(25));
    harness::frame();
    CHECK(drawn());
    CHECK_EQ(countOther(BAR_LR[0], BAR_LR[1], 50, BAR_LR[3], GREEN), 0);
    CHECK_EQ(countOther(BAR_LR[0] + 50, BAR_LR[1], 150, BAR_LR[3], BLUE), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/bargraph: threshold lines at their values") {
    set(V_A, intValue(10));
    set(V_B, intValue(25));
    set(V_C, intValue(75));
    show(PAGE_ID_W_BAR);
    CHECK_EQ(countOther(BAR_LR[0] + 50, BAR_LR[1], 1, BAR_LR[3] - 1, RED), 0);
    CHECK_EQ(countOther(BAR_LR[0] + 150, BAR_LR[1], 1, BAR_LR[3] - 1, YELLOW), 0);
    CHECK_EQ(rgb(BAR_LR[0] + 100, BAR_LR[1] + 10), BLUE);
}

TEST("widgets/bargraph: bottom-top bar grows from the bottom") {
    set(V_A, intValue(50));
    show(PAGE_ID_W_BAR);
    CHECK_EQ(countOther(BAR_BT[0], BAR_BT[1] + 100, BAR_BT[2], 100, GREEN), 0);
    CHECK_EQ(countOther(BAR_BT[0], BAR_BT[1], BAR_BT[2], 50, BLUE), 0);
}

TEST_BUG("widgets/bargraph: vertical bar without display value draws no value text", "W12") {
    set(V_A, intValue(50));
    show(PAGE_ID_W_BAR);
    // only the bar and the background, the vertical branches ignore the flag
    CHECK_EQ(BAR_BT[2] * BAR_BT[3] - count(RECT(BAR_BT), GREEN) - count(RECT(BAR_BT), BLUE), 0);
}

TEST("widgets/bargraph: the value is drawn in the bar with display value") {
    set(V_A, intValue(50));
    show(PAGE_ID_W_BAR);
    // w_bar_text: white text
    CHECK(count(RECT(BAR_TEXT), WHITE) > 0);
    CHECK(count(RECT(BAR_TEXT), GREEN) > 0);
}

TEST_BUG("widgets/bargraph: horizontal bar with display value and no text style draws the bar", "W12") {
    // Studio always builds a text style (the default style), only assets
    // from elsewhere have 0 here
    ((BarGraphWidget *)pageWidget(PAGE_ID_W_BAR, 2))->textStyle = 0;
    set(V_A, intValue(50));
    show(PAGE_ID_W_BAR);
    CHECK_EQ(countOther(BAR_TEXT[0], BAR_TEXT[1], 90, BAR_TEXT[3], GREEN), 0);
}

TEST_BUG("widgets/bargraph: a value far above max fills the whole bar", "W13") {
    // (int)roundf of a too large float: INT_MIN on x86 (saturates on ARM64)
    set(V_A, Value(1e10f, VALUE_TYPE_FLOAT));
    noLines();
    show(PAGE_ID_W_BAR);
    CHECK_EQ(countOther(BAR_LR[0], BAR_LR[1], 190, BAR_LR[3], GREEN), 0);
}

TEST("widgets/bargraph: a value below min shows no bar") {
    set(V_A, intValue(-50));
    noLines();
    show(PAGE_ID_W_BAR);
    CHECK_EQ(countOther(RECT(BAR_LR), BLUE), 0);
}

TEST("widgets/bargraph: NaN value is not redrawn in every frame") {
    set(V_A, Value(NAN, VALUE_TYPE_FLOAT));
    noLines();
    show(PAGE_ID_W_BAR);
    CHECK_EQ(framesWithDrawing(5), 0);
}

////////////////////////////////////////////////////////////////////////////////
// YTGraph (native YT data provider w_yt)

namespace {

void ytStatic(float value) {
    auto &yt = g_native.yt;
    yt.method = YT_GRAPH_UPDATE_METHOD_STATIC;
    yt.values.assign(400, value);
    yt.size = 400;
    yt.position = 0;
    // STATIC: y = h - 1 - floor(h / 2 + (value + offset) * (h - 1) / div / vertDivisions)
    yt.div = 25;
    yt.offset = 0;
    yt.vertDivisions = 4;
    yt.horzDivisions = 4;
}

void ytScroll(std::initializer_list<std::pair<int, float>> ranges, uint32_t position) {
    auto &yt = g_native.yt;
    yt.method = YT_GRAPH_UPDATE_METHOD_SCROLL;
    yt.values.assign(position + 1, NAN);
    for (auto &range : ranges) {
        for (uint32_t i = range.first; i <= position; i++) {
            yt.values[i] = range.second;
        }
    }
    yt.size = position + 1;
    yt.position = position;
    yt.min = 0;
    yt.max = 100;
}

} // namespace

TEST("widgets/ytgraph: static graph draws the grid and the values") {
    ytStatic(30.0f);
    show(PAGE_ID_W_YT);
    // value 30: y = 199 - floor(100 + 30 * 1.99) = 40
    CHECK_EQ(rgb(YT[0] + 30, YT[1] + 40), RED);
    CHECK_EQ(rgb(YT[0] + 30, YT[1] + 30), BLACK);
    // grid: 4 divisions in both directions
    CHECK_EQ(rgb(YT[0] + 100, YT[1] + 20), GRAY);
    CHECK_EQ(rgb(YT[0] + 30, YT[1] + 50), GRAY);
}

TEST("widgets/ytgraph: static graph is redrawn when the position changes") {
    ytStatic(30.0f);
    for (int i = 200; i < 400; i++) {
        g_native.yt.values[i] = 75.0f;
    }
    show(PAGE_ID_W_YT);
    CHECK_EQ(rgb(YT[0] + 30, YT[1] + 40), RED);
    g_native.yt.position = 200;
    harness::frame();
    CHECK(drawn());
    // value 75 at the left now: y = 199 - floor(100 + 75 * 1.99) = -50, out of the graph
    CHECK_EQ(rgb(YT[0] + 30, YT[1] + 40), BLACK);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/ytgraph: a graph outside of a list is not redrawn in every frame") {
    // the widget cursor is -1 outside of a list, iChannel is an int
    // (NEW-widgets-1: a uint8_t, WIDGET_STATE compared 255 with -1 in every frame)
    ytStatic(30.0f);
    show(PAGE_ID_W_YT);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/ytgraph: a graph in a list is not redrawn without a change") {
    set(V_STRS, stringArray({ "a" }));
    ytStatic(30.0f);
    show(PAGE_ID_W_YT_LIST);
    CHECK_EQ(rgb(YT[0] + 30, YT[1] + 40), RED);
    CHECK_EQ(framesWithDrawing(3), 0);
    g_native.yt.values.assign(400, 0.0f);
    g_native.yt.refreshCounter++;
    harness::frame();
    CHECK(drawn());
    // value 0: y = 199 - floor(100) = 99
    CHECK_EQ(rgb(YT[0] + 30, YT[1] + 99), RED);
    CHECK_EQ(rgb(YT[0] + 30, YT[1] + 40), BLACK);
}

TEST("widgets/ytgraph: a touch of a static graph goes to the graph") {
    ytStatic(30.0f);
    show(PAGE_ID_W_YT);
    tap(YT[0] + 200, YT[1] + 100);
    // hasOnTouch used g_widgetCursor, which is the root widget when the touch
    // is processed (W1)
    CHECK(g_native.yt.touchDrags > 0);
}

TEST("widgets/ytgraph: NaN div is not redrawn in every frame") {
    // in a list and without values (see W13); a NaN div is the same NaN
    set(V_STRS, stringArray({ "a" }));
    ytStatic(30.0f);
    g_native.yt.size = 0;
    g_native.yt.div = NAN;
    show(PAGE_ID_W_YT_LIST);
    CHECK_EQ(framesWithDrawing(5), 0);
}

TEST_BUG("widgets/ytgraph: NaN div draws no trace", "W13@san") {
    // floor(NaN) converted to int, then h - 1 - INT_MIN overflows
    set(V_STRS, stringArray({ "a" }));
    ytStatic(30.0f);
    g_native.yt.div = NAN;
    show(PAGE_ID_W_YT_LIST);
    CHECK_EQ(count(YT[0], YT[1], YT[2], YT[3], RED), 0);
}

TEST("widgets/ytgraph: scrolling graph draws the newest value on the right") {
    // values 0..30 are 50 (y = 199 - round(199 * 0.5) = 99)
    ytScroll({ { 0, 50.0f } }, 30);
    show(PAGE_ID_W_YT);
    CHECK_EQ(rgb(YT[0] + 399, YT[1] + 99), RED);
    CHECK_EQ(rgb(YT[0] + 390, YT[1] + 99), RED);
    CHECK_EQ(rgb(YT[0] + 390, YT[1] + 120), BLACK);
}

TEST("widgets/ytgraph: scrolling graph moves when a new value comes") {
    ytScroll({ { 0, 50.0f } }, 30);
    show(PAGE_ID_W_YT);
    // value 31 is 25: y = 199 - round(199 * 0.25) = 149
    g_native.yt.values.push_back(25.0f);
    g_native.yt.size = 32;
    g_native.yt.position = 31;
    harness::frame();
    CHECK(drawn());
    CHECK_EQ(rgb(YT[0] + 399, YT[1] + 149), RED);
    CHECK_EQ(rgb(YT[0] + 398, YT[1] + 99), RED);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST_BUG("widgets/ytgraph: after a gap the scrolling graph starts at the value, not at the top", "W14") {
    // 0..9: 50, 10..19: NaN, 20..30: 75 (y = 199 - round(199 * 0.75) = 50);
    // position 20 is in column 399 - (30 - 20) = 389
    ytScroll({ { 0, 50.0f }, { 10, NAN }, { 20, 75.0f } }, 30);
    show(PAGE_ID_W_YT);
    CHECK_EQ(rgb(YT[0] + 389, YT[1] + 50), RED);
    // no vertical line from the top edge down to the value
    CHECK_EQ(countOther(YT[0] + 389, YT[1] + 1, 1, 40, BLACK), 0);
}

////////////////////////////////////////////////////////////////////////////////
// ListGraph (flow arrays)

namespace {

void listGraph(Value y1) {
    set(V_A, doubleArray({ 1, 1, 1 }));   // length of the graph
    set(V_B, doubleArray({ 1, 1, 1 }));   // dwell: 3 equal parts of 100 px
    set(V_C, y1);
    set(V_D, doubleArray({}));             // no y2
    set(V_INT, intValue(-3));              // no cursor row
}

} // namespace

TEST("widgets/lgraph: the list is drawn as steps") {
    // y1 0, 5, 10: min 0, max 10 (getArrayMinMax)
    listGraph(doubleArray({ 0, 5, 10 }));
    show(PAGE_ID_W_LGRAPH);
    // value 5: y = 149 - int(5 * 150 / 10) = 74
    CHECK_EQ(rgb(LG[0] + 150, LG[1] + 74), RED);
    // value 0: bottom row, value 10: top row (clamped)
    CHECK_EQ(rgb(LG[0] + 50, LG[1] + 149), RED);
    CHECK_EQ(rgb(LG[0] + 250, LG[1] + 0), RED);
    CHECK_EQ(rgb(LG[0] + 150, LG[1] + 30), BLACK);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/lgraph: a touch moves the cursor to the touched step") {
    listGraph(doubleArray({ 0, 5, 10 }));
    set(V_INT, intValue(1));
    show(PAGE_ID_W_LGRAPH);
    tap(LG[0] + 250, LG[1] + 75);
    // step 2, the column (cursor % 3) stays
    CHECK_EQ(get(V_INT).getInt(), 2 * 3 + 1);
    // cursor row background
    CHECK_EQ(rgb(LG[0] + 220, LG[1] + 20), GRAY);
}

TEST("widgets/lgraph: a change of the y1 list redraws the graph") {
    listGraph(doubleArray({ 0, 5, 10 }));
    show(PAGE_ID_W_LGRAPH);
    CHECK_EQ(rgb(LG[0] + 150, LG[1] + 74), RED);
    // new array with the same length, data (the length list) and cursor are the same
    set(V_C, doubleArray({ 0, 10, 10 }));
    harness::frame(2);
    CHECK_EQ(rgb(LG[0] + 150, LG[1] + 74), BLACK);
    CHECK_EQ(rgb(LG[0] + 150, LG[1] + 0), RED);
}

TEST("widgets/lgraph: all-zero list is drawn at the bottom") {
    // getArrayMinMax gives 0 .. 1 for an empty range
    listGraph(doubleArray({ 0, 0, 0 }));
    show(PAGE_ID_W_LGRAPH);
    CHECK_EQ(rgb(LG[0] + 150, LG[1] + 149), RED);
}

////////////////////////////////////////////////////////////////////////////////
// Progress

TEST("widgets/progress: horizontal and vertical part of the value") {
    set(V_A, intValue(25));
    show(PAGE_ID_W_PROGRESS);
    // 25 % of 200 px
    CHECK_EQ(countOther(PR_H[0], PR_H[1], 50, PR_H[3], GREEN), 0);
    CHECK_EQ(countOther(PR_H[0] + 50, PR_H[1], 150, PR_H[3], BLUE), 0);
    // vertical: from the bottom
    CHECK_EQ(countOther(PR_V[0], PR_V[1] + 150, PR_V[2], 50, GREEN), 0);
    CHECK_EQ(countOther(PR_V[0], PR_V[1], PR_V[2], 150, BLUE), 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    set(V_A, intValue(75));
    harness::frame();
    CHECK(drawn());
    CHECK_EQ(countOther(PR_H[0], PR_H[1], 150, PR_H[3], GREEN), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/progress: values outside of the range are clamped") {
    set(V_A, intValue(150));
    show(PAGE_ID_W_PROGRESS);
    CHECK_EQ(countOther(RECT(PR_H), GREEN), 0);
    set(V_A, intValue(-20));
    harness::frame();
    CHECK_EQ(countOther(RECT(PR_H), BLUE), 0);
}

TEST_BUG("widgets/progress: a value far above max fills the whole bar", "W13") {
    // (value - min) * 100 / (max - min) converted to int: INT_MIN on x86
    set(V_A, Value(1e10f, VALUE_TYPE_FLOAT));
    show(PAGE_ID_W_PROGRESS);
    CHECK_EQ(countOther(RECT(PR_H), GREEN), 0);
}
