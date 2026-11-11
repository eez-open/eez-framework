/*
 * eez-framework tests: gui/raster (polygon fill, strokes, rounded rectangles).
 *
 * raster.cpp has no framework dependencies, so it is tested directly with a
 * target that records what is drawn: every pixel blended with the "over"
 * operator, and every write outside the clip rectangle as a violation. Shapes
 * are drawn in opaque white, so the alpha of a pixel is its coverage.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include <eez/conf-internal.h>
#include <eez/gui/raster.h>

#include "render_util.h"
#include "test.h"

using namespace eez::gui;
using raster::Path;

namespace {

const float PI = 3.14159265358979f;
const uint32_t WHITE = 0xFFFFFFFF;

struct TestTarget : raster::Target {
    int w;
    int h;
    std::vector<uint32_t> px;
    int violations = 0;
    int spans = 0;
    int fills = 0;

    TestTarget(int w_, int h_, uint32_t background = 0) : w(w_), h(h_), px((size_t)w_ * h_, background) {
        setClip(0, 0, w - 1, h - 1);
    }

    void setClip(int x1, int y1, int x2, int y2) {
        clipX1 = x1;
        clipY1 = y1;
        clipX2 = x2;
        clipY2 = y2;
    }

    void blend(int x, int y, uint32_t c) {
        if (x < clipX1 || x > clipX2 || y < clipY1 || y > clipY2 || x < 0 || y < 0 || x >= w || y >= h) {
            violations++;
            return;
        }
        uint32_t &d = px[(size_t)y * w + x];
        float a = (c >> 24) / 255.0f;
        float da = (d >> 24) / 255.0f;
        float ra = a + da * (1 - a);
        if (ra <= 0) {
            return;
        }
        uint32_t result = 0;
        for (int i = 0; i < 3; i++) {
            float cc = (c >> (8 * i)) & 0xFF;
            float dc = (d >> (8 * i)) & 0xFF;
            float v = (cc * a + dc * da * (1 - a)) / ra;
            result |= (uint32_t)(v + 0.5f) << (8 * i);
        }
        result |= (uint32_t)(ra * 255 + 0.5f) << 24;
        d = result;
    }

    void blendSpan(int x, int y, int n, const uint32_t *pixels) override {
        spans++;
        if (n <= 0) {
            violations++;
        }
        for (int i = 0; i < n; i++) {
            if (pixels[i] >> 24) {
                blend(x + i, y, pixels[i]);
            }
        }
    }

    void fillRect(int x1, int y1, int x2, int y2, uint32_t color) override {
        fills++;
        if (x1 > x2 || y1 > y2) {
            violations++;
        }
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                blend(x, y, color);
            }
        }
    }

    uint32_t at(int x, int y) const { return px[(size_t)y * w + x]; }
    int alpha(int x, int y) const { return at(x, y) >> 24; }
    float coverage(int x, int y) const { return alpha(x, y) / 255.0f; }

    double area() const {
        double sum = 0;
        for (auto p : px) {
            sum += (p >> 24) / 255.0;
        }
        return sum;
    }

    int drawnPixels() const {
        int n = 0;
        for (auto p : px) {
            if (p >> 24) n++;
        }
        return n;
    }

    render::Image32 image() const {
        render::Image32 image(w, h);
        for (size_t i = 0; i < px.size(); i++) {
            image.px[i] = px[i] | 0xFF000000u;
        }
        return image;
    }
};

void rectPath(Path &path, float x1, float y1, float x2, float y2) {
    path.moveTo(x1, y1);
    path.lineTo(x2, y1);
    path.lineTo(x2, y2);
    path.lineTo(x1, y2);
    path.close();
}

void circlePath(Path &path, float cx, float cy, float r) {
    path.arc(cx, cy, r, r, 0, 2 * PI);
    path.close();
}

void radii(float r[8], float v) {
    for (int i = 0; i < 8; i++) r[i] = v;
}

// Arcs are replaced by lines with the points on the arc, at most ARC_TOLERANCE
// (0.05 px) from it, so a shape with arcs is a little smaller: by at most the
// arc length times the tolerance (plus the rounding of the alpha).
bool areaWithArcs(double area, double exact, double arcLength) {
    return area <= exact + 0.5 && area >= exact - 0.05 * arcLength - 0.5;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// fillPath

TEST("render/raster: fillPath of a pixel aligned square covers exactly its pixels") {
    TestTarget t(40, 40);
    Path path;
    rectPath(path, 10, 10, 20, 20);
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.violations, 0);
    CHECK_EQ(t.drawnPixels(), 100);
    CHECK_EQ(t.alpha(10, 10), 255);
    CHECK_EQ(t.alpha(19, 19), 255);
    CHECK_EQ(t.alpha(9, 10), 0);
    CHECK_EQ(t.alpha(20, 15), 0);
    CHECK_EQ(t.alpha(15, 20), 0);
}

TEST("render/raster: fillPath of a square at half pixels covers the edge pixels by half") {
    TestTarget t(40, 40);
    Path path;
    rectPath(path, 10.5f, 10.5f, 20.5f, 20.5f);
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.violations, 0);
    CHECK_NEAR(t.area(), 100.0, 0.1);
    CHECK_EQ(t.alpha(15, 15), 255);
    CHECK_NEAR(t.coverage(10, 15), 0.5, 0.01);
    CHECK_NEAR(t.coverage(20, 15), 0.5, 0.01);
    CHECK_NEAR(t.coverage(15, 10), 0.5, 0.01);
    CHECK_NEAR(t.coverage(10, 10), 0.25, 0.01);
    CHECK_NEAR(t.coverage(20, 20), 0.25, 0.01);
}

TEST("render/raster: fillPath of a triangle has the area of the triangle") {
    TestTarget t(64, 64);
    Path path;
    path.moveTo(5, 5);
    path.lineTo(45, 5);
    path.lineTo(5, 45);
    path.close();
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.violations, 0);
    CHECK_NEAR(t.area(), 800.0, 0.5);
    // the diagonal pixels are half covered
    CHECK_NEAR(t.coverage(24, 25), 0.5, 0.01);
}

TEST("render/raster: fillPath of a circle has the area of the circle") {
    TestTarget t(64, 64);
    Path path;
    circlePath(path, 32, 32, 20);
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.violations, 0);
    CHECK(areaWithArcs(t.area(), PI * 400, 2 * PI * 20));
    CHECK_EQ(t.alpha(32, 32), 255);
    CHECK_EQ(t.alpha(32, 11), 0);
}

TEST("render/raster: fillPath of a circle is symmetric") {
    TestTarget t(64, 64);
    Path path;
    circlePath(path, 32, 32, 15.3f);
    raster::fillPath(t, path, WHITE);
    int worst = 0;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            int a = t.alpha(x, y);
            worst = std::max(worst, abs(a - t.alpha(63 - x, y)));
            worst = std::max(worst, abs(a - t.alpha(x, 63 - y)));
            worst = std::max(worst, abs(a - t.alpha(y, x)));
        }
    }
    // the arc is approximated by lines (0.05 px), so the mirror images differ a little
    CHECK(worst <= 12);
}

TEST("render/raster: fillPath with the direction reversed is the same") {
    TestTarget a(40, 40), b(40, 40);
    Path cw, ccw;
    cw.moveTo(5, 5); cw.lineTo(30, 8); cw.lineTo(20, 33); cw.close();
    ccw.moveTo(20, 33); ccw.lineTo(30, 8); ccw.lineTo(5, 5); ccw.close();
    raster::fillPath(a, cw, WHITE);
    raster::fillPath(b, ccw, WHITE);
    CHECK(a.px == b.px);
}

TEST("render/raster: fillPath nonzero winding, a reversed inner contour is a hole") {
    TestTarget t(40, 40);
    Path path;
    rectPath(path, 5, 5, 35, 35);
    // inner square in the other direction
    path.moveTo(15, 15);
    path.lineTo(15, 25);
    path.lineTo(25, 25);
    path.lineTo(25, 15);
    path.close();
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.alpha(20, 20), 0);
    CHECK_EQ(t.alpha(10, 10), 255);
    CHECK_NEAR(t.area(), 900.0 - 100.0, 0.05);
}

TEST("render/raster: fillPath nonzero winding, an inner contour in the same direction is filled once") {
    TestTarget t(40, 40);
    Path path;
    rectPath(path, 5, 5, 35, 35);
    rectPath(path, 15, 15, 25, 25);
    raster::fillPath(t, path, 0x80FFFFFF);
    // coverage is clamped to 1, the alpha is not doubled
    CHECK_EQ(t.alpha(20, 20), 128);
    CHECK_EQ(t.alpha(10, 10), 128);
}

TEST("render/raster: Path lineTo without moveTo starts a contour, lineTo after close starts another") {
    TestTarget a(40, 40), b(40, 40);
    Path p1;
    p1.lineTo(5, 5); p1.lineTo(15, 5); p1.lineTo(5, 15); p1.close();
    p1.lineTo(20, 20); p1.lineTo(30, 20); p1.lineTo(20, 30);
    Path p2;
    p2.moveTo(5, 5); p2.lineTo(15, 5); p2.lineTo(5, 15); p2.close();
    p2.moveTo(20, 20); p2.lineTo(30, 20); p2.lineTo(20, 30); p2.close();
    raster::fillPath(a, p1, WHITE);
    raster::fillPath(b, p2, WHITE);
    CHECK(a.px == b.px);
    CHECK_NEAR(a.area(), 100.0, 0.1);
}

TEST("render/raster: fillPath of an arc with the sweep reversed is the same shape") {
    TestTarget a(64, 64), b(64, 64);
    Path p1, p2;
    p1.moveTo(32, 32); p1.arc(32, 32, 25, 25, 0, PI); p1.close();
    p2.moveTo(32, 32); p2.arc(32, 32, 25, 25, PI, 0); p2.close();
    raster::fillPath(a, p1, WHITE);
    raster::fillPath(b, p2, WHITE);
    CHECK(areaWithArcs(a.area(), PI * 625 / 2, PI * 25));
    CHECK_NEAR(a.area(), b.area(), 0.01);
}

TEST("render/raster: fillPath of an ellipse has the area of the ellipse") {
    TestTarget t(80, 64);
    Path path;
    path.arc(40, 32, 30, 12, 0, 2 * PI);
    path.close();
    raster::fillPath(t, path, WHITE);
    CHECK(areaWithArcs(t.area(), PI * 30 * 12, 2 * PI * 22.8));
}

TEST("render/raster: fillPath of an empty, degenerate or transparent path draws nothing") {
    TestTarget t(40, 40);
    Path empty;
    raster::fillPath(t, empty, WHITE);
    Path line;
    line.moveTo(5, 5); line.lineTo(30, 30); line.lineTo(10, 10); line.close();
    raster::fillPath(t, line, WHITE);
    Path square;
    rectPath(square, 5, 5, 30, 30);
    raster::fillPath(t, square, 0x00FFFFFF);
    CHECK_EQ(t.drawnPixels(), 0);
    CHECK_EQ(t.violations, 0);
    CHECK(empty.isEmpty());
    Path cleared;
    rectPath(cleared, 5, 5, 30, 30);
    cleared.clear();
    CHECK(cleared.isEmpty());
}

TEST("render/raster: fillPath with a semi-transparent color scales the coverage") {
    TestTarget t(40, 40);
    Path path;
    rectPath(path, 10.5f, 10, 20.5f, 20);
    raster::fillPath(t, path, 0x80FFFFFF);
    CHECK_EQ(t.alpha(15, 15), 128);
    CHECK_EQ(t.alpha(10, 15), 64);
}

TEST("render/raster: fillPath clipped is the unclipped drawing inside the clip rectangle") {
    TestTarget full(64, 64), clipped(64, 64);
    clipped.setClip(10, 20, 40, 45);
    Path path;
    circlePath(path, 30, 30, 22.4f);
    raster::fillPath(full, path, WHITE);
    raster::fillPath(clipped, path, WHITE);
    CHECK_EQ(clipped.violations, 0);
    int differing = 0;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            bool inside = x >= 10 && x <= 40 && y >= 20 && y <= 45;
            uint32_t expected = inside ? full.at(x, y) : 0;
            if (clipped.at(x, y) != expected) differing++;
        }
    }
    CHECK_EQ(differing, 0);
}

TEST("render/raster: fillPath partly left of and above the target is the visible part of the shape") {
    // the same circle, once at (0, 0) and once moved by (40, 30) into the target
    TestTarget a(64, 64), b(128, 128);
    Path p1, p2;
    circlePath(p1, 0.3f, 0.7f, 20);
    circlePath(p2, 40.3f, 30.7f, 20);
    raster::fillPath(a, p1, WHITE);
    raster::fillPath(b, p2, WHITE);
    CHECK_EQ(a.violations, 0);
    int worst = 0;
    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 30; x++) {
            worst = std::max(worst, abs(a.alpha(x, y) - b.alpha(x + 40, y + 30)));
        }
    }
    CHECK(worst <= 1);
}

TEST("render/raster: fillPath moved by whole pixels gives the same pixels moved") {
    TestTarget a(64, 64), b(64, 64);
    Path p1, p2;
    p1.moveTo(5.3f, 7.1f); p1.lineTo(28.8f, 9.4f); p1.lineTo(17.2f, 30.6f); p1.close();
    p2.moveTo(5.3f + 13, 7.1f + 21); p2.lineTo(28.8f + 13, 9.4f + 21); p2.lineTo(17.2f + 13, 30.6f + 21); p2.close();
    raster::fillPath(a, p1, WHITE);
    raster::fillPath(b, p2, WHITE);
    int worst = 0;
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            worst = std::max(worst, abs(a.alpha(x, y) - b.alpha(x + 13, y + 21)));
        }
    }
    CHECK(worst <= 1);
}

TEST("render/raster: fillPath completely outside of the target draws nothing") {
    TestTarget t(40, 40);
    Path path;
    rectPath(path, 50, 5, 70, 30);
    rectPath(path, -30, -30, -1, -1);
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.drawnPixels(), 0);
    CHECK_EQ(t.spans, 0);
}

TEST("render/raster: fillPath of a shape larger than the target fills it") {
    TestTarget t(40, 40);
    Path path;
    rectPath(path, -1000, -1000, 1000, 1000);
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.drawnPixels(), 1600);
    CHECK_NEAR(t.area(), 1600.0, 0.01);
    CHECK_EQ(t.violations, 0);
}

////////////////////////////////////////////////////////////////////////////////
// strokePath

TEST("render/raster: strokePath of a 1 px horizontal line through pixel centers") {
    TestTarget t(64, 40);
    Path path;
    path.moveTo(10.5f, 20.5f);
    path.lineTo(50.5f, 20.5f);
    raster::strokePath(t, path, 1.0f, WHITE);
    CHECK_EQ(t.violations, 0);
    for (int x = 11; x <= 50; x++) {
        CHECK_EQ(t.alpha(x, 20), 255);
        CHECK_EQ(t.alpha(x, 19), 0);
        CHECK_EQ(t.alpha(x, 21), 0);
    }
    // round caps reach half the width beyond the ends
    CHECK_EQ(t.alpha(10, 20), 255);
    CHECK_EQ(t.alpha(8, 20), 0);
    CHECK_EQ(t.alpha(52, 20), 0);
}

TEST("render/raster: strokePath of a 2 px vertical line covers two columns") {
    TestTarget t(40, 64);
    Path path;
    path.moveTo(20, 10.5f);
    path.lineTo(20, 50.5f);
    raster::strokePath(t, path, 2.0f, WHITE);
    for (int y = 12; y <= 48; y++) {
        CHECK_EQ(t.alpha(19, y), 255);
        CHECK_EQ(t.alpha(20, y), 255);
        CHECK_EQ(t.alpha(18, y), 0);
        CHECK_EQ(t.alpha(21, y), 0);
    }
}

TEST("render/raster: strokePath of a single point is a round dot") {
    TestTarget t(40, 40);
    Path path;
    path.moveTo(20, 20);
    raster::strokePath(t, path, 8.0f, WHITE);
    CHECK_EQ(t.alpha(19, 19), 255);
    CHECK_EQ(t.alpha(20, 20), 255);
    CHECK_EQ(t.alpha(10, 20), 0);
    // round, the corners of the bounding square are not covered
    CHECK_EQ(t.alpha(16, 16), 0);
    int worst = 0;
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            worst = std::max(worst, abs(t.alpha(x, y) - t.alpha(39 - x, y)));
            worst = std::max(worst, abs(t.alpha(x, y) - t.alpha(y, x)));
        }
    }
    CHECK_EQ(worst, 0);
}

TEST("render/raster: strokePath of a closed contour draws the closing segment") {
    TestTarget open(40, 40), closed(40, 40);
    Path p1, p2;
    p1.moveTo(5.5f, 5.5f); p1.lineTo(30.5f, 5.5f); p1.lineTo(30.5f, 30.5f);
    p2.moveTo(5.5f, 5.5f); p2.lineTo(30.5f, 5.5f); p2.lineTo(30.5f, 30.5f); p2.close();
    raster::strokePath(open, p1, 1.0f, WHITE);
    raster::strokePath(closed, p2, 1.0f, WHITE);
    // the diagonal from (30.5, 30.5) back to (5.5, 5.5)
    CHECK_EQ(open.alpha(18, 18), 0);
    CHECK(closed.alpha(18, 18) > 200);
}

TEST("render/raster: strokePath joins are round and overlaps are not counted twice") {
    TestTarget t(40, 40);
    Path path;
    path.moveTo(5.5f, 20.5f); path.lineTo(20.5f, 20.5f); path.lineTo(20.5f, 5.5f);
    raster::strokePath(t, path, 3.0f, 0x80FFFFFF);
    // the join pixel is drawn once with the line alpha
    CHECK_EQ(t.alpha(20, 20), 128);
    CHECK_EQ(t.alpha(12, 20), 128);
}

TEST("render/raster: strokePath with a zero, negative or NaN width draws nothing") {
    TestTarget t(40, 40);
    Path path;
    path.moveTo(5, 5);
    path.lineTo(30, 30);
    raster::strokePath(t, path, 0, WHITE);
    raster::strokePath(t, path, -2, WHITE);
    raster::strokePath(t, path, NAN, WHITE);
    raster::strokePath(t, path, 2, 0x00FFFFFF);
    CHECK_EQ(t.drawnPixels(), 0);
}

TEST("render/raster: strokePath clipped is the unclipped drawing inside the clip rectangle") {
    TestTarget full(64, 64), clipped(64, 64);
    clipped.setClip(0, 15, 30, 63);
    Path path;
    path.moveTo(3.2f, 4.7f); path.lineTo(58.1f, 33.3f); path.lineTo(12.4f, 60.2f);
    raster::strokePath(full, path, 3.5f, WHITE);
    raster::strokePath(clipped, path, 3.5f, WHITE);
    CHECK_EQ(clipped.violations, 0);
    int differing = 0;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            bool inside = x <= 30 && y >= 15;
            if (clipped.at(x, y) != (inside ? full.at(x, y) : 0)) differing++;
        }
    }
    CHECK_EQ(differing, 0);
}

TEST("render/raster: strokePath of a diagonal line has the area of its capsule") {
    TestTarget t(64, 64);
    Path path;
    path.moveTo(10, 10);
    path.lineTo(50, 40);
    raster::strokePath(t, path, 4.0f, WHITE);
    // length 50, width 4, two half-disc caps of radius 2
    CHECK_NEAR(t.area(), 50 * 4 + PI * 4, 6.0);
}

////////////////////////////////////////////////////////////////////////////////
// roundedRect

TEST("render/raster: roundedRect without radius fills exactly the rectangle (inclusive)") {
    TestTarget t(40, 40);
    float r[8];
    radii(r, 0);
    raster::roundedRect(t, 5, 6, 24, 15, 0, r, 0, WHITE);
    CHECK_EQ(t.violations, 0);
    CHECK_EQ(t.drawnPixels(), 20 * 10);
    CHECK_NEAR(t.area(), 200.0, 0.01);
    CHECK_EQ(t.alpha(5, 6), 255);
    CHECK_EQ(t.alpha(24, 15), 255);
}

TEST("render/raster: roundedRect of a single pixel") {
    TestTarget t(10, 10);
    float r[8];
    radii(r, 0);
    raster::roundedRect(t, 4, 4, 4, 4, 0, r, 0, WHITE);
    CHECK_EQ(t.drawnPixels(), 1);
    CHECK_EQ(t.alpha(4, 4), 255);
}

TEST("render/raster: roundedRect with x2 < x1 or y2 < y1 draws nothing") {
    TestTarget t(40, 40);
    float r[8];
    radii(r, 5);
    raster::roundedRect(t, 20, 5, 10, 30, 0, r, 0, WHITE);
    raster::roundedRect(t, 5, 20, 30, 10, 0, r, 0, WHITE);
    raster::roundedRect(t, 20, 5, 10, 30, 2, r, WHITE, WHITE);
    CHECK_EQ(t.drawnPixels(), 0);
    CHECK_EQ(t.violations, 0);
}

TEST("render/raster: roundedRect with radius has transparent corners and a full middle") {
    TestTarget t(64, 64);
    float r[8];
    radii(r, 10);
    raster::roundedRect(t, 2, 2, 61, 41, 0, r, 0, WHITE);
    CHECK_EQ(t.alpha(2, 2), 0);
    CHECK_EQ(t.alpha(61, 41), 0);
    CHECK_EQ(t.alpha(31, 2), 255);
    CHECK_EQ(t.alpha(2, 21), 255);
    // no seam between the bands and the strips: the rows between the corners are full
    for (int y = 13; y <= 30; y++) {
        for (int x = 2; x <= 61; x++) {
            if (t.alpha(x, y) != 255) {
                CHECK_EQ(t.alpha(x, y), 255);
                y = 100;
                break;
            }
        }
    }
    CHECK(areaWithArcs(t.area(), 60.0 * 40 - (4 - PI) * 100, 2 * PI * 10));
}

TEST("render/raster: roundedRect is symmetric") {
    TestTarget t(64, 48);
    float r[8];
    radii(r, 13.5f);
    raster::roundedRect(t, 3, 4, 60, 43, 3, r, 0xFF00FFFF, 0xFFFF0000);
    int worst = 0;
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 64; x++) {
            uint32_t a = t.at(x, y);
            uint32_t mx = t.at(63 - x, y);
            uint32_t my = t.at(x, 47 - y);
            for (int i = 0; i < 32; i += 8) {
                worst = std::max(worst, abs((int)((a >> i) & 0xFF) - (int)((mx >> i) & 0xFF)));
                worst = std::max(worst, abs((int)((a >> i) & 0xFF) - (int)((my >> i) & 0xFF)));
            }
        }
    }
    CHECK(worst <= 2);
}

TEST("render/raster: roundedRect radii larger than the rectangle are reduced in proportion") {
    TestTarget a(64, 32), b(64, 32);
    float big[8], fit[8];
    radii(big, 1000);
    radii(fit, 10);
    // 40 x 20: radius 1000 is reduced to 10 (half the height), a pill
    raster::roundedRect(a, 10, 5, 49, 24, 0, big, 0, WHITE);
    raster::roundedRect(b, 10, 5, 49, 24, 0, fit, 0, WHITE);
    CHECK(a.px == b.px);
}

TEST("render/raster: roundedRect negative radii are square corners") {
    TestTarget a(40, 40), b(40, 40);
    float neg[8], zero[8];
    radii(neg, -5);
    radii(zero, 0);
    raster::roundedRect(a, 5, 5, 30, 30, 0, neg, 0, WHITE);
    raster::roundedRect(b, 5, 5, 30, 30, 0, zero, 0, WHITE);
    CHECK(a.px == b.px);
}

TEST("render/raster: roundedRect with only one rounded corner") {
    TestTarget t(64, 64);
    float r[8] = { 0, 0, 0, 0, 20, 20, 0, 0 }; // bottom right
    raster::roundedRect(t, 2, 2, 61, 61, 0, r, 0, WHITE);
    CHECK_EQ(t.alpha(2, 2), 255);
    CHECK_EQ(t.alpha(61, 2), 255);
    CHECK_EQ(t.alpha(2, 61), 255);
    CHECK_EQ(t.alpha(61, 61), 0);
}

TEST("render/raster: roundedRect elliptical corners") {
    TestTarget t(80, 64);
    // rx 20, ry 8 at every corner
    float r[8] = { 20, 8, 20, 8, 20, 8, 20, 8 };
    raster::roundedRect(t, 2, 2, 77, 61, 0, r, 0, WHITE);
    CHECK(areaWithArcs(t.area(), 76.0 * 60 - (4 - PI) * 20 * 8, 2 * PI * 15));
    // along the top edge the corner ends 20 px in, along the left edge 8 px down
    CHECK(t.alpha(14, 2) < 128);
    CHECK_EQ(t.alpha(23, 2), 255);
    CHECK_EQ(t.alpha(2, 11), 255);
}

TEST("render/raster: roundedRect line and fill: the line in the line color, the fill inside it") {
    TestTarget t(64, 64);
    float r[8];
    radii(r, 8);
    const uint32_t lineColor = 0xFF0000FF; // red
    const uint32_t fillColor = 0xFFFF0000; // blue
    raster::roundedRect(t, 4, 4, 59, 59, 4, r, lineColor, fillColor);
    CHECK_EQ(t.at(31, 4), lineColor);
    CHECK_EQ(t.at(31, 7), lineColor);
    CHECK_EQ(t.at(31, 8), fillColor);
    CHECK_EQ(t.at(31, 31), fillColor);
    CHECK_EQ(t.at(4, 31), lineColor);
    CHECK_EQ(t.alpha(4, 4), 0);
}

TEST("render/raster: roundedRect line only leaves the inside untouched") {
    TestTarget t(64, 64);
    float r[8];
    radii(r, 8);
    raster::roundedRect(t, 4, 4, 59, 59, 2, r, WHITE, 0);
    CHECK_EQ(t.alpha(31, 31), 0);
    CHECK_EQ(t.alpha(31, 4), 255);
    CHECK_EQ(t.alpha(31, 5), 255);
    CHECK_EQ(t.alpha(31, 6), 0);
}

TEST("render/raster: roundedRect line wider than the rectangle fills it with the line color") {
    TestTarget t(40, 40);
    float r[8];
    radii(r, 0);
    raster::roundedRect(t, 10, 10, 15, 15, 20, r, 0xFF0000FF, 0xFFFF0000);
    CHECK_EQ(t.violations, 0);
    CHECK_EQ(t.at(12, 12), 0xFF0000FFu);
    CHECK_EQ(t.alpha(9, 12), 0);
    CHECK_EQ(t.alpha(16, 12), 0);
}

TEST("render/raster: roundedRect clipped is the unclipped drawing inside the clip rectangle") {
    TestTarget full(96, 96), clipped(96, 96);
    clipped.setClip(0, 0, 40, 30);
    float r[8];
    radii(r, 12);
    raster::roundedRect(full, 5, 5, 90, 90, 3, r, 0xFF00FF00, 0xFF808080);
    raster::roundedRect(clipped, 5, 5, 90, 90, 3, r, 0xFF00FF00, 0xFF808080);
    CHECK_EQ(clipped.violations, 0);
    int differing = 0;
    for (int y = 0; y < 96; y++) {
        for (int x = 0; x < 96; x++) {
            bool inside = x <= 40 && y <= 30;
            if (clipped.at(x, y) != (inside ? full.at(x, y) : 0)) differing++;
        }
    }
    CHECK_EQ(differing, 0);
}

TEST("render/raster: roundedRect larger than the target stays inside the clip rectangle") {
    TestTarget t(40, 40);
    float r[8];
    radii(r, 30);
    raster::roundedRect(t, -100, -50, 200, 300, 5, r, WHITE, 0xFF808080);
    CHECK_EQ(t.violations, 0);
    CHECK_EQ(t.drawnPixels(), 1600);
}

TEST("render/raster: roundedRect draws every pixel once (bands, strips and fill don't overlap)") {
    TestTarget t(128, 96);
    float r[8];
    radii(r, 9);
    raster::roundedRect(t, 0, 0, 127, 95, 0, r, 0, 0x80FFFFFF);
    // a pixel blended twice with alpha 128 would be about 191
    CHECK_EQ(t.alpha(64, 48), 128);
    CHECK_EQ(t.alpha(1, 48), 128);
    CHECK_EQ(t.alpha(64, 1), 128);
    CHECK_EQ(t.alpha(5, 13), 128);
    CHECK(t.fills >= 1);
}

////////////////////////////////////////////////////////////////////////////////
// golden image of the raster shapes

TEST("render/raster: golden shapes") {
    TestTarget t(400, 200, 0xFF000000);

    Path p;
    circlePath(p, 40, 40, 30);
    raster::fillPath(t, p, 0xFF00C0FF);

    Path star;
    for (int i = 0; i < 5; i++) {
        float a = -PI / 2 + i * 4 * PI / 5;
        star.lineTo(120 + 32 * cosf(a), 42 + 32 * sinf(a));
    }
    star.close();
    raster::fillPath(t, star, 0xFF40FF40);

    Path ell;
    ell.arc(210, 40, 36, 18, 0, 2 * PI);
    ell.close();
    raster::fillPath(t, ell, 0xA0FF8000);

    Path wave;
    for (int i = 0; i <= 60; i++) {
        wave.lineTo(260 + i * 2.2f, 40 + 25 * sinf(i * 0.25f));
    }
    raster::strokePath(t, wave, 3.0f, 0xFFFFFFFF);

    Path zig;
    zig.moveTo(10, 120); zig.lineTo(40, 90); zig.lineTo(70, 150); zig.lineTo(100, 100);
    raster::strokePath(t, zig, 1.0f, 0xFF00FFFF);
    Path zig2;
    zig2.moveTo(10, 160); zig2.lineTo(40, 130); zig2.lineTo(70, 190); zig2.lineTo(100, 140);
    raster::strokePath(t, zig2, 6.0f, 0x80FFFFFF);

    float r1[8];
    radii(r1, 12);
    raster::roundedRect(t, 120, 90, 219, 189, 4, r1, 0xFFFFFFFF, 0xFF804020);
    float r2[8] = { 0, 0, 25, 10, 5, 5, 30, 30 };
    raster::roundedRect(t, 230, 90, 319, 189, 2, r2, 0xFF00FF00, 0x80FF00FF);
    float r3[8];
    radii(r3, 100);
    raster::roundedRect(t, 330, 90, 389, 119, 0, r3, 0, 0xFF0080FF);
    raster::roundedRect(t, 330, 130, 389, 189, 9, r3, 0xFFFFFF00, 0);

    CHECK_EQ(t.violations, 0);
    CHECK_GOLDEN("raster_shapes", t.image());
}

////////////////////////////////////////////////////////////////////////////////
// D8: non-finite and huge coordinates

TEST("render/raster: fillPath with a NaN y coordinate does not fill the bounding box") {
    // (20, NaN) is not the first or last point: it is left out (D8: it got into
    // the edges and a NaN cell filled the rows of the box)
    TestTarget t(64, 64);
    Path path;
    path.moveTo(10, 10);
    path.lineTo(20, NAN);
    path.lineTo(30, 10);
    path.lineTo(20, 30);
    path.close();
    raster::fillPath(t, path, WHITE);
    // corners of the bounding box, outside of the finite part of the shape
    CHECK_EQ(t.alpha(11, 28), 0);
    CHECK_EQ(t.alpha(28, 28), 0);
}

TEST("render/raster: strokePath from a NaN point does not fill a square") {
    TestTarget t(64, 64);
    Path path;
    path.moveTo(NAN, NAN);
    path.lineTo(30, 30);
    raster::strokePath(t, path, 2.0f, WHITE);
    // a NaN distance is coverage 1: the whole 4x4 box around (30, 30) is drawn,
    // its corner is more than the line width away from the point
    CHECK_EQ(t.alpha(28, 28), 0);
}

TEST("render/raster: strokePath to a huge x coordinate draws the visible part") {
    // the coordinates are clamped to +-1e6 (D8: (int)3e9 is undefined, INT_MIN
    // on x86, getBounds overflowed and the segment got no columns)
    TestTarget t(64, 40);
    Path path;
    path.moveTo(10, 20);
    path.lineTo(3e9f, 20);
    raster::strokePath(t, path, 2.0f, WHITE);
    CHECK_EQ(t.alpha(30, 20), 255);
    CHECK_EQ(t.alpha(60, 19), 255);
}

TEST("render/raster: arc with a huge radius is not a single chord") {
    // radius 1e7: the number of segments is computed with asin, which keeps the
    // precision, and is at most 1024 (D8: 1 - 0.05 / r rounded to 1, acos gave
    // 0 and the number was (int)inf). The quarter disc passes through (50, 50)
    // at 45 degrees, (40, 40) is 14 px inside of it.
    const float r = 1e7f;
    const float d = r * 0.70710678f;
    TestTarget t(100, 100);
    Path path;
    path.moveTo(50 - d, 50 - d);
    path.arc(50 - d, 50 - d, r, r, 0, PI / 2);
    path.close();
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.alpha(40, 40), 255);
    CHECK_EQ(t.alpha(60, 60), 0);
}

TEST("render/raster: strokePath to an infinite x coordinate draws the visible part") {
    // an infinite coordinate is clamped to 1e6 (D8: (int)ceilf(inf) is
    // undefined, getBounds overflowed and the segment got no columns)
    TestTarget t(64, 40);
    Path path;
    path.moveTo(10, 20);
    path.lineTo(INFINITY, 20);
    raster::strokePath(t, path, 2.0f, WHITE);
    CHECK_EQ(t.alpha(30, 20), 255);
    CHECK_EQ(t.alpha(5, 20), 0);
}
