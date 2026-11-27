/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * eez-framework tests: the focus icon (AppContext::getFocusIcon,
 * ValueParts::labelIcon) before the label of a focused DisplayData or Input, on
 * the pages rows_page (projects/parts/rows.mjs, the label at the left) and
 * layout_parts (projects/parts/layout.mjs, the label above the value).
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

namespace {

// the colors of the parts (rows.mjs, layout.mjs)
const uint32_t RED = rgb(0xF8, 0, 0);
const uint32_t GREEN = rgb(0, 0xFC, 0);
const uint32_t BLUE = rgb(0, 0, 0xF8);
const uint32_t YELLOW = rgb(0xF8, 0xFC, 0);
const uint32_t WHITE = rgb(0xF8, 0xFC, 0xF8);

// rows_page: the rows 400 x 40 at x = 20, the padding of the row style 16, of
// the icon style (rows_icon: red, the right padding 12) 12
const int X = 20;
const int W = 400;
const int H = 40;
const int PADDING = 16;
const int GAP = 12;
// a DisplayData "Voltage" (yellow) left of the value and the unit (blue)
const int UNIT = 160;
// an Input with an icon "#" (red) before the label "Limit" (yellow)
const int INPUT = 260;

// layout_parts: the DisplayData "LBL" (green, VRoboto 26 px high, from y = 24)
// above the value (white) at (20, 20) 360 x 110, and one without a label at
// (460, 20) 300 x 60
const int PARTS[] = { 20, 20, 360, 110 };
const int LABEL_TOP = 24;
const int LABEL_HEIGHT = 26;
const int NO_LABEL[] = { 460, 20, 300, 60 };

struct Box {
    int x1 = 100000;
    int y1 = 100000;
    int x2 = -1;
    int y2 = -1;
    int count = 0;
    bool found() const { return count > 0; }
};

// the pixels of the color in the area
Box find(uint32_t color, int x, int y, int w, int h) {
    Box box;
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if ((harness::pixel(px, py) | 0xFF000000u) == color) {
                if (px < box.x1) box.x1 = px;
                if (py < box.y1) box.y1 = py;
                if (px > box.x2) box.x2 = px;
                if (py > box.y2) box.y2 = py;
                box.count++;
            }
        }
    }
    return box;
}

Box inRow(int y, uint32_t color) {
    return find(color, X, y, W, H);
}

int g_focusX;
int g_focusY;

bool isFocused(const WidgetCursor &widgetCursor) {
    return widgetCursor.x == g_focusX && widgetCursor.y == g_focusY;
}

void focus(int x, int y) {
    g_focusX = x;
    g_focusY = y;
    harness::g_appContextOverrides.isFocusWidget = isFocused;
}

// "#" in the style rows_icon
const char *redIcon(const WidgetCursor &, const Style *&iconStyle) {
    iconStyle = getStyle(STYLE_ID_ROWS_ICON);
    return "#";
}

// "#" without a style
const char *plainIcon(const WidgetCursor &, const Style *&iconStyle) {
    iconStyle = nullptr;
    return "#";
}

} // namespace

TEST("render/focus_icon: the focus icon is before the label at the left, the value stays") {
    render::showPage(PAGE_ID_ROWS_PAGE);
    harness::g_appContextOverrides.getFocusIcon = redIcon;
    Box label = inRow(UNIT, YELLOW);
    Box unit = inRow(UNIT, BLUE);
    REQUIRE(label.found());
    REQUIRE(unit.found());
    CHECK(!inRow(UNIT, RED).found());

    focus(X, UNIT);
    harness::frame(2);
    Box icon = inRow(UNIT, RED);
    REQUIRE(icon.found());
    // at the left of the content box, the label after it and the gap
    CHECK(icon.x1 >= X + PADDING);
    CHECK(icon.x1 < X + PADDING + 12);
    Box focusedLabel = inRow(UNIT, YELLOW);
    CHECK(focusedLabel.x1 > icon.x2 + GAP - 2);
    CHECK_EQ(focusedLabel.count, label.count);
    // the value and the unit at the right don't move
    Box focusedUnit = inRow(UNIT, BLUE);
    CHECK_EQ(focusedUnit.x1, unit.x1);
    CHECK_EQ(focusedUnit.x2, unit.x2);
    CHECK_EQ(render::compareWithFullRedraw("focus_icon_left"), 0);

    // the focus moves away: no icon, the label back in its place
    harness::g_appContextOverrides.isFocusWidget = nullptr;
    harness::frame(2);
    CHECK(!inRow(UNIT, RED).found());
    CHECK_EQ(inRow(UNIT, YELLOW).x1, label.x1);
    CHECK_EQ(render::compareWithFullRedraw("focus_icon_left_away"), 0);
}

TEST("render/focus_icon: an Input has the focus icon between its icon and the label") {
    render::showPage(PAGE_ID_ROWS_PAGE);
    harness::g_appContextOverrides.getFocusIcon = redIcon;
    Box icon = inRow(INPUT, RED);
    Box label = inRow(INPUT, YELLOW);
    REQUIRE(icon.found());
    REQUIRE(label.found());

    focus(X, INPUT);
    harness::frame(2);
    // the icon of the widget where it was, the focus icon (the same text in
    // the same style) after it
    Box icons = inRow(INPUT, RED);
    CHECK_EQ(icons.x1, icon.x1);
    CHECK_EQ(icons.count, 2 * icon.count);
    Box focusedLabel = inRow(INPUT, YELLOW);
    CHECK(focusedLabel.x1 > icons.x2 + GAP - 2);
    CHECK(focusedLabel.x1 > label.x1 + GAP);
    CHECK_EQ(render::compareWithFullRedraw("focus_icon_input"), 0);
}

TEST("render/focus_icon: on the line of a label above the value") {
    render::showPage(PAGE_ID_LAYOUT_PARTS);
    harness::g_appContextOverrides.getFocusIcon = redIcon;
    Box label = find(GREEN, PARTS[0], PARTS[1], PARTS[2], PARTS[3]);
    Box value = find(WHITE, PARTS[0], PARTS[1], PARTS[2], PARTS[3]);
    REQUIRE(label.found());
    REQUIRE(value.found());

    focus(PARTS[0], PARTS[1]);
    harness::frame(2);
    Box icon = find(RED, PARTS[0], PARTS[1], PARTS[2], PARTS[3]);
    REQUIRE(icon.found());
    // in the line of the label, before it
    CHECK(icon.y1 >= LABEL_TOP);
    CHECK(icon.y2 < LABEL_TOP + LABEL_HEIGHT);
    CHECK(icon.x1 >= label.x1);
    Box focusedLabel = find(GREEN, PARTS[0], PARTS[1], PARTS[2], PARTS[3]);
    CHECK(focusedLabel.x1 > icon.x2 + GAP - 2);
    CHECK_EQ(focusedLabel.y1, label.y1);
    CHECK_EQ(focusedLabel.count, label.count);
    // the value doesn't move
    Box focusedValue = find(WHITE, PARTS[0], PARTS[1], PARTS[2], PARTS[3]);
    CHECK_EQ(focusedValue.x1, value.x1);
    CHECK_EQ(focusedValue.y1, value.y1);
    CHECK_EQ(focusedValue.count, value.count);
    CHECK_EQ(render::compareWithFullRedraw("focus_icon_above"), 0);
}

TEST("render/focus_icon: no focus icon without a label") {
    render::showPage(PAGE_ID_LAYOUT_PARTS);
    harness::g_appContextOverrides.getFocusIcon = redIcon;
    focus(NO_LABEL[0], NO_LABEL[1]);
    harness::frame(2);
    CHECK(find(WHITE, NO_LABEL[0], NO_LABEL[1], NO_LABEL[2], NO_LABEL[3]).found());
    CHECK(!find(RED, NO_LABEL[0], NO_LABEL[1], NO_LABEL[2], NO_LABEL[3]).found());
}

TEST("render/focus_icon: no focus icon from the default app context") {
    render::showPage(PAGE_ID_ROWS_PAGE);
    Box label = inRow(UNIT, YELLOW);
    REQUIRE(label.found());
    focus(X, UNIT);
    harness::frame(2);
    CHECK(!inRow(UNIT, RED).found());
    CHECK_EQ(inRow(UNIT, YELLOW).x1, label.x1);
    CHECK_EQ(inRow(UNIT, YELLOW).count, label.count);
}

TEST("render/focus_icon: without a style the icon is in the font and the color of the label") {
    render::showPage(PAGE_ID_ROWS_PAGE);
    harness::g_appContextOverrides.getFocusIcon = plainIcon;
    Box label = inRow(UNIT, YELLOW);
    REQUIRE(label.found());
    focus(X, UNIT);
    harness::frame(2);
    CHECK(!inRow(UNIT, RED).found());
    Box focused = inRow(UNIT, YELLOW);
    // "#" in yellow at the left of the content box (centered in a square of
    // the height of the label font, no gap), the label after it
    CHECK(focused.count > label.count);
    CHECK(focused.x1 >= X + PADDING);
    CHECK(focused.x2 > label.x2 + 10);
}
