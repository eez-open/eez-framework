/*
 * eez-framework tests: composition of the render buffers (pages, popups with
 * shadow and backdrop, transparent pages, toasts), damage tracking and the
 * invariant that after incremental updates the presented frame is the same as
 * a full redraw (refreshScreen + frame).
 *
 * Damage is measured with markers (render_util.h, putMarkers): pixels put into
 * the buffer the next frame is rendered into survive where that frame does not
 * compose, so the share of overwritten markers is the composed share of the
 * frame.
 */

#include <atomic>
#include <thread>

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

uint32_t px(int x, int y) {
    return harness::pixel(x, y) | 0xFF000000u;
}

// changes the text of the native variable and runs a frame
void change(int index, int n) {
    char text[32];
    snprintf(text, sizeof(text), "%c%d", 'a' + index, n);
    render::setText(index, text);
}

// share (0 .. 1) of the frame composed by the next frame, see putMarkers
float composedShare(void (*changeFunc)(int), int n) {
    render::putMarkers();
    changeFunc(n);
    harness::frame();
    return 1.0f - (float)render::countMarkers() / render::numMarkers();
}

void changeA(int n) { change(0, n); }
void changeAB(int n) { change(0, n); change(1, n); }
void changeF(int n) { change(5, n); }
void changeNothing(int) {}

// a few frames with the change, so the stale part of the next render buffer
// is the previous change only
void settle(void (*changeFunc)(int)) {
    for (int i = 0; i < 3; i++) {
        changeFunc(100 + i);
        harness::frame();
    }
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// golden images

TEST("render/compose: golden popup with shadow and blurred backdrop") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    CHECK_GOLDEN("compose_popup", render::captureFrame());
}

TEST("render/compose: golden two popups") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    render::pushPage(PAGE_ID_RENDER_POPUP2);
    CHECK_GOLDEN("compose_popup_stack", render::captureFrame());
}

TEST("render/compose: golden transparent page") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_TRANSPARENT);
    CHECK_GOLDEN("compose_transparent", render::captureFrame());
}

TEST("render/compose: golden info toast") {
    render::showPage(PAGE_ID_RENDER_TEXT_ALIGN);
    harness::appContext()->infoMessage("Info toast message");
    harness::frame(3);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    CHECK_GOLDEN("compose_toast", render::captureFrame());
}

TEST("render/compose: golden five small popups") {
    render::showPage(PAGE_ID_MAIN);
    for (int i = 0; i < 5; i++) {
        harness::appContext()->pushPage(PAGE_ID_RENDER_STACK1 + i);
    }
    harness::frame(3);
    CHECK_GOLDEN("compose_stack5", render::captureFrame());
}

////////////////////////////////////////////////////////////////////////////////
// composition

TEST("render/compose: the popup is drawn over a blurred and tinted backdrop") {
    render::setText(0, "WWWWWWWWWW");
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    auto before = render::captureFrame();
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    // popup at (200, 120, 400, 240): its background inside, its border
    CHECK_EQ(px(400, 300), rgb(0, 0, 0x80));
    CHECK(px(200, 120) != rgb(0, 0, 0x80));
    // outside: the page below is blurred and tinted, the text a is no longer sharp
    render::Image32 textBefore(120, 30);
    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 120; x++) {
            textBefore.at(x, y) = before.at(10 + x, 10 + y);
        }
    }
    CHECK(render::countDiff(render::captureRegion(10, 10, 120, 30), textBefore) > 100);
}

TEST("render/compose: closing a popup restores the page below exactly") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    auto before = render::captureFrame();
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    render::pushPage(PAGE_ID_RENDER_POPUP2);
    harness::appContext()->popPage();
    harness::frame(2);
    harness::appContext()->popPage();
    harness::frame(2);
    CHECK_EQ(render::countDiff(render::captureFrame(), before), 0);
}

TEST("render/compose: a popup with shadow and backdrop is the same as its full redraw") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1, 5);
    CHECK_EQ(render::compareWithFullRedraw("popup"), 0);
}

TEST("render/compose: a toast is the same as its full redraw") {
    render::showPage(PAGE_ID_RENDER_TEXT_ALIGN);
    harness::appContext()->infoMessage("Info toast message");
    harness::frame(3);
    CHECK_EQ(render::compareWithFullRedraw("toast"), 0);
}

// The top popup is sharp. The ones below it are behind the blurred and tinted
// backdrops of all the popups above them, still orange and darker the lower
// they are: e.g. 77 54 30, 100 67 32, 133 87 37, 180 121 56 for five popups,
// 62 46 30 for the lowest of six.
static void checkStack(int n) {
    int previousR = -1;
    for (int i = 0; i < n; i++) {
        uint32_t c = px(20 + i * 125 + 55, 330);
        if (i == n - 1) {
            CHECK_EQ(c, rgb(0xF8, 0x80, 0));
        } else {
            CHECK(render::R(c) > render::G(c) && render::G(c) > render::B(c));
            CHECK((int)render::R(c) > previousR);
        }
        previousR = render::R(c);
    }
}

TEST("render/compose: five small popups are all visible") {
    render::showPage(PAGE_ID_MAIN);
    for (int i = 0; i < 5; i++) {
        harness::appContext()->pushPage(PAGE_ID_RENDER_STACK1 + i);
    }
    harness::frame(3);
    checkStack(5);
}

TEST("render/compose: six small popups over a page (seven render buffers)") {
    // the render buffers are allocated when they are needed (D5: six of them
    // and the seventh layer was drawn past the array)
    render::showPage(PAGE_ID_MAIN);
    for (int i = 0; i < 6; i++) {
        harness::appContext()->pushPage(PAGE_ID_RENDER_STACK1 + i);
    }
    harness::frame(3);
    checkStack(6);
}

////////////////////////////////////////////////////////////////////////////////
// incremental updates == full redraw

TEST("render/compose: changing texts (opaque, rounded with border, opacity) equal a full redraw") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    for (int n = 0; n < 6; n++) {
        change(0, n);
        change(1, n);
        change(3, n);
        change(4, n);
        harness::frame();
    }
    CHECK_EQ(render::compareWithFullRedraw("dynamic"), 0);
}

TEST("render/compose: a text shrinking and growing equals a full redraw") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    const char *texts[] = { "WWWWWWWW", "i", "", "Wide text here", "x" };
    for (auto t : texts) {
        render::setText(0, t);
        render::setText(4, t);
        harness::frame();
    }
    CHECK_EQ(render::compareWithFullRedraw("shrink"), 0);
}

TEST("render/compose: changes below an open popup equal a full redraw") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    for (int n = 0; n < 4; n++) {
        change(0, n);
        change(4, n);
        harness::frame();
    }
    CHECK_EQ(render::compareWithFullRedraw("below_popup"), 0);
}

TEST("render/compose: changes on an open popup equal a full redraw") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    for (int n = 0; n < 4; n++) {
        change(5, n);
        harness::frame();
    }
    CHECK_EQ(render::compareWithFullRedraw("on_popup"), 0);
}

TEST("render/compose: changes below a transparent page equal a full redraw") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_TRANSPARENT);
    for (int n = 0; n < 4; n++) {
        change(0, n);
        change(1, n);
        harness::frame();
    }
    CHECK_EQ(render::compareWithFullRedraw("below_transparent"), 0);
}

TEST("render/compose: a blinking text without radius equals a full redraw") {
    render::showPage(PAGE_ID_RENDER_BLINK, 1);
    for (int i = 0; i < 3; i++) {
        render::frameToBlink(true);
        render::frameToBlink(false);
    }
    render::frameToBlink(true);
    // render_bl 1 at (20, 20, 200, 50)
    CHECK_EQ(render::compareWithFullRedraw("blink", 20, 20, 200, 50), 0);
}

TEST("render/compose: a changing rounded text without a border equals a full redraw") {
    // render_dyn_r_nb: opaque, radius 10, no border: counted as transparent, so
    // the background is redrawn under the antialiased edge (D4: counted as
    // opaque, the edge was blended again over the previous one on every redraw)
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    for (int n = 0; n < 6; n++) {
        change(2, n);
        harness::frame();
    }
    // the widget c: container (180, 80) + (20, 20), 140 x 40
    CHECK_EQ(render::compareWithFullRedraw("radius_no_border", 200, 100, 140, 40), 0);
}

TEST("render/compose: a blinking rounded text without a border equals a full redraw") {
    render::showPage(PAGE_ID_RENDER_BLINK, 1);
    for (int i = 0; i < 3; i++) {
        render::frameToBlink(true);
        render::frameToBlink(false);
    }
    render::frameToBlink(true);
    // render_bl 2 (render_blink_r) at (240, 20, 200, 50)
    CHECK_EQ(render::compareWithFullRedraw("blink_radius", 240, 20, 200, 50), 0);
}

////////////////////////////////////////////////////////////////////////////////
// damage tracking (counts)

TEST("render/compose: the marker measure: an idle frame composes nothing") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    harness::frame(3);
    float share = composedShare(changeNothing, 0);
    printf("    composed share: %.3f\n", share);
    CHECK_EQ(share, 0.0f);
}

TEST("render/compose: a small change composes only its part of the frame") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    settle(changeA);
    float share = composedShare(changeA, 0);
    printf("    composed share: %.3f\n", share);
    // text a is 120 x 30 of 800 x 480 (0.9 %)
    CHECK(share > 0.0f);
    CHECK(share < 0.05f);
}

TEST("render/compose: opening a popup composes the whole frame") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    harness::frame(2);
    render::putMarkers();
    harness::appContext()->pushPage(PAGE_ID_RENDER_POPUP1);
    harness::frame();
    CHECK_EQ(render::countMarkers(), 0);
}

TEST("render/compose: two small changes far apart compose only their parts") {
    // text a (top left) and text b (bottom right) are two damage rectangles
    // (DP2: one rectangle per frame made it nearly the whole frame)
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    settle(changeAB);
    float share = composedShare(changeAB, 0);
    printf("    composed share: %.3f\n", share);
    CHECK(share < 0.1f);
}

TEST("render/compose: a small change on a popup with backdrop composes only its part") {
    // the layers below the popup and its blurred backdrop come from the backdrop
    // cache (DP1: every dirty frame was a full compose with blur)
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    settle(changeF);
    float share = composedShare(changeF, 0);
    printf("    composed share: %.3f\n", share);
    CHECK(share < 0.1f);
}

////////////////////////////////////////////////////////////////////////////////
// screenshot

TEST("render/compose: takeScreenshot from another thread is the RGB of the presented frame") {
    // the GUI loop runs on this thread, takeScreenshot waits until a frame
    // copies the presented frame
    render::showPage(PAGE_ID_RENDER_COLORS);
    auto frame = render::captureFrame();
    std::atomic<bool> done(false);
    const uint8_t *screenshot = nullptr;
    std::thread thread([&] {
        screenshot = display::takeScreenshot();
        done = true;
    });
    for (int i = 0; i < 1000000 && !done; i++) {
        harness::frame();
    }
    thread.join();
    REQUIRE(screenshot != nullptr);
    int differing = 0;
    for (int y = 0; y < (int)DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < (int)DISPLAY_WIDTH; x++) {
            const uint8_t *p = screenshot + 3 * (y * DISPLAY_WIDTH + x);
            uint32_t c = frame.at(x, y);
            if (p[0] != render::R(c) || p[1] != render::G(c) || p[2] != render::B(c)) differing++;
        }
    }
    CHECK_EQ(differing, 0);
    display::releaseScreenshot();
}
