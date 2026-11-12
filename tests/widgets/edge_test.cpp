/*
 * eez-framework tests: edge data of the widgets (empty arrays, NaN and inf,
 * out of range indexes, data of an unexpected type). Behaviour that is not a
 * known bug is pinned as it is.
 */

#include <math.h>

#include "wtest.h"

#include <eez/core/unit.h>

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

const int T_DATA[] = { 10, 60, 200, 40 };   // w_text, data w_a
const int DD_ALL[] = { 10, 10, 200, 40 };   // w_dd, data w_a
const int BG[] = { 10, 10, 300, 50 };       // w_bgroup
const int LG[] = { 10, 10, 300, 150 };      // w_lgraph
const int QR[] = { 10, 10, 200, 200 };      // w_qr
const int L_H[] = { 300, 10, 300, 50 };     // w_list, data w_strs
const int CB[] = { 10, 10, 200, 40 };       // w_check
const int RA0[] = { 300, 10, 200, 40 };
const int SW[] = { 10, 10, 100, 40 };       // w_switch
const int DL[] = { 10, 10, 200, 40 };       // w_ddl

#define RECT(R) R[0], R[1], R[2], R[3]

} // namespace

TEST("widgets/text: NaN data is not redrawn in every frame") {
    set(V_A, Value(NAN, VALUE_TYPE_FLOAT));
    show(PAGE_ID_W_TEXT);
    CHECK_EQ(framesWithDrawing(5), 0);
}

TEST("widgets/dd: infinity is not redrawn without a change") {
    set(V_A, Value(INFINITY, VALUE_TYPE_FLOAT));
    // the text of infinity is INFINITY_SYMBOL (U+0091), which has no glyph in
    // the font of the test project: only the background is drawn
    char text[64];
    get(V_A).toText(text, sizeof(text));
    CHECK_STR(text, INFINITY_SYMBOL);
    show(PAGE_ID_W_DD);
    CHECK_EQ(countOther(RECT(DD_ALL), GRAY), 0);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/dd: a value of a non-float type with a unit option is not drawn") {
    set(V_A, intValue(42));
    show(PAGE_ID_W_DD);
    // option 4 (unit) of an integer: empty
    CHECK_EQ(countOther(10, 210, 200, 40, GRAY), 0);
    CHECK(count(RECT(DD_ALL), YELLOW) > 0);
}

TEST("widgets/bgroup: a selected index outside of the buttons selects none") {
    set(V_A, stringArray({ "a", "b", "c" }));
    set(V_INT, intValue(5));
    show(PAGE_ID_W_BGROUP);
    CHECK_EQ(rgb(BG[0] + 2, BG[1] + 2), BLUE);
    CHECK_EQ(rgb(BG[0] + 102, BG[1] + 2), BLUE);
    CHECK_EQ(rgb(BG[0] + 202, BG[1] + 2), BLUE);
}

TEST("widgets/lgraph: an empty dwell list draws only the background") {
    set(V_A, doubleArray({ 1, 1, 1 }));
    set(V_B, doubleArray({}));
    set(V_C, doubleArray({ 0, 5, 10 }));
    set(V_D, doubleArray({}));
    set(V_INT, intValue(0));
    show(PAGE_ID_W_LGRAPH);
    CHECK_EQ(countOther(RECT(LG), BLACK), 0);
}

TEST("widgets/lgraph: data that is not an array draws only the background") {
    set(V_A, Value());
    set(V_B, Value());
    set(V_C, Value());
    set(V_D, Value());
    set(V_INT, intValue(0));
    show(PAGE_ID_W_LGRAPH);
    CHECK_EQ(countOther(RECT(LG), BLACK), 0);
}

TEST("widgets/qrcode: an empty string draws the smallest code") {
    set(V_A, str(""));
    show(PAGE_ID_W_QR);
    CHECK(count(RECT(QR), TEXT) > 0);
}

TEST("widgets/list: data that is not an array shows no items") {
    set(V_STRS, Value());
    show(PAGE_ID_W_LIST);
    // only the list background (gap 0)
    CHECK_EQ(countOther(RECT(L_H), BLUE), 0);
}

TEST("widgets/checkbox: a number is checked when it is not 0") {
    set(V_BOOL, boolValue(false));
    show(PAGE_ID_W_CHECK);
    auto unchecked = block(RECT(CB));
    set(V_BOOL, boolValue(true));
    harness::frame();
    auto checked = block(RECT(CB));
    CHECK(diffBlock(unchecked, RECT(CB)) > 0);
    // the boolean variable is set to 1 from the flow side: same as true
    set(V_BOOL, intValue(0));
    harness::frame();
    CHECK_EQ(diffBlock(unchecked, RECT(CB)), 0);
    set(V_BOOL, intValue(2));
    harness::frame();
    CHECK_EQ(diffBlock(checked, RECT(CB)), 0);
}

TEST("widgets/radio: undefined data is not checked") {
    set(V_INT, intValue(1));
    show(PAGE_ID_W_CHECK);
    auto unchecked = block(RECT(RA0));
    set(V_INT, Value());
    harness::frame();
    CHECK_EQ(diffBlock(unchecked, RECT(RA0)), 0);
}

TEST("widgets/switch: undefined data is off") {
    set(V_B, intValue(0));
    set(V_C, intValue(100));
    set(V_INT, Value());
    show(PAGE_ID_W_SWITCH);
    CHECK_EQ(rgb(SW[0] + 19, SW[1] + 20), WHITE);
    CHECK_EQ(rgb(SW[0] + 80, SW[1] + 20), GRAY);
}

TEST("widgets/ddl: options that are not an array show no text") {
    set(V_A, Value());
    set(V_INT, intValue(0));
    show(PAGE_ID_W_DDL);
    CHECK_EQ(count(DL[0], DL[1], 150, DL[3], YELLOW), 0);
}

TEST("widgets/text: a very long string is clipped to the widget") {
    std::string s(500, 'W');
    set(V_A, str(s.c_str()));
    show(PAGE_ID_W_TEXT);
    CHECK(count(RECT(T_DATA), YELLOW) > 0);
    // nothing outside of the widget
    CHECK_EQ(countOther(T_DATA[0] + T_DATA[2], T_DATA[1], 50, T_DATA[3], BLACK), 0);
    CHECK_EQ(countOther(0, T_DATA[1], T_DATA[0], T_DATA[3], BLACK), 0);
}
