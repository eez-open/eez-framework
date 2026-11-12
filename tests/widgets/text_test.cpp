/*
 * eez-framework tests: Text and MultilineText widgets (pages w_text, w_mtext).
 */

#include <string.h>

#include "wtest.h"

#include <eez/gui/widgets/text.h>

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

// w_text: x, y, w, h of the widgets
const int T_STATIC[] = { 10, 10, 200, 40 };
const int T_DATA[] = { 10, 60, 200, 40 };
const int T_OVERRIDE[] = { 10, 110, 200, 40 };
const int T_ELLIPSIS[] = { 10, 160, 120, 40 };
const int T_REF[] = { 10, 210, 300, 40 };
const int T_FIT[] = { 300, 10, 400, 40 };
const int T_NOTHING[] = { 400, 110, 200, 40 };

// w_mtext
const int M_STATIC[] = { 10, 10, 150, 120 };
const int M_DATA[] = { 200, 10, 150, 120 };
const int M_OVERRIDE[] = { 400, 10, 150, 120 };

#define RECT(R) R[0], R[1], R[2], R[3]

// overrideStyle hook: the style of the widget g_overrideWidget is g_overrideStyle
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
// Text

TEST("widgets/text: static text is drawn in the colors of its style") {
    show(PAGE_ID_W_TEXT);
    // background gray, glyphs yellow (antialiased edges in between)
    CHECK_EQ(rgb(T_STATIC[0] + 2, T_STATIC[1] + 2), GRAY);
    CHECK(count(RECT(T_STATIC), YELLOW) > 20);
    CHECK_EQ(count(RECT(T_STATIC), GRAY) + count(RECT(T_STATIC), YELLOW) > T_STATIC[2] * T_STATIC[3] / 2, true);
    // outside of the widgets: page background
    CHECK_EQ(rgb(250, 300), BLACK);
}

TEST("widgets/text: a page without changes is not drawn again") {
    set(V_A, str("abc"));
    set(V_B, str("fit"));
    show(PAGE_ID_W_TEXT);
    CHECK_EQ(framesWithDrawing(10), 0);
}

TEST("widgets/text: data change redraws the text") {
    set(V_A, str("abc"));
    show(PAGE_ID_W_TEXT);
    int before = count(RECT(T_DATA), YELLOW);
    set(V_A, str("abcdefghij"));
    harness::frame();
    CHECK(drawn());
    CHECK(count(RECT(T_DATA), YELLOW) > before);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/text: the same string in a new value is not drawn again") {
    set(V_A, str("abc"));
    show(PAGE_ID_W_TEXT);
    // strings are compared by content
    set(V_A, str("abc"));
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/text: a string changed in place in the same buffer is drawn again") {
    // like a data function that returns its text in a static buffer: the
    // state keeps a copy of the string, not the pointer to the buffer
    static char buffer[16];
    strcpy(buffer, "abc");
    set(V_A, Value(buffer, VALUE_TYPE_STRING));
    show(PAGE_ID_W_TEXT);
    int before = count(RECT(T_DATA), YELLOW);
    strcpy(buffer, "abcdefghij");
    harness::frame();
    CHECK(drawn());
    CHECK(count(RECT(T_DATA), YELLOW) > before);
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
    auto longText = block(RECT(T_DATA));
    strcpy(buffer, "xy");
    harness::frame();
    CHECK(drawn());
    CHECK(diffBlock(longText, RECT(T_DATA)) > 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/text: integer data is drawn as its text") {
    set(V_A, intValue(42));
    show(PAGE_ID_W_TEXT);
    // the same pixels as the string "42" in the same widget
    auto pixels = block(RECT(T_DATA));
    set(V_A, str("42"));
    harness::frame();
    CHECK_EQ(diffBlock(pixels, RECT(T_DATA)), 0);
    CHECK(count(RECT(T_DATA), YELLOW) > 0);
}

TEST("widgets/text: undefined and null data draw only the background") {
    set(V_A, Value());
    show(PAGE_ID_W_TEXT);
    CHECK_EQ(countOther(RECT(T_DATA), GRAY), 0);
    set(V_A, Value(0, VALUE_TYPE_NULL));
    harness::frame(2);
    CHECK_EQ(countOther(RECT(T_DATA), GRAY), 0);
    CHECK_EQ(harness::flowErrors().size(), 0u);
}

TEST("widgets/text: a text widget without text and data draws nothing") {
    show(PAGE_ID_W_TEXT);
    // not even its background: the page background stays
    CHECK_EQ(countOther(RECT(T_NOTHING), BLACK), 0);
}

TEST("widgets/text: fit to content width follows the text and clears what it leaves") {
    set(V_B, str("WWWWWWWWWWWWWWWWWWWW"));
    show(PAGE_ID_W_TEXT);
    // 20 x 'W' (14 px each) is wider than 250 px
    CHECK(count(T_FIT[0] + 250, T_FIT[1], 10, T_FIT[3], GRAY) > 0);
    set(V_B, str("W"));
    harness::frame();
    // the widget is as wide as one 'W', the rest is the page background again
    CHECK_EQ(countOther(T_FIT[0] + 30, T_FIT[1], 300, T_FIT[3], BLACK), 0);
    CHECK(count(T_FIT[0], T_FIT[1], 14, T_FIT[3], GRAY) > 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/text: file ellipsis keeps both ends of a long text") {
    g_native.ell = "oooooooooooooooooooo"; // 20 x 10 px, the widget is 120 px
    g_native.ellOptions = STRING_OPTIONS_FILE_ELLIPSIS;
    // 5 + 5 characters and "..." fit into 120 px
    g_native.ref = "ooooo...ooooo";
    show(PAGE_ID_W_TEXT);
    CHECK_EQ(diffBlocks(T_ELLIPSIS[0], T_ELLIPSIS[1], T_REF[0], T_REF[1], T_ELLIPSIS[2], T_ELLIPSIS[3]), 0);
}

TEST("widgets/text: a short text with file ellipsis is drawn whole") {
    g_native.ell = "ooooo";
    g_native.ellOptions = STRING_OPTIONS_FILE_ELLIPSIS;
    g_native.ref = "ooooo";
    show(PAGE_ID_W_TEXT);
    CHECK_EQ(diffBlocks(T_ELLIPSIS[0], T_ELLIPSIS[1], T_REF[0], T_REF[1], T_ELLIPSIS[2], T_ELLIPSIS[3]), 0);
}

TEST("widgets/text: file ellipsis does not cut a UTF-8 character") {
    // "ooooo" + U+00E9 (2 bytes, no glyph in the font, 0 px) + 15 x "o": the
    // ends are cut at whole characters (W10: the left end stopped after the
    // first byte of U+00E9, which took the first '.' of the ellipsis as its
    // second byte)
    g_native.ell = "ooooo\xC3\xA9ooooooooooooooo";
    g_native.ellOptions = STRING_OPTIONS_FILE_ELLIPSIS;
    g_native.ref = "ooooo...ooooo";
    show(PAGE_ID_W_TEXT);
    CHECK_EQ(diffBlocks(T_ELLIPSIS[0], T_ELLIPSIS[1], T_REF[0], T_REF[1], T_ELLIPSIS[2], T_ELLIPSIS[3]), 0);
}

TEST("widgets/text: overridden style is used on a full redraw") {
    g_overrideWidget = pageWidget(PAGE_ID_W_TEXT, 2);
    g_overrideStyle = STYLE_ID_W_TXT_ALT;
    g_hooks.overrideStyle = overrideStyleHook;
    show(PAGE_ID_W_TEXT);
    // w_txt_alt: green on blue
    CHECK_EQ(rgb(T_OVERRIDE[0] + 2, T_OVERRIDE[1] + 2), BLUE);
    CHECK(count(RECT(T_OVERRIDE), GREEN) > 0);
    // the other texts with the same style are not affected
    CHECK_EQ(rgb(T_STATIC[0] + 2, T_STATIC[1] + 2), GRAY);
}

TEST("widgets/text: a change of the overridden style redraws the text") {
    g_overrideWidget = pageWidget(PAGE_ID_W_TEXT, 2);
    g_overrideStyle = 0;
    g_hooks.overrideStyle = overrideStyleHook;
    show(PAGE_ID_W_TEXT);
    CHECK_EQ(rgb(T_OVERRIDE[0] + 2, T_OVERRIDE[1] + 2), GRAY);
    // only the hook result changes, the state of the widget does not
    g_overrideStyle = STYLE_ID_W_TXT_ALT;
    harness::frame(3);
    CHECK_EQ(rgb(T_OVERRIDE[0] + 2, T_OVERRIDE[1] + 2), BLUE);
}

TEST("widgets/text: incremental frames equal a full redraw after many changes") {
    set(V_A, str("one"));
    set(V_B, str("WWWWWWWW"));
    show(PAGE_ID_W_TEXT);
    const char *values[] = { "two", "", "three three", "4", "five" };
    for (auto value : values) {
        set(V_A, str(value));
        set(V_B, str(value));
        harness::frame();
    }
    set(V_A, intValue(7));
    harness::frame();
    std::string where;
    CHECK_EQ(diffWithFullRedraw(&where), 0);
}

////////////////////////////////////////////////////////////////////////////////
// MultilineText

TEST("widgets/mtext: a long text is broken into several lines") {
    show(PAGE_ID_W_MTEXT);
    // "one two three four five six" does not fit into 150 px
    CHECK_EQ(textLines(RECT(M_STATIC), YELLOW), 2);
    CHECK_EQ(framesWithDrawing(5), 0);
}

TEST("widgets/mtext: data change redraws, the same value does not") {
    set(V_A, str("short"));
    show(PAGE_ID_W_MTEXT);
    CHECK_EQ(textLines(RECT(M_DATA), YELLOW), 1);
    set(V_A, str("short"));
    CHECK_EQ(framesWithDrawing(2), 0);
    set(V_A, str("a much longer text on more lines"));
    harness::frame();
    CHECK(drawn());
    CHECK(textLines(RECT(M_DATA), YELLOW) > 1);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/mtext: a string changed in place in the same buffer is drawn again") {
    static char buffer[64];
    strcpy(buffer, "short");
    set(V_A, Value(buffer, VALUE_TYPE_STRING));
    show(PAGE_ID_W_MTEXT);
    CHECK_EQ(textLines(RECT(M_DATA), YELLOW), 1);
    strcpy(buffer, "a much longer text on more lines");
    harness::frame();
    CHECK(drawn());
    CHECK(textLines(RECT(M_DATA), YELLOW) > 1);
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/mtext: a number is drawn as its text") {
    set(V_A, intValue(12345));
    show(PAGE_ID_W_MTEXT);
    CHECK(count(RECT(M_DATA), YELLOW) > 0);
}

TEST("widgets/mtext: overridden style is used for drawing") {
    // MultilineText draws with the overridden style (W8: it took only the
    // blink flag from it)
    g_overrideWidget = pageWidget(PAGE_ID_W_MTEXT, 2);
    g_overrideStyle = STYLE_ID_W_TXT_ALT;
    g_hooks.overrideStyle = overrideStyleHook;
    show(PAGE_ID_W_MTEXT);
    CHECK_EQ(rgb(M_OVERRIDE[0] + 2, M_OVERRIDE[1] + 2), BLUE);
}
