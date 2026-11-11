/*
 * eez-framework tests: gui/geometry (clipSegment, Rect) and the display state
 * (turnOn/turnOff, dirty flag).
 */

#include <math.h>

#include <eez/conf-internal.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>
#include <eez/gui/geometry.h>

#include "document.h"
#include "harness.h"
#include "render_util.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;

namespace {

const RectangleF R = { 0, 0, 100, 50 };

} // namespace

TEST("render/geometry: clipSegment accepts a segment inside unchanged") {
    PointF p1 = { 10, 10 }, p2 = { 90, 40 };
    CHECK(clipSegment(R, p1, p2));
    CHECK_EQ(p1.x, 10.0f);
    CHECK_EQ(p2.y, 40.0f);
}

TEST("render/geometry: clipSegment rejects a segment outside on one side") {
    PointF p1 = { -10, 10 }, p2 = { -5, 40 };
    CHECK(!clipSegment(R, p1, p2));
    PointF p3 = { 10, 60 }, p4 = { 90, 70 };
    CHECK(!clipSegment(R, p3, p4));
}

TEST("render/geometry: clipSegment clips a horizontal and a vertical line to the edges") {
    PointF p1 = { -50, 20 }, p2 = { 150, 20 };
    REQUIRE(clipSegment(R, p1, p2));
    CHECK_EQ(p1.x, 0.0f);
    CHECK_EQ(p2.x, 100.0f);
    CHECK_EQ(p1.y, 20.0f);
    PointF p3 = { 30, -20 }, p4 = { 30, 80 };
    REQUIRE(clipSegment(R, p3, p4));
    CHECK_EQ(p3.y, 0.0f);
    CHECK_EQ(p4.y, 50.0f);
    CHECK_EQ(p3.x, 30.0f);
}

TEST("render/geometry: clipSegment clips a diagonal crossing two edges") {
    PointF p1 = { -10, -5 }, p2 = { 110, 55 };
    REQUIRE(clipSegment(R, p1, p2));
    CHECK_NEAR(p1.x, 0, 1e-4);
    CHECK_NEAR(p1.y, 0, 1e-4);
    CHECK_NEAR(p2.x, 100, 1e-4);
    CHECK_NEAR(p2.y, 50, 1e-4);
}

TEST("render/geometry: clipSegment rejects a diagonal passing the corner outside") {
    PointF p1 = { 90, -20 }, p2 = { 130, 20 };
    CHECK(!clipSegment(R, p1, p2));
}

TEST("render/geometry: clipSegment of a point") {
    PointF p1 = { 10, 10 }, p2 = { 10, 10 };
    CHECK(clipSegment(R, p1, p2));
    PointF p3 = { 200, 10 }, p4 = { 200, 10 };
    CHECK(!clipSegment(R, p3, p4));
}

TEST("render/geometry: clipSegment to an infinite end point") {
    PointF p1 = { 10, 20 }, p2 = { INFINITY, 20 };
    REQUIRE(clipSegment(R, p1, p2));
    CHECK_EQ(p2.x, 100.0f);
    CHECK_EQ(p2.y, 20.0f);
}

TEST("render/geometry: clipSegment classifies a NaN point as inside") {
    // pinned: NaN fails every comparison, the segment is accepted with the NaN
    // (the raster leaves the NaN point out, see D8)
    PointF p3 = { NAN, 20 }, p4 = { 50, 20 };
    CHECK(clipSegment(R, p3, p4));
    CHECK(isnan(p3.x));
}

TEST("render/geometry: Rect equality") {
    Rect a = { 1, 2, 3, 4 };
    Rect b = { 1, 2, 3, 4 };
    Rect c = { 1, 2, 3, 5 };
    CHECK(a == b);
    CHECK(a != c);
    CHECK(!(a == c));
}

////////////////////////////////////////////////////////////////////////////////
// display state

TEST("render/display: turnOff and turnOn change the display state") {
    CHECK(display::isOn());
    display::turnOff();
    harness::frame(2);
    CHECK(!display::isOn());
    CHECK_EQ((int)display::g_displayState, (int)display::OFF);
    display::turnOn();
    harness::frame(2);
    CHECK(display::isOn());
}

TEST("render/display: with SDL the frames are still drawn while the display is off") {
    // pinned: update() returns early in the OFF state only without SDL
    render::showPage(PAGE_ID_RENDER_DYNAMIC);
    display::turnOff();
    render::setText(0, "WWWW");
    harness::frame(2);
    auto off = render::captureRegion(10, 10, 120, 30);
    display::turnOn();
    refreshScreen();
    harness::frame(2);
    CHECK_EQ(render::countDiff(off, render::captureRegion(10, 10, 120, 30)), 0);
}

TEST("render/display: drawing sets the dirty flag of the render buffer") {
    render::resetDrawState();
    display::clearRenderBufferDirty();
    CHECK(!display::isRenderBufferDirty());
    display::setColor(COLOR_ID_TEXT);
    display::fillRect(0, 0, 0, 0);
    CHECK(display::isRenderBufferDirty());
    display::clearRenderBufferDirty();
    display::markRenderBufferDirty();
    CHECK(display::isRenderBufferDirty());
}

TEST("render/display: getDisplayWidth and getDisplayHeight") {
    CHECK_EQ(display::getDisplayWidth(), 800);
    CHECK_EQ(display::getDisplayHeight(), 480);
}
