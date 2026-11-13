/*
 * eez-framework tests: Container (static, horizontal, vertical layout), List,
 * Grid, Select, UserWidget (pages w_cont, w_cont_tl, w_list, w_select, w_uw).
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "wtest.h"

#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/private.h>
#include <eez/gui/widgets/containers/user_widget.h>

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

// w_cont
const int C_STATIC[] = { 10, 10, 300, 100 };
const int C_H[] = { 10, 150, 300, 50 };    // children 50 px wide: red (visible w_bool2), green
const int C_V[] = { 400, 150, 100, 200 };  // children 50 px high: red (visible w_bool2), green

// w_cont_tl, timeline position 1 s
const int CT_GROW[] = { 10, 10, 300, 50 };   // grows to 400 px; children red 200 (w_bool2), green 150
const int CT_CHILD[] = { 10, 100, 300, 50 }; // children red 50 -> 100 px, green 50

// w_list
const int L_V[] = { 10, 10, 200, 100 };   // items 200x50 green, gap 5, list blue
const int L_H[] = { 300, 10, 300, 50 };   // items 60x50 texts
const int G[] = { 10, 150, 200, 100 };    // items 100x50 green, 2 per row

// w_select
const int S[] = { 10, 10, 200, 50 };

// w_uw
const int UW_B0[] = { 10, 10, 150, 50 };
const int UW_B1[] = { 200, 10, 150, 50 };
const int UW_T0[] = { 10, 100, 150, 50 };
const int UW_T1[] = { 200, 100, 150, 50 };

#define RECT(R) R[0], R[1], R[2], R[3]
#define CENTER(R) R[0] + R[2] / 2, R[1] + R[3] / 2

void timelineAtEnd(int pageId) {
    auto flowState = pageFlowState(pageId);
    REQUIRE(flowState != nullptr);
    flowState->timelinePosition = 1.0f;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// Container

TEST("widgets/container: background and children") {
    set(V_BOOL, boolValue(true));
    show(PAGE_ID_W_CONT);
    CHECK_EQ(rgb(C_STATIC[0] + 5, C_STATIC[1] + 5), GRAY);
    CHECK_EQ(rgb(C_STATIC[0] + 200, C_STATIC[1] + 80), GRAY);
    // child text (w_txt: yellow on gray) at 10, 10 in the container
    CHECK(count(C_STATIC[0] + 10, C_STATIC[1] + 10, 100, 40, YELLOW) > 0);
    CHECK_EQ(rgb(C_STATIC[0] + C_STATIC[2] + 2, C_STATIC[1] + 5), BLACK);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/container: hidden container hides its children") {
    set(V_BOOL, boolValue(true));
    show(PAGE_ID_W_CONT);
    set(V_BOOL, boolValue(false));
    harness::frame();
    CHECK_EQ(countOther(RECT(C_STATIC), BLACK), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
    set(V_BOOL, boolValue(true));
    harness::frame();
    CHECK(count(C_STATIC[0] + 10, C_STATIC[1] + 10, 100, 40, YELLOW) > 0);
}

TEST("widgets/container: horizontal layout puts the children one after another") {
    set(V_BOOL2, boolValue(true));
    show(PAGE_ID_W_CONT);
    CHECK_EQ(countOther(C_H[0], C_H[1], 50, 50, RED), 0);
    CHECK_EQ(countOther(C_H[0] + 50, C_H[1], 50, 50, GREEN), 0);
    CHECK_EQ(countOther(C_H[0] + 100, C_H[1], 200, 50, GRAY), 0);
    CHECK_EQ(countOther(C_V[0], C_V[1], 100, 50, RED), 0);
    CHECK_EQ(countOther(C_V[0], C_V[1] + 50, 100, 50, GREEN), 0);
    CHECK_EQ(countOther(C_V[0], C_V[1] + 100, 100, 100, GRAY), 0);
}

TEST("widgets/container: horizontal and vertical layout close the place of a hidden child") {
    set(V_BOOL2, boolValue(true));
    show(PAGE_ID_W_CONT);
    set(V_BOOL2, boolValue(false));
    harness::frame();
    CHECK_EQ(countOther(C_H[0], C_H[1], 50, 50, GREEN), 0);
    CHECK_EQ(countOther(C_H[0] + 50, C_H[1], 250, 50, GRAY), 0);
    CHECK_EQ(countOther(C_V[0], C_V[1], 100, 50, GREEN), 0);
    CHECK_EQ(countOther(C_V[0], C_V[1] + 50, 100, 150, GRAY), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
    set(V_BOOL2, boolValue(true));
    harness::frame();
    CHECK_EQ(countOther(C_H[0] + 50, C_H[1], 50, 50, GREEN), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST_BUG("widgets/container: horizontal layout uses the animated width of a child", "W19") {
    timelineAtEnd(PAGE_ID_W_CONT_TL);
    show(PAGE_ID_W_CONT_TL);
    // the red child is 100 px wide at the end of its timeline
    CHECK_EQ(countOther(CT_CHILD[0], CT_CHILD[1], 100, 50, RED), 0);
    CHECK_EQ(countOther(CT_CHILD[0] + 100, CT_CHILD[1], 50, 50, GREEN), 0);
}

TEST("widgets/container: the rest of a grown horizontal container is cleared") {
    set(V_BOOL2, boolValue(true));
    timelineAtEnd(PAGE_ID_W_CONT_TL);
    show(PAGE_ID_W_CONT_TL);
    // 400 px wide: red 200, green 150, gray 50
    CHECK_EQ(countOther(CT_GROW[0] + 360, CT_GROW[1], 40, 50, GRAY), 0);
    set(V_BOOL2, boolValue(false));
    harness::frame();
    // green moved to the start; the place where it was is cleared up to the
    // grown width (the container is drawn again when its layout changes, part
    // of W19 fixed by the row layout, gui/layout.h)
    CHECK_EQ(countOther(CT_GROW[0], CT_GROW[1], 150, 50, GREEN), 0);
    CHECK_EQ(countOther(CT_GROW[0] + 150, CT_GROW[1], 250, 50, GRAY), 0);
}

////////////////////////////////////////////////////////////////////////////////
// List, Grid

TEST("widgets/list: items one below the other, clipped to the list") {
    set(V_A, intArray({ 1, 2, 3, 4 }));
    show(PAGE_ID_W_LIST);
    // (the list is scrollable: scroll indicator on the right)
    CHECK_EQ(countOther(L_V[0], L_V[1], 190, 50, GREEN), 0);
    CHECK_EQ(countOther(L_V[0], L_V[1] + 55, 190, 45, GREEN), 0);
    CHECK_EQ(rgb(L_V[0] + 10, L_V[1] + 100), BLACK);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/list: fewer items clear the place of the removed ones") {
    set(V_A, intArray({ 1, 2 }));
    show(PAGE_ID_W_LIST);
    set(V_A, intArray({ 1 }));
    harness::frame();
    // after the last item: the list background
    CHECK_EQ(countOther(L_V[0], L_V[1] + 50, 200, 50, BLUE), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST_BUG("widgets/list: an empty list with a gap fills itself and only itself", "W11") {
    set(V_A, intArray({}));
    show(PAGE_ID_W_LIST);
    // offset = n * h + (n - 1) * gap = -5 for n == 0
    CHECK_EQ(countOther(L_V[0], L_V[1], 200, 100, BLUE), 0);
    CHECK_EQ(countOther(L_V[0], L_V[1] - 5, 200, 5, BLACK), 0);
}

TEST("widgets/list: horizontal list with the item index in the expression") {
    set(V_STRS, stringArray({ "a", "b", "c" }));
    show(PAGE_ID_W_LIST);
    for (int i = 0; i < 3; i++) {
        CHECK(count(L_H[0] + i * 60, L_H[1], 60, 50, YELLOW) > 0);
    }
    CHECK_EQ(countOther(L_H[0] + 180, L_H[1], 120, 50, BLUE), 0);
    set(V_STRS, stringArray({ "a", "x", "c" }));
    auto before = block(L_H[0] + 60, L_H[1], 60, 50);
    harness::frame();
    CHECK(diffBlock(before, L_H[0] + 60, L_H[1], 60, 50) > 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/grid: items in rows") {
    set(V_C, intArray({ 1, 2, 3, 4 }));
    show(PAGE_ID_W_LIST);
    CHECK_EQ(countOther(RECT(G), GREEN), 0);
    set(V_C, intArray({ 1, 2, 3 }));
    harness::frame();
    CHECK_EQ(countOther(G[0], G[1], 200, 50, GREEN), 0);
    CHECK_EQ(countOther(G[0], G[1] + 50, 100, 50, GREEN), 0);
}

TEST_BUG("widgets/grid: fewer items clear the place of the removed ones", "W11") {
    set(V_C, intArray({ 1, 2, 3, 4 }));
    show(PAGE_ID_W_LIST);
    set(V_C, intArray({ 1, 2, 3 }));
    harness::frame();
    // the last cell: as on a full redraw (the grid draws no background)
    CHECK_EQ(countOther(G[0] + 100, G[1] + 50, 100, 50, BLACK), 0);
}

////////////////////////////////////////////////////////////////////////////////
// Select

TEST("widgets/select: shows the child of the index") {
    set(V_INT, intValue(0));
    show(PAGE_ID_W_SELECT);
    CHECK_EQ(countOther(RECT(S), RED), 0);
    set(V_INT, intValue(1));
    harness::frame();
    CHECK_EQ(countOther(RECT(S), GREEN), 0);
    set(V_INT, intValue(2));
    harness::frame();
    CHECK_EQ(countOther(RECT(S), BLUE), 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/select: an index outside of the children shows the first child") {
    // no signal of the wrong index (review C5)
    set(V_INT, intValue(7));
    show(PAGE_ID_W_SELECT);
    CHECK_EQ(countOther(RECT(S), RED), 0);
    set(V_INT, intValue(-1));
    harness::frame();
    CHECK_EQ(countOther(RECT(S), RED), 0);
}

////////////////////////////////////////////////////////////////////////////////
// UserWidget

TEST("widgets/userwidget: each instance draws the layout with its property values") {
    show(PAGE_ID_W_UW);
    CHECK(count(RECT(UW_T0), YELLOW) > 0);
    CHECK(count(RECT(UW_T1), YELLOW) > 0);
    CHECK(diffBlocks(UW_T0[0], UW_T0[1], UW_T1[0], UW_T1[1], UW_T0[2], UW_T0[3]) > 0);
    CHECK_EQ(rgb(UW_B0[0] + 2, UW_B0[1] + 2), BLUE);
    CHECK_EQ(diffBlocks(UW_B0[0], UW_B0[1], UW_B1[0], UW_B1[1], UW_B0[2], UW_B0[3]), 0);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/userwidget: a tap on a button in the layout executes its action") {
    show(PAGE_ID_W_UW);
    tap(CENTER(UW_B1));
    CHECK_EQ(g_native.clicks, 1);
}

TEST_BUG("widgets/userwidget: only the pressed instance is drawn as active", "W21") {
    show(PAGE_ID_W_UW);
    press(CENTER(UW_B1));
    harness::frame();
    CHECK_EQ(rgb(UW_B1[0] + 2, UW_B1[1] + 2), RED);
    // the same layout widget, cursor and iterators: equal WidgetCursors
    CHECK_EQ(rgb(UW_B0[0] + 2, UW_B0[1] + 2), BLUE);
    release();
}

namespace {

// second copy of the main assets, as external assets (e.g. a module)
Assets *g_externalAssets;

int resolveExternalPage(int pageId, Assets **assets) {
    *assets = g_externalAssets;
    return PAGE_ID_W_UW_TEXT;
}

} // namespace

TEST_BUG("widgets/userwidget: the widgets after a user widget with an external layout use their own assets", "W9") {
    auto header = (Header *)assets;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
    size_t size = offsetof(Assets, settings) + header->decompressedSize;
#pragma GCC diagnostic pop
    g_externalAssets = (Assets *)calloc(1, size);
    int err = 0;
    REQUIRE(decompressAssetsData(assets, sizeof(assets), g_externalAssets, (uint32_t)size, &err));
    g_externalAssets->external = 1;
    g_hooks.resolveExternalPage = resolveExternalPage;
    // the second text user widget gets its layout from the external assets
    ((UserWidgetWidget *)pageWidget(PAGE_ID_W_UW, 3))->layout = -1;

    show(PAGE_ID_W_UW);
    REQUIRE(g_native.probeCalls > 0);
    // the probe text after the user widgets is a widget of the main assets
    CHECK(g_native.probeAssets == g_mainAssets);
}

////////////////////////////////////////////////////////////////////////////////
// List data evaluation, state buffer overflow

TEST("widgets/list: data from an expression with a native variable") {
    g_native.counterArrSize = 2;
    show(PAGE_ID_W_LIST_COUNT);
    CHECK_EQ(countOther(10, 10, 200, 100, GREEN), 0);
    g_native.counterArrSize = 1;
    harness::frame();
    CHECK_EQ(countOther(10, 10, 200, 50, GREEN), 0);
    CHECK_EQ(countOther(10, 60, 200, 50, BLUE), 0);
}

TEST("widgets/list: the data of a list is evaluated once per frame") {
    g_native.counterArrSize = 2;
    show(PAGE_ID_W_LIST_COUNT);
    int before = g_native.counterArrGets;
    harness::frame(5);
    // COUNT only: YT_DATA_GET_SIZE is asked for a new state or after a change
    // (WP2: both evaluated the expression in every frame)
    CHECK(g_native.counterArrGets - before <= 5);
}

TEST("widgets/widget: widgets that do not fit into the state buffer are skipped") {
    // 80 x 48 cells of 10x10 px: more than 128 KB of widget states
    Value cells = Value::makeArrayRef(80 * 48, flow::defs_v3::ARRAY_TYPE_INTEGER, 0xce11ce11);
    for (int i = 0; i < 80 * 48; i++) {
        cells.getArray()->values[i] = intValue(i);
    }
    set(V_A, cells);
    show(PAGE_ID_W_MANY);
    // the first cells are drawn, the last ones are not
    CHECK_EQ(countOther(0, 0, 10, 10, GREEN), 0);
    CHECK_EQ(countOther(790, 470, 10, 10, BLACK), 0);
    harness::frame(3);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_MANY);
}

TEST("widgets/container: right-to-left mirrors the static and the horizontal layout") {
    // set by the SetPageDirection flow action
    g_isRTL = true;
    set(V_BOOL, boolValue(true));
    set(V_BOOL2, boolValue(true));
    show(PAGE_ID_W_CONT);
    // static container: 800 - (10 + 300) = 490, its text: 300 - (10 + 100) = 190 in it
    CHECK_EQ(rgb(490 + 5, C_STATIC[1] + 5), GRAY);
    CHECK_EQ(rgb(C_STATIC[0] + 5, C_STATIC[1] + 5), BLACK);
    CHECK(count(490 + 190, C_STATIC[1] + 10, 100, 40, YELLOW) > 0);
    // horizontal container: first child on the right
    CHECK_EQ(countOther(490 + 250, C_H[1], 50, 50, RED), 0);
    CHECK_EQ(countOther(490 + 200, C_H[1], 50, 50, GREEN), 0);
    CHECK_EQ(countOther(490, C_H[1], 200, 50, GRAY), 0);
}
