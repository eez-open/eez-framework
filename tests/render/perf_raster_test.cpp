/*
 * eez-framework tests: gui/raster draws in strips of rows and only computes the
 * pixels near the edges of a shape (DP3).
 *
 * The target records the pixels like the target of raster_test.cpp and counts
 * the pixels given to blendSpan and to fillRect: the pixels between the edges
 * of a shape are drawn as runs of one color (fillRect), the pixels far from a
 * line are not given to the target at all.
 */

#include <math.h>
#include <stdlib.h>

#include <vector>

#include <eez/conf-internal.h>
#include <eez/gui/raster.h>

#include "test.h"

using namespace eez::gui;
using raster::Path;

namespace {

const float PI = 3.14159265358979f;
const uint32_t WHITE = 0xFFFFFFFF;

struct CountingTarget : raster::Target {
    int w;
    int h;
    std::vector<uint32_t> px;
    int violations = 0;
    int spans = 0;
    long spanPixels = 0;
    int fills = 0;
    long fillPixels = 0;

    CountingTarget(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_, 0) {
        clipX1 = 0;
        clipY1 = 0;
        clipX2 = w - 1;
        clipY2 = h - 1;
    }

    void blend(int x, int y, uint32_t c) {
        if (x < clipX1 || x > clipX2 || y < clipY1 || y > clipY2) {
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
        spanPixels += n;
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
        if (x1 > x2 || y1 > y2 || !(color >> 24)) {
            violations++;
        }
        fillPixels += (long)(x2 - x1 + 1) * (y2 - y1 + 1);
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                blend(x, y, color);
            }
        }
    }

    int alpha(int x, int y) const { return px[(size_t)y * w + x] >> 24; }

    double area() const {
        double sum = 0;
        for (auto p : px) {
            sum += (p >> 24) / 255.0;
        }
        return sum;
    }
};

void rectPath(Path &path, float x1, float y1, float x2, float y2) {
    path.moveTo(x1, y1);
    path.lineTo(x2, y1);
    path.lineTo(x2, y2);
    path.lineTo(x1, y2);
    path.close();
}

// a line across the chart (600 x 300 at (0, 0)), as a line chart draws it
std::vector<float> chartPoints() {
    std::vector<float> points;
    for (int i = 0; i < 600; i++) {
        points.push_back(i + 0.5f);
        points.push_back(150 + 120 * sinf(i * 0.02f) + 20 * sinf(i * 0.7f));
    }
    return points;
}

} // namespace

TEST("render/raster: strokePolyline is strokePath of a path with the same points") {
    std::vector<std::vector<float>> cases = {
        chartPoints(),
        { 5.5f, 5.5f, 40.2f, 30.7f, 12.0f, 50.0f },
        { 20, 20 },                                     // a dot
        { 10, 10, NAN, 30, 50, 40, 60, 10, 70, 50 },    // a gap at the NaN point
        { 10, 20, 3e9f, 20 },                           // a huge coordinate
        {},                                             // nothing
    };
    float widths[] = { 2.0f, 1.0f, 8.0f, 3.5f, 2.0f, 1.0f };
    for (size_t k = 0; k < cases.size(); k++) {
        auto &points = cases[k];
        size_t numPoints = points.size() / 2;
        CountingTarget a(640, 320), b(640, 320);
        Path path;
        for (size_t i = 0; i < numPoints; i++) {
            path.lineTo(points[2 * i], points[2 * i + 1]);
        }
        raster::strokePath(a, path, widths[k], 0xC0FFC000);
        raster::strokePolyline(b, points.data(), numPoints, widths[k], 0xC0FFC000);
        CHECK(a.px == b.px);
        CHECK_EQ(b.violations, 0);
    }
}

TEST("render/raster: a line across a chart gives the target only the pixels near the line") {
    // the box of the line is 600 x 300 pixels, the line is about 10 % of it
    CountingTarget t(640, 320);
    auto points = chartPoints();
    raster::strokePolyline(t, points.data(), points.size() / 2, 2.0f, WHITE);
    CHECK_EQ(t.violations, 0);
    CHECK(t.area() > 1500);
    CHECK(t.spanPixels < 600 * 300 / 4);
    CHECK_EQ(t.fills, 0);
}

TEST("render/raster: fillPath of a shape larger than the target fills the inside with fillRect") {
    CountingTarget t(200, 100);
    Path path;
    rectPath(path, -1000, -1000, 1000, 1000);
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.violations, 0);
    CHECK_NEAR(t.area(), 200.0 * 100, 0.01);
    // the pixels next to the left side of the target (where the left side of
    // the shape is moved to) and one rectangle for the rest
    CHECK(t.spanPixels <= 2 * 100);
    CHECK_EQ(t.fills, 1);
}

TEST("render/raster: fillPath of a large circle computes only the pixels near the circle") {
    CountingTarget t(500, 500);
    Path path;
    path.arc(250, 250, 200, 200, 0, 2 * PI);
    path.close();
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.violations, 0);
    CHECK_EQ(t.alpha(250, 250), 255);
    CHECK_EQ(t.alpha(250, 51), 255);
    CHECK_EQ(t.alpha(250, 49), 0);
    CHECK_EQ(t.alpha(51, 51), 0);
    CHECK(t.spanPixels < 2 * PI * 200 * 3);
    CHECK(t.fills >= 1);
    CHECK_NEAR(t.area(), PI * 200 * 200, 0.05 * 2 * PI * 200 + 1);
}

TEST("render/raster: fillPath of many squares in a row (more ranges than are kept) is exact") {
    // 40 x 30 squares of 6 px with a gap of 2 px, every other one: integer
    // coordinates, so every pixel is covered completely or not at all
    CountingTarget t(330, 250);
    Path path;
    std::vector<bool> set;
    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 40; x++) {
            bool on = ((x * 7 + y * 3) % 5) < 2;
            set.push_back(on);
            if (on) {
                rectPath(path, 5 + x * 8.0f, 5 + y * 8.0f, 11 + x * 8.0f, 11 + y * 8.0f);
            }
        }
    }
    raster::fillPath(t, path, WHITE);
    CHECK_EQ(t.violations, 0);
    int wrong = 0;
    for (int y = 0; y < 250; y++) {
        for (int x = 0; x < 330; x++) {
            int cx = (x - 5) / 8, cy = (y - 5) / 8;
            bool inside = x >= 5 && y >= 5 && cx < 40 && cy < 30 && (x - 5) % 8 < 6 && (y - 5) % 8 < 6 && set[cy * 40 + cx];
            if (t.alpha(x, y) != (inside ? 255 : 0)) {
                wrong++;
            }
        }
    }
    CHECK_EQ(wrong, 0);
}

TEST("render/raster: roundedRect draws the straight middle of the bands with fillRect") {
    // 600 x 200, radius 10, line 2: the top and the bottom band are 14 rows,
    // the side strips 4 columns; only the corners and the strips are pixels
    CountingTarget t(640, 240);
    float r[8];
    for (auto &v : r) {
        v = 10;
    }
    raster::roundedRect(t, 20, 20, 619, 219, 2, r, 0xFF0000FF, 0xFFFF0000);
    CHECK_EQ(t.violations, 0);
    CHECK(t.spanPixels < 4 * 20 * 20 + 2 * 4 * 200);
    CHECK_EQ(t.spanPixels + t.fillPixels <= 600L * 200, true);
    CHECK_EQ(t.px[(size_t)20 * 640 + 320], 0xFF0000FFu);  // the line in the middle of the top band
    CHECK_EQ(t.px[(size_t)25 * 640 + 320], 0xFFFF0000u);  // the fill in the middle of the top band
    CHECK_EQ(t.alpha(20, 20), 0);
}

TEST("render/raster: roundedRect with a translucent line and fill draws every pixel once") {
    CountingTarget t(320, 120);
    float r[8];
    for (auto &v : r) {
        v = 12;
    }
    raster::roundedRect(t, 0, 0, 319, 119, 3, r, 0x80FFFFFF, 0x80FFFFFF);
    // a pixel drawn twice with alpha 128 would be about 191
    CHECK_EQ(t.alpha(160, 0), 128);   // line, middle of the top band
    CHECK_EQ(t.alpha(160, 5), 128);   // fill, middle of the top band
    CHECK_EQ(t.alpha(160, 60), 128);  // fill, middle
    CHECK_EQ(t.alpha(1, 60), 128);    // line, left strip
    CHECK_EQ(t.alpha(160, 118), 128); // line, middle of the bottom band
    int worst = 0;
    for (auto p : t.px) {
        worst = (int)(p >> 24) > worst ? (int)(p >> 24) : worst;
    }
    CHECK(worst <= 129);
}
