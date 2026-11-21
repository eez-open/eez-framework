/*
 * eez-framework tests: a group of settings, a column with dividers
 * (CONTAINER_WIDGET_DIVIDERS_FLAG) of switch rows (SwitchWidget with a label,
 * the "On"/"Off" text and an enabled expression), page gui_switch_rows.
 */

#include "gui_test.h"

using namespace gui_test;

// the group: 600x143 at 100, 100, border 1; the rows 598x70 at y 101 and 172,
// the divider at y 171; the track of a row 64x32 at x 619, 19 px below the top
static const int ROW1_Y = 101;
static const int ROW2_Y = 172;
static const int DIVIDER_Y = 171;
static const int TRACK_X = 619;

static const uint32_t BLACK = 0x000000;
static const uint32_t WHITE = 0xFFFFFF;
static const uint32_t ACCENT = 0xC06020;   // #2060c0
static const uint32_t ERROR = 0x0000C0;    // #c00000
static const uint32_t BOOKMARK = 0x00FFFF; // #ffff00
static const uint32_t BACKDROP = 0x202020;

// color of the presented pixel without alpha (0xBBGGRR)
static uint32_t rgb(int x, int y) {
    return harness::pixel(x, y) & 0xFFFFFF;
}

// the number of pixels of the color in the box
static int count(int x1, int y1, int x2, int y2, uint32_t color) {
    int n = 0;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            if (rgb(x, y) == color) {
                n++;
            }
        }
    }
    return n;
}

static void showRows() {
    app()->showPage(PAGE_ID_GUI_SWITCH_ROWS);
    harness::frame(2);
    harness::advance(400);
    harness::frame(2);
}

TEST("gui/switch rows: the column has a divider between its children") {
    showRows();
    CHECK_EQ(rgb(400, DIVIDER_Y), ACCENT);
    CHECK_EQ(rgb(101, DIVIDER_Y), ACCENT);
    CHECK_EQ(rgb(698, DIVIDER_Y), ACCENT);
    // not over the rows, not outside of the border
    CHECK_EQ(rgb(400, DIVIDER_Y - 1), BLACK);
    CHECK_EQ(rgb(400, DIVIDER_Y + 1), BLACK);
    // the border of the group
    CHECK_EQ(rgb(400, 100), ACCENT);
    CHECK_EQ(rgb(400, 242), ACCENT);
}

TEST("gui/switch rows: the label is at the left, the track at the right") {
    showRows();
    CHECK(count(117, ROW1_Y, 300, ROW1_Y + 69, WHITE) > 0);
    // the off part of the track and the knob at the left
    CHECK_EQ(rgb(TRACK_X + 55, ROW1_Y + 35), BACKDROP);
    CHECK_EQ(rgb(TRACK_X + 15, ROW1_Y + 35), WHITE);
    // nothing right of the track (right padding 16)
    CHECK_EQ(rgb(TRACK_X + 64 + 4, ROW1_Y + 35), BLACK);
}

TEST("gui/switch rows: the state text is Off, then On in the checked state of its style") {
    showRows();
    CHECK(count(500, ROW1_Y, TRACK_X - 1, ROW1_Y + 69, ERROR) > 0);
    CHECK_EQ(count(500, ROW1_Y, TRACK_X - 1, ROW1_Y + 69, BOOKMARK), 0);
    tap(200, ROW1_Y + 35);
    CHECK(g_switch);
    // after the animation of the knob (150 ms)
    harness::frame(2);
    CHECK(count(500, ROW1_Y, TRACK_X - 1, ROW1_Y + 69, BOOKMARK) > 0);
    CHECK_EQ(count(500, ROW1_Y, TRACK_X - 1, ROW1_Y + 69, ERROR), 0);
    // the on part of the track and the knob at the right
    CHECK_EQ(rgb(TRACK_X + 10, ROW1_Y + 35), ACCENT);
    CHECK_EQ(rgb(TRACK_X + 50, ROW1_Y + 35), WHITE);
}

TEST("gui/switch rows: both ends of the track are the same, on and off") {
    showRows();
    for (int y = ROW1_Y + 19; y < ROW1_Y + 19 + 32; y++) {
        CHECK_EQ(rgb(TRACK_X + 63, y), rgb(TRACK_X, y));
    }
    tap(200, ROW1_Y + 35);
    CHECK(g_switch);
    harness::frame(2);
    // the last column is the on part too
    CHECK(rgb(TRACK_X + 63, ROW1_Y + 35) != BACKDROP);
    for (int y = ROW1_Y + 19; y < ROW1_Y + 19 + 32; y++) {
        CHECK_EQ(rgb(TRACK_X + 63, y), rgb(TRACK_X, y));
    }
}

TEST("gui/switch rows: a tap on the label toggles the switch") {
    showRows();
    tap(150, ROW1_Y + 35);
    CHECK(g_switch);
    tap(150, ROW1_Y + 35);
    CHECK(!g_switch);
}

TEST("gui/switch rows: a row without the state text has no state text") {
    showRows();
    CHECK_EQ(count(500, ROW2_Y, TRACK_X - 1, ROW2_Y + 69, ERROR), 0);
    tap(200, ROW2_Y + 35);
    CHECK(g_switch2);
    CHECK_EQ(count(500, ROW2_Y, TRACK_X - 1, ROW2_Y + 69, BOOKMARK), 0);
}

TEST("gui/switch rows: a switch that isn't enabled is drawn disabled and doesn't change") {
    g_enabled = false;
    showRows();
    // the disabled state styles: the track, the label
    CHECK_EQ(rgb(TRACK_X + 55, ROW2_Y + 35), ERROR);
    CHECK(count(117, ROW2_Y, 300, ROW2_Y + 69, BACKDROP) > 0);
    CHECK_EQ(count(117, ROW2_Y, 300, ROW2_Y + 69, WHITE), 0);
    tap(200, ROW2_Y + 35);
    CHECK(!g_switch2);
    g_enabled = true;
    harness::frame(2);
    CHECK_EQ(rgb(TRACK_X + 55, ROW2_Y + 35), BACKDROP);
    tap(200, ROW2_Y + 35);
    CHECK(g_switch2);
}

TEST("gui/switch rows: a pressed row is drawn in the pressed state style") {
    showRows();
    // the left padding of the row: the background of the group when not pressed
    CHECK_EQ(rgb(105, ROW1_Y + 35), BLACK);
    press(200, ROW1_Y + 35);
    harness::frame();
    CHECK_EQ(rgb(105, ROW1_Y + 35), BOOKMARK);
    // the track keeps its colors
    CHECK_EQ(rgb(TRACK_X + 55, ROW1_Y + 35), BACKDROP);
    // the finger outside: not pressed
    moveTo(400, 400);
    harness::frame();
    CHECK_EQ(rgb(105, ROW1_Y + 35), BLACK);
    releaseTouch();
    CHECK(!g_switch);
}

TEST("gui/switch rows: a switch that isn't enabled is not drawn pressed") {
    g_enabled = false;
    showRows();
    press(200, ROW2_Y + 35);
    harness::frame();
    CHECK_EQ(rgb(105, ROW2_Y + 35), BLACK);
    releaseTouch();
}
