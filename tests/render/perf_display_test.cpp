/*
 * eez-framework tests: the cost of a frame (display::getFrameStats) for the
 * performance findings of gui/display.cpp: composed, transferred (presented)
 * and blurred pixels for small changes (DP2), for changes above and below a
 * layer with a backdrop (DP1), and the minimum frame interval (DP4).
 *
 * Every measuring test prints its numbers; the checks are the limits the
 * damage tracking has to keep, and every incremental frame is compared with a
 * full redraw.
 */

#include <chrono>
#include <stdio.h>
#include <string.h>

#include <eez/conf-internal.h>
#include <eez/core/os.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>
#include <eez/gui/draw.h>
#include <eez/gui/thread.h>
#include <eez/gui/widgets/display_data.h>

#include "document.h"
#include "harness.h"
#include "render_util.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;

namespace {

const uint32_t SCREEN = DISPLAY_WIDTH * DISPLAY_HEIGHT;

// render_dynamic: text a at (10, 10, 120 x 30), text b at (670, 440, 120 x 30)
const uint32_t TEXT_AREA = 120 * 30;

void setText(int index, int n) {
    char text[32];
    snprintf(text, sizeof(text), "%c%d", 'a' + index, n);
    render::setText(index, text);
}

struct Totals {
    uint64_t composed = 0;
    uint64_t presented = 0;
    uint64_t blurred = 0;
    uint32_t composedRects = 0;
    uint32_t presentedRects = 0;
    int frames = 0;

    void add() {
        auto &stats = display::getFrameStats();
        composed += stats.composedPixels;
        presented += stats.presentedPixels;
        blurred += stats.blurredPixels;
        composedRects += stats.composedRects;
        presentedRects += stats.presentedRects;
        frames++;
    }

    void print(const char *what) const {
        int n = frames ? frames : 1;
        printf("    %s, per frame (%d frames): composed %llu px (%.1f %%) in %.1f rects, presented %llu px (%.1f %%) in %.1f rects, blurred %llu px\n",
            what, frames,
            (unsigned long long)(composed / n), 100.0 * composed / n / SCREEN, (double)composedRects / n,
            (unsigned long long)(presented / n), 100.0 * presented / n / SCREEN, (double)presentedRects / n,
            (unsigned long long)(blurred / n));
    }
};

////////////////////////////////////////////////////////////////////////////////
// A keypad popup: an internal page with shadow and backdrop (like a popup page
// with x > 0) with a text field whose cursor blinks, as in the numeric keypad.

const int POPUP_X = 200;
const int POPUP_Y = 120;
const int POPUP_W = 400;
const int POPUP_H = 240;

// the text field of the keypad, in the popup
const int FIELD_X = POPUP_X + 20;
const int FIELD_Y = POPUP_Y + 20;
const int FIELD_W = 360;
const int FIELD_H = 40;

class KeypadPopup : public InternalPage {
public:
    KeypadPopup() {
        x = POPUP_X;
        y = POPUP_Y;
        width = POPUP_W;
        height = POPUP_H;
    }

    static bool isCursorVisible() {
        return millis() % (2 * CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS) < CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS;
    }

    void updateInternalPage() override {
        bool cursorVisible = isCursorVisible();
        bool all = !g_widgetCursor.hasPreviousState;
        if (!all && cursorVisible == m_cursorVisible) {
            return;
        }
        m_cursorVisible = cursorVisible;
        m_redraws++;

        if (all) {
            display::setColor(COLOR_ID_RENDER_NAVY);
            display::fillRect(x, y, x + width - 1, y + height - 1);
        }

        drawText("12.345", -1, FIELD_X, FIELD_Y, FIELD_W, FIELD_H, getStyle(STYLE_ID_RENDER_DYN),
            false, false, nullptr, nullptr, false, cursorVisible ? 6 : -1);
    }

    WidgetCursor findWidgetInternalPage(int, int, bool) override {
        return WidgetCursor();
    }

    int redraws() const { return m_redraws; }

private:
    bool m_cursorVisible = false;
    int m_redraws = 0;
};

const int KEYPAD_POPUP_PAGE_ID = FIRST_INTERNAL_PAGE_ID + 100;

KeypadPopup g_keypadPopup;

void pushKeypadPopup() {
    harness::appContext()->pushPage(KEYPAD_POPUP_PAGE_ID, &g_keypadPopup);
    harness::frame(3);
}

// runs frames until the cursor of the keypad popup changes, returns the stats of that frame
void frameToCursorChange(Totals &totals) {
    int redraws = g_keypadPopup.redraws();
    for (int i = 0; i < 100; i++) {
        harness::frame();
        if (g_keypadPopup.redraws() != redraws) {
            totals.add();
            return;
        }
    }
    test::fail(__FILE__, __LINE__, "the cursor of the keypad popup doesn't blink");
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// DP2: several damage rectangles, only the frame damage is presented

TEST("render/perf_display: two small changes far apart compose and present only their parts") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    for (int i = 0; i < 3; i++) {
        setText(0, i);
        setText(1, i);
        harness::frame();
    }

    Totals totals;
    for (int i = 0; i < 4; i++) {
        setText(0, 10 + i);
        setText(1, 10 + i);
        harness::frame();
        totals.add();
    }
    totals.print("text a and text b change");

    // the two texts (and the same texts of the frame before)
    CHECK(totals.composed / totals.frames <= 2 * TEXT_AREA);
    CHECK(totals.presented / totals.frames <= 2 * TEXT_AREA);
    CHECK_EQ(render::compareWithFullRedraw("perf_two_changes"), 0);
}

TEST("render/perf_display: changes moving between two texts present only what changed") {
    // text a in one frame, text b in the next one: the render buffer of the
    // frame misses the change of the frame before (stale), which is composed
    // but not presented again
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    for (int i = 0; i < 4; i++) {
        setText(i % 2, i);
        harness::frame();
    }

    Totals totals;
    for (int i = 0; i < 6; i++) {
        setText(i % 2, 10 + i);
        harness::frame();
        totals.add();
    }
    totals.print("text a and text b in turn");

    CHECK(totals.composed / totals.frames <= 2 * TEXT_AREA);
    CHECK(totals.presented / totals.frames <= TEXT_AREA);
    CHECK_EQ(render::compareWithFullRedraw("perf_moving_changes"), 0);
}

TEST("render/perf_display: the same text changing in every frame") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    for (int i = 0; i < 3; i++) {
        setText(0, i);
        harness::frame();
    }

    Totals totals;
    for (int i = 0; i < 4; i++) {
        setText(0, 10 + i);
        harness::frame();
        totals.add();
    }
    totals.print("text a");

    CHECK(totals.composed / totals.frames <= TEXT_AREA);
    CHECK(totals.presented / totals.frames <= TEXT_AREA);
    CHECK_EQ(render::compareWithFullRedraw("perf_same_text"), 0);
}

TEST("render/perf_display: an idle frame composes and presents nothing") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    harness::frame(3);
    harness::frame();
    auto &stats = display::getFrameStats();
    CHECK_EQ(stats.composedPixels, 0u);
    CHECK_EQ(stats.presentedPixels, 0u);
    CHECK_EQ(stats.blurredPixels, 0u);
}

////////////////////////////////////////////////////////////////////////////////
// DP1: layers with a backdrop

TEST("render/perf_display: a blinking cursor in a keypad popup with backdrop") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    pushKeypadPopup();
    REQUIRE(harness::appContext()->getActivePageId() == KEYPAD_POPUP_PAGE_ID);

    Totals totals;
    for (int i = 0; i < 4; i++) {
        frameToCursorChange(totals);
    }
    totals.print("cursor blink");

    // the text field (and the same field of the frame before), no blur
    CHECK(totals.composed / totals.frames <= 2 * FIELD_W * FIELD_H);
    CHECK(totals.presented / totals.frames <= FIELD_W * FIELD_H);
    CHECK_EQ(totals.blurred, 0u);

    CHECK_EQ(render::compareWithFullRedraw("perf_keypad_cursor"), 0);
}

TEST("render/perf_display: a change below a keypad popup with backdrop") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    pushKeypadPopup();

    // text a is outside of the popup, but under the backdrop
    Totals totals;
    for (int i = 0; i < 4; i++) {
        // in the middle of the cursor phase, so only text a changes
        while (millis() % CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS > CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS - 4 * harness::FRAME_MS) {
            harness::frame();
        }
        setText(0, 10 + i);
        harness::frame();
        totals.add();
    }
    totals.print("text a below the backdrop");

    // the blurred text spreads by a few pixels: much less than the screen
    CHECK(totals.composed / totals.frames < SCREEN / 10);
    CHECK(totals.presented / totals.frames < SCREEN / 20);
    CHECK(totals.blurred / totals.frames < SCREEN / 10);

    CHECK_EQ(render::compareWithFullRedraw("perf_below_keypad"), 0);
}

TEST("render/perf_display: the backdrop blurred again in parts equals the backdrop of a popup opened again") {
    // the incremental frames blur only the parts of the backdrop around the
    // changes, opening the popup blurs the whole backdrop
    render::setText(0, "WWWWWWWWWW");
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    const char *texts[] = { "a1", "Wide text here", "i", "WWWW" };
    for (auto text : texts) {
        render::setText(0, text);
        render::setText(1, text);
        render::setText(4, text);
        harness::frame();
    }
    auto incremental = render::captureFrame();

    harness::appContext()->popPage();
    harness::frame(2);
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    int differing = render::countDiff(incremental, render::captureFrame());
    if (differing) {
        render::writeMismatch("perf_backdrop_parts", incremental, render::captureFrame(), 0);
    }
    CHECK_EQ(differing, 0);
}

TEST("render/perf_display: many changes at once equal a full redraw") {
    // more rectangles than the damage region keeps: they are merged
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    Totals totals;
    for (int n = 0; n < 6; n++) {
        for (int i = 0; i < 5; i++) {
            setText(i, 10 * n + i);
        }
        harness::frame();
        totals.add();
    }
    totals.print("texts a to e");
    CHECK_EQ(render::compareWithFullRedraw("perf_many_changes"), 0);

    render::pushPage(PAGE_ID_RENDER_POPUP1);
    for (int n = 0; n < 6; n++) {
        for (int i = 0; i < 6; i++) {
            setText(i, 10 * n + i);
        }
        harness::frame();
    }
    CHECK_EQ(render::compareWithFullRedraw("perf_many_changes_popup"), 0);
}

TEST("render/perf_display: changes below and on two popups with backdrop equal a full redraw") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1);
    render::pushPage(PAGE_ID_RENDER_POPUP2);

    Totals below;
    for (int n = 0; n < 6; n++) {
        setText(0, n);
        setText(4, n);
        harness::frame();
        if (n >= 2) {
            below.add();
        }
    }
    below.print("texts a and e below two popups");
    CHECK(below.composed / below.frames < SCREEN / 10);
    CHECK_EQ(render::compareWithFullRedraw("perf_below_two_popups"), 0);

    // text f is on the first popup, below the backdrop of the second one
    Totals between;
    for (int n = 0; n < 6; n++) {
        setText(5, n);
        harness::frame();
        if (n >= 2) {
            between.add();
        }
    }
    between.print("text f between two popups");
    CHECK(between.composed / between.frames < SCREEN / 10);
    CHECK_EQ(render::compareWithFullRedraw("perf_between_two_popups"), 0);

    // text b is outside of both popups, text f on the second popup
    Totals both;
    for (int n = 0; n < 6; n++) {
        setText(1, n);
        setText(5, n);
        harness::frame();
        if (n >= 2) {
            both.add();
        }
    }
    both.print("text b below and text f between two popups");
    CHECK_EQ(render::compareWithFullRedraw("perf_two_popups_both"), 0);
}

TEST("render/perf_display: a change on a popup with backdrop over a changing page") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_POPUP1);

    Totals totals;
    for (int n = 0; n < 8; n++) {
        setText(5, n);
        if (n % 2) {
            setText(1, n);
        }
        harness::frame();
        if (n >= 2) {
            totals.add();
        }
    }
    totals.print("text f on the popup, text b below in every second frame");
    CHECK(totals.composed / totals.frames < SCREEN / 10);
    CHECK_EQ(render::compareWithFullRedraw("perf_popup_over_changes"), 0);
}

TEST("render/perf_display: changes below a transparent page") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    render::pushPage(PAGE_ID_RENDER_TRANSPARENT);

    Totals totals;
    for (int n = 0; n < 6; n++) {
        setText(0, n);
        harness::frame();
        if (n >= 2) {
            totals.add();
        }
    }
    // the page draws its own blurred background again when the page below
    // changes (setBufferBackgroundBelow), it covers its backdrop, so the
    // backdrop is not blurred again when the page is composed
    totals.print("text a below a transparent page");
    CHECK(totals.blurred / totals.frames <= SCREEN);
    CHECK_EQ(render::compareWithFullRedraw("perf_below_transparent"), 0);
}

////////////////////////////////////////////////////////////////////////////////
// DP4: the minimum time between two frames

TEST("render/perf_display: frames 16 ms apart on the fake clock don't wait") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    auto start = std::chrono::steady_clock::now();
    harness::frame(10);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    printf("    10 frames: %lld ms\n", (long long)ms);
    // ten frames of the idle page (the sanitizer build is slow)
    CHECK(ms < 150);
}

TEST("render/perf_display: a frame right after the previous one waits for the rest of 16 ms") {
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    // the VSYNC message queued by the last frame, without moving the fake clock
    auto start = std::chrono::steady_clock::now();
    processGuiQueue(0);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    printf("    frame without time passed: %lld ms\n", (long long)ms);
    CHECK(ms >= 15);
}
