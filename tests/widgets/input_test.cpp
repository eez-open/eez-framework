/*
 * eez-framework tests: interactive widgets, UpDown, ScrollBar, Input, Roller,
 * Switch, Slider, DropDownList, Checkbox, Radio (pages w_updown, w_sbar,
 * w_input, w_roller, w_switch, w_ddl, w_check).
 */

#include <math.h>

#include "wtest.h"

#include <eez/flow/flow.h>
#include <eez/flow/flow_defs_v3.h>

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

// w_updown: 200x50, buttons 26 px (font height) on both sides
const int UD[] = { 10, 10, 200, 50 };

// w_sbar: 300x40, buttons 40 px, track 220 px, thumb 40 px (min = button size)
const int SB[] = { 10, 10, 300, 40 };

// w_input
const int IN_TEXT[] = { 10, 10, 200, 40 };
const int IN_PW[] = { 10, 60, 200, 40 };
const int IN_NUM[] = { 10, 110, 200, 40 };
const int IN_PW2[] = { 10, 160, 200, 40 };
// w_e in A shown in mA with the precision 0.0005 A, a reference text (w_str)
const int IN_SHOWN[] = { 10, 210, 200, 40 };
const int IN_REF[] = { 250, 210, 200, 40 };

// w_roller: items 26 px high, the selected one in the middle (y + 87 .. y + 113)
const int RO_NAT[] = { 10, 10, 120, 200 };
const int RO_FLOW[] = { 200, 10, 120, 200 };

// w_switch: switch (padding 2) and slider (padding 2)
const int SW[] = { 10, 10, 100, 40 };
const int SL[] = { 10, 100, 300, 40 };

// w_ddl
const int DL[] = { 10, 10, 200, 40 };

// w_check: check boxes and radio buttons (white on black, left aligned)
const int CB[] = { 10, 10, 200, 40 };
const int CB_OV[] = { 10, 60, 200, 40 };
const int RA0[] = { 300, 10, 200, 40 };
const int RA1[] = { 300, 60, 200, 40 };
const int RAS[] = { 300, 110, 200, 40 };

#define RECT(R) R[0], R[1], R[2], R[3]
#define CENTER(R) R[0] + R[2] / 2, R[1] + R[3] / 2

const Widget *g_overrideWidget;
int g_overrideStyle;

int overrideStyleHook(const WidgetCursor &widgetCursor, int styleId) {
    if (g_overrideStyle && widgetCursor.widget == g_overrideWidget) {
        return g_overrideStyle;
    }
    return styleId;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// UpDown

TEST("widgets/updown: the value between the down and up buttons") {
    set(V_INT, intValue(2));
    show(PAGE_ID_W_UPDOWN);
    CHECK_EQ(rgb(UD[0] + 2, UD[1] + 2), BLUE);
    CHECK_EQ(rgb(UD[0] + UD[2] - 3, UD[1] + 2), BLUE);
    CHECK_EQ(rgb(UD[0] + 40, UD[1] + 2), GRAY);
    CHECK(count(UD[0] + 26, UD[1], UD[2] - 52, UD[3], YELLOW) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/updown: a tap on the right half increments, on the left half decrements, within min and max") {
    set(V_INT, intValue(2));
    show(PAGE_ID_W_UPDOWN);
    tap(UD[0] + 150, UD[1] + 25);
    CHECK_EQ(get(V_INT).getInt(), 3);
    tap(UD[0] + 150, UD[1] + 25);
    CHECK_EQ(get(V_INT).getInt(), 3);
    tap(UD[0] + 50, UD[1] + 25);
    CHECK_EQ(get(V_INT).getInt(), 2);
    set(V_INT, intValue(0));
    harness::frame();
    tap(UD[0] + 50, UD[1] + 25);
    CHECK_EQ(get(V_INT).getInt(), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// ScrollBar ($ScrollbarState: numItems, itemsPerPage, positionIncrement, position)

namespace {

void scrollbar(int numItems, int itemsPerPage, int increment, int position) {
    Value value = Value::makeArrayRef(4, flow::defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE, 0x5b5b5b5b);
    value.getArray()->values[0] = intValue(numItems);
    value.getArray()->values[1] = intValue(itemsPerPage);
    value.getArray()->values[2] = intValue(increment);
    value.getArray()->values[3] = intValue(position);
    set(V_SB, value);
}

int position() {
    return get(V_SB).getArray()->values[3].getInt();
}

} // namespace

TEST("widgets/scrollbar: the thumb at the position") {
    scrollbar(100, 10, 1, 0);
    show(PAGE_ID_W_SBAR);
    // track from 50 (x + 40) to 270, thumb 40 px
    CHECK_EQ(rgb(SB[0] + 60, SB[1] + 20), GREEN);
    CHECK_EQ(rgb(SB[0] + 150, SB[1] + 20), GRAY);
    CHECK_EQ(rgb(SB[0] + 5, SB[1] + 5), BLUE);
    CHECK_EQ(framesWithDrawing(3), 0);
    // position 45 of 90: thumb at 40 + 45 * 180 / 90 = 130
    scrollbar(100, 10, 1, 45);
    harness::frame();
    CHECK_EQ(rgb(SB[0] + 150, SB[1] + 20), GREEN);
    CHECK_EQ(rgb(SB[0] + 60, SB[1] + 20), GRAY);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/scrollbar: buttons move by the increment, the track by a page") {
    scrollbar(100, 10, 1, 0);
    show(PAGE_ID_W_SBAR);
    tap(SB[0] + SB[2] - 20, SB[1] + 20);
    CHECK_EQ(position(), 1);
    tap(SB[0] + 20, SB[1] + 20);
    CHECK_EQ(position(), 0);
    tap(SB[0] + 200, SB[1] + 20);
    CHECK_EQ(position(), 10);
    // not below 0
    tap(SB[0] + 20, SB[1] + 20);
    tap(SB[0] + 20, SB[1] + 20);
    tap(SB[0] + 20, SB[1] + 20);
    CHECK_EQ(position(), 7);
}

TEST("widgets/scrollbar: without anything to scroll only the track is drawn") {
    scrollbar(5, 10, 1, 0);
    show(PAGE_ID_W_SBAR);
    CHECK_EQ(countOther(RECT(SB), GRAY), 0);
    tap(SB[0] + SB[2] - 20, SB[1] + 20);
    CHECK_EQ(position(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// Input

namespace {

// strings for the text inputs (a number in a text input is shown as its text,
// see W7)
void inputs(const char *text, const char *password, Value number, const char *password2) {
    set(V_A, str(text));
    set(V_B, str(password));
    set(V_C, number);
    set(V_D, str(password2));
}

} // namespace

TEST("widgets/input: text input shows the string and redraws on change") {
    inputs("abc", "", intValue(0), "");
    show(PAGE_ID_W_INPUT);
    CHECK(count(RECT(IN_TEXT), YELLOW) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    auto before = block(RECT(IN_TEXT));
    set(V_A, str("abcd"));
    harness::frame();
    CHECK(drawn());
    CHECK(diffBlock(before, RECT(IN_TEXT)) > 0);
}

TEST("widgets/input: password shows one * per character") {
    inputs("", "abc", intValue(0), "xyz");
    show(PAGE_ID_W_INPUT);
    CHECK(count(RECT(IN_PW), YELLOW) > 0);
    CHECK_EQ(diffBlocks(IN_PW[0], IN_PW[1], IN_PW2[0], IN_PW2[1], IN_PW[2], IN_PW[3]), 0);
    set(V_D, str("xy"));
    harness::frame();
    CHECK(diffBlocks(IN_PW[0], IN_PW[1], IN_PW2[0], IN_PW2[1], IN_PW[2], IN_PW[3]) > 0);
}

TEST("widgets/input: password shows one * for a non-ASCII character") {
    inputs("", "\xC3\xA9", intValue(0), "a"); // U+00E9, 2 bytes
    show(PAGE_ID_W_INPUT);
    CHECK_EQ(diffBlocks(IN_PW[0], IN_PW[1], IN_PW2[0], IN_PW2[1], IN_PW[2], IN_PW[3]), 0);
}

TEST("widgets/input: number input shows the value") {
    inputs("", "", intValue(2), "");
    show(PAGE_ID_W_INPUT);
    CHECK(count(RECT(IN_NUM), YELLOW) > 0);
    auto two = block(RECT(IN_NUM));
    set(V_C, Value(2.5f, VALUE_TYPE_FLOAT));
    harness::frame();
    CHECK(diffBlock(two, RECT(IN_NUM)) > 0);
}

TEST("widgets/input: a number is shown in the display unit with the decimals of the precision") {
    inputs("", "", intValue(0), "");
    set(V_E, Value(0.75f, UNIT_AMPER));
    show(PAGE_ID_W_INPUT);
    // 0.0005 A is 0.5 mA: one decimal
    set(V_STR, str("750.0 mA"));
    harness::frame(2);
    CHECK_EQ(diffBlocks(IN_SHOWN[0], IN_SHOWN[1], IN_REF[0], IN_REF[1], IN_REF[2], IN_REF[3]), 0);

    set(V_E, Value(0.0125f, UNIT_AMPER));
    set(V_STR, str("12.5 mA"));
    harness::frame(2);
    CHECK_EQ(diffBlocks(IN_SHOWN[0], IN_SHOWN[1], IN_REF[0], IN_REF[1], IN_REF[2], IN_REF[3]), 0);
}

TEST("widgets/input: text input with a number shows the number") {
    // a number is shown as its text (W7: getInputWidgetData made a string ref
    // of the getString() of an integer)
    inputs("", "", intValue(0), "");
    set(V_A, intValue(5));
    show(PAGE_ID_W_INPUT);
    CHECK(count(RECT(IN_TEXT), YELLOW) > 0);
}

////////////////////////////////////////////////////////////////////////////////
// Roller

TEST("widgets/roller: the selected item in the middle in the selected style") {
    set(V_INT, intValue(3));
    show(PAGE_ID_W_ROLLER);
    // selected: yellow, the others gray
    CHECK(count(RO_FLOW[0], RO_FLOW[1] + 87, RO_FLOW[2], 26, YELLOW) > 0);
    CHECK(count(RO_FLOW[0], RO_FLOW[1], RO_FLOW[2], 80, GRAY) > 0);
    CHECK_EQ(count(RO_FLOW[0], RO_FLOW[1], RO_FLOW[2], 80, YELLOW), 0);
    CHECK_EQ(get(V_INT).getInt(), 3);
}

TEST("widgets/roller: drawing does not change the bound flow variable") {
    // render assigns every visible item to the flow variable to evaluate its
    // text, directly (no change notification), and restores it
    g_native.rol = 3;
    set(V_INT, intValue(3));
    show(PAGE_ID_W_ROLLER);
    CHECK_EQ(get(V_INT).getType(), VALUE_TYPE_INT32);
    CHECK_EQ(get(V_INT).getInt(), 3);
    // the items of the flow roller are the items of the native roller (rows
    // of 26 px, the selected item at y + 87), and they differ from each other
    for (int y = 87 - 3 * 26; y + 26 <= 200; y += 26) {
        CHECK_EQ(diffBlocks(RO_NAT[0], RO_NAT[1] + y, RO_FLOW[0], RO_FLOW[1] + y, 120, 26), 0);
    }
    CHECK(diffBlocks(RO_FLOW[0], RO_FLOW[1] + 35, RO_FLOW[0], RO_FLOW[1] + 61, 120, 26) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/roller: drawing restores a double flow variable exactly") {
    // the items are assigned as the declared type (integer)
    set(V_INT, doubleValue(3.0));
    show(PAGE_ID_W_ROLLER);
    CHECK_EQ(get(V_INT).getType(), VALUE_TYPE_DOUBLE);
    CHECK(get(V_INT).getDouble() == 3.0);
}

TEST("widgets/roller: drawing restores the native variable") {
    // a native variable is set to every visible item through SET
    g_native.rol = 3;
    g_native.rolSets = 0;
    show(PAGE_ID_W_ROLLER);
    CHECK(g_native.rolSets > 0);
    CHECK_EQ(g_native.rol, 3);
}

TEST("widgets/roller: dragging up by one item selects the next item") {
    set(V_INT, intValue(3));
    show(PAGE_ID_W_ROLLER);
    press(RO_FLOW[0] + 60, RO_FLOW[1] + 150);
    // more than DISPLAY_HEIGHT / 25 px starts the drag
    harness::touch(RO_FLOW[0] + 60, RO_FLOW[1] + 150 - 26);
    harness::frame(30);
    release();
    harness::frame(100);
    CHECK_EQ(get(V_INT).getInt(), 4);
    CHECK(count(RO_FLOW[0], RO_FLOW[1] + 87, RO_FLOW[2], 26, YELLOW) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/roller: a tap below the middle selects the next item") {
    // the touch up event has the position of the touch
    // (platform/simulator/touch.cpp on CM5)
    set(V_INT, intValue(3));
    show(PAGE_ID_W_ROLLER);
    tap(RO_FLOW[0] + 60, RO_FLOW[1] + 180);
    harness::frame(100);
    CHECK_EQ(get(V_INT).getInt(), 4);
}

////////////////////////////////////////////////////////////////////////////////
// Switch

namespace {

// the slider on the same page needs a range (min == max: W13)
void sliderRange() {
    set(V_B, intValue(0));
    set(V_C, intValue(100));
}

} // namespace

TEST("widgets/switch: off is gray with the knob left, on is green with the knob right") {
    sliderRange();
    set(V_INT, intValue(0));
    show(PAGE_ID_W_SWITCH);
    // inside the padding: x 12 .. 107, knob 32 px
    CHECK_EQ(rgb(SW[0] + 19, SW[1] + 20), WHITE);
    CHECK_EQ(rgb(SW[0] + 80, SW[1] + 20), GRAY);
    set(V_INT, intValue(1));
    harness::frame(20);
    CHECK_EQ(rgb(SW[0] + 81, SW[1] + 20), WHITE);
    CHECK_EQ(rgb(SW[0] + 20, SW[1] + 20), GREEN);
}

TEST("widgets/switch: a tap toggles it, the animation ends") {
    sliderRange();
    set(V_INT, intValue(0));
    show(PAGE_ID_W_SWITCH);
    tap(CENTER(SW));
    CHECK_EQ(get(V_INT).getInt(), 1);
    CHECK_EQ(rgb(SW[0] + 81, SW[1] + 20), WHITE);
    CHECK_EQ(framesWithDrawing(3), 0);
    tap(CENTER(SW));
    CHECK_EQ(get(V_INT).getInt(), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// Slider: inside the padding x 12 .. 307, knob 36 px, its center from 30 to 289

TEST("widgets/slider: the knob at the value") {
    set(V_B, intValue(0));
    set(V_C, intValue(100));
    set(V_DOUBLE, doubleValue(50));
    show(PAGE_ID_W_SWITCH);
    // center 30 + 0.5 * 259 = 160
    CHECK_EQ(rgb(SL[0] + 150, SL[1] + 20), WHITE);
    CHECK_EQ(rgb(SL[0] + 40, SL[1] + 20), GRAY);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/slider: dragging the knob sets the value") {
    set(V_B, intValue(0));
    set(V_C, intValue(100));
    set(V_DOUBLE, doubleValue(50));
    show(PAGE_ID_W_SWITCH);
    press(SL[0] + 150, SL[1] + 20);
    harness::touch(SL[0] + 150 - 65, SL[1] + 20);
    harness::frame(3);
    // 65 px of 259 px
    CHECK_NEAR(get(V_DOUBLE).getDouble(), 50 - 65 * 100.0 / 259, 0.01);
    harness::touch(SL[0] + 299, SL[1] + 20);
    harness::frame(3);
    CHECK_NEAR(get(V_DOUBLE).getDouble(), 100, 0.001);
    release();
    harness::frame(2);
    CHECK_EQ(rgb(SL[0] + 277, SL[1] + 20), WHITE);
}

TEST_BUG("widgets/slider: min equal to max draws the knob at the start", "W13") {
    // (value - min) / (max - min) is NaN, converted to int
    set(V_B, intValue(50));
    set(V_C, intValue(50));
    set(V_DOUBLE, doubleValue(50));
    show(PAGE_ID_W_SWITCH);
    CHECK_EQ(rgb(SL[0] + 20, SL[1] + 20), WHITE);
}

////////////////////////////////////////////////////////////////////////////////
// DropDownList

namespace {

// center of the item in the select popup (container padding 4, item height 4 + 26 + 4)
void tapPopupItem(int i) {
    auto page = (InternalPage *)harness::appContext()->getActivePage();
    tap(page->x + 10, page->y + 4 + i * 34 + 17);
}

} // namespace

TEST("widgets/ddl: shows the selected option") {
    set(V_A, stringArray({ "one", "two", "three" }));
    set(V_INT, intValue(1));
    show(PAGE_ID_W_DDL);
    CHECK(count(DL[0], DL[1], 150, DL[3], YELLOW) > 0);
    auto two = block(RECT(DL));
    CHECK_EQ(framesWithDrawing(3), 0);
    set(V_INT, intValue(2));
    harness::frame();
    CHECK(diffBlock(two, RECT(DL)) > 0);
}

TEST("widgets/ddl: an index outside of the options shows no text") {
    set(V_A, stringArray({ "one", "two", "three" }));
    set(V_INT, intValue(5));
    show(PAGE_ID_W_DDL);
    // only the arrow on the right
    CHECK_EQ(count(DL[0], DL[1], 150, DL[3], YELLOW), 0);
    CHECK(count(DL[0] + 160, DL[1], 40, DL[3], YELLOW) > 0);
}

TEST("widgets/ddl: a tap opens the options, a tap on an option selects it") {
    set(V_A, stringArray({ "one", "two", "three" }));
    set(V_INT, intValue(0));
    show(PAGE_ID_W_DDL);
    tap(CENTER(DL));
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)INTERNAL_PAGE_ID_SELECT_FROM_ENUM);
    tapPopupItem(2);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_DDL);
    CHECK_EQ(get(V_INT).getInt(), 2);
}

TEST_BUG("widgets/ddl: the open options survive a restart of the flow", "W15@san") {
    set(V_A, stringArray({ "one", "two", "three" }));
    set(V_INT, intValue(0));
    show(PAGE_ID_W_DDL);
    tap(CENTER(DL));
    REQUIRE(harness::appContext()->getActivePageId() == (int)INTERNAL_PAGE_ID_SELECT_FROM_ENUM);
    // e.g. module flow error, then *RST: the flow states are freed and created again
    flow::stop();
    harness::frame(2);
    flow::start(g_mainAssets);
    harness::frame(2);
    // the popup still uses the widget cursor (flow state) from the tap
    tapPopupItem(1);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_DDL);
}

////////////////////////////////////////////////////////////////////////////////
// Checkbox, Radio

TEST("widgets/checkbox: drawn checked when the data is true") {
    set(V_BOOL, boolValue(false));
    show(PAGE_ID_W_CHECK);
    CHECK(count(RECT(CB), WHITE) > 0);
    auto unchecked = block(RECT(CB));
    CHECK_EQ(framesWithDrawing(3), 0);
    set(V_BOOL, boolValue(true));
    harness::frame();
    CHECK(drawn());
    CHECK(diffBlock(unchecked, RECT(CB)) > 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/checkbox: a tap toggles the boolean") {
    set(V_BOOL, boolValue(false));
    show(PAGE_ID_W_CHECK);
    tap(CENTER(CB));
    CHECK_EQ(get(V_BOOL).getType(), VALUE_TYPE_BOOLEAN);
    CHECK_EQ(get(V_BOOL).getBoolean(), true);
    tap(CENTER(CB));
    CHECK_EQ(get(V_BOOL).getBoolean(), false);
}

TEST("widgets/checkbox: a change of the overridden style redraws it") {
    g_overrideWidget = pageWidget(PAGE_ID_W_CHECK, 1);
    g_overrideStyle = 0;
    g_hooks.overrideStyle = overrideStyleHook;
    show(PAGE_ID_W_CHECK);
    CHECK_EQ(rgb(CB_OV[0] + 150, CB_OV[1] + 2), BLACK);
    // w_txt_alt: green on blue
    g_overrideStyle = STYLE_ID_W_TXT_ALT;
    harness::frame(3);
    CHECK_EQ(rgb(CB_OV[0] + 150, CB_OV[1] + 2), BLUE);
}

TEST("widgets/radio: checked when the data equals its value, a tap sets the value") {
    set(V_INT, intValue(0));
    show(PAGE_ID_W_CHECK);
    // the circle (19 px) without the label
    auto checkedIcon = block(RA0[0], RA0[1], 20, RA0[3]);
    auto uncheckedIcon = block(RA1[0], RA1[1], 20, RA1[3]);
    CHECK(diffBlock(checkedIcon, RA1[0], RA1[1], 20, RA1[3]) > 0);
    tap(CENTER(RA1));
    CHECK_EQ(get(V_INT).getInt(), 1);
    CHECK_EQ(diffBlock(uncheckedIcon, RA0[0], RA0[1], 20, RA0[3]), 0);
    CHECK_EQ(diffBlock(checkedIcon, RA1[0], RA1[1], 20, RA1[3]), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/radio: strings are compared as strings") {
    set(V_STR, str("a"));
    show(PAGE_ID_W_CHECK);
    auto unchecked = block(RECT(RAS));
    set(V_STR, str("b"));
    harness::frame();
    CHECK(diffBlock(unchecked, RECT(RAS)) > 0);
    tap(CENTER(RAS));
    CHECK_STR(get(V_STR).getString(), "b");
}
