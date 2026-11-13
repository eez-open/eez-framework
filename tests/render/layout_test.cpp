/*
 * eez-framework tests: rows and columns (gui/layout.h, projects/parts/
 * layout.py): justify content, grow, align items and align self, a hidden
 * child, sizes from the content, the baseline of two fonts, a column of rows,
 * right to left. The children have their own background color; a child box is
 * the box of the pixels of its color in the area of its container.
 */

#include <string>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>
#include <eez/gui/font.h>

#include "document.h"
#include "harness.h"
#include "render_util.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;
using render::rgb;

////////////////////////////////////////////////////////////////////////////////
// native variables of the part

namespace {

bool g_layoutVisible = true;
std::string g_layoutText = "Text";

} // namespace

namespace eez {
namespace gui {

void data_layout_visible(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_layoutVisible, VALUE_TYPE_BOOLEAN);
    }
}

void data_layout_text(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value::makeStringRef(g_layoutText.c_str(), (int)g_layoutText.size(), 0x4C415900);
    }
}

} // namespace gui
} // namespace eez

namespace {

const uint32_t RED = rgb(0xF8, 0, 0);
const uint32_t GREEN = rgb(0, 0xFC, 0);
const uint32_t BLUE = rgb(0, 0, 0xF8);
const uint32_t GRAY = rgb(0x80, 0x80, 0x80);
const uint32_t PURPLE = rgb(0x80, 0, 0xF8);
const uint32_t ORANGE = rgb(0xF8, 0x80, 0);
const uint32_t YELLOW = rgb(0xF8, 0xFC, 0);

struct Box {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool operator==(const Box &other) const {
        return x == other.x && y == other.y && w == other.w && h == other.h;
    }
};

std::string toString(const Box &box) {
    return "(" + std::to_string(box.x) + ", " + std::to_string(box.y) + ", " + std::to_string(box.w) + ", " +
           std::to_string(box.h) + ")";
}

// the box of the pixels of the color in the area, w = 0 if there are none
Box find(uint32_t color, int x, int y, int w, int h) {
    int x1 = x + w;
    int y1 = y + h;
    int x2 = x - 1;
    int y2 = y - 1;
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if ((harness::pixel(px, py) | 0xFF000000u) == color) {
                if (px < x1) x1 = px;
                if (py < y1) y1 = py;
                if (px > x2) x2 = px;
                if (py > y2) y2 = py;
            }
        }
    }
    Box box;
    if (x2 >= x1) {
        box.x = x1;
        box.y = y1;
        box.w = x2 - x1 + 1;
        box.h = y2 - y1 + 1;
    }
    return box;
}

#define CHECK_BOX(BOX, X, Y, W, H) \
    do { \
        Box expected_; \
        expected_.x = X; \
        expected_.y = Y; \
        expected_.w = W; \
        expected_.h = H; \
        Box actual_ = BOX; \
        if (!(actual_ == expected_)) { \
            test::fail(__FILE__, __LINE__, std::string(#BOX ": ") + toString(actual_) + " != " + toString(expected_)); \
        } \
    } while (0)

// the rows of the page layout_rows (parts/layout.py): 400 x 50 at x = 20,
// y = 10 + 56 * k, the content box 5 px inside
int rowY(int k) {
    return 10 + 56 * k;
}

Box inRow(int k, uint32_t color) {
    return find(color, 20, rowY(k), 400, 50);
}

// the right side of the page: 340 wide at x = 440
Box inRight(int y, int h, uint32_t color) {
    return find(color, 440, y, 340, h);
}

void showRows() {
    g_isRTL = false;
    g_layoutVisible = true;
    g_layoutText = "Text";
    render::showPage(PAGE_ID_LAYOUT_ROWS);
}

int measure(int styleId, const char *text) {
    font::Font font = styleGetFont(getStyle(styleId));
    return display::measureStr(text, -1, font, 0);
}

} // namespace

////////////////////////////////////////////////////////////////////////////////

TEST("render/layout: a row places its children by justify content, with the gap") {
    showRows();
    // start
    CHECK_BOX(inRow(0, RED), 25, rowY(0) + 5, 40, 20);
    CHECK_BOX(inRow(0, GREEN), 75, rowY(0) + 5, 60, 30);
    CHECK_BOX(inRow(0, BLUE), 145, rowY(0) + 5, 30, 40);
    // center: 240 free, 120 before
    CHECK_BOX(inRow(1, RED), 145, rowY(1) + 5, 40, 20);
    CHECK_BOX(inRow(1, GREEN), 195, rowY(1) + 5, 60, 30);
    CHECK_BOX(inRow(1, BLUE), 265, rowY(1) + 5, 30, 40);
    // end
    CHECK_BOX(inRow(2, RED), 265, rowY(2) + 5, 40, 20);
    CHECK_BOX(inRow(2, BLUE), 385, rowY(2) + 5, 30, 40);
    // space between: 120 more between the children
    CHECK_BOX(inRow(3, RED), 25, rowY(3) + 5, 40, 20);
    CHECK_BOX(inRow(3, GREEN), 195, rowY(3) + 5, 60, 30);
    CHECK_BOX(inRow(3, BLUE), 385, rowY(3) + 5, 30, 40);
}

TEST("render/layout: a growing child takes the free space") {
    showRows();
    CHECK_BOX(inRow(4, RED), 25, rowY(4) + 5, 40, 20);
    CHECK_BOX(inRow(4, GREEN), 75, rowY(4) + 5, 300, 30);
    CHECK_BOX(inRow(4, BLUE), 385, rowY(4) + 5, 30, 40);
}

TEST("render/layout: align items and align self place the children on the cross axis") {
    showRows();
    // center in the 40 px content height
    CHECK_BOX(inRow(5, RED), 25, rowY(5) + 15, 40, 20);
    CHECK_BOX(inRow(5, GREEN), 75, rowY(5) + 10, 60, 30);
    CHECK_BOX(inRow(5, BLUE), 145, rowY(5) + 5, 30, 40);
    // end
    CHECK_BOX(inRow(6, RED), 25, rowY(6) + 25, 40, 20);
    CHECK_BOX(inRow(6, GREEN), 75, rowY(6) + 15, 60, 30);
    // stretch
    CHECK_BOX(inRow(7, RED), 25, rowY(7) + 5, 40, 40);
    CHECK_BOX(inRow(7, GREEN), 75, rowY(7) + 5, 60, 40);
    // align self: green at the end, blue (20 high) in the center
    CHECK_BOX(inRight(10, 50, RED), 445, 15, 40, 20);
    CHECK_BOX(inRight(10, 50, GREEN), 495, 25, 60, 30);
    CHECK_BOX(inRight(10, 50, BLUE), 565, 25, 30, 20);
}

TEST("render/layout: a hidden child takes no space, the row is drawn again") {
    showRows();
    CHECK_BOX(inRight(66, 50, GREEN), 495, 71, 60, 30);
    CHECK_BOX(inRight(66, 50, BLUE), 565, 71, 30, 40);

    g_layoutVisible = false;
    harness::frame();
    CHECK_EQ(inRight(66, 50, GREEN).w, 0);
    CHECK_BOX(inRight(66, 50, BLUE), 495, 71, 30, 40);
    CHECK_EQ(render::compareWithFullRedraw("layout_hidden", 440, 66, 340, 50), 0);

    g_layoutVisible = true;
    harness::frame();
    CHECK_BOX(inRight(66, 50, GREEN), 495, 71, 60, 30);
    CHECK_BOX(inRight(66, 50, BLUE), 565, 71, 30, 40);
    CHECK_EQ(render::compareWithFullRedraw("layout_shown", 440, 66, 340, 50), 0);
}

TEST("render/layout: a text as wide as its content moves the next child when it changes") {
    showRows();
    int w = measure(STYLE_ID_LAYOUT_TEXT, "Text");
    CHECK(w > 0);
    CHECK_BOX(inRight(122, 50, PURPLE), 445, 127, w, 40);
    CHECK_BOX(inRight(122, 50, BLUE), 445 + w + 10, 127, 30, 40);

    g_layoutText = "A longer text";
    harness::frame();
    int w2 = measure(STYLE_ID_LAYOUT_TEXT, "A longer text");
    CHECK(w2 > w);
    CHECK_BOX(inRight(122, 50, PURPLE), 445, 127, w2, 40);
    CHECK_BOX(inRight(122, 50, BLUE), 445 + w2 + 10, 127, 30, 40);
    CHECK_EQ(render::compareWithFullRedraw("layout_text_longer", 440, 122, 340, 50), 0);

    g_layoutText = "Text";
    harness::frame();
    CHECK_BOX(inRight(122, 50, BLUE), 445 + w + 10, 127, 30, 40);
    CHECK_EQ(render::compareWithFullRedraw("layout_text_shorter", 440, 122, 340, 50), 0);
}

TEST("render/layout: the first lines of texts in two fonts are on one baseline") {
    showRows();
    // the small font: height 26, ascent 21; the big font: height 52, ascent
    // 42; the rectangle (no text) has its bottom on the baseline
    int small = measure(STYLE_ID_LAYOUT_TEXT_SMALL, "Ag");
    int big = measure(STYLE_ID_LAYOUT_TEXT_BIG, "Ag");
    CHECK_EQ(getTextBaseline(getStyle(STYLE_ID_LAYOUT_TEXT_SMALL), 26), 21);
    CHECK_EQ(getTextBaseline(getStyle(STYLE_ID_LAYOUT_TEXT_BIG), 52), 42);
    CHECK_BOX(inRight(178, 70, YELLOW), 445 + small + 10, 183, big, 52);
    CHECK_BOX(inRight(178, 70, ORANGE), 445, 183 + 21, small, 26);
    CHECK_BOX(inRight(178, 70, RED), 445 + small + 10 + big + 10, 183 + 22, 40, 20);
}

TEST("render/layout: a column with a row as big as its children and a stretched row") {
    showRows();
    // inner_fit: 3 + 40 + 4 + 60 + 3 wide, 3 + 30 + 3 high
    CHECK_BOX(find(GRAY, 440, 260, 340, 42), 445, 265, 110, 36);
    CHECK_BOX(find(RED, 440, 260, 340, 42), 448, 268, 40, 20);
    CHECK_BOX(find(GREEN, 440, 260, 340, 42), 492, 268, 60, 30);
    // inner_wide: the content width of the column, 3 + 40 + 3 high, 6 below
    CHECK_BOX(find(GRAY, 440, 302, 340, 60), 445, 307, 330, 46);
    CHECK_BOX(find(BLUE, 440, 302, 340, 60), 448, 310, 30, 40);
}

TEST("render/layout: a column places its children on the y axis") {
    g_isRTL = false;
    render::showPage(PAGE_ID_LAYOUT_COLUMNS);
    // content 110 x 290 at (25, 25); end: 180 free; centered on x
    CHECK_BOX(find(RED, 20, 20, 120, 300), 60, 205, 40, 20);
    CHECK_BOX(find(GREEN, 20, 20, 120, 300), 50, 235, 60, 30);
    CHECK_BOX(find(BLUE, 20, 20, 120, 300), 65, 275, 30, 40);
}

TEST("render/layout: right to left mirrors a row") {
    showRows();
    g_isRTL = true;
    refreshScreen();
    harness::frame();
    // the page mirrors the row to x = 800 - (20 + 400), the row its children:
    // the first one on the right at 380 + 400 - (5 + 40)
    CHECK_BOX(find(RED, 380, rowY(0), 400, 50), 735, rowY(0) + 5, 40, 20);
    CHECK_BOX(find(GREEN, 380, rowY(0), 400, 50), 665, rowY(0) + 5, 60, 30);
    CHECK_BOX(find(BLUE, 380, rowY(0), 400, 50), 625, rowY(0) + 5, 30, 40);
    g_isRTL = false;
}

////////////////////////////////////////////////////////////////////////////////
// a value with a label, a unit and a quantity line (gui/draw.h drawValueParts,
// the page layout_parts)

namespace {

const uint32_t WHITE = rgb(0xF8, 0xFC, 0xF8);

void showParts() {
    g_isRTL = false;
    render::showPage(PAGE_ID_LAYOUT_PARTS);
}

// "LBL" above "12.345" and "mA": 4 + the widest line + 4
int partsWidth() {
    int label = measure(STYLE_ID_LAYOUT_PL, "LBL");
    int line = measure(STYLE_ID_LAYOUT_PV, "12.345") + 6 + measure(STYLE_ID_LAYOUT_PU, "mA");
    return 4 + (label > line ? label : line) + 4;
}

} // namespace

TEST("render/layout: the label is above the value, the unit after the value on its baseline") {
    showParts();
    // the widget at (20, 20): the label line from y = 24 (26 + the padding 2),
    // the value line from y = 52 (height 52, the baseline at 94)
    Box label = find(GREEN, 20, 20, 360, 110);
    Box value = find(WHITE, 20, 20, 360, 110);
    Box unit = find(YELLOW, 20, 20, 360, 110);
    CHECK(label.w > 0 && value.w > 0 && unit.w > 0);
    CHECK(label.x >= 24 && label.x <= 27);
    CHECK(label.y >= 24 && label.y + label.h <= 50);
    CHECK(value.x >= 24 && value.x <= 28);
    CHECK(value.y >= 52 && value.y + value.h <= 104);
    // digits and "mA" end on the baseline
    int valueBottom = value.y + value.h;
    int unitBottom = unit.y + unit.h;
    CHECK(valueBottom >= 93 && valueBottom <= 95);
    CHECK(unitBottom >= valueBottom - 1 && unitBottom <= valueBottom + 1);
    // the unit drawn 6 px after the advance of the value (its glyph starts
    // after its left side bearing)
    int unitX = 24 + measure(STYLE_ID_LAYOUT_PV, "12.345") + 6;
    CHECK(unit.x >= unitX && unit.x <= unitX + 3);
}

TEST("render/layout: a value with a label is as big as its content and on the baseline in a row") {
    showParts();
    int small = measure(STYLE_ID_LAYOUT_TEXT_SMALL, "Ag");
    CHECK_EQ(getTextBaseline(getStyle(STYLE_ID_LAYOUT_TEXT_SMALL), 26), 21);
    // 4 + 28 (the label) + 52 (the value line) + 4 high, the baseline at
    // 4 + 28 + 42 = 74: the text at 74 - 21 = 53
    CHECK_BOX(find(BLUE, 20, 150, 420, 110), 25 + small + 10, 155, partsWidth(), 88);
    CHECK_BOX(find(ORANGE, 20, 150, 420, 110), 25, 155 + 53, small, 26);
}

TEST("render/layout: an Input with a label and a quantity line, on the baseline in a row") {
    showParts();
    int small = measure(STYLE_ID_LAYOUT_TEXT_SMALL, "Ag");
    int x = 25 + small + 10;
    // inside the borders (1 px, the bottom 2 px)
    CHECK_BOX(find(BLUE, 20, 280, 420, 120), x + 1, 286, 198, 97);
    // the baseline at 1 + 4 + 28 (the label) + 42 = 75: the text at 75 - 21
    CHECK_BOX(find(ORANGE, 20, 280, 420, 120), 25, 285 + 54, small, 26);
    // the quantity line: the bottom border across the box, the top border in
    // the color of the other sides
    CHECK_BOX(find(RED, x, 285, 200, 100), x, 285 + 98, 200, 2);
    CHECK_BOX(find(PURPLE, x, 285, 200, 98), x, 285, 200, 98);
    Box label = find(GREEN, x, 285, 200, 100);
    CHECK(label.w > 0 && label.y >= 290 && label.y + label.h <= 290 + 26);
    Box value = find(WHITE, x, 285, 200, 100);
    CHECK(value.w > 0 && value.y >= 290 + 28 && value.y + value.h <= 290 + 28 + 52);
}

TEST("render/layout: a rounded box with a color for each side of its border") {
    // 460, 280, 200x100, radius 12: the bottom 4 px red, the others 2 px purple
    showParts();
    auto at = [](int x, int y) { return harness::pixel(x, y) | 0xFF000000u; };
    // the middle of each side
    CHECK_EQ(at(560, 280), PURPLE);
    CHECK_EQ(at(560, 281), PURPLE);
    CHECK_EQ(at(460, 330), PURPLE);
    CHECK_EQ(at(659, 330), PURPLE);
    for (int y = 376; y < 380; y++) {
        CHECK_EQ(at(560, y), RED);
    }
    CHECK_EQ(at(560, 375), BLUE);
    // the bottom border goes along the bottom into the corners, the side
    // borders end above them (the sides meet on the line from the outer to the
    // inner corner)
    CHECK_EQ(at(474, 378), RED);
    CHECK_EQ(at(645, 378), RED);
    CHECK_EQ(at(460, 360), PURPLE);
    Box red = find(RED, 460, 280, 200, 100);
    CHECK(red.w > 0 && red.y >= 280 + 100 - 16 && red.y + red.h == 380);
    Box purple = find(PURPLE, 460, 280, 200, 100);
    CHECK(purple.y == 280 && purple.y + purple.h <= 380 - 4);
}

TEST("render/layout: the page with the value parts is drawn as a full redraw") {
    showParts();
    CHECK_EQ(render::compareWithFullRedraw("layout_parts"), 0);
}

TEST("render/layout: right aligned values end at one x, the units are in the right padding") {
    showParts();
    // the widgets at x = 460, 300 wide, the right padding 40: the values end
    // at 460 + 300 - 1 - 40 (the last pixel one before), the units 6 px after
    Box v = find(WHITE, 460, 20, 300, 60);
    Box ma = find(WHITE, 460, 90, 300, 60);
    Box vUnit = find(YELLOW, 460, 20, 300, 60);
    Box maUnit = find(YELLOW, 460, 90, 300, 60);
    CHECK(v.w > 0 && ma.w > 0 && vUnit.w > 0 && maUnit.w > 0);
    int vEnd = v.x + v.w;
    int maEnd = ma.x + ma.w;
    CHECK(vEnd >= 715 && vEnd <= 719);
    CHECK(maEnd >= 715 && maEnd <= 719);
    int unitX = 719 + 6;
    CHECK(vUnit.x >= unitX && vUnit.x <= unitX + 3);
    CHECK(maUnit.x >= unitX && maUnit.x <= unitX + 3);
}
