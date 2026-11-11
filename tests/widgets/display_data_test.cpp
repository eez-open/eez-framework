/*
 * eez-framework tests: DisplayData widget (page w_dd).
 */

#include <math.h>

#include "wtest.h"

#include <eez/core/unit.h>

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

// w_dd: the same data (w_a) with display options 0..5, a reference text
const int DD_OPTION[6][4] = {
    { 10, 10, 200, 40 },
    { 10, 60, 200, 40 },
    { 10, 110, 200, 40 },
    { 10, 160, 200, 40 },
    { 10, 210, 200, 40 },
    { 10, 260, 200, 40 },
};
const int DD_REF[] = { 250, 10, 200, 40 };
const int DD_ROUNDED[] = { 500, 10, 200, 40 };
const int DD_XSCROLL[] = { 500, 60, 200, 40 };
const int DD_REFRESH[] = { 500, 110, 200, 40 };
const int DD_IUF[] = { 500, 160, 250, 40 };

#define RECT(R) R[0], R[1], R[2], R[3]

// the display option widget shows the same pixels as the reference text with text
int diffWithRef(int option, const char *text) {
    g_native.ref = text;
    harness::frame(2);
    return diffBlocks(DD_OPTION[option][0], DD_OPTION[option][1], DD_REF[0], DD_REF[1], DD_REF[2], DD_REF[3]);
}

} // namespace

TEST("widgets/dd: a float with a unit is drawn as its text") {
    Value value(2.5f, UNIT_VOLT);
    char text[64];
    value.toText(text, sizeof(text));
    CHECK_STR(text, "2.5 V");
    set(V_A, value);
    show(PAGE_ID_W_DD);
    CHECK(count(RECT(DD_OPTION[0]), YELLOW) > 0);
    CHECK_EQ(diffWithRef(0, "2.5 V"), 0);
}

TEST("widgets/dd: display options select the parts of a float") {
    set(V_A, Value(2.5f, UNIT_VOLT));
    show(PAGE_ID_W_DD);
    CHECK_EQ(diffWithRef(1, "2"), 0);       // integer
    CHECK_EQ(diffWithRef(2, ".5 V"), 0);    // from the fraction (Studio: "fraction and unit")
    CHECK_EQ(diffWithRef(3, ".5"), 0);      // the fraction (Studio: "fraction")
    CHECK_EQ(diffWithRef(4, "V"), 0);       // unit
    CHECK_EQ(diffWithRef(5, "2.5"), 0);     // integer and fraction
}

TEST("widgets/dd: a float without a fraction is drawn with .0") {
    set(V_A, Value(3.0f, UNIT_VOLT));
    show(PAGE_ID_W_DD);
    char text[64];
    get(V_A).toText(text, sizeof(text));
    CHECK_STR(text, "3.0 V");
    CHECK_EQ(diffWithRef(3, ".0"), 0);
}

TEST("widgets/dd: other values than floats are shown only with the integer options") {
    set(V_A, str("abc"));
    show(PAGE_ID_W_DD);
    CHECK_EQ(diffWithRef(0, "abc"), 0);
    CHECK_EQ(diffWithRef(1, "abc"), 0);
    CHECK_EQ(diffWithRef(5, "abc"), 0);
    // the fraction and unit options show nothing
    CHECK_EQ(countOther(RECT(DD_OPTION[2]), GRAY), 0);
    CHECK_EQ(countOther(RECT(DD_OPTION[3]), GRAY), 0);
    CHECK_EQ(countOther(RECT(DD_OPTION[4]), GRAY), 0);
}

TEST("widgets/dd: redrawn when the value changes and only then") {
    set(V_A, Value(1.5f, UNIT_VOLT));
    show(PAGE_ID_W_DD);
    CHECK_EQ(framesWithDrawing(5), 0);
    set(V_A, Value(1.5f, UNIT_VOLT));
    CHECK_EQ(framesWithDrawing(2), 0);
    set(V_A, Value(1.25f, UNIT_VOLT));
    harness::frame();
    CHECK(drawn());
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(diffWithRef(0, "1.25 V"), 0);
}

TEST("widgets/dd: undefined data draws the background only") {
    set(V_A, Value());
    show(PAGE_ID_W_DD);
    CHECK_EQ(countOther(RECT(DD_OPTION[0]), GRAY), 0);
}

TEST("widgets/dd: NaN data is not redrawn in every frame") {
    set(V_A, Value(NAN, VALUE_TYPE_FLOAT));
    show(PAGE_ID_W_DD);
    // NaN != NaN in the state comparison
    CHECK_EQ(framesWithDrawing(5), 0);
}

TEST("widgets/dd: x scroll moves the text to the left") {
    // 25 x 'W' = 350 px, left aligned
    g_native.xscrollText = "WWWWWWWWWWWWWWWWWWWWWWWWW";
    g_native.xscroll = 0;
    show(PAGE_ID_W_DD);
    CHECK(count(DD_XSCROLL[0] + 150, DD_XSCROLL[1], 50, DD_XSCROLL[3], YELLOW) > 0);
    g_native.xscroll = 200;
    harness::frame(2);
    // the text ends at 350 - 200 = 150 px
    CHECK(count(DD_XSCROLL[0], DD_XSCROLL[1], 140, DD_XSCROLL[3], YELLOW) > 0);
    CHECK_EQ(countOther(DD_XSCROLL[0] + 160, DD_XSCROLL[1], 40, DD_XSCROLL[3], GRAY), 0);
}

TEST("widgets/dd: x scroll above 255 px moves the text") {
    g_native.xscrollText = "WWWWWWWWWWWWWWWWWWWWWWWWW";
    g_native.xscroll = 260;
    show(PAGE_ID_W_DD);
    // the text ends at 350 - 260 = 90 px (xScroll is stored in uint8_t: 4 px)
    CHECK_EQ(countOther(DD_XSCROLL[0] + 100, DD_XSCROLL[1], 100, DD_XSCROLL[3], GRAY), 0);
}

TEST("widgets/dd: x scroll above 255 px is not redrawn in every frame") {
    g_native.xscrollText = "abc";
    g_native.xscroll = 300;
    show(PAGE_ID_W_DD);
    // the stored uint8_t never equals the int from the data
    CHECK_EQ(framesWithDrawing(5), 0);
}

TEST("widgets/dd: refresh rate delays a change that follows a shown change") {
    set(V_C, intValue(1));
    show(PAGE_ID_W_DD);
    set(V_C, intValue(2));
    harness::frame(80);
    auto shown2 = block(RECT(DD_REFRESH));
    // 2 is shown, 3 comes within the refresh rate (1000 ms)
    set(V_C, intValue(3));
    harness::frame(20); // 320 ms
    CHECK_EQ(diffBlock(shown2, RECT(DD_REFRESH)), 0);
    harness::frame(60); // 1280 ms
    CHECK(diffBlock(shown2, RECT(DD_REFRESH)) > 0);
}

TEST_BUG("widgets/dd: refresh rate does not delay the first change after a stable period", "W20") {
    set(V_C, intValue(1));
    show(PAGE_ID_W_DD);
    // stable for 2 s, longer than the refresh rate
    harness::frame(125);
    auto shown1 = block(RECT(DD_REFRESH));
    set(V_C, intValue(2));
    harness::frame(2);
    CHECK(diffBlock(shown1, RECT(DD_REFRESH)) > 0);
}

TEST("widgets/dd: rounded opaque style looks the same after incremental redraws") {
    // w_rounded: red, border radius 12, no border: counted as transparent, so
    // the background is drawn under the antialiased corners before each redraw
    // (D4: counted as opaque)
    set(V_B, intValue(0));
    show(PAGE_ID_W_DD);
    for (int i = 1; i <= 10; i++) {
        set(V_B, intValue(i));
        harness::frame();
    }
    std::string where;
    CHECK_EQ(diffWithFullRedraw(&where), 0);
}

TEST("widgets/dd: integer, unit and fraction option draws the parts of a float") {
    set(V_D, Value(12.75f, UNIT_VOLT));
    show(PAGE_ID_W_DD);
    CHECK(count(RECT(DD_IUF), YELLOW) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    // a string is drawn whole as the integer part
    set(V_D, str("abc"));
    harness::frame();
    CHECK(drawn());
    CHECK(count(RECT(DD_IUF), YELLOW) > 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}
