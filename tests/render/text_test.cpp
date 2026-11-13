/*
 * eez-framework tests: text measuring and drawing (display::measureStr,
 * drawStr, font tags, UTF-8, missing glyphs; draw.cpp drawText alignment,
 * cursor and character positions, multiline layout).
 *
 * Font RobotoCondensed (FONT_ID_ROBOTO_CONDENSED, ascent 21, descent 5) has the
 * glyphs 32..126 and 128: 'A' dx 11 (bitmap 11 x 14 on the baseline), 'W' dx
 * 14, ' ' dx 4. Icons_WaveformS (FONT_ID_ICONS_WAVEFORM_S = 2) has 195..203,
 * dx 39. A font tag is U+E000 followed by the code point of the font index,
 * U+E001 ends it, U+E002 is the tag of an external assets font.
 */

#include <string.h>

#include <algorithm>
#include <string>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>
#include <eez/gui/draw.h>

#include "document.h"
#include "harness.h"
#include "render_util.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;
using render::rgb;

// UTF-8 of the tags
#define TAG_START "\xEE\x80\x80"
#define TAG_END "\xEE\x80\x81"
#define TAG_EXTERNAL "\xEE\x80\x82"
#define ICON_FONT "\x02"
// U+00C3 (an icon of Icons_WaveformS, missing in RobotoCondensed)
#define ICON "\xC3\x83"

namespace {

font::Font roboto() {
    return font::Font(getFontData(FONT_ID_ROBOTO_CONDENSED));
}

int width(const char *text, int textLength = -1) {
    auto font = roboto();
    return display::measureStr(text, textLength, font);
}

uint32_t px(int x, int y) {
    return harness::pixel(x, y) | 0xFF000000u;
}

bool drawnBounds(uint32_t background, int x1, int y1, int x2, int y2, int &bx1, int &by1, int &bx2, int &by2) {
    bx1 = by1 = 1 << 30;
    bx2 = by2 = -1;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if (px(x, y) != background) {
                bx1 = std::min(bx1, x);
                by1 = std::min(by1, y);
                bx2 = std::max(bx2, x);
                by2 = std::max(by2, y);
            }
        }
    }
    return bx2 >= 0;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// glyphs and measuring

TEST("render/text: glyph advance of present, missing and grouped glyphs") {
    auto font = roboto();
    CHECK_EQ((int)display::measureGlyph('A', font), 11);
    CHECK_EQ((int)display::measureGlyph('W', font), 14);
    CHECK_EQ((int)display::measureGlyph(' ', font), 4);
    // 127 is missing between 126 and 128, 128 is a group of its own
    CHECK_EQ((int)display::measureGlyph(127, font), 0);
    CHECK_EQ((int)display::measureGlyph(128, font), 8);
    CHECK_EQ((int)display::measureGlyph(0x20AC, font), 0);
    CHECK_EQ((int)display::measureGlyph(31, font), 0);
    font::Font icons(getFontData(FONT_ID_ICONS_WAVEFORM_S));
    CHECK_EQ((int)display::measureGlyph(195, icons), 39);
    CHECK_EQ((int)display::measureGlyph('A', icons), 0);
}

TEST("render/text: font metrics") {
    auto font = roboto();
    CHECK_EQ((int)font.getAscent(), 21);
    CHECK_EQ((int)font.getDescent(), 5);
    CHECK_EQ((int)font.getHeight(), 26);
    CHECK(!font::Font());
    CHECK((bool)font);
}

TEST("render/text: measureStr sums the advances, textLength limits it") {
    CHECK_EQ(width("AW"), 25);
    CHECK_EQ(width("A W"), 29);
    CHECK_EQ(width("AWA", 2), 25);
    CHECK_EQ(width(""), 0);
    CHECK_EQ(width("AWA", 0), 0);
}

TEST("render/text: measureStr with max_width returns max_width when the text is wider") {
    auto font = roboto();
    CHECK_EQ(display::measureStr("AAAA", -1, font, 30), 30);
    CHECK_EQ(display::measureStr("AA", -1, font, 30), 22);
    CHECK_EQ(display::measureStr("AAAA", 4, font, 30), 30);
}

TEST("render/text: measureStr decodes UTF-8, missing glyphs have no width") {
    // U+00C4 (2 bytes), U+20AC (3 bytes), U+1F600 (4 bytes) are not in the font
    CHECK_EQ(width("\xC3\x84" "A"), 11);
    CHECK_EQ(width("\xE2\x82\xAC" "A"), 11);
    CHECK_EQ(width("\xF0\x9F\x98\x80" "A"), 11);
    // U+0080 is in the font (glyph 128)
    CHECK_EQ(width("\xC2\x80"), 8);
}

TEST("render/text: measureStr textLength counts code points, not bytes") {
    // two code points in three bytes
    CHECK_EQ(width("\xC3\x84" "A", 2), 11);
    CHECK_EQ(width("\xC3\x84" "A", 1), 0);
    // strlen (bytes) works only because the loop stops at the NUL
    CHECK_EQ(width("\xC3\x84" "A", 3), 11);
}

TEST("render/text: measureStr with font tags") {
    CHECK_EQ(width("A" TAG_START ICON_FONT ICON), 11 + 39);
    CHECK_EQ(width("A" TAG_START ICON_FONT ICON TAG_END "A"), 11 + 39 + 11);
    // the icon in the default font is missing
    CHECK_EQ(width("A" ICON), 11);
    // a letter in the icon font is missing
    CHECK_EQ(width(TAG_START ICON_FONT "A"), 0);
}

TEST("render/text: measureStr starting in another font") {
    auto font = roboto();
    CHECK_EQ(display::measureStr(ICON, -1, font, 0, FONT_ID_ICONS_WAVEFORM_S), 39);
    CHECK_EQ(display::measureStr(ICON TAG_END "A", -1, font, 0, FONT_ID_ICONS_WAVEFORM_S), 39 + 11);
}

TEST("render/text: font tag of an unknown font or of an external font is the default font") {
    // index 99 ('c') is not a font of the main assets
    CHECK_EQ(width(TAG_START "c" "A"), 11);
    // U+E002 is an external assets font, there is none with the main assets
    render::resetDrawState();
    CHECK_EQ(width(TAG_EXTERNAL ICON_FONT "A"), 11);
}

TEST("render/text: getFontIndexAtEnd follows the tags") {
    CHECK_EQ(display::getFontIndexAtEnd("a" TAG_START ICON_FONT "bc", -1, 0), 2);
    CHECK_EQ(display::getFontIndexAtEnd(TAG_START ICON_FONT "a" TAG_END, -1, 0), 0);
    CHECK_EQ(display::getFontIndexAtEnd("abc", -1, 5), 5);
    CHECK_EQ(display::getFontIndexAtEnd(TAG_START ICON_FONT "a" TAG_END, 3, 0), 2);
    CHECK_EQ(display::getFontIndexAtEnd(TAG_EXTERNAL ICON_FONT, -1, 0), -2);
}

////////////////////////////////////////////////////////////////////////////////
// drawStr

TEST("render/text: drawStr puts the glyph on the baseline at the ascent") {
    render::resetDrawState();
    render::clearRect(0, 0, 199, 99);
    auto font = roboto();
    display::setColorByValue(rgb(255, 255, 255));
    display::drawStr("A", -1, 50, 20, 0, 0, 799, 479, font, -1);
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0, 0), 0, 0, 199, 99, bx1, by1, bx2, by2));
    // 'A': x 0, y 0, 11 x 14 -> rows 20 + 21 - 14 .. 20 + 21 - 1
    CHECK(bx1 >= 50 && bx2 <= 60);
    CHECK_EQ(by1, 27);
    CHECK_EQ(by2, 40);
}

TEST("render/text: drawStr is clipped to its clip rectangle and the clip rectangle") {
    render::resetDrawState();
    render::clearRect(0, 0, 199, 99);
    auto font = roboto();
    display::setColorByValue(rgb(255, 255, 255));
    display::drawStr("WWWW", -1, 10, 10, 15, 0, 30, 30, font, -1);
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0, 0), 0, 0, 199, 99, bx1, by1, bx2, by2));
    CHECK(bx1 >= 15);
    CHECK(bx2 <= 30);
    CHECK(by2 <= 30);
    render::clearRect(0, 0, 199, 99);
    auto saved = display::intersectClipRect(0, 0, 20, 100);
    display::drawStr("WWWW", -1, 10, 10, 0, 0, 799, 479, font, -1);
    display::restoreClipRect(saved);
    REQUIRE(drawnBounds(rgb(0, 0, 0), 0, 0, 199, 99, bx1, by1, bx2, by2));
    CHECK(bx2 <= 19);
}

TEST("render/text: drawStr draws the cursor after the character at cursorPosition - 1") {
    render::resetDrawState();
    render::clearRect(0, 0, 199, 99);
    auto font = roboto();
    display::setColorByValue(rgb(255, 255, 255));
    // "A " and the cursor at 2: x 50 + 11 + 4, CURSOR_WIDTH 2 centered on it
    display::drawStr("A ", -1, 50, 10, 0, 10, 199, 60, font, 2);
    CHECK_EQ(px(64, 35), rgb(255, 255, 255));
    CHECK_EQ(px(65, 35), rgb(255, 255, 255));
    CHECK_EQ(px(66, 35), rgb(0, 0, 0));
    CHECK_EQ(px(63, 35), rgb(0, 0, 0));
}

TEST("render/text: drawStr with font tags draws the icon in the icon font") {
    render::resetDrawState();
    render::clearRect(0, 0, 199, 99);
    auto font = roboto();
    display::setColorByValue(rgb(255, 255, 255));
    display::drawStr(TAG_START ICON_FONT ICON, -1, 10, 10, 0, 0, 799, 479, font, -1);
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0, 0), 0, 0, 199, 99, bx1, by1, bx2, by2));
    CHECK(bx2 - bx1 > 30);
    // the same without the tag: nothing (missing glyph)
    render::clearRect(0, 0, 199, 99);
    display::drawStr(ICON, -1, 10, 10, 0, 0, 799, 479, font, -1);
    CHECK(!drawnBounds(rgb(0, 0, 0), 0, 0, 199, 99, bx1, by1, bx2, by2));
}

////////////////////////////////////////////////////////////////////////////////
// drawText (style alignment)

TEST("render/text: drawText left/top alignment uses the padding") {
    render::resetDrawState();
    // render_pad: padding top 2, right 8, bottom 4, left 16, teal, left/top
    drawText("A", -1, 100, 100, 100, 50, render::style(STYLE_ID_RENDER_PAD));
    CHECK_EQ(px(100, 100), rgb(0, 0x80, 0x80));
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0x80, 0x80), 100, 100, 199, 149, bx1, by1, bx2, by2));
    CHECK_EQ(bx1, 116);
    CHECK_EQ(by1, 100 + 2 + 21 - 14);
}

TEST("render/text: drawText right/bottom alignment leaves one pixel more than the padding") {
    // x_offset = x2 - paddingRight - width (x2 inclusive), as in Studio's drawText
    render::resetDrawState();
    drawText("A", -1, 100, 100, 100, 50, render::style(STYLE_ID_RENDER_PAD_RB));
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0x80, 0x80), 100, 100, 199, 149, bx1, by1, bx2, by2));
    // text box: x 199 - 8 - 11 = 180 .. 190, glyph rows: y 149 - 4 - 26 = 119, + 7 .. + 20
    CHECK_EQ(bx2, 190);
    CHECK_EQ(by2, 119 + 20);
}

TEST("render/text: drawText centered") {
    render::resetDrawState();
    // render_cell_cc: navy, border 1, padding 4 (the padding is not used when centered)
    drawText("A", -1, 100, 100, 101, 50, render::style(STYLE_ID_RENDER_CELL_CC));
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0, 0x80), 101, 101, 199, 148, bx1, by1, bx2, by2));
    // inside the border: x 101 .. 199 (99), (99 - 11) / 2 = 44
    CHECK_EQ(bx1, 145);
    // y 101 .. 148 (48), (48 - 26) / 2 = 11, glyph at + 7
    CHECK_EQ(by1, 101 + 11 + 7);
}

TEST("render/text: drawText of a text wider than the box starts at the left edge when centered") {
    render::resetDrawState();
    drawText("WWWWWWWWWW", -1, 100, 100, 50, 30, render::style(STYLE_ID_RENDER_CLIP));
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0x80, 0, 0xF8), 100, 100, 149, 129, bx1, by1, bx2, by2));
    CHECK_EQ(bx1, 100);
    CHECK_EQ(bx2, 149);
    CHECK_EQ(px(150, 115), harness::pixel(150, 115) | 0xFF000000u);
}

TEST("render/text: drawText with a transparent color draws only the background") {
    render::resetDrawState();
    render::clearRect(90, 90, 210, 160);
    Style style = *render::style(STYLE_ID_RENDER_DYN);
    style.color = TRANSPARENT_COLOR_INDEX;
    drawText("WWW", -1, 100, 100, 100, 50, &style);
    int bx1, by1, bx2, by2;
    CHECK(!drawnBounds(rgb(0, 0, 0x80), 100, 100, 199, 149, bx1, by1, bx2, by2));
    CHECK_EQ(px(150, 125), rgb(0, 0, 0x80));
}

TEST("render/text: drawText with a style without a font draws only the background") {
    render::resetDrawState();
    Style style = *render::style(STYLE_ID_RENDER_DYN);
    style.font = 0;
    drawText("WWW", -1, 100, 100, 100, 50, &style);
    int bx1, by1, bx2, by2;
    CHECK(!drawnBounds(rgb(0, 0, 0x80), 100, 100, 199, 149, bx1, by1, bx2, by2));
}

TEST("render/text: drawText with the style opacity: the background is blended, the text is not") {
    render::resetDrawState();
    render::clearRect(90, 90, 210, 160, rgb(0, 0, 0));
    // render_op_text: navy background, yellow text, opacity 128
    drawText("WWWWW", -1, 100, 100, 100, 50, render::style(STYLE_ID_RENDER_OP_TEXT));
    CHECK_NEAR(render::B(px(101, 101)), 0x80 * 128 / 255.0, 1);
    int yellow = 0;
    for (int y = 100; y < 150; y++) {
        for (int x = 100; x < 200; x++) {
            if (px(x, y) == rgb(0xF8, 0xFC, 0)) yellow++;
        }
    }
    CHECK(yellow > 20);
}

////////////////////////////////////////////////////////////////////////////////
// character positions

TEST("render/text: getCursorXPosition and getCharIndexAtPosition of a left aligned text") {
    auto style = render::style(STYLE_ID_RENDER_LEFT_TOP);
    // "AWA" at x 100: A 100..110, W 111..124, A 125..135
    CHECK_EQ(getCursorXPosition(0, "AWA", -1, 100, 0, 200, 30, style), 100);
    CHECK_EQ(getCursorXPosition(1, "AWA", -1, 100, 0, 200, 30, style), 111);
    CHECK_EQ(getCursorXPosition(2, "AWA", -1, 100, 0, 200, 30, style), 125);
    CHECK_EQ(getCursorXPosition(3, "AWA", -1, 100, 0, 200, 30, style), 136);
    // the index of the character whose middle is right of the position
    CHECK_EQ(getCharIndexAtPosition(104, "AWA", -1, 100, 0, 200, 30, style), 0);
    CHECK_EQ(getCharIndexAtPosition(106, "AWA", -1, 100, 0, 200, 30, style), 1);
    CHECK_EQ(getCharIndexAtPosition(120, "AWA", -1, 100, 0, 200, 30, style), 2);
    CHECK_EQ(getCharIndexAtPosition(140, "AWA", -1, 100, 0, 200, 30, style), 3);
    CHECK_EQ(getCharIndexAtPosition(0, "AWA", -1, 100, 0, 200, 30, style), 0);
}

TEST("render/text: getCursorXPosition counts code points of UTF-8 text") {
    auto style = render::style(STYLE_ID_RENDER_LEFT_TOP);
    // U+00C4 is missing (0 wide): "AÄA"
    CHECK_EQ(getCursorXPosition(2, "A\xC3\x84" "A", -1, 100, 0, 200, 30, style), 111);
    CHECK_EQ(getCursorXPosition(3, "A\xC3\x84" "A", -1, 100, 0, 200, 30, style), 122);
}

TEST("render/text: getCursorXPosition with font tags") {
    auto style = render::style(STYLE_ID_RENDER_LEFT_TOP);
    // code points: 0 'A', 1 tag, 2 font index, 3 icon, 4 end tag, 5 'A'
    const char *text = "A" TAG_START ICON_FONT ICON TAG_END "A";
    CHECK_EQ(getCursorXPosition(1, text, -1, 100, 0, 300, 30, style), 111);
    CHECK_EQ(getCursorXPosition(4, text, -1, 100, 0, 300, 30, style), 150);
    CHECK_EQ(getCursorXPosition(6, text, -1, 100, 0, 300, 30, style), 161);
}

TEST("render/text: getCursorXPosition of a centered text") {
    // render_cell_cc: border 1, centered: "AW" (25) in 100 - 2 = 98 -> x 101 + 36
    auto style = render::style(STYLE_ID_RENDER_CELL_CC);
    CHECK_EQ(getCursorXPosition(0, "AW", -1, 100, 0, 100, 30, style), 137);
    CHECK_EQ(getCursorXPosition(2, "AW", -1, 100, 0, 100, 30, style), 162);
}

////////////////////////////////////////////////////////////////////////////////
// multiline text

TEST("render/text: measureMultilineText of one line, lines and paragraphs") {
    // line height 0.9 * 26 = 23; height = lines * 23 + 26 - 23, a paragraph adds 0.2 * 23
    auto style = render::style(STYLE_ID_RENDER_ML);
    CHECK_EQ(measureMultilineText("Hello", 0, 0, 400, 300, style, 0, 0), 26);
    CHECK_EQ(measureMultilineText("Hello\nWorld", 0, 0, 400, 300, style, 0, 0), 23 + 4 + 26);
    // 100 px wide box (90 inside the border and padding): two words of 5 'W' (70) each
    CHECK_EQ(measureMultilineText("WWWWW WWWWW", 0, 0, 100, 300, style, 0, 0), 23 + 26);
    CHECK_EQ(measureMultilineText("", 0, 0, 400, 300, style, 0, 0), 26 - 23);
}

TEST("render/text: measureMultilineText with indents") {
    auto style = render::style(STYLE_ID_RENDER_ML);
    // "WWWWW WW" is 70 + 4 + 28 = 102 > 90: two lines; with a first line
    // indent of 20 "WWWWW" alone is 90 and still fits
    CHECK_EQ(measureMultilineText("WWWWW WW", 0, 0, 100, 300, style, 0, 0), 23 + 26);
    CHECK_EQ(measureMultilineText("WWWWW", 0, 0, 100, 300, style, 20, 0), 26);
    // with 21 it doesn't fit even into the empty line: cut, the rest on the next line
    CHECK_EQ(measureMultilineText("WWWWW", 0, 0, 100, 300, style, 21, 0), 23 + 26);
    // the hanging indent is used from the second line: "WW" and then "WWWWW"
    // after 20 fits, after 21 it is cut again
    CHECK_EQ(measureMultilineText("WW WWWWW", 0, 0, 100, 300, style, 0, 20), 23 + 26);
    CHECK_EQ(measureMultilineText("WW WWWWW", 0, 0, 100, 300, style, 0, 21), 23 * 2 + 26);
}

TEST("render/text: measureMultilineText of a word wider than the box cuts it") {
    auto style = render::style(STYLE_ID_RENDER_ML);
    // 12 'W' is 168 > 90: cut into lines of 6 'W' (84)
    CHECK_EQ(measureMultilineText("WWWWWWWWWWWW", 0, 0, 100, 300, style, 0, 0), 23 + 26);
}

TEST("render/text: measureMultilineText stops at the bottom of the box") {
    auto style = render::style(STYLE_ID_RENDER_ML);
    // inside 100 - 2 - 8 = 90 px: 3 lines of 23
    int h = measureMultilineText("A\nB\nC\nD\nE\nF", 0, 0, 400, 100, style, 0, 0);
    CHECK(h <= 90 + 3);
    CHECK(h >= 23 + 4 + 23);
}

TEST("render/text: multiline layout keeps a font tag with its index") {
    auto style = render::style(STYLE_ID_RENDER_ML);
    CHECK_EQ(measureMultilineText("ab" TAG_START ICON_FONT "cd", 0, 0, 400, 300, style, 0, 0), 26);
    CHECK_EQ(measureMultilineText("ab" TAG_START ICON_FONT ICON " " ICON TAG_END " cd", 0, 0, 400, 300, style, 0, 0), 26);
}

TEST("render/text: getCharIndexAtPosition with font tags counts the tag and its index") {
    auto style = render::style(STYLE_ID_RENDER_LEFT_TOP);
    // 0 'A' (100 .. 110), 1 tag, 2 index, 3 icon (111 .. 149), 4 end tag, 5 'A' (150 .. 160)
    const char *text = "A" TAG_START ICON_FONT ICON TAG_END "A";
    CHECK_EQ(getCharIndexAtPosition(104, text, -1, 100, 0, 300, 30, style), 0);
    CHECK_EQ(getCharIndexAtPosition(120, text, -1, 100, 0, 300, 30, style), 3);
    CHECK_EQ(getCharIndexAtPosition(140, text, -1, 100, 0, 300, 30, style), 5);
    CHECK_EQ(getCharIndexAtPosition(170, text, -1, 100, 0, 300, 30, style), 6);
}

////////////////////////////////////////////////////////////////////////////////
// integer, unit and fraction (DisplayData option)

namespace {

void drawValue(int x, int y, int w, int h, const Style *style, const char *integer, const char *unit, const char *fraction) {
    drawIntegerUnitAndFraction(
        integer, (int)strlen(integer), unit, (int)strlen(unit), fraction, (int)strlen(fraction),
        x, y, w, h, style, font::Font(), false, nullptr, nullptr);
}

} // namespace

TEST("render/text: drawIntegerUnitAndFraction right aligned keeps the integer in place") {
    // right padding 60 is the room for the unit/fraction column
    render::resetDrawState();
    Style style = *render::style(STYLE_ID_RENDER_CELL_RC);
    style.paddingRight = 60;
    drawValue(100, 100, 200, 40, &style, "12", "V", ".3");
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0, 0x80), 101, 101, 298, 138, bx1, by1, bx2, by2));
    int integerLeft = bx1;
    drawValue(100, 100, 200, 40, &style, "12", "mV", ".345");
    REQUIRE(drawnBounds(rgb(0, 0, 0x80), 101, 101, 298, 138, bx1, by1, bx2, by2));
    CHECK_EQ(bx1, integerLeft);
    // the integer ends at x2 - paddingRight: '1' and '2' are left of it
    CHECK(integerLeft < 299 - 60);
}

TEST("render/text: drawIntegerUnitAndFraction right aligned moves left when the column does not fit") {
    render::resetDrawState();
    Style style = *render::style(STYLE_ID_RENDER_CELL_RC);
    style.paddingRight = 4;
    drawValue(100, 100, 200, 40, &style, "12", "V", ".3");
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0, 0x80), 101, 101, 298, 138, bx1, by1, bx2, by2));
    int left1 = bx1;
    drawValue(100, 100, 200, 40, &style, "12", "V", ".34567");
    REQUIRE(drawnBounds(rgb(0, 0, 0x80), 101, 101, 298, 138, bx1, by1, bx2, by2));
    CHECK(bx1 < left1);
    // the column ends inside the box (x2 = 298 inside the border)
    CHECK(bx2 <= 298);
}

TEST("render/text: golden integer, unit and fraction") {
    render::resetDrawState();
    render::clearRect(0, 0, 399, 199);
    drawValue(10, 10, 180, 40, render::style(STYLE_ID_RENDER_CELL_LC), "123", "V", ".456");
    drawValue(210, 10, 180, 40, render::style(STYLE_ID_RENDER_CELL_CC), "-7", "mA", ".05");
    drawValue(10, 60, 180, 40, render::style(STYLE_ID_RENDER_CELL_RC), "1", "kHz", ".2");
    Style padded = *render::style(STYLE_ID_RENDER_CELL_RC);
    padded.paddingRight = 50;
    drawValue(210, 60, 180, 40, &padded, "42", "W", "");
    drawValue(10, 110, 180, 40, render::style(STYLE_ID_RENDER_CELL_LT), "0", "", ".000");
    drawValue(210, 110, 60, 40, render::style(STYLE_ID_RENDER_CELL_RB), "123456", "V", ".789");
    CHECK_GOLDEN("integer_unit_fraction", render::captureRegion(0, 0, 400, 200));
}

////////////////////////////////////////////////////////////////////////////////
// known bugs

TEST("render/text: measureStr stops at the NUL after an incomplete UTF-8 sequence") {
    // "ab" + lead byte of a 3-byte sequence + NUL: the NUL ends the sequence
    // (D2: the decoder took the NUL and 'X' as continuation bytes and measured
    // "YZ" after the terminator)
    const char text[] = "ab\xE2\0XYZ";
    CHECK_EQ(width(text), width("ab"));
}

TEST("render/text: drawStr draws nothing after the NUL of an incomplete UTF-8 sequence") {
    render::resetDrawState();
    render::clearRect(0, 0, 199, 99);
    auto font = roboto();
    display::setColorByValue(rgb(255, 255, 255));
    const char text[] = "A\xE2\0XWW";
    display::drawStr(text, -1, 10, 10, 0, 0, 799, 479, font, -1);
    int bx1, by1, bx2, by2;
    REQUIRE(drawnBounds(rgb(0, 0, 0), 0, 0, 199, 99, bx1, by1, bx2, by2));
    CHECK(bx2 <= 10 + 11);
}

TEST("render/text: a font tag at the end of the text does not take the NUL as its index") {
    // the NUL is not a font index (D2: the tag read it and the loop went on
    // with "AB")
    const char text[] = "A" TAG_START "\0AB";
    CHECK_EQ(width(text), width("A"));
}

TEST_BUG("render/text: multiline layout does not break a line at a font index '\\n'", "D10") {
    // font index 10 is the code point '\n', the byte scanner sees a paragraph end
    auto style = render::style(STYLE_ID_RENDER_ML);
    CHECK_EQ(measureMultilineText("ab" TAG_START "\n" "cd", 0, 0, 400, 300, style, 0, 0), 26);
}

TEST_BUG("render/text: getCursorXPosition with a style without a font does not crash", "D7") {
    Style style = *render::style(STYLE_ID_RENDER_LEFT_TOP);
    style.font = 0;
    int x = getCursorXPosition(1, "abc", 3, 100, 0, 100, 30, &style);
    CHECK(x >= 100);
}

TEST_BUG("render/text: measureMultilineText with a style without a font does not crash", "D7") {
    Style style = *render::style(STYLE_ID_RENDER_ML);
    style.font = 0;
    CHECK_EQ(measureMultilineText("abc", 0, 0, 100, 100, &style, 0, 0), 0);
}
