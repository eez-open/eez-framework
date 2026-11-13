/*
 * eez-framework tests: the style properties of EEZ-GUI v3 (projects/parts/
 * styles.py): states (state styles, Style::stateStyles), focus
 * ring, shadow, borders with a radius and a different width on each side,
 * children clipped to the rounded corners of a container, padding of centered
 * text, letter spacing, line height and ellipsis.
 */

#include <stdlib.h>
#include <string.h>

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
// native variables and action of the part

namespace {

bool g_stylesVisible = true;
std::string g_stylesText = "Text";
int g_stylesClicks;

} // namespace

namespace eez {
namespace gui {

void data_styles_visible(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_stylesVisible, VALUE_TYPE_BOOLEAN);
    }
}

void data_styles_text(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value::makeStringRef(g_stylesText.c_str(), (int)g_stylesText.size(), 0x53545900);
    }
}

void action_styles_on_click() {
    g_stylesClicks++;
}

} // namespace gui
} // namespace eez

namespace {

uint32_t px(int x, int y) {
    return harness::pixel(x, y) | 0xFF000000u;
}

const uint32_t BG = rgb(0x20, 0x20, 0x20);
const uint32_t PAGE_BG = rgb(0x40, 0x60, 0x80);
const uint32_t WHITE = rgb(0xF8, 0xFC, 0xF8);
const uint32_t GRAY = rgb(0x80, 0x80, 0x80);
const uint32_t RED = rgb(0xF8, 0, 0);
const uint32_t GREEN = rgb(0, 0xFC, 0);
const uint32_t BLUE = rgb(0, 0, 0xF8);
const uint32_t YELLOW = rgb(0xF8, 0xFC, 0);
const uint32_t ORANGE = rgb(0xF8, 0x80, 0);
const uint32_t PURPLE = rgb(0x80, 0, 0xF8);

// the boxes of the pages (parts/styles.py)
const int BTN[] = { 20, 20, 160, 50 };
const int DISABLED[] = { 200, 20, 160, 50 };
const int TOGGLE[] = { 380, 20, 160, 50 };
const int GROUP[] = { 560, 20, 220, 50 };
const int CHECK[] = { 20, 90, 160, 50 };
const int PRIMARY[] = { 200, 90, 160, 50 };
const int FOCUS[] = { 380, 90, 160, 50 };
const int PLAIN[] = { 560, 90, 220, 50 };

const int CARD[] = { 40, 30, 200, 120 };
const int SIDES[] = { 300, 30, 200, 120 };
const int ROUNDED[] = { 40, 200, 200, 120 };
const int CENTER[] = { 300, 200, 200, 40 };
const int CENTER_PAD[] = { 300, 260, 200, 40 };

#define CENTER_OF(R) R[0] + R[2] / 2, R[1] + R[3] / 2

void showStates() {
    g_stylesVisible = true;
    g_stylesText = "Text";
    render::showPage(PAGE_ID_STYLES_STATES);
}

void showShapes() {
    g_stylesVisible = true;
    g_stylesText = "Text";
    render::showPage(PAGE_ID_STYLES_SHAPES);
}

void press(int x, int y) {
    harness::touch(x, y);
    harness::frame(2);
}

void release() {
    harness::release();
    harness::frame(2);
    // the next touch is not an accidental double tap
    harness::frame(25);
}

// the first column (from the left) in the rectangle with a pixel that is not
// the color, -1 if there is none
int firstColumnNot(int x, int y, int w, int h, uint32_t color) {
    for (int i = x; i < x + w; i++) {
        for (int j = y; j < y + h; j++) {
            if (px(i, j) != color) {
                return i;
            }
        }
    }
    return -1;
}

int luminance(uint32_t c) {
    return render::R(c) + render::G(c) + render::B(c);
}

const Style *style(int styleId) {
    return getStyle(styleId);
}

// the text drawn with drawText into a clean box, the box captured
render::Image32 drawTextImage(const char *text, int styleId, int w, int h) {
    render::resetDrawState();
    render::clearRect(0, 0, w + 19, h + 19, BG);
    drawText(text, -1, 10, 10, w, h, style(styleId));
    return render::captureBuffer((const uint32_t *)display::g_renderBuffer, 0, 0, w + 20, h + 20);
}

// The longest start of the text (whole characters) that fits into maxWidth
// with the ellipsis after it, with the ellipsis (see TextEllipsis in draw.cpp).
std::string cutWithEllipsis(const char *text, font::Font font, int maxWidth, const char *ellipsis) {
    int ellipsisWidth = display::measureStr(ellipsis, -1, font, 0);
    std::string prefix;
    std::string result = ellipsis;
    int n = (int)strlen(text);
    for (int i = 1; i <= n; i++) {
        std::string candidate(text, i);
        if (display::measureStr(candidate.c_str(), -1, font, 0) + ellipsisWidth > maxWidth) {
            break;
        }
        result = candidate + ellipsis;
    }
    return result;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// assets

TEST("render/styles: a style with state styles has their style ids") {
    auto s = style(STYLE_ID_STYLES_BTN);
    CHECK_EQ((int)s->stateStyles[STYLE_STATE_PRESSED], (int)STYLE_ID_STYLES_BTN_PRESSED);
    CHECK_EQ((int)s->stateStyles[STYLE_STATE_FOCUSED], (int)STYLE_ID_STYLES_BTN_FOCUSED);
    CHECK_EQ((int)s->stateStyles[STYLE_STATE_DISABLED], (int)STYLE_ID_STYLES_BTN_DISABLED);
    CHECK_EQ((int)s->stateStyles[STYLE_STATE_SELECTED], (int)STYLE_ID_STYLES_BTN_SELECTED);
    CHECK_EQ((int)s->stateStyles[STYLE_STATE_CHECKED], (int)STYLE_ID_STYLES_BTN_CHECKED);
}

TEST("render/styles: a style without states has none") {
    auto s = style(STYLE_ID_STYLES_PLAIN);
    for (int i = 0; i < STYLE_STATE_COUNT; i++) {
        CHECK_EQ((int)s->stateStyles[i], 0);
    }
    // the states are only of the base style of the state styles (styles_box
    // has the same properties as styles_btn except the background)
    s = style(STYLE_ID_STYLES_BOX);
    for (int i = 0; i < STYLE_STATE_COUNT; i++) {
        CHECK_EQ((int)s->stateStyles[i], 0);
    }
}

TEST("render/styles: a variant style with its own state styles, the same state style is one entry") {
    auto primary = style(STYLE_ID_STYLES_BTN_PRIMARY);

    // pressed: the background of styles_btn_pressed, the color of
    // styles_btn_primary_pressed
    REQUIRE(primary->stateStyles[STYLE_STATE_PRESSED] != 0);
    auto pressed = getStyle(primary->stateStyles[STYLE_STATE_PRESSED]);
    CHECK_EQ(pressed->backgroundColor, style(STYLE_ID_STYLES_BTN_PRESSED)->backgroundColor);
    CHECK_EQ(pressed->color, style(STYLE_ID_STYLES_BTN_PRIMARY_PRESSED)->color);
    CHECK(pressed->color != primary->color);

    // disabled: the same as styles_btn_disabled (background and color of the
    // state over the purple background), so it is that style
    CHECK_EQ((int)primary->stateStyles[STYLE_STATE_DISABLED], (int)STYLE_ID_STYLES_BTN_DISABLED);
}

TEST("render/styles: getStateStyle takes the first state in the order disabled, pressed, checked, selected, focused") {
    auto s = style(STYLE_ID_STYLES_BTN);
    unsigned applied;

    auto check = [&](unsigned states, int expectedStyleId, unsigned expectedApplied) {
        CHECK(getStateStyle(s, states, &applied) == style(expectedStyleId));
        CHECK_EQ(applied, expectedApplied);
    };

    check(0, STYLE_ID_STYLES_BTN, 0);
    check(STYLE_STATE_FLAG(STYLE_STATE_FOCUSED), STYLE_ID_STYLES_BTN_FOCUSED, STYLE_STATE_FLAG(STYLE_STATE_FOCUSED));
    check(
        STYLE_STATE_FLAG(STYLE_STATE_FOCUSED) | STYLE_STATE_FLAG(STYLE_STATE_SELECTED),
        STYLE_ID_STYLES_BTN_SELECTED, STYLE_STATE_FLAG(STYLE_STATE_SELECTED)
    );
    check(
        STYLE_STATE_FLAG(STYLE_STATE_SELECTED) | STYLE_STATE_FLAG(STYLE_STATE_CHECKED),
        STYLE_ID_STYLES_BTN_CHECKED, STYLE_STATE_FLAG(STYLE_STATE_CHECKED)
    );
    check(
        STYLE_STATE_FLAG(STYLE_STATE_CHECKED) | STYLE_STATE_FLAG(STYLE_STATE_PRESSED),
        STYLE_ID_STYLES_BTN_PRESSED, STYLE_STATE_FLAG(STYLE_STATE_PRESSED)
    );
    check(
        STYLE_STATE_FLAG(STYLE_STATE_PRESSED) | STYLE_STATE_FLAG(STYLE_STATE_DISABLED),
        STYLE_ID_STYLES_BTN_DISABLED, STYLE_STATE_FLAG(STYLE_STATE_DISABLED)
    );

    // a style without the state is the style itself
    CHECK(getStateStyle(style(STYLE_ID_STYLES_PLAIN), STYLE_STATE_FLAG(STYLE_STATE_PRESSED), &applied) == style(STYLE_ID_STYLES_PLAIN));
    CHECK_EQ(applied, 0u);
}

TEST("render/styles: pressed without a pressed state style swaps the colors, not over a transparent background") {
    auto state = getWidgetStateStyle(style(STYLE_ID_STYLES_BTN), true, true);
    CHECK(state.style == style(STYLE_ID_STYLES_BTN_PRESSED));
    CHECK(!state.active);

    // focused without a focused state style: the style itself
    auto plain = style(STYLE_ID_STYLES_PLAIN);
    state = getWidgetStateStyle(plain, true, true);
    CHECK(state.style == plain);
    CHECK(state.active);
    CHECK_EQ(getStyleColor(plain, true), plain->backgroundColor);
    CHECK_EQ(getStyleBackgroundColor(plain, true), plain->color);
    CHECK_EQ(getStyleColor(plain, false), plain->color);

    auto transparent = style(STYLE_ID_STYLES_PLAIN_TRANSPARENT);
    CHECK_EQ(getStyleColor(transparent, true), transparent->color);
    CHECK_EQ(getStyleBackgroundColor(transparent, true), transparent->backgroundColor);
}

TEST("render/styles: the new properties are in the style") {
    auto s = style(STYLE_ID_STYLES_SPACING);
    CHECK_EQ((int)s->letterSpacing, 3 * 64);

    s = style(STYLE_ID_STYLES_LINES);
    CHECK_EQ((int)s->lineHeight, 30);

    s = style(STYLE_ID_STYLES_ELLIPSIS);
    CHECK((s->flags & STYLE_FLAGS_ELLIPSIS) != 0);
    CHECK((style(STYLE_ID_STYLES_CENTER)->flags & STYLE_FLAGS_ELLIPSIS) == 0);

    s = style(STYLE_ID_STYLES_BTN_FOCUSED);
    CHECK_EQ((int)s->focusRingWidth, 2);
    CHECK_EQ((int)s->focusRingOffset, 3);
    CHECK(s->focusRingColor != TRANSPARENT_COLOR_INDEX);
    // not set: transparent, no ring
    CHECK_EQ((int)style(STYLE_ID_STYLES_PLAIN)->focusRingColor, (int)TRANSPARENT_COLOR_INDEX);

    s = style(STYLE_ID_STYLES_CARD);
    CHECK_EQ((int)s->shadowBlur, 8);
    CHECK_EQ((int)s->shadowOffsetX, 0);
    CHECK_EQ((int)s->shadowOffsetY, 4);
    CHECK_EQ((int)s->shadowSpread, 0);
    CHECK(styleHasShadow(s));
    CHECK(!styleHasShadow(style(STYLE_ID_STYLES_BOX)));
}

////////////////////////////////////////////////////////////////////////////////
// states

TEST("render/styles: golden states") {
    showStates();
    CHECK_GOLDEN("styles_states", render::captureFrame());
}

TEST("render/styles: the widgets are drawn in their states") {
    showStates();
    CHECK_EQ(px(BTN[0] + 4, BTN[1] + 4), BLUE);
    // no disabled style in the widget: the disabled state of its style
    CHECK_EQ(px(DISABLED[0] + 4, DISABLED[1] + 4), GRAY);
    // checked ToggleButton without a checked style: the checked state
    CHECK_EQ(px(TOGGLE[0] + 4, TOGGLE[1] + 4), GREEN);
    // the selected button (1 of 3) in the selected state, the others not
    int buttonWidth = GROUP[2] / 3;
    CHECK_EQ(px(GROUP[0] + 4, GROUP[1] + 4), BLUE);
    CHECK_EQ(px(GROUP[0] + buttonWidth + 4, GROUP[1] + 4), ORANGE);
    CHECK_EQ(px(GROUP[0] + 2 * buttonWidth + 4, GROUP[1] + 4), BLUE);
    // checked checkbox
    CHECK_EQ(px(CHECK[0] + 4, CHECK[1] + 4), GREEN);
    CHECK_EQ(px(PRIMARY[0] + 4, PRIMARY[1] + 4), PURPLE);
    // not focused
    CHECK_EQ(px(FOCUS[0], FOCUS[1] + 25), GRAY);
}

TEST("render/styles: a pressed widget is drawn in the pressed state") {
    showStates();

    press(CENTER_OF(BTN));
    CHECK_EQ(px(BTN[0] + 4, BTN[1] + 4), RED);
    CHECK_EQ(px(PRIMARY[0] + 4, PRIMARY[1] + 4), PURPLE);
    release();
    CHECK_EQ(px(BTN[0] + 4, BTN[1] + 4), BLUE);
    CHECK_EQ(g_stylesClicks > 0, true);

    // the pressed state of styles_btn and of styles_btn_primary: red, yellow text
    press(CENTER_OF(PRIMARY));
    CHECK_EQ(px(PRIMARY[0] + 4, PRIMARY[1] + 4), RED);
    int yellow = 0;
    for (int y = PRIMARY[1]; y < PRIMARY[1] + PRIMARY[3]; y++) {
        for (int x = PRIMARY[0]; x < PRIMARY[0] + PRIMARY[2]; x++) {
            if (px(x, y) == YELLOW) {
                yellow++;
            }
        }
    }
    CHECK(yellow > 20);
    release();
    CHECK_EQ(px(PRIMARY[0] + 4, PRIMARY[1] + 4), PURPLE);

    // a style without the pressed state: the color and the background swapped
    press(CENTER_OF(PLAIN));
    CHECK_EQ(px(PLAIN[0] + 4, PLAIN[1] + 4), WHITE);
    release();
    CHECK_EQ(px(PLAIN[0] + 4, PLAIN[1] + 4), BG);

    CHECK_EQ(render::compareWithFullRedraw("styles_pressed"), 0);
}

static bool isFocusText(const WidgetCursor &widgetCursor) {
    return widgetCursor.x == FOCUS[0] && widgetCursor.y == FOCUS[1];
}

TEST("render/styles: a focused widget is drawn in the focused state, with the focus ring") {
    harness::g_appContextOverrides.isFocusWidget = isFocusText;
    showStates();

    // the border of the focused state
    CHECK_EQ(px(FOCUS[0], FOCUS[1] + 25), YELLOW);
    // the background between the border and the ring, the ring 3 px inside
    // the edge, 2 px wide, the background inside of it
    CHECK_EQ(px(FOCUS[0] + 2, FOCUS[1] + 25), BLUE);
    CHECK_EQ(px(FOCUS[0] + 3, FOCUS[1] + 25), YELLOW);
    CHECK_EQ(px(FOCUS[0] + 4, FOCUS[1] + 25), YELLOW);
    CHECK_EQ(px(FOCUS[0] + 5, FOCUS[1] + 25), BLUE);
    CHECK_EQ(px(FOCUS[0] + FOCUS[2] - 1 - 3, FOCUS[1] + 25), YELLOW);

    CHECK_GOLDEN("styles_focus", render::captureRegion(FOCUS[0], FOCUS[1], FOCUS[2], FOCUS[3]));

    // the focus moves away
    harness::g_appContextOverrides.isFocusWidget = nullptr;
    harness::frame(2);
    CHECK_EQ(px(FOCUS[0], FOCUS[1] + 25), GRAY);
    CHECK_EQ(px(FOCUS[0] + 3, FOCUS[1] + 25), BLUE);
    CHECK_EQ(render::compareWithFullRedraw("styles_focus"), 0);
}

////////////////////////////////////////////////////////////////////////////////
// shapes

TEST("render/styles: golden shapes") {
    showShapes();
    CHECK_GOLDEN("styles_shapes", render::captureFrame());
}

TEST("render/styles: a shadow is drawn around the widget") {
    showShapes();

    int x = CARD[0] + CARD[2] / 2;
    int bottom = CARD[1] + CARD[3];

    // inside the box: the box
    CHECK_EQ(px(x, CARD[1] + 10), BG);
    // below the box (offset 4, blur 8): darker than the page, fading away
    CHECK(luminance(px(x, bottom + 1)) < luminance(PAGE_BG) - 60);
    CHECK(luminance(px(x, bottom + 1)) < luminance(px(x, bottom + 8)));
    CHECK(luminance(px(x, bottom + 8)) < luminance(PAGE_BG));
    // above the box less of it
    CHECK(luminance(px(x, CARD[1] - 2)) > luminance(px(x, bottom + 1)));
    // farther than its reach (3 sigma of the blur, plus the offset): the page
    CHECK_EQ(px(x, bottom + 4 + 13), PAGE_BG);
    CHECK_EQ(px(x, CARD[1] - 13), PAGE_BG);
    // in the rounded corner, outside of the box: shadow, not the page
    CHECK(px(CARD[0], CARD[1] + CARD[3] - 1) != PAGE_BG);
}

TEST("render/styles: the shadow goes with its widget") {
    showShapes();
    int x = CARD[0] + CARD[2] / 2;
    int bottom = CARD[1] + CARD[3];

    g_stylesVisible = false;
    harness::frame(2);
    CHECK_EQ(px(x, bottom + 1), PAGE_BG);
    CHECK_EQ(px(x, CARD[1] + 10), PAGE_BG);
    CHECK_EQ(render::compareWithFullRedraw("styles_shadow_hidden"), 0);

    g_stylesVisible = true;
    harness::frame(2);
    CHECK(luminance(px(x, bottom + 1)) < luminance(PAGE_BG) - 60);
    CHECK_EQ(render::compareWithFullRedraw("styles_shadow_shown"), 0);
}

TEST("render/styles: a rounded border with a different width on each side") {
    showShapes();

    int midX = SIDES[0] + SIDES[2] / 2;
    int midY = SIDES[1] + SIDES[3] / 2;
    int right = SIDES[0] + SIDES[2] - 1;
    int bottom = SIDES[1] + SIDES[3] - 1;

    // top 1, right 4, bottom 2, left 8
    CHECK_EQ(px(midX, SIDES[1]), YELLOW);
    CHECK_EQ(px(midX, SIDES[1] + 1), BG);
    CHECK_EQ(px(right, midY), YELLOW);
    CHECK_EQ(px(right - 3, midY), YELLOW);
    CHECK_EQ(px(right - 4, midY), BG);
    CHECK_EQ(px(midX, bottom), YELLOW);
    CHECK_EQ(px(midX, bottom - 1), YELLOW);
    CHECK_EQ(px(midX, bottom - 2), BG);
    CHECK_EQ(px(SIDES[0], midY), YELLOW);
    CHECK_EQ(px(SIDES[0] + 7, midY), YELLOW);
    CHECK_EQ(px(SIDES[0] + 8, midY), BG);

    // rounded: the corner pixel is the page
    CHECK_EQ(px(SIDES[0], SIDES[1]), PAGE_BG);
    CHECK_EQ(px(right, bottom), PAGE_BG);
}

TEST("render/styles: the children of a rounded container are inside of its corners") {
    showShapes();

    int x1 = ROUNDED[0];
    int y1 = ROUNDED[1];
    int x2 = ROUNDED[0] + ROUNDED[2] - 1;
    int y2 = ROUNDED[1] + ROUNDED[3] - 1;

    // the child fills the container, the corners are not covered
    CHECK_EQ(px(x1, y1), PAGE_BG);
    CHECK_EQ(px(x2, y1), PAGE_BG);
    CHECK_EQ(px(x1, y2), PAGE_BG);
    CHECK_EQ(px(x2, y2), PAGE_BG);
    CHECK_EQ(px(x1 + 2, y1 + 2), PAGE_BG);
    // the border along the straight sides, the child inside of it
    CHECK_EQ(px(x1 + ROUNDED[2] / 2, y1), WHITE);
    CHECK_EQ(px(x1 + ROUNDED[2] / 2, y1 + 10), RED);
    CHECK_EQ(px(x1, y1 + ROUNDED[3] / 4), WHITE);

    // no red pixel outside of the rounded inside of the border: in the corner
    // squares, farther from the center of the corner than the inner radius
    // (16 - 1) and 1 px
    int reds = 0;
    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 16; dx++) {
            float cx = 16.0f - (dx + 0.5f);
            float cy = 16.0f - (dy + 0.5f);
            if (cx * cx + cy * cy > 16.0f * 16.0f) {
                if (px(x1 + dx, y1 + dy) == RED || px(x2 - dx, y2 - dy) == RED) {
                    reds++;
                }
            }
        }
    }
    CHECK_EQ(reds, 0);
}

TEST("render/styles: a child of a rounded container drawn again stays inside of the corners") {
    showShapes();

    g_stylesText = "Changed";
    harness::frame(2);
    CHECK_EQ(px(ROUNDED[0], ROUNDED[1] + ROUNDED[3] - 1), PAGE_BG);
    CHECK_EQ(render::compareWithFullRedraw("styles_rounded_child"), 0);
}

TEST("render/styles: padding moves centered text") {
    showShapes();
    int x = firstColumnNot(CENTER[0], CENTER[1], CENTER[2], CENTER[3], BG);
    int xPad = firstColumnNot(CENTER_PAD[0], CENTER_PAD[1], CENTER_PAD[2], CENTER_PAD[3], BG);
    REQUIRE(x != -1);
    REQUIRE(xPad != -1);
    // padding left 60: the text is centered in the 140 px right of it
    CHECK_EQ(xPad - x, 30);
}

////////////////////////////////////////////////////////////////////////////////
// text

TEST("render/styles: letter spacing is between the characters") {
    font::Font font = styleGetFont(style(STYLE_ID_STYLES_SPACING));
    CHECK_EQ(font.letterSpacing, 3 * 64);

    font::Font plain = font;
    plain.letterSpacing = 0;

    CHECK_EQ(display::measureStr("Spacing", -1, font), display::measureStr("Spacing", -1, plain) + 6 * 3);
    CHECK_EQ(display::measureStr("S", -1, font), display::measureStr("S", -1, plain));

    // the cursor positions include it
    CHECK_EQ(display::getCursorXPosition(7, "Spacing", -1, 0, 0, 0, 0, 800, 480, font), display::measureStr("Spacing", -1, font));
    CHECK_EQ(
        display::getCursorXPosition(1, "Spacing", -1, 0, 0, 0, 0, 800, 480, font),
        display::getCursorXPosition(1, "Spacing", -1, 0, 0, 0, 0, 800, 480, plain) + 3
    );

    // vector font: with kerning and ligatures
    font::Font vector(getFontData(FONT_ID_VROBOTO));
    int width = display::measureStr("AVATAR fi", -1, vector);
    vector.letterSpacing = 2 * 64;
    // 9 characters, "fi" is one glyph: 7 gaps
    CHECK_EQ(display::measureStr("AVATAR fi", -1, vector), width + 7 * 2);
}

TEST("render/styles: line height of multiline text") {
    auto lines = style(STYLE_ID_STYLES_LINES);
    font::Font font = styleGetFont(lines);
    const char *text = "One two three four five six seven";

    int height = measureMultilineText(text, 0, 0, 80, 400, lines, 0, 0);
    CHECK(height > font.getHeight() + 30);
    CHECK_EQ((height - font.getHeight()) % 30, 0);
}

TEST("render/styles: text that doesn't fit ends with an ellipsis") {
    const char *text = "A text that does not fit";

    // the bitmap font has no U+2026: three dots, 152 px inside the padding
    font::Font font = styleGetFont(style(STYLE_ID_STYLES_ELLIPSIS));
    std::string expected = cutWithEllipsis(text, font, 160 - 8, "...");
    CHECK(expected.size() > 3 && expected.size() < strlen(text));
    auto actual = drawTextImage(text, STYLE_ID_STYLES_ELLIPSIS, 160, 40);
    auto reference = drawTextImage(expected.c_str(), STYLE_ID_STYLES_ELLIPSIS, 160, 40);
    CHECK_EQ(render::countDiff(actual, reference), 0);

    // the vector font has it
    font::Font vector = styleGetFont(style(STYLE_ID_STYLES_ELLIPSIS_V));
    expected = cutWithEllipsis(text, vector, 160 - 8, "\xE2\x80\xA6");
    actual = drawTextImage(text, STYLE_ID_STYLES_ELLIPSIS_V, 160, 40);
    reference = drawTextImage(expected.c_str(), STYLE_ID_STYLES_ELLIPSIS_V, 160, 40);
    CHECK_EQ(render::countDiff(actual, reference), 0);

    // text that fits is not changed
    actual = drawTextImage("Fits", STYLE_ID_STYLES_ELLIPSIS, 160, 40);
    reference = drawTextImage("Fits", STYLE_ID_STYLES_CENTER, 160, 40);
    CHECK(render::countDiff(actual, reference) > 0); // left aligned vs centered
    actual = drawTextImage("Fits", STYLE_ID_STYLES_ELLIPSIS_CENTER, 160, 40);
    CHECK_EQ(render::countDiff(actual, reference), 0);

    // nothing of the text fits: only the ellipsis
    actual = drawTextImage("Long", STYLE_ID_STYLES_ELLIPSIS, 18, 40);
    reference = drawTextImage("...", STYLE_ID_STYLES_ELLIPSIS, 18, 40);
    CHECK_EQ(render::countDiff(actual, reference), 0);
}
