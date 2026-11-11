/*
 * eez-framework tests: gui/display (colors, primitives, bitmaps, clipping,
 * damage probes, shadow) and the draw.cpp helpers that use them, drawn
 * directly into the render buffer (the presented frame after harness::init).
 */

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

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

namespace {

uint32_t px(int x, int y) {
    return harness::pixel(x, y) | 0xFF000000u;
}

// bounding box of the pixels in the rectangle that are not the color
bool findBoundsNot(uint32_t color, int x1, int y1, int x2, int y2, int &bx1, int &by1, int &bx2, int &by2) {
    bx1 = by1 = 1 << 30;
    bx2 = by2 = -1;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if (px(x, y) != (color | 0xFF000000u)) {
                bx1 = std::min(bx1, x);
                by1 = std::min(by1, y);
                bx2 = std::max(bx2, x);
                by2 = std::max(by2, y);
            }
        }
    }
    return bx2 >= 0;
}

Image makeImage(std::vector<uint8_t> &buffer, int width, int height, int bpp) {
    Image image;
    image.width = width;
    image.height = height;
    image.bpp = bpp;
    image.lineOffset = 0;
    image.pixels = buffer.data();
    return image;
}

uint8_t luminosityStepMin() {
    return DISPLAY_BACKGROUND_LUMINOSITY_STEP_MIN;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// colors

TEST("render/display: theme color by index (RGB565 exact)") {
    // accent #2060c0 and render_navy #000080 have no low bits, exact in RGB565
    CHECK_EQ(display::getColorFromIndex(COLOR_ID_ACCENT), rgb(0x20, 0x60, 0xC0));
    CHECK_EQ(display::getColorFromIndex(COLOR_ID_RENDER_NAVY), rgb(0, 0, 0x80));
    CHECK_EQ(display::getColorFromIndex(COLOR_ID_BACKGROUND), rgb(0, 0, 0));
}

TEST("render/display: custom color (not a theme color) by index") {
    // #336699 in RGB565: 0x33 & 0xF8, 0x66 & 0xFC, 0x99 & 0xF8
    CHECK_EQ(display::getColorFromIndex(COLOR_ID_CUSTOM_336699), rgb(0x30, 0x64, 0x98));
}

TEST("render/display: getColorRGBAFromIndex is the color with alpha 255") {
    display::ColorRGBA c;
    display::getColorRGBAFromIndex(COLOR_ID_ACCENT, &c);
    CHECK_EQ((int)c.r, 0x20);
    CHECK_EQ((int)c.g, 0x60);
    CHECK_EQ((int)c.b, 0xC0);
    CHECK_EQ((int)c.a, 255);
}

TEST("render/display: setColor and setBackColor by index, by RGB and by value") {
    display::setColor(COLOR_ID_RENDER_GRAY);
    CHECK_EQ(display::getColor(), rgb(0x80, 0x80, 0x80));
    display::setBackColor(COLOR_ID_RENDER_NAVY);
    CHECK_EQ(display::getBackColor(), rgb(0, 0, 0x80));
    display::setColor(1, 2, 3);
    CHECK_EQ(display::getColor(), rgb(1, 2, 3));
    display::setBackColor(4, 5, 6);
    CHECK_EQ(display::getBackColor(), rgb(4, 5, 6));
    display::setColorByValue(rgb(7, 8, 9));
    CHECK_EQ(display::getColor(), rgb(7, 8, 9));
}

TEST("render/display: color index of an external assets color with the main assets") {
    // 65534 - k (int16 -2 - k) is a color of the assets of the widget cursor
    // (module colors); with the main assets it is the main theme color k, past
    // the theme colors the custom colors, past those 0
    render::resetDrawState();
    uint32_t themeCount = getThemeColorsCount(0);
    CHECK_EQ(display::getColorFromIndex(65534 - COLOR_ID_RENDER_NAVY), display::getColorFromIndex(COLOR_ID_RENDER_NAVY));
    CHECK_EQ(display::getColorFromIndex(65534 - COLOR_ID_ACCENT), display::getColorFromIndex(COLOR_ID_ACCENT));
    CHECK_EQ(display::getColorFromIndex(65534 - COLOR_ID_CUSTOM_336699), display::getColorFromIndex(COLOR_ID_CUSTOM_336699));
    CHECK(themeCount <= (uint32_t)COLOR_ID_CUSTOM_336699);
    CHECK_EQ(display::getColorFromIndex(65534 - 1000), 0u);
}

TEST_BUG("render/display: white theme color is white (RGB565 expansion replicates the high bits)", "D11") {
    // #ffffff is (248, 252, 248)
    CHECK_EQ(display::getColorFromIndex(COLOR_ID_TEXT), rgb(255, 255, 255));
}

TEST_BUG("render/display: white pixel of a 16 bpp bitmap is white", "D11") {
    render::resetDrawState();
    std::vector<uint8_t> buffer = { 0xFF, 0xFF };
    Image image = makeImage(buffer, 1, 1, 16);
    display::drawBitmap(&image, 100, 100);
    CHECK_EQ(px(100, 100), rgb(255, 255, 255));
}

TEST_BUG("render/display: the background luminosity step changes the colors at 32 bpp", "D11") {
    display::setColor(COLOR_ID_RENDER_GRAY);
    uint32_t normal = display::getColor();
    g_hooks.getDisplayBackgroundLuminosityStep = luminosityStepMin;
    display::onLuminocityChanged();
    display::setColor(COLOR_ID_RENDER_GRAY);
    uint32_t darker = display::getColor();
    CHECK(render::R(darker) < render::R(normal));
}

TEST_BUG("render/display: the transparent color index is a transparent color", "D7") {
    // TRANSPARENT_COLOR_INDEX (0xFFFF) reads g_colors[65535 - themeCount], past the colors
    uint32_t color = display::getColorFromIndex(TRANSPARENT_COLOR_INDEX);
    CHECK_EQ(color >> 24, 0u);
}

// The lookups read the offset table past its end, so what a single id gives
// depends on the bytes after the list: check a range of ids past the count.

TEST_BUG("render/display: font ids past the fonts of the assets have no font data", "D7") {
    int count = (int)g_mainAssets->fonts.count;
    for (int id = count + 1; id <= count + 64; id++) {
        if (getFontData(id) != nullptr) {
            test::fail(__FILE__, __LINE__, test::format("font id %d (count %d) has font data", id, count));
            break;
        }
    }
}

TEST_BUG("render/display: bitmap ids past the bitmaps of the assets have no bitmap", "D7") {
    int count = (int)g_mainAssets->bitmaps.count;
    for (int id = count + 1; id <= count + 64; id++) {
        if (getBitmap(id) != nullptr) {
            test::fail(__FILE__, __LINE__, test::format("bitmap id %d (count %d) has a bitmap", id, count));
            break;
        }
    }
}

TEST("render/display: a transparent style with a background image id without a bitmap does not crash") {
    // an external assets id with the main assets has no bitmap:
    // drawBorderAndBackground checks the result of getBitmap (D7)
    render::resetDrawState();
    Style style = *render::style(STYLE_ID_RENDER_DYN);
    style.backgroundColor = TRANSPARENT_COLOR_INDEX;
    style.backgroundImage = -5;
    drawRectangle(100, 100, 50, 50, &style);
    CHECK(true);
}

TEST("render/display: font and bitmap lookups of valid and none ids") {
    CHECK(getFontData(FONT_ID_ROBOTO_CONDENSED) != nullptr);
    CHECK(getFontData(0) == nullptr);
    CHECK(getBitmap(0) == nullptr);
    auto bitmap = getBitmap(BITMAP_ID_RENDER_BMP32);
    REQUIRE(bitmap != nullptr);
    CHECK_EQ((int)bitmap->w, 24);
    CHECK_EQ((int)bitmap->h, 16);
    CHECK_EQ((int)bitmap->bpp, 32);
    CHECK_EQ((int)getBitmap(BITMAP_ID_RENDER_BMP16)->bpp, 16);
    CHECK_EQ(getBitmapIdByName("render_bmp_big"), (int)BITMAP_ID_RENDER_BMP_BIG);
    // a negative id is an external assets font, none with the main assets
    render::resetDrawState();
    CHECK(getFontData(-1) == nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// primitives

TEST("render/display: fillRect fills the rectangle inclusive of x2 and y2") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::setColor(COLOR_ID_RENDER_NAVY);
    display::fillRect(10, 20, 29, 24);
    int bx1, by1, bx2, by2;
    REQUIRE(findBoundsNot(0, 0, 0, 99, 99, bx1, by1, bx2, by2));
    CHECK_EQ(bx1, 10);
    CHECK_EQ(by1, 20);
    CHECK_EQ(bx2, 29);
    CHECK_EQ(by2, 24);
    CHECK_EQ(px(15, 22), rgb(0, 0, 0x80));
}

TEST("render/display: fillRect with x2 < x1 draws nothing") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::setColor(COLOR_ID_RENDER_NAVY);
    display::fillRect(30, 20, 10, 40);
    display::fillRect(10, 40, 30, 20);
    int bx1, by1, bx2, by2;
    CHECK(!findBoundsNot(0, 0, 0, 99, 99, bx1, by1, bx2, by2));
}

TEST("render/display: fillRect is clipped to the clip rectangle") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    auto saved = display::intersectClipRect(20, 20, 10, 10);
    display::setColor(COLOR_ID_RENDER_NAVY);
    display::fillRect(0, 0, 99, 99);
    display::restoreClipRect(saved);
    int bx1, by1, bx2, by2;
    REQUIRE(findBoundsNot(0, 0, 0, 99, 99, bx1, by1, bx2, by2));
    CHECK_EQ(bx1, 20);
    CHECK_EQ(by1, 20);
    CHECK_EQ(bx2, 29);
    CHECK_EQ(by2, 29);
}

TEST("render/display: fillRect partly outside of the display is clipped to it") {
    render::resetDrawState();
    display::setColor(COLOR_ID_RENDER_NAVY);
    display::fillRect(-50, -50, 5, 5);
    display::fillRect(795, 475, 900, 900);
    CHECK_EQ(px(0, 0), rgb(0, 0, 0x80));
    CHECK_EQ(px(799, 479), rgb(0, 0, 0x80));
}

TEST("render/display: intersectClipRect nests and returns the previous rectangle") {
    render::resetDrawState();
    auto c0 = display::intersectClipRect(10, 10, 100, 100);
    CHECK_EQ(c0.x1, 0);
    CHECK_EQ(c0.x2, (int)DISPLAY_WIDTH - 1);
    auto c1 = display::intersectClipRect(50, 0, 200, 30);
    CHECK_EQ(c1.x1, 10);
    CHECK_EQ(c1.y2, 109);
    auto &clip = display::getClipRect();
    CHECK_EQ(clip.x1, 50);
    CHECK_EQ(clip.y1, 10);
    CHECK_EQ(clip.x2, 109);
    CHECK_EQ(clip.y2, 29);
    // disjoint: empty (x1 > x2)
    auto c2 = display::intersectClipRect(500, 0, 10, 10);
    CHECK(display::getClipRect().x1 > display::getClipRect().x2);
    display::restoreClipRect(c2);
    display::restoreClipRect(c1);
    display::restoreClipRect(c0);
    CHECK_EQ(display::getClipRect().x2, (int)DISPLAY_WIDTH - 1);
}

TEST("render/display: fillRect with opacity blends over the pixels") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99, rgb(0, 0, 200));
    display::setColorByValue(rgb(200, 0, 0));
    display::setOpacity(128);
    display::fillRect(0, 0, 9, 9);
    display::setOpacity(0);
    display::fillRect(20, 0, 29, 9);
    display::setOpacity(255);
    uint32_t c = px(5, 5);
    CHECK_NEAR(render::R(c), 200 * 128 / 255.0, 1);
    CHECK_NEAR(render::B(c), 200 * 127 / 255.0, 1);
    CHECK_EQ(px(25, 5), rgb(0, 0, 200));
}

TEST("render/display: drawHLine and drawVLine take the length minus one") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::setColor(COLOR_ID_TEXT);
    display::drawHLine(10, 10, 9);
    display::drawVLine(50, 10, 4);
    int bx1, by1, bx2, by2;
    REQUIRE(findBoundsNot(0, 0, 0, 40, 40, bx1, by1, bx2, by2));
    CHECK_EQ(bx2 - bx1 + 1, 10);
    CHECK_EQ(by2 - by1 + 1, 1);
    REQUIRE(findBoundsNot(0, 45, 0, 99, 99, bx1, by1, bx2, by2));
    CHECK_EQ(by2 - by1 + 1, 5);
}

TEST("render/display: drawRect draws the outline only") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::setColor(COLOR_ID_TEXT);
    display::drawRect(10, 10, 30, 20);
    CHECK(px(10, 10) != rgb(0, 0, 0));
    CHECK(px(30, 20) != rgb(0, 0, 0));
    CHECK(px(20, 10) != rgb(0, 0, 0));
    CHECK_EQ(px(20, 15), rgb(0, 0, 0));
    CHECK_EQ(px(31, 20), rgb(0, 0, 0));
}

TEST("render/display: drawFocusFrame is a 3 px magenta frame") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::drawFocusFrame(10, 10, 40, 30);
    CHECK_EQ(px(10, 10), rgb(255, 0, 255));
    CHECK_EQ(px(12, 25), rgb(255, 0, 255));
    CHECK_EQ(px(13, 25), rgb(0, 0, 0));
    CHECK_EQ(px(49, 39), rgb(255, 0, 255));
    CHECK_EQ(px(50, 39), rgb(0, 0, 0));
}

TEST("render/display: drawPixel with and without opacity") {
    render::resetDrawState();
    render::clearRect(0, 0, 9, 9, rgb(0, 0, 0));
    display::setColorByValue(rgb(255, 255, 255));
    display::drawPixel(1, 1);
    display::drawPixel(2, 1, 51);
    CHECK_EQ(px(1, 1), rgb(255, 255, 255));
    CHECK_EQ(px(2, 1), rgb(51, 51, 51));
    // outside of the clip rectangle
    auto saved = display::intersectClipRect(5, 5, 2, 2);
    display::drawPixel(1, 2);
    display::restoreClipRect(saved);
    CHECK_EQ(px(1, 2), rgb(0, 0, 0));
}

TEST("render/display: drawLine includes both end points, horizontal and vertical lines") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::setColor(COLOR_ID_TEXT);
    drawLine(10, 10, 30, 17);
    CHECK(px(10, 10) != rgb(0, 0, 0));
    CHECK(px(30, 17) != rgb(0, 0, 0));
    CHECK_EQ(px(31, 17), rgb(0, 0, 0));
    drawLine(60, 50, 40, 50);
    int bx1, by1, bx2, by2;
    REQUIRE(findBoundsNot(0, 35, 45, 99, 55, bx1, by1, bx2, by2));
    CHECK_EQ(bx1, 40);
    CHECK_EQ(bx2, 60);
    drawLine(70, 80, 70, 60);
    REQUIRE(findBoundsNot(0, 65, 57, 75, 99, bx1, by1, bx2, by2));
    CHECK_EQ(by1, 60);
    CHECK_EQ(by2, 80);
}

TEST("render/display: bitBlt copies a rectangle within the buffer, overlapping moves") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    for (int y = 10; y < 30; y++) {
        for (int x = 10; x < 30; x++) {
            display::setColorByValue(rgb(x * 8, y * 8, 100));
            display::drawPixel(x, y);
        }
    }
    auto before = render::captureRegion(10, 10, 20, 20);
    // move down-right by 3 (overlapping), the copy must not read moved rows
    display::bitBlt(10, 10, 29, 29, 13, 13);
    auto after = render::captureRegion(13, 13, 20, 20);
    CHECK_EQ(render::countDiff(before, after), 0);
    // and back up-left by 5
    display::bitBlt(13, 13, 32, 32, 8, 8);
    CHECK_EQ(render::countDiff(before, render::captureRegion(8, 8, 20, 20)), 0);
}

////////////////////////////////////////////////////////////////////////////////
// bitmaps

TEST("render/display: drawBitmap 32 bpp blends the pixels with their alpha") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99, rgb(0, 0, 200));
    // bytes R, G, B, A
    std::vector<uint8_t> buffer = {
        255, 0, 0, 255,   255, 0, 0, 128,
        0, 255, 0, 0,     255, 255, 255, 255,
    };
    Image image = makeImage(buffer, 2, 2, 32);
    display::drawBitmap(&image, 10, 10);
    CHECK_EQ(px(10, 10), rgb(255, 0, 0));
    uint32_t half = px(11, 10);
    CHECK_NEAR(render::R(half), 128, 1);
    CHECK_NEAR(render::B(half), 100, 1);
    CHECK_EQ(px(10, 11), rgb(0, 0, 200));
    CHECK_EQ(px(11, 11), rgb(255, 255, 255));
}

TEST("render/display: drawBitmap 32 bpp with the opacity scales the alpha") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99, rgb(0, 0, 0));
    std::vector<uint8_t> buffer = { 200, 200, 200, 255 };
    Image image = makeImage(buffer, 1, 1, 32);
    display::setOpacity(128);
    display::drawBitmap(&image, 10, 10);
    display::setOpacity(255);
    CHECK_NEAR(render::R(px(10, 10)), 200 * 128 / 255.0, 1);
}

TEST("render/display: drawBitmap 16 bpp converts RGB565") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99, rgb(50, 50, 50));
    // red, green, blue, black (rrrrrggggggbbbbb, little endian)
    std::vector<uint8_t> buffer = { 0x00, 0xF8, 0xE0, 0x07, 0x1F, 0x00, 0x00, 0x00 };
    Image image = makeImage(buffer, 2, 2, 16);
    display::drawBitmap(&image, 10, 10);
    CHECK(render::R(px(10, 10)) >= 248 && render::G(px(10, 10)) == 0 && render::B(px(10, 10)) == 0);
    CHECK(render::G(px(11, 10)) >= 252 && render::R(px(11, 10)) == 0 && render::B(px(11, 10)) == 0);
    CHECK(render::B(px(10, 11)) >= 248 && render::R(px(10, 11)) == 0 && render::G(px(10, 11)) == 0);
    // 16 bpp has no alpha, black is drawn
    CHECK_EQ(px(11, 11), rgb(0, 0, 0));
    CHECK_EQ(px(12, 10), rgb(50, 50, 50));
}

TEST("render/display: drawBitmap 24 bpp unclipped") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    std::vector<uint8_t> buffer;
    for (int i = 0; i < 4 * 3; i++) {
        buffer.push_back((uint8_t)(i * 20));
        buffer.push_back((uint8_t)(255 - i * 20));
        buffer.push_back(77);
    }
    Image image = makeImage(buffer, 4, 3, 24);
    display::drawBitmap(&image, 20, 30);
    for (int i = 0; i < 12; i++) {
        CHECK_EQ(px(20 + i % 4, 30 + i / 4), rgb(i * 20, 255 - i * 20, 77));
    }
    CHECK_EQ(px(24, 30), rgb(0, 0, 0));
    CHECK_EQ(px(20, 33), rgb(0, 0, 0));
}

TEST("render/display: drawBitmap 16 and 32 bpp clipped horizontally and vertically") {
    render::resetDrawState();
    render::clearRect(0, 0, 199, 99);
    // 8 x 6 images, pixel (x, y) has a unique color
    std::vector<uint8_t> b32, b16;
    for (int y = 0; y < 6; y++) {
        for (int x = 0; x < 8; x++) {
            b32.push_back((uint8_t)(x * 30)); b32.push_back((uint8_t)(y * 40)); b32.push_back(10); b32.push_back(255);
            uint16_t c = (uint16_t)(((x * 4) << 11) | ((y * 8) << 5) | 3);
            b16.push_back(c & 0xFF); b16.push_back(c >> 8);
        }
    }
    Image i32 = makeImage(b32, 8, 6, 32);
    Image i16 = makeImage(b16, 8, 6, 16);
    // unclipped references
    display::drawBitmap(&i32, 10, 10);
    display::drawBitmap(&i16, 10, 30);
    auto ref32 = render::captureRegion(10, 10, 8, 6);
    auto ref16 = render::captureRegion(10, 30, 8, 6);
    // clipped to x 103..105, y 12..13 of an image at (100, 10)
    auto saved = display::intersectClipRect(103, 12, 3, 2);
    display::drawBitmap(&i32, 100, 10);
    display::restoreClipRect(saved);
    saved = display::intersectClipRect(103, 32, 3, 2);
    display::drawBitmap(&i16, 100, 30);
    display::restoreClipRect(saved);
    for (int y = 0; y < 6; y++) {
        for (int x = 0; x < 8; x++) {
            bool inside = x >= 3 && x <= 5 && y >= 2 && y <= 3;
            CHECK_EQ(px(100 + x, 10 + y), inside ? ref32.at(x, y) : rgb(0, 0, 0));
            CHECK_EQ(px(100 + x, 30 + y), inside ? ref16.at(x, y) : rgb(0, 0, 0));
        }
    }
    // nothing below the images
    int bx1, by1, bx2, by2;
    CHECK(!findBoundsNot(0, 90, 40, 199, 99, bx1, by1, bx2, by2));
}

TEST("render/display: drawBitmap 24 bpp clipped horizontally draws only the image rows") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    // a red 4 x 4 image in a larger buffer of green (with D1 the loop read past
    // the image)
    std::vector<uint8_t> buffer(3 * 4 * 4 * 4, 0);
    for (int i = 0; i < (int)buffer.size(); i += 3) {
        buffer[i + (i < 3 * 4 * 4 ? 0 : 1)] = 255;
    }
    Image image = makeImage(buffer, 4, 4, 24);
    // clipped to 2 columns: lineOffset 2, srcEnd = src + 3 * (2 + 3 * 2) * 4 is
    // reached after 8 rows of 3 * (2 + 2) bytes
    auto saved = display::intersectClipRect(20, 0, 2, 100);
    display::drawBitmap(&image, 20, 10);
    display::restoreClipRect(saved);
    CHECK_EQ(px(20, 10), rgb(255, 0, 0));
    CHECK_EQ(px(21, 13), rgb(255, 0, 0));
    for (int y = 14; y < 20; y++) {
        CHECK_EQ(px(20, y), rgb(0, 0, 0));
    }
}

////////////////////////////////////////////////////////////////////////////////
// antialiased drawing through the display

TEST("render/display: fillRoundedRect fills with the back color, the line in the foreground color") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::setColor(COLOR_ID_RENDER_GRAY);
    display::setBackColor(COLOR_ID_RENDER_NAVY);
    display::fillRoundedRect(10, 10, 59, 49, 2, 6, true, true);
    CHECK_EQ(px(35, 10), rgb(0x80, 0x80, 0x80));
    CHECK_EQ(px(35, 30), rgb(0, 0, 0x80));
    CHECK_EQ(px(10, 10), rgb(0, 0, 0));
    // the line only
    render::clearRect(0, 0, 99, 99);
    display::drawRoundedRect(10, 10, 59, 49, 2, 6, 6, 6, 6, 6, 6, 6, 6);
    CHECK_EQ(px(35, 10), rgb(0x80, 0x80, 0x80));
    CHECK_EQ(px(35, 30), rgb(0, 0, 0));
}

TEST("render/display: fillRoundedRect with an extra clip rectangle") {
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::setBackColor(COLOR_ID_RENDER_NAVY);
    display::fillRoundedRect(10, 10, 60, 30, 0, 5, false, true, 0, 0, 30, 479);
    CHECK_EQ(px(20, 20), rgb(0, 0, 0x80));
    CHECK_EQ(px(30, 20), rgb(0, 0, 0x80));
    CHECK_EQ(px(31, 20), rgb(0, 0, 0));
    CHECK_EQ(px(45, 20), rgb(0, 0, 0));
}

TEST_BUG("render/display: fillRoundedRect with the extra clip rectangle starting at x = -1", "D9") {
    // -1 is the "no extra clip" sentinel, so the clip x -1 .. 30 is ignored
    render::resetDrawState();
    render::clearRect(0, 0, 99, 99);
    display::setBackColor(COLOR_ID_RENDER_NAVY);
    display::fillRoundedRect(10, 10, 60, 30, 0, 5, false, true, -1, 0, 30, 479);
    CHECK_EQ(px(20, 20), rgb(0, 0, 0x80));
    CHECK_EQ(px(45, 20), rgb(0, 0, 0));
}

TEST("render/display: fillPath, strokePath and drawPolyline use the color, opacity and clip rectangle") {
    render::resetDrawState();
    render::clearRect(0, 0, 199, 99);
    display::setColorByValue(rgb(200, 100, 0));
    display::Path path;
    path.moveTo(10, 10); path.lineTo(50, 10); path.lineTo(50, 50); path.lineTo(10, 50); path.close();
    display::fillPath(path);
    CHECK_EQ(px(30, 30), rgb(200, 100, 0));
    display::setOpacity(128);
    float points[] = { 60.5f, 30.5f, 100.5f, 30.5f };
    display::drawPolyline(points, 2, 1.0f);
    display::setOpacity(255);
    CHECK_NEAR(render::R(px(80, 30)), 100, 1);
    auto saved = display::intersectClipRect(110, 0, 20, 100);
    display::Path line;
    line.moveTo(100, 60.5f);
    line.lineTo(190, 60.5f);
    display::strokePath(line, 1.0f);
    display::restoreClipRect(saved);
    CHECK_EQ(px(105, 60), rgb(0, 0, 0));
    CHECK_EQ(px(120, 60), rgb(200, 100, 0));
    CHECK_EQ(px(135, 60), rgb(0, 0, 0));
}

////////////////////////////////////////////////////////////////////////////////
// damage probes

TEST("render/display: a damage probe collects the bounding box of what is drawn") {
    render::resetDrawState();
    display::DamageProbe probe;
    display::beginDamageProbe(probe);
    CHECK(probe.isEmpty());
    display::setColor(COLOR_ID_TEXT);
    display::fillRect(10, 20, 30, 40);
    display::fillRect(50, 5, 55, 8);
    display::endDamageProbe(probe);
    CHECK_EQ(probe.x1, 10);
    CHECK_EQ(probe.y1, 5);
    CHECK_EQ(probe.x2, 55);
    CHECK_EQ(probe.y2, 40);
}

TEST("render/display: nested damage probes, the inner one is added to the outer one") {
    render::resetDrawState();
    display::DamageProbe outer, inner;
    display::setColor(COLOR_ID_TEXT);
    display::beginDamageProbe(outer);
    display::fillRect(0, 0, 5, 5);
    display::beginDamageProbe(inner);
    display::fillRect(50, 50, 60, 60);
    display::endDamageProbe(inner);
    display::endDamageProbe(outer);
    CHECK_EQ(inner.x1, 50);
    CHECK_EQ(inner.x2, 60);
    CHECK_EQ(outer.x1, 0);
    CHECK_EQ(outer.y1, 0);
    CHECK_EQ(outer.x2, 60);
    CHECK_EQ(outer.y2, 60);
}

TEST("render/display: a damage probe sees clipped drawing, antialiased spans and glyphs") {
    render::resetDrawState();
    display::DamageProbe probe;
    display::beginDamageProbe(probe);
    auto saved = display::intersectClipRect(100, 100, 50, 50);
    display::setColor(COLOR_ID_TEXT);
    display::fillRect(0, 0, 799, 479);
    display::restoreClipRect(saved);
    display::endDamageProbe(probe);
    CHECK_EQ(probe.x1, 100);
    CHECK_EQ(probe.y2, 149);

    display::DamageProbe probe2;
    display::beginDamageProbe(probe2);
    display::setBackColor(COLOR_ID_RENDER_NAVY);
    display::fillRoundedRect(200, 200, 239, 219, 0, 8, false, true);
    display::endDamageProbe(probe2);
    CHECK_EQ(probe2.x1, 200);
    CHECK_EQ(probe2.x2, 239);
    CHECK_EQ(probe2.y1, 200);
    CHECK_EQ(probe2.y2, 219);

    display::DamageProbe probe3;
    font::Font font(getFontData(FONT_ID_ROBOTO_CONDENSED));
    display::beginDamageProbe(probe3);
    display::drawStr("A", -1, 300, 300, 0, 0, 799, 479, font, -1);
    display::endDamageProbe(probe3);
    CHECK(!probe3.isEmpty());
    CHECK(probe3.x1 >= 300 && probe3.x2 <= 311);
    CHECK(probe3.y1 >= 300 && probe3.y2 < 300 + 26);
}

////////////////////////////////////////////////////////////////////////////////
// shadow

TEST("render/display: the shadow stays inside expandRectWithShadow and outside of the window") {
    render::resetDrawState();
    render::clearRect(0, 0, 799, 479, rgb(255, 255, 255));
    int x1 = 300, y1 = 200, x2 = 399, y2 = 279;
    drawShadow(x1, y1, x2, y2);
    int ex1 = x1, ey1 = y1, ex2 = x2, ey2 = y2;
    expandRectWithShadow(ex1, ey1, ex2, ey2);
    CHECK(ex1 < x1 && ey1 < y1 && ex2 > x2 && ey2 > y2);
    int bx1, by1, bx2, by2;
    REQUIRE(findBoundsNot(rgb(255, 255, 255), 0, 0, 799, 479, bx1, by1, bx2, by2));
    CHECK(bx1 >= ex1 && by1 >= ey1 && bx2 <= ex2 && by2 <= ey2);
    // the window itself is not drawn
    CHECK_EQ(px(350, 240), rgb(255, 255, 255));
    // offset down: darker below the window than above it
    CHECK(render::R(px(350, y2 + 5)) < render::R(px(350, y1 - 5)));
}

TEST("render/display: changed shadow parameters change the shadow, the old ones give the old shadow") {
    render::resetDrawState();
    auto params = getShadowParams();
    render::clearRect(0, 0, 799, 479, rgb(255, 255, 255));
    drawShadow(300, 200, 399, 279);
    auto first = render::captureFrame();

    ShadowParams other = params;
    other.blur = 4;
    other.offsetX = 10;
    other.r = 255;
    setShadowParams(other);
    render::clearRect(0, 0, 799, 479, rgb(255, 255, 255));
    drawShadow(300, 200, 399, 279);
    CHECK(render::countDiff(render::captureFrame(), first) > 100);

    setShadowParams(params);
    render::clearRect(0, 0, 799, 479, rgb(255, 255, 255));
    drawShadow(300, 200, 399, 279);
    CHECK_EQ(render::countDiff(render::captureFrame(), first), 0);
}

////////////////////////////////////////////////////////////////////////////////
// golden image of the primitives

TEST("render/display: golden primitives") {
    render::resetDrawState();
    render::clearRect(0, 0, 399, 199, rgb(16, 16, 32));

    display::setColorByValue(rgb(255, 255, 255));
    drawLine(10, 10, 90, 40);
    drawLine(10, 40, 90, 10);
    drawAntialiasedLine(10, 50, 90, 80);
    drawAntialiasedLine(10, 80, 90, 52);
    display::setColorByValue(rgb(0, 200, 255));
    display::drawRect(100, 10, 160, 50);
    display::drawFocusFrame(170, 10, 60, 40);
    display::setColorByValue(rgb(255, 200, 0));
    display::setOpacity(100);
    display::fillRect(240, 10, 300, 50);
    display::setOpacity(255);

    display::setColorByValue(rgb(255, 255, 255));
    display::setBackColor(0, 128, 64);
    display::fillRoundedRect(100, 60, 180, 110, 3, 4, 10, 18, 6, 0, 0, 25, 25, true, true);
    display::fillRoundedRect(190, 60, 270, 110, 1, 12, false, true, 200, 0, 250, 479);
    display::setColorByValue(rgb(255, 64, 64));
    display::drawRoundedRect(280, 60, 390, 110, 5, 20, 20, 20, 20, 20, 20, 20, 20);

    float points[] = { 10, 180, 40, 120, 70, 170, 100, 125, 130, 185 };
    display::setColorByValue(rgb(120, 255, 120));
    display::drawPolyline(points, 5, 2.5f);

    font::Font font(getFontData(FONT_ID_ROBOTO_CONDENSED));
    display::setColorByValue(rgb(255, 255, 255));
    display::drawStr("Glyphs AWxyz 0123", -1, 150, 120, 150, 120, 399, 199, font, -1);
    display::drawStr("Clipped text", -1, 150, 150, 150, 155, 220, 170, font, 4);
    display::setOpacity(128);
    display::drawStr("Half opacity", -1, 260, 150, 0, 0, 399, 199, font, -1);
    display::setOpacity(255);

    CHECK_GOLDEN("display_primitives", render::captureRegion(0, 0, 400, 200));
}

TEST("render/display: golden checkbox and radio") {
    render::resetDrawState();
    render::clearRect(0, 0, 399, 199);
    auto style = render::style(STYLE_ID_RENDER_CB);
    drawCheckboxOrRadio(10, 10, 180, 40, style, false, false, "Checkbox");
    drawCheckboxOrRadio(10, 60, 180, 40, style, false, true, "Checked");
    drawCheckboxOrRadio(200, 10, 180, 40, style, true, false, "Radio");
    drawCheckboxOrRadio(200, 60, 180, 40, style, true, true, "Selected");
    drawCheckboxOrRadio(10, 110, 180, 40, style, false, true, "Active", true);
    drawCheckboxOrRadio(200, 110, 60, 40, style, true, true, "Clipped label");
    drawCheckboxOrRadio(10, 160, 30, 30, style, false, true, nullptr);
    CHECK_GOLDEN("checkbox_radio", render::captureRegion(0, 0, 400, 200));
}

TEST("render/display: measureCheckboxOrRadio is the box, a space and the label") {
    auto style = render::style(STYLE_ID_RENDER_CB);
    font::Font font(getFontData(FONT_ID_ROBOTO_CONDENSED));
    int box = font.getHeight() * 3 / 4;
    CHECK_EQ(measureCheckboxOrRadio(style, nullptr), box);
    CHECK_EQ(measureCheckboxOrRadio(style, ""), box);
    CHECK_EQ(measureCheckboxOrRadio(style, "AW"), box + display::measureGlyph(' ', font) + display::measureStr("AW", -1, font));
}
