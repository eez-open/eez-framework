/*
 * eez-framework tests: Rectangle, Bitmap, visibility, the per-frame work of
 * hidden widgets, the widget state machinery of widget.cpp (pages w_rect,
 * w_overlap, w_timeline, w_badtype).
 */

#include <math.h>

#include "wtest.h"

#include <eez/flow/flow.h>
#include <eez/flow/private.h>

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

// w_rect
const int R_RED[] = { 10, 10, 100, 50 };
const int R_VIS[] = { 150, 10, 100, 50 };
const int R_BMP[] = { 300, 10, 40, 20 };

// w_overlap: text (gray) under a blue rectangle
const int O_TEXT[] = { 10, 10, 150, 60 };
const int O_RECT[] = { 60, 40, 150, 60 };

// w_timeline
const int TL_MOVE[] = { 10, 10, 50, 50 };   // left: 10 -> 210 in 1 s
const int TL_FADE[] = { 10, 100, 50, 50 };  // opacity: 1 -> 0 (easeOutBack) in 1 s

#define RECT(R) R[0], R[1], R[2], R[3]

void setTimelinePosition(int pageId, float position) {
    auto flowState = pageFlowState(pageId);
    REQUIRE(flowState != nullptr);
    flowState->timelinePosition = position;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// Rectangle, Bitmap, visibility

TEST("widgets/rectangle: filled with the background color of its style") {
    show(PAGE_ID_W_RECT);
    CHECK_EQ(countOther(RECT(R_RED), RED), 0);
    CHECK_EQ(rgb(R_RED[0] - 1, R_RED[1]), BLACK);
    CHECK_EQ(rgb(R_RED[0] + R_RED[2], R_RED[1] + R_RED[3]), BLACK);
    CHECK_EQ(framesWithDrawing(5), 0);
}

TEST("widgets/bitmap: drawn at the widget position") {
    show(PAGE_ID_W_RECT);
    // w_bmp: 40x20, left half red, right half blue
    CHECK_EQ(countOther(R_BMP[0], R_BMP[1], 20, 20, RED), 0);
    CHECK_EQ(countOther(R_BMP[0] + 20, R_BMP[1], 20, 20, BLUE), 0);
    CHECK_EQ(rgb(R_BMP[0] + 41, R_BMP[1] + 10), BLACK);
}

TEST("widgets/visible: a hidden widget leaves the page background") {
    set(V_BOOL, boolValue(true));
    show(PAGE_ID_W_RECT);
    CHECK_EQ(countOther(RECT(R_VIS), GREEN), 0);
    set(V_BOOL, boolValue(false));
    harness::frame();
    CHECK(drawn());
    CHECK_EQ(countOther(RECT(R_VIS), BLACK), 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
    set(V_BOOL, boolValue(true));
    harness::frame();
    CHECK_EQ(countOther(RECT(R_VIS), GREEN), 0);
}

TEST("widgets/visible: children of a hidden container are not evaluated") {
    show(PAGE_ID_W_RECT);
    int before = g_native.counterB;
    harness::frame(5);
    CHECK_EQ(g_native.counterB, before);
    CHECK_EQ(g_native.counterB, 0);
}

TEST("widgets/visible: data of a hidden widget is not evaluated in every frame") {
    show(PAGE_ID_W_RECT);
    int before = g_native.counterA;
    harness::frame(5);
    // a hidden widget returns from updateState after its visibility
    // (WIDGET_STATE_START; WP2: the whole updateState ran in every frame)
    CHECK_EQ(g_native.counterA - before, 0);
}

////////////////////////////////////////////////////////////////////////////////
// overlapping children (page with allowOverlappingChildren)

TEST("widgets/overlap: the widget on top is repainted over a changed widget below it") {
    set(V_A, str("one"));
    show(PAGE_ID_W_OVERLAP);
    // overlapped area: blue rectangle on top
    CHECK_EQ(countOther(O_RECT[0], O_RECT[1], O_TEXT[0] + O_TEXT[2] - O_RECT[0], O_TEXT[1] + O_TEXT[3] - O_RECT[1], BLUE), 0);
    set(V_A, str("a much longer text"));
    harness::frame();
    CHECK(drawn());
    CHECK_EQ(countOther(O_RECT[0], O_RECT[1], O_TEXT[0] + O_TEXT[2] - O_RECT[0], O_TEXT[1] + O_TEXT[3] - O_RECT[1], BLUE), 0);
    CHECK_EQ(rgb(O_TEXT[0] + 2, O_TEXT[1] + 2), GRAY);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// timeline

TEST("widgets/timeline: the widget is at the position of the timeline") {
    setTimelinePosition(PAGE_ID_W_TIMELINE, 0.5f);
    show(PAGE_ID_W_TIMELINE);
    // left 10 + 0.5 * (210 - 10) = 110
    CHECK_EQ(countOther(110, TL_MOVE[1], TL_MOVE[2], TL_MOVE[3], RED), 0);
    CHECK_EQ(countOther(TL_MOVE[0], TL_MOVE[1], 50, TL_MOVE[3], BLACK), 0);
    setTimelinePosition(PAGE_ID_W_TIMELINE, 1.0f);
    harness::frame(2);
    CHECK_EQ(countOther(210, TL_MOVE[1], TL_MOVE[2], TL_MOVE[3], RED), 0);
    CHECK_EQ(countOther(110, TL_MOVE[1], TL_MOVE[2], TL_MOVE[3], BLACK), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/timeline: opacity at the end of the keyframe") {
    setTimelinePosition(PAGE_ID_W_TIMELINE, 0.0f);
    show(PAGE_ID_W_TIMELINE);
    CHECK_EQ(countOther(RECT(TL_FADE), GREEN), 0);
    setTimelinePosition(PAGE_ID_W_TIMELINE, 1.0f);
    harness::frame(2);
    // opacity 0: not drawn
    CHECK_EQ(countOther(RECT(TL_FADE), BLACK), 0);
}

TEST_BUG("widgets/timeline: opacity overshoot of the easing function is clamped", "W18") {
    // easeOutBack(0.6) = 1.0994: opacity 1 + 1.0994 * (0 - 1) = -0.0994, which
    // (uint8_t)roundf(255 * opacity) turns into 231 instead of 0
    setTimelinePosition(PAGE_ID_W_TIMELINE, 0.6f);
    show(PAGE_ID_W_TIMELINE);
    CHECK_EQ(countOther(RECT(TL_FADE), BLACK), 0);
}

TEST_BUG("widgets/timeline: a page with a timeline is drawn while the flow is stopped", "W18") {
    show(PAGE_ID_W_TIMELINE);
    flow::stop();
    // the flow stops in the next tick, then the page has no flow state
    harness::frame(3);
    CHECK(flow::isFlowStopped());
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_TIMELINE);
}

////////////////////////////////////////////////////////////////////////////////
// widget type from the assets

TEST_BUG("widgets/widget: a widget type unknown to the framework is ignored", "W17") {
    // e.g. module assets built with a newer Studio
    pageWidget(PAGE_ID_W_BADTYPE, 0)->type = 34;
    show(PAGE_ID_W_BADTYPE);
    CHECK_EQ(countOther(10, 10, 100, 50, BLACK), 0);
}
