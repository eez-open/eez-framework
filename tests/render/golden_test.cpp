/*
 * eez-framework tests: golden images of the render pages (projects/parts/render.py)
 * and pixel checks on them.
 *
 * The presented frame is compared with render/golden/<name>.png (see
 * render_util.h; EEZ_TESTS_UPDATE_GOLDEN=1 writes the baselines). The golden
 * images pin today's output, including the known bugs that are visible in
 * them (e.g. D11 colors, DK2 bitmap centering): when such a bug is fixed the
 * golden image has to be updated too.
 */

#include <stdio.h>

#include <eez/conf-internal.h>
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

void goldenPage(const char *name, int pageId) {
    render::showPage(pageId, 3);
    CHECK_GOLDEN(name, render::captureFrame());
}

uint32_t px(int x, int y) {
    return harness::pixel(x, y) | 0xFF000000u;
}

const int RENDER_PAGES[] = {
    PAGE_ID_RENDER_TEXT_ALIGN, PAGE_ID_RENDER_BORDERS, PAGE_ID_RENDER_RADIUS, PAGE_ID_RENDER_OPACITY,
    PAGE_ID_RENDER_COLORS, PAGE_ID_RENDER_FONTS, PAGE_ID_RENDER_MULTILINE, PAGE_ID_RENDER_BITMAPS,
    PAGE_ID_RENDER_DYNAMIC,
};

} // namespace

////////////////////////////////////////////////////////////////////////////////

TEST("render/golden: text alignment, padding and clipping") {
    goldenPage("page_text_align", PAGE_ID_RENDER_TEXT_ALIGN);
}

TEST("render/golden: borders") {
    goldenPage("page_borders", PAGE_ID_RENDER_BORDERS);
}

TEST("render/golden: border radius") {
    goldenPage("page_radius", PAGE_ID_RENDER_RADIUS);
}

TEST("render/golden: opacity and overlapping widgets") {
    goldenPage("page_opacity", PAGE_ID_RENDER_OPACITY);
}

TEST("render/golden: theme and custom colors") {
    goldenPage("page_colors", PAGE_ID_RENDER_COLORS);
}

TEST("render/golden: font tags, UTF-8 and missing glyphs") {
    goldenPage("page_fonts", PAGE_ID_RENDER_FONTS);
}

TEST("render/golden: multiline text") {
    goldenPage("page_multiline", PAGE_ID_RENDER_MULTILINE);
}

TEST("render/golden: bitmaps and background images") {
    goldenPage("page_bitmaps", PAGE_ID_RENDER_BITMAPS);
}

TEST("render/golden: blink off") {
    render::showPage(PAGE_ID_RENDER_BLINK, 1);
    render::frameToBlink(false);
    CHECK_GOLDEN("page_blink_off", render::captureFrame());
}

TEST("render/golden: blink on") {
    render::showPage(PAGE_ID_RENDER_BLINK, 1);
    render::frameToBlink(true);
    CHECK_GOLDEN("page_blink_on", render::captureFrame());
}

TEST("render/golden: dynamic texts") {
    render::setText(0, "Top left");
    render::setText(1, "Bottom right");
    render::setText(2, "Radius");
    render::setText(3, "Radius border");
    render::setText(4, "Opacity");
    goldenPage("page_dynamic", PAGE_ID_RENDER_DYNAMIC);
}

////////////////////////////////////////////////////////////////////////////////
// every page: the presented frame after a few frames is the full redraw

TEST("render/golden: a static page after more frames is the same as its full redraw") {
    for (int pageId : RENDER_PAGES) {
        render::showPage(pageId, 5);
        char name[32];
        snprintf(name, sizeof(name), "page_%d", pageId);
        CHECK_EQ(render::compareWithFullRedraw(name), 0);
    }
}

TEST("render/golden: switching pages draws the new page completely") {
    render::showPage(PAGE_ID_RENDER_RADIUS, 3);
    auto radius = render::captureFrame();
    render::showPage(PAGE_ID_RENDER_TEXT_ALIGN, 3);
    render::showPage(PAGE_ID_RENDER_RADIUS, 3);
    CHECK_EQ(render::countDiff(render::captureFrame(), radius), 0);
}

////////////////////////////////////////////////////////////////////////////////
// pixel checks

TEST("render/golden: the background of a transparent text is its container's") {
    render::showPage(PAGE_ID_RENDER_TEXT_ALIGN);
    // container render_dark_green #004000 at (20, 280), text "On green" at +10, +10
    CHECK_EQ(px(32, 292), rgb(0, 0x40, 0));
    // "Opaque" text with the default (black) background at +190, +10
    CHECK_EQ(px(212, 292), rgb(0, 0, 0));
}

TEST("render/golden: uneven border sizes") {
    render::showPage(PAGE_ID_RENDER_BORDERS);
    // render_border_uneven "1 4 2 8" (top right bottom left) at (380, 20, 160, 80)
    uint32_t green = display::getColorFromIndex(COLOR_ID_RENDER_GREEN);
    uint32_t navy = rgb(0, 0, 0x80);
    CHECK_EQ(px(380 + 7, 50), green);
    CHECK_EQ(px(380 + 8, 50), navy);
    CHECK_EQ(px(539 - 3, 50), green);
    CHECK_EQ(px(539 - 4, 50), navy);
    CHECK_EQ(px(450, 20), green);
    CHECK_EQ(px(450, 21), navy);
    CHECK_EQ(px(450, 99), green);
    CHECK_EQ(px(450, 98), green);
    CHECK_EQ(px(450, 97), navy);
}

TEST("render/golden: a transparent border color keeps the space of the border") {
    render::showPage(PAGE_ID_RENDER_BORDERS);
    // render_border_tcolor: border 4 transparent, navy background at (20, 120)
    CHECK_EQ(px(21, 160), rgb(0, 0, 0));
    CHECK_EQ(px(24, 160), rgb(0, 0, 0x80));
}

TEST("render/golden: a rounded rectangle without a border has transparent corners") {
    render::showPage(PAGE_ID_RENDER_RADIUS);
    // render_r_noborder (radius 8) at (200, 20, 160, 80)
    CHECK_EQ(px(200, 20), rgb(0, 0, 0));
    CHECK_EQ(px(359, 99), rgb(0, 0, 0));
    CHECK_EQ(px(280, 20), rgb(0, 0, 0xF8));
    CHECK_EQ(px(200, 60), rgb(0, 0, 0xF8));
}

TEST("render/golden: the bottom right radius rounds the bottom right corner") {
    // render_r_br: borderRadius "0 0 20 0" (TL TR BR BL) at (200, 120, 160, 80).
    // Studio writes the radii TL TR BR BL, the Style struct has the same order
    // (NEW-render-1: it read TL TR BL BR).
    render::showPage(PAGE_ID_RENDER_RADIUS);
    uint32_t red = rgb(0xF8, 0, 0);
    CHECK_EQ(px(200, 120), red);
    CHECK_EQ(px(359, 120), red);
    CHECK_EQ(px(359, 199), rgb(0, 0, 0));
    CHECK_EQ(px(200, 199), red);
}

TEST("render/golden: style opacity blends the background over what is below") {
    render::showPage(PAGE_ID_RENDER_OPACITY);
    // render_op_red128 (#f80000, opacity 128) at (60, 60) over render_bg_green (#00fc00) at (20, 20)
    uint32_t c = px(70, 70);
    CHECK_NEAR(render::R(c), 0xF8 * 128 / 255.0, 1);
    CHECK_NEAR(render::G(c), 0xFC * 127 / 255.0, 1);
    // over the black page
    uint32_t c2 = px(250, 70);
    CHECK_NEAR(render::R(c2), 0xF8 * 128 / 255.0, 1);
    CHECK_EQ(render::G(c2), 0);
}

TEST("render/golden: 32 bpp bitmap alpha over the widget background") {
    render::showPage(PAGE_ID_RENDER_BITMAPS);
    // bitmap 4 (render_bmp32, 24 x 16) at (260, 20, 60, 40) on gray, centered
    // at x 260 + (59 - 24) / 2 = 277, y 20 + (39 - 16) / 2 = 31: the transparent
    // hole (8 .. 15, 10 .. 13) shows the gray background
    CHECK_EQ(px(277 + 10, 31 + 11), rgb(0x80, 0x80, 0x80));
    // opaque checkers
    uint32_t c = px(277 + 1, 31 + 9);
    CHECK(c == rgb(255, 0, 0) || c == rgb(0, 0, 255));
}

TEST_BUG("render/golden: a centered bitmap has equal margins", "DK2") {
    // render_bm dk2: box (20, 240, 44, 36), bitmap 24 x 16: margins 10 and 10;
    // drawBitmap centers with (x2 - x1) - width, one pixel to the left and up
    render::showPage(PAGE_ID_RENDER_BITMAPS);
    uint32_t gray = rgb(0x80, 0x80, 0x80);
    CHECK_EQ(px(29, 255), gray);
    CHECK(px(30, 255) != gray);
    CHECK(px(53, 255) != gray);
    CHECK_EQ(px(54, 255), gray);
}

TEST("render/golden: a bitmap larger than its widget is drawn outside of the widget") {
    // pinned: drawBitmap aligns the image in the widget but does not clip it to
    // the widget (render_bmp_big 64 x 48 in a 40 x 30 box at (560, 40))
    render::showPage(PAGE_ID_RENDER_BITMAPS);
    CHECK(px(555, 50) != rgb(0, 0, 0));
    CHECK(px(549, 32) != rgb(0, 0, 0));
}

TEST("render/golden: a background image is drawn from the top left of the widget, clipped to it") {
    render::showPage(PAGE_ID_RENDER_BITMAPS);
    // render_bgimg16 (render_bmp_big 64 x 48 stripes) in a 40 x 30 box at (460, 120)
    CHECK(px(460, 120) != rgb(0, 0, 0x80));
    CHECK_EQ(px(500, 125), rgb(0, 0, 0));
    // in a 200 x 80 box: navy right of and below the image
    CHECK_EQ(px(20 + 70, 130), rgb(0, 0, 0x80));
    CHECK_EQ(px(30, 120 + 60), rgb(0, 0, 0x80));
}
