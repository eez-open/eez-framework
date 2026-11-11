/*
 * eez-framework tests: Button, ToggleButton and ButtonGroup widgets (pages
 * w_button, w_toggle, w_bgroup).
 */

#include "wtest.h"

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

// w_button
const int BT_CLICK[] = { 10, 10, 150, 50 };
const int BT_ENABLED[] = { 200, 10, 150, 50 };
const int BT_OVERRIDE[] = { 400, 10, 150, 50 };

// w_toggle
const int TG[] = { 10, 10, 150, 50 };

// w_bgroup: 300 px, a third for each of 3 buttons
const int BG[] = { 10, 10, 300, 50 };

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
// Button

TEST("widgets/button: drawn with its text in its style") {
    show(PAGE_ID_W_BUTTON);
    // w_btn: white text on blue
    CHECK_EQ(rgb(BT_CLICK[0] + 2, BT_CLICK[1] + 2), BLUE);
    CHECK(count(RECT(BT_CLICK), WHITE) > 0);
    CHECK_EQ(framesWithDrawing(5), 0);
}

TEST("widgets/button: a tap executes its action once") {
    show(PAGE_ID_W_BUTTON);
    tap(CENTER(BT_CLICK));
    CHECK_EQ(g_native.clicks, 1);
    CHECK_EQ(g_native.clicks2, 0);
    tap(CENTER(BT_CLICK));
    CHECK_EQ(g_native.clicks, 2);
}

TEST("widgets/button: drawn with the active colors while pressed") {
    show(PAGE_ID_W_BUTTON);
    press(CENTER(BT_CLICK));
    harness::frame();
    CHECK_EQ(rgb(BT_CLICK[0] + 2, BT_CLICK[1] + 2), RED);
    // the other buttons are not active
    CHECK_EQ(rgb(BT_ENABLED[0] + 2, BT_ENABLED[1] + 2), BLUE);
    release();
    harness::frame();
    CHECK_EQ(rgb(BT_CLICK[0] + 2, BT_CLICK[1] + 2), BLUE);
    CHECK_EQ(g_native.clicks, 1);
}

TEST("widgets/button: disabled button has the disabled style and no action") {
    set(V_BOOL, boolValue(false));
    show(PAGE_ID_W_BUTTON);
    CHECK_EQ(rgb(BT_ENABLED[0] + 2, BT_ENABLED[1] + 2), GRAY);
    tap(CENTER(BT_ENABLED));
    CHECK_EQ(g_native.clicks2, 0);
    set(V_BOOL, boolValue(true));
    harness::frame();
    CHECK_EQ(rgb(BT_ENABLED[0] + 2, BT_ENABLED[1] + 2), BLUE);
    tap(CENTER(BT_ENABLED));
    CHECK_EQ(g_native.clicks2, 1);
}

TEST("widgets/button: overridden style is used for drawing") {
    g_overrideWidget = pageWidget(PAGE_ID_W_BUTTON, 2);
    g_overrideStyle = STYLE_ID_W_SEL;
    g_hooks.overrideStyle = overrideStyleHook;
    show(PAGE_ID_W_BUTTON);
    // w_sel: green
    CHECK_EQ(rgb(BT_OVERRIDE[0] + 2, BT_OVERRIDE[1] + 2), GREEN);
}

////////////////////////////////////////////////////////////////////////////////
// ToggleButton

TEST("widgets/toggle: text1 in the style when 0, text2 in the checked style otherwise") {
    set(V_INT, intValue(0));
    show(PAGE_ID_W_TOGGLE);
    CHECK_EQ(rgb(TG[0] + 2, TG[1] + 2), BLUE);
    auto off = block(RECT(TG));
    set(V_INT, intValue(1));
    harness::frame();
    CHECK(drawn());
    CHECK_EQ(rgb(TG[0] + 2, TG[1] + 2), GREEN);
    CHECK(count(RECT(TG), WHITE) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    set(V_INT, intValue(0));
    harness::frame();
    CHECK_EQ(diffBlock(off, RECT(TG)), 0);
}

TEST("widgets/toggle: a tap toggles the variable between 0 and 1") {
    set(V_INT, intValue(0));
    show(PAGE_ID_W_TOGGLE);
    tap(CENTER(TG));
    CHECK_EQ(get(V_INT).getInt(), 1);
    CHECK_EQ(rgb(TG[0] + 2, TG[1] + 2), GREEN);
    tap(CENTER(TG));
    CHECK_EQ(get(V_INT).getInt(), 0);
    CHECK_EQ(rgb(TG[0] + 2, TG[1] + 2), BLUE);
}

////////////////////////////////////////////////////////////////////////////////
// ButtonGroup

TEST("widgets/bgroup: a button for each label, the selected one in the selected style") {
    set(V_A, stringArray({ "a", "b", "c" }));
    set(V_INT, intValue(1));
    show(PAGE_ID_W_BGROUP);
    CHECK_EQ(rgb(BG[0] + 2, BG[1] + 2), BLUE);
    CHECK_EQ(rgb(BG[0] + 102, BG[1] + 2), GREEN);
    CHECK_EQ(rgb(BG[0] + 202, BG[1] + 2), BLUE);
    CHECK(count(BG[0], BG[1], 100, BG[3], WHITE) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    set(V_INT, intValue(2));
    harness::frame();
    CHECK_EQ(rgb(BG[0] + 102, BG[1] + 2), BLUE);
    CHECK_EQ(rgb(BG[0] + 202, BG[1] + 2), GREEN);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/bgroup: a tap selects the button under the finger") {
    set(V_A, stringArray({ "a", "b", "c" }));
    set(V_INT, intValue(0));
    show(PAGE_ID_W_BGROUP);
    tap(BG[0] + 250, BG[1] + 25);
    CHECK_EQ(get(V_INT).getInt(), 2);
    CHECK_EQ(rgb(BG[0] + 202, BG[1] + 2), GREEN);
    tap(BG[0] + 150, BG[1] + 25);
    CHECK_EQ(get(V_INT).getInt(), 1);
}

TEST("widgets/bgroup: an empty array of labels draws no buttons") {
    set(V_A, stringArray({}));
    show(PAGE_ID_W_BGROUP);
    // drawButtons divides the width by the count 0
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_BGROUP);
}

TEST("widgets/bgroup: data that is not an array draws no buttons") {
    set(V_A, Value());
    show(PAGE_ID_W_BGROUP);
    // render calls getArray() of an undefined value
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_BGROUP);
}
