/*
 * eez-framework tests: the rows of a settings group (projects/parts/rows.mjs):
 * the icon and the trailing icon of the Text widget (Text, DisplayData,
 * Button) and of the Input, the label of the DropDownList.
 */

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>

#include "document.h"
#include "harness.h"
#include "render_util.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;
using render::rgb;

////////////////////////////////////////////////////////////////////////////////
// native variables and action of the part

namespace {

const char *g_rowsIcon = "#";
float g_rowsValue = 1.25f;
int g_rowsClicks;

} // namespace

namespace eez {
namespace gui {

void data_rows_icon(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_rowsIcon);
    }
}

void data_rows_value(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_rowsValue, UNIT_VOLT);
    }
}

void action_rows_on_click() {
    g_rowsClicks++;
}

} // namespace gui
} // namespace eez

namespace {

uint32_t px(int x, int y) {
    return harness::pixel(x, y) | 0xFF000000u;
}

// the colors of the part (parts/rows.mjs)
const uint32_t BG = rgb(0x20, 0x20, 0x20);
const uint32_t PAGE_BG = rgb(0x40, 0x60, 0x80);
const uint32_t WHITE = rgb(0xF8, 0xFC, 0xF8);
const uint32_t RED = rgb(0xF8, 0, 0);
const uint32_t GREEN = rgb(0, 0xFC, 0);
const uint32_t BLUE = rgb(0, 0, 0xF8);
const uint32_t YELLOW = rgb(0xF8, 0xFC, 0);
const uint32_t PRESSED = rgb(0x80, 0x40, 0);

// the rows of the page (parts/rows.mjs): at x = 20, 400 x 40, the horizontal
// padding of the row style 16, of the icon styles 12 at the side of the text
const int X = 20;
const int W = 400;
const int H = 40;
const int PADDING = 16;
const int GAP = 12;
// an icon is centered in a square of the height of its font: the most a side
// of the icon is from the side of the square
const int ICON_SLACK = 12;
const int BOTH = 10;
const int NAV = 60;
const int ICON_ROW = 110;
const int UNIT = 160;
const int PLAIN = 210;
const int INPUT = 260;
const int DROPDOWN = 310;
const int DROPDOWN_HALF = 360;
const int DROPDOWN_PLAIN = 410;
const int FIT_X = 450;
const int FIT_Y = 10;
// the rows with a long value, 330 wide
const int LONG_X = 450;
const int LONG_W = 330;
const int LONG_ELLIPSIS = 60;
const int LONG_CLIP = 110;

// The first and the last x of the pixels of a color in the row at y (in the
// box x1 .. x2, the whole row by default); min > max if there is none. Only
// the pixels a glyph covers fully have its color.
struct Span {
    int min = 100000;
    int max = -1;
    bool found() const { return max >= 0; }
};

Span span(int y, uint32_t color, int x1 = X, int x2 = X + W - 1) {
    Span s;
    for (int yy = y; yy < y + H; yy++) {
        for (int xx = x1; xx <= x2; xx++) {
            if (px(xx, yy) == color) {
                if (xx < s.min) s.min = xx;
                if (xx > s.max) s.max = xx;
            }
        }
    }
    return s;
}

void show() {
    g_rowsIcon = "#";
    g_rowsValue = 1.25f;
    render::showPage(PAGE_ID_ROWS_PAGE);
}

void press(int y, int x = X + W / 2) {
    harness::touch(x, y + H / 2);
    harness::frame(2);
}

void release() {
    harness::release();
    harness::frame(2);
    // the next touch is not an accidental double tap
    harness::frame(25);
}

} // namespace

////////////////////////////////////////////////////////////////////////////////

TEST("render/rows: golden") {
    show();
    CHECK_GOLDEN("rows_page", render::captureFrame());
}

TEST("render/rows: the icon is at the left and the trailing icon at the right of the content box") {
    show();
    Span icon = span(BOTH, RED);
    Span trailing = span(BOTH, GREEN);
    REQUIRE(icon.found());
    REQUIRE(trailing.found());
    // inside the padding of the row, at its ends
    CHECK(icon.min >= X + PADDING);
    CHECK(icon.max < X + PADDING + 24);
    CHECK(trailing.max <= X + W - 1 - PADDING);
    CHECK(trailing.min > X + W - 1 - PADDING - 24);
}

TEST("render/rows: the label and the value are between the icons, after the gaps") {
    show();
    Span icon = span(BOTH, RED);
    Span label = span(BOTH, YELLOW);
    Span value = span(BOTH, WHITE);
    Span trailing = span(BOTH, GREEN);
    REQUIRE(label.found());
    REQUIRE(value.found());
    // the label left: after the icon and its right padding
    CHECK(label.min > icon.max + GAP - 2);
    CHECK(label.min < icon.max + GAP + 12 + ICON_SLACK);
    // the value right aligned: it ends before the left padding of the
    // trailing icon
    CHECK(value.max < trailing.min - GAP + 4);
    CHECK(value.max > trailing.min - GAP - 12 - ICON_SLACK);
    CHECK(label.max < value.min);
}

TEST("render/rows: a widget with only a trailing icon") {
    show();
    CHECK(!span(NAV, RED).found());
    Span text = span(NAV, WHITE);
    Span trailing = span(NAV, GREEN);
    REQUIRE(text.found());
    REQUIRE(trailing.found());
    // the text at the left of the content box
    CHECK(text.min >= X + PADDING);
    CHECK(text.min < X + PADDING + 6);
    CHECK(trailing.max <= X + W - 1 - PADDING);
    CHECK(trailing.min > X + W - 1 - PADDING - 24);
}

TEST("render/rows: the whole row is the touch target and is drawn pressed") {
    show();
    int clicks = g_rowsClicks;
    // on the trailing icon
    press(NAV, X + W - PADDING - 4);
    CHECK_EQ(px(X + 4, NAV + 4), PRESSED);
    CHECK_EQ(px(X + W - 4, NAV + H - 4), PRESSED);
    // the icon is still there
    CHECK(span(NAV, GREEN).found());
    release();
    CHECK_EQ(px(X + 4, NAV + 4), BG);
    CHECK_EQ(g_rowsClicks, clicks + 1);
}

TEST("render/rows: an icon by data, an empty text is no icon") {
    show();
    Span icon = span(ICON_ROW, RED);
    Span text = span(ICON_ROW, WHITE);
    REQUIRE(icon.found());
    CHECK(text.min > icon.max + GAP - 2);
    int textMin = text.min;

    g_rowsIcon = "";
    harness::frame(3);
    CHECK(!span(ICON_ROW, RED).found());
    text = span(ICON_ROW, WHITE);
    // the text takes the room of the icon and its padding
    CHECK(text.min >= X + PADDING);
    CHECK(text.min < textMin - GAP);
    CHECK_EQ(render::compareWithFullRedraw("rows_icon_empty"), 0);

    g_rowsIcon = "#";
    harness::frame(3);
    CHECK(span(ICON_ROW, RED).found());
    CHECK_EQ(render::compareWithFullRedraw("rows_icon_back"), 0);
}

TEST("render/rows: the unit ends before the trailing icon") {
    show();
    Span value = span(UNIT, WHITE);
    Span unit = span(UNIT, BLUE);
    Span trailing = span(UNIT, GREEN);
    REQUIRE(value.found());
    REQUIRE(unit.found());
    REQUIRE(trailing.found());
    CHECK(value.max < unit.min);
    CHECK(unit.max < trailing.min - GAP + 4);

    // a longer value moves left, the unit stays
    int unitMax = unit.max;
    int valueMin = value.min;
    g_rowsValue = 12.125f;
    harness::frame(3);
    CHECK_EQ(span(UNIT, BLUE).max, unitMax);
    CHECK(span(UNIT, WHITE).min < valueMin);
    CHECK_EQ(render::compareWithFullRedraw("rows_unit"), 0);
}

TEST("render/rows: icons without a style are in the font and the color of the widget") {
    show();
    CHECK(!span(PLAIN, RED).found());
    CHECK(!span(PLAIN, GREEN).found());
    // "<" at the left end, "mid" at the right before ">"
    Span all = span(PLAIN, WHITE);
    REQUIRE(all.found());
    CHECK(all.min >= X + PADDING);
    CHECK(all.min < X + PADDING + 12 + ICON_SLACK);
    CHECK(all.max <= X + W - 1 - PADDING);
    CHECK(all.max > X + W - 1 - PADDING - 12 - ICON_SLACK);
    // nothing in the middle of the row: the value is at the right
    CHECK(!span(PLAIN, WHITE, X + 60, X + W / 2).found());
}

TEST("render/rows: the Input has the icons, the label and the value") {
    show();
    Span icon = span(INPUT, RED);
    Span label = span(INPUT, YELLOW);
    Span value = span(INPUT, WHITE);
    Span trailing = span(INPUT, GREEN);
    REQUIRE(icon.found());
    REQUIRE(label.found());
    REQUIRE(value.found());
    REQUIRE(trailing.found());
    CHECK(icon.min >= X + PADDING);
    CHECK(label.min > icon.max + GAP - 2);
    CHECK(label.max < value.min);
    CHECK(value.max < trailing.min - GAP + 4);
    CHECK(trailing.max <= X + W - 1 - PADDING);
}

TEST("render/rows: a DropDownList with a label is a row with its box at the right") {
    show();
    // the row style has the padding 5 16: the box is 150 wide inside of it
    const int ROW_PADDING_X = 16;
    const int ROW_PADDING_Y = 5;
    int boxX2 = X + W - 1 - ROW_PADDING_X;
    int boxX1 = boxX2 - 150 + 1;
    CHECK_EQ(px(boxX1 + 4, DROPDOWN + ROW_PADDING_Y + 4), BLUE);
    CHECK_EQ(px(boxX2 - 4, DROPDOWN + H - ROW_PADDING_Y - 5), BLUE);
    CHECK_EQ(px(boxX1 - 4, DROPDOWN + H / 2), BG);
    // the padding of the row around the box
    CHECK_EQ(px(boxX2 + 4, DROPDOWN + H / 2), BG);
    CHECK_EQ(px(boxX2 - 20, DROPDOWN + 2), BG);
    CHECK_EQ(px(boxX2 - 20, DROPDOWN + H - 3), BG);
    // the label at the left of the row
    Span label = span(DROPDOWN, YELLOW);
    REQUIRE(label.found());
    CHECK(label.min >= X + ROW_PADDING_X);
    CHECK(label.min < X + ROW_PADDING_X + 12);
    CHECK(label.max < boxX1);
    // the selected option in the box
    CHECK(span(DROPDOWN, WHITE, boxX1 + 2, boxX2 - 2).found());

    // without a box width: half of the content box
    int halfX1 = boxX2 - (W - 2 * ROW_PADDING_X) / 2 + 1;
    CHECK_EQ(px(halfX1 + 4, DROPDOWN_HALF + H / 2), BLUE);
    CHECK_EQ(px(halfX1 - 4, DROPDOWN_HALF + H / 2), BG);

    // without a label: the whole widget
    CHECK_EQ(px(X + 4, DROPDOWN_PLAIN + 4), BLUE);
    CHECK(!span(DROPDOWN_PLAIN, YELLOW).found());
}

TEST("render/rows: a touch on the label of a DropDownList opens its list") {
    show();
    int pageId = harness::appContext()->getActivePageId();
    press(DROPDOWN, X + 20);
    release();
    CHECK(harness::appContext()->getActivePageId() != pageId);
    harness::appContext()->popPage();
    harness::frame(3);
}

TEST("render/rows: fit to content has the width of the icons, the gaps and the text") {
    show();
    // the widget ends where its background ends
    int w = 0;
    while (w < 300 && px(FIT_X + w, FIT_Y + 2) == BG) {
        w++;
    }
    CHECK(w > 60);
    CHECK(w < 200);
    Span icon = span(FIT_Y, RED, FIT_X, FIT_X + 299);
    Span text = span(FIT_Y, WHITE, FIT_X, FIT_X + 299);
    Span trailing = span(FIT_Y, GREEN, FIT_X, FIT_X + 299);
    REQUIRE(icon.found());
    REQUIRE(text.found());
    REQUIRE(trailing.found());
    CHECK(icon.min >= FIT_X + PADDING);
    CHECK(text.min > icon.max + GAP - 2);
    CHECK(trailing.min > text.max + GAP - 4);
    CHECK(trailing.max <= FIT_X + w - 1 - PADDING);
    CHECK(trailing.max > FIT_X + w - 1 - PADDING - 12 - ICON_SLACK);
}

TEST("render/rows: a value that doesn't fit next to the label is cut with an ellipsis") {
    show();
    Span label = span(LONG_ELLIPSIS, YELLOW, LONG_X, LONG_X + LONG_W - 1);
    Span value = span(LONG_ELLIPSIS, WHITE, LONG_X, LONG_X + LONG_W - 1);
    Span trailing = span(LONG_ELLIPSIS, GREEN, LONG_X, LONG_X + LONG_W - 1);
    REQUIRE(label.found());
    REQUIRE(value.found());
    REQUIRE(trailing.found());
    // the value starts after the label and ends before the trailing icon
    CHECK(value.min > label.max);
    CHECK(value.max < trailing.min);
    // it fills what is left
    CHECK(value.min < label.max + 30);
    CHECK(value.max > trailing.min - GAP - ICON_SLACK - 40);
}

TEST("render/rows: without the ellipsis a long value is not drawn over the label") {
    show();
    Span label = span(LONG_CLIP, YELLOW, LONG_X, LONG_X + LONG_W - 1);
    Span value = span(LONG_CLIP, WHITE, LONG_X, LONG_X + LONG_W - 1);
    REQUIRE(label.found());
    REQUIRE(value.found());
    CHECK(value.min > label.max);
    // right aligned: it ends at the right of the content box
    CHECK(value.max <= LONG_X + LONG_W - 1 - PADDING);
    CHECK(value.max > LONG_X + LONG_W - 1 - PADDING - 12);
}
