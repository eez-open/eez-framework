/*
 * eez-framework tests: vector fonts (EEZ_OPTION_GUI_VECTOR_FONTS), drawn from
 * fonts/Roboto-Regular.ttf at runtime with HarfBuzz and FreeType. VRoboto and
 * VRobotoTnum (tnum, letter spacing 1 px) are made from the same file (parts/
 * render.py), the page render_vector shows them.
 */

#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <string>
#include <thread>

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

namespace {

font::Font vroboto() {
    return font::Font(getFontData(FONT_ID_VROBOTO));
}

font::Font vrobotoTnum() {
    return font::Font(getFontData(FONT_ID_VROBOTO_TNUM));
}

int measure(font::Font font, const char *text) {
    return display::measureStr(text, -1, font);
}

uint32_t px(int x, int y) {
    return harness::pixel(x, y) | 0xFF000000u;
}

// pixels in the rectangle that are not the color
int countOther(int x, int y, int w, int h, uint32_t color) {
    int count = 0;
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (px(i, j) != color) {
                count++;
            }
        }
    }
    return count;
}

const uint32_t NAVY = rgb(0, 0, 0x80);

} // namespace

////////////////////////////////////////////////////////////////////////////////
// assets

TEST("render/vector: a vector font is of the vector kind with its font file") {
    auto font = vroboto();
    REQUIRE(font.isVector());
    auto data = font.getVectorFontData();
    const FontFile *file = data->file;
    REQUIRE(file != nullptr);
    CHECK(file->size > 100000u);
    // 14 pt at 100 dpi, in 1/64 px
    CHECK_EQ(data->size, (uint32_t)(14 * 100 * 64 / 72 + 0.5));
    CHECK_EQ((int)font.getAscent(), 21);
    CHECK_EQ((int)font.getHeight(), 26);
}

TEST("render/vector: two vector fonts of one font file share it in the assets") {
    const FontFile *file1 = vroboto().getVectorFontData()->file;
    const FontFile *file2 = vrobotoTnum().getVectorFontData()->file;
    CHECK(file1 == file2);
    CHECK_STR(static_cast<const char *>(vrobotoTnum().getVectorFontData()->features), "tnum");
    CHECK_EQ(vrobotoTnum().getVectorFontData()->letterSpacing, 64);
}

TEST("render/vector: a vector font has no glyphs in the assets") {
    auto font = vroboto();
    CHECK(font.getGlyph('A') == nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// measuring

TEST("render/vector: text is measured") {
    auto font = vroboto();
    CHECK(measure(font, "A") > 5);
    CHECK(measure(font, "Hello") > measure(font, "Hell"));
    CHECK_EQ(measure(font, ""), 0);
}

TEST("render/vector: a kerning pair is narrower than its glyphs") {
    auto font = vroboto();
    CHECK(measure(font, "AV") < measure(font, "A") + measure(font, "V"));
    CHECK(measure(font, "Ty") < measure(font, "T") + measure(font, "y"));
}

TEST("render/vector: tnum digits have one width") {
    auto font = vrobotoTnum();
    CHECK_EQ(measure(font, "1111"), measure(font, "8888"));
    CHECK_EQ(measure(font, "1.111"), measure(font, "8.888"));
}

TEST("render/vector: the letter spacing is added after every glyph") {
    // "xox" has no digits, so tnum changes nothing
    int plain = measure(vroboto(), "xox");
    int spaced = measure(vrobotoTnum(), "xox");
    CHECK(abs(spaced - (plain + 3)) <= 1);
}

TEST("render/vector: measureStr stops at max_width") {
    auto font = vroboto();
    int width = measure(font, "The quick brown fox");
    CHECK_EQ(display::measureStr("The quick brown fox", -1, font, width / 2), width / 2);
}

TEST("render/vector: measureStr of a part of the text counts characters") {
    auto font = vroboto();
    // 2 characters of a text with a 2-byte UTF-8 character
    CHECK_EQ(display::measureStr("\xC3\xBC" "ab", 2, font), measure(font, "\xC3\xBC" "a"));
}

TEST("render/vector: measureGlyph of a vector font") {
    auto font = vroboto();
    CHECK(display::measureGlyph(' ', font) > 0);
    CHECK_EQ(display::measureGlyph('W', font), measure(font, "W"));
}

TEST("render/vector: font tags switch between vector and bitmap fonts") {
    // U+E000 + font index switches to the font, U+E001 back
    auto font = vroboto();
    char text[32];
    snprintf(text, sizeof(text), "AB\xEE\x80\x80%cC\xEE\x80\x81" "D", (char)FONT_ID_ROBOTO_CONDENSED);
    font::Font bitmap(getFontData(FONT_ID_ROBOTO_CONDENSED));
    CHECK_EQ(measure(font, text), measure(font, "AB") + measure(bitmap, "C") + measure(font, "D"));
    CHECK_EQ(display::getFontIndexAtEnd(text, -1, 0), 0);
}

////////////////////////////////////////////////////////////////////////////////
// cursor

TEST("render/vector: the cursor positions grow and match the character at a position") {
    auto font = vroboto();
    const char *text = "Hello";
    int last = -1;
    for (int i = 0; i <= 5; i++) {
        int x = display::getCursorXPosition(i, text, -1, 100, 0, 0, 0, 800, 480, font);
        CHECK(x > last);
        last = x;
        if (i < 5) {
            // just right of the cursor is the character
            CHECK_EQ(display::getCharIndexAtPosition(x + 1, text, -1, 100, 0, 0, 0, 800, 480, font), i);
        }
    }
    CHECK_EQ(last, 100 + measure(font, text));
}

TEST("render/vector: editable text has no ligatures, the cursor can be between f and i") {
    // Roboto has an fi ligature
    auto font = vroboto();
    int x0 = display::getCursorXPosition(0, "fi", -1, 0, 0, 0, 0, 800, 480, font);
    int x1 = display::getCursorXPosition(1, "fi", -1, 0, 0, 0, 0, 800, 480, font);
    int x2 = display::getCursorXPosition(2, "fi", -1, 0, 0, 0, 0, 800, 480, font);
    CHECK(x0 < x1);
    CHECK(x1 < x2);
}

////////////////////////////////////////////////////////////////////////////////
// drawing

TEST("render/vector: golden vector fonts") {
    render::setText(0, "Dynamic 0.123 A");
    render::showPage(PAGE_ID_RENDER_VECTOR);
    CHECK_GOLDEN("page_vector", render::captureFrame());
}

TEST("render/vector: text is drawn in its box") {
    render::showPage(PAGE_ID_RENDER_VECTOR);
    // render_v kern: (20, 20, 360, 40), padding 4, left aligned
    CHECK(countOther(24, 24, 352, 32, NAVY) > 200);
    // nothing left of the padding
    CHECK_EQ(countOther(20, 20, 4, 40, NAVY), 0);
}

TEST("render/vector: drawStr draws the glyphs where measureStr ends") {
    render::resetDrawState();
    render::clearRect(0, 0, 399, 59, NAVY);
    auto font = vroboto();
    display::setColor(COLOR_ID_TEXT);
    display::drawStr("Wave", -1, 10, 10, 0, 0, 399, 59, font, -1);
    int width = measure(font, "Wave");
    // drawn up to the measured width (the last glyph can reach a little past its
    // advance), not further
    CHECK(countOther(10, 10, width, 26, NAVY) > 50);
    CHECK_EQ(countOther(10 + width + 3, 0, 50, 60, NAVY), 0);
}

TEST("render/vector: a changing text draws the same as a full redraw") {
    render::setText(0, "one");
    render::showPage(PAGE_ID_RENDER_VECTOR);
    const char *texts[] = { "two 2.5 V", "Wave AV", "x", "" };
    for (auto text : texts) {
        render::setText(0, text);
        harness::frame();
        CHECK_EQ(render::compareWithFullRedraw("vector_dynamic"), 0);
    }
}

////////////////////////////////////////////////////////////////////////////////
// threads

TEST("render/vector: another thread measures text while the GUI thread draws") {
    // the event queue measures text in the background thread
    render::showPage(PAGE_ID_RENDER_VECTOR);
    std::atomic<bool> stop(false);
    std::atomic<int> errors(0);
    std::thread thread([&]() {
        auto font = vroboto();
        int expected = measure(font, "Background 123");
        while (!stop) {
            if (measure(font, "Background 123") != expected) {
                errors++;
            }
            char text[32];
            snprintf(text, sizeof(text), "%d", rand());
            measure(font, text);
        }
    });
    for (int i = 0; i < 20; i++) {
        char text[32];
        snprintf(text, sizeof(text), "frame %d", i);
        render::setText(0, text);
        harness::frame();
    }
    stop = true;
    thread.join();
    CHECK_EQ(errors.load(), 0);
}
