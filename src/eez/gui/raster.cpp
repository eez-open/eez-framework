/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <math.h>

#include <eez/gui/raster.h>

namespace eez {
namespace gui {
namespace raster {

static const float PI = 3.14159265358979f;

// largest distance (pixels) of an arc from the lines that replace it
static const float ARC_TOLERANCE = 0.05f;

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }

static int arcSegments(float radius, float sweep) {
    // a line over the angle t is radius * (1 - cos(t / 2)) away from the arc
    float t = radius > ARC_TOLERANCE ? 2.0f * acosf(1.0f - ARC_TOLERANCE / radius) : PI;
    int n = (int)ceilf(fabsf(sweep) / t);
    return n < 1 ? 1 : n;
}

static inline uint32_t colorWithAlpha(uint32_t color, float alpha) {
    return (color & 0x00FFFFFF) | ((uint32_t)((color >> 24) * alpha + 0.5f) << 24);
}

////////////////////////////////////////////////////////////////////////////////
// Path

void Path::moveTo(float x, float y) {
    m_contours.push_back({ (uint32_t)(m_points.size() / 2), 1, false });
    m_points.push_back(x);
    m_points.push_back(y);
}

void Path::lineTo(float x, float y) {
    if (m_contours.empty() || m_contours.back().closed) {
        moveTo(x, y);
        return;
    }
    m_points.push_back(x);
    m_points.push_back(y);
    m_contours.back().count++;
}

void Path::arc(float cx, float cy, float rx, float ry, float a1, float a2) {
    int n = arcSegments(maxf(rx, ry), a2 - a1);
    for (int i = 0; i <= n; i++) {
        float a = a1 + (a2 - a1) * i / n;
        lineTo(cx + rx * cosf(a), cy + ry * sinf(a));
    }
}

void Path::close() {
    if (!m_contours.empty()) {
        m_contours.back().closed = true;
    }
}

void Path::clear() {
    m_points.clear();
    m_contours.clear();
}

////////////////////////////////////////////////////////////////////////////////
// Coverage of polygons
//
// Every edge adds to the cells of the rows it crosses its share of each pixel
// right of it, with the sign of its direction (down +, up -). The running sum
// along a row is then the winding number times the covered part of the pixel
// for every pixel of the row, its absolute value (up to 1) is the coverage.

namespace {

class PolygonCoverage {
public:
    // the rectangle of pixels w x h at (x, y)
    void reset(int x, int y, int w, int h) {
        m_x = x;
        m_y = y;
        m_w = w;
        m_h = h;
        m_stride = w + 2; // an edge at x == w also writes the cells w and w + 1
        m_cells.assign((size_t)m_stride * h, 0.0f);
    }

    void addLine(float x0, float y0, float x1, float y1);

    // converts the cells of each row to the coverage of the pixels (0 .. 1)
    void resolve() {
        for (int row = 0; row < m_h; row++) {
            float *cells = &m_cells[(size_t)row * m_stride];
            float sum = 0;
            for (int i = 0; i < m_w; i++) {
                sum += cells[i];
                float coverage = fabsf(sum);
                cells[i] = coverage < 1.0f ? coverage : 1.0f;
            }
        }
    }

    // coverage of the pixels of the row (after resolve)
    const float *row(int i) const { return &m_cells[(size_t)i * m_stride]; }

private:
    void addLineInside(float x0, float y0, float x1, float y1);

    int m_x, m_y, m_w, m_h;
    int m_stride;
    std::vector<float> m_cells;
};

void PolygonCoverage::addLine(float x0, float y0, float x1, float y1) {
    x0 -= m_x;
    y0 -= m_y;
    x1 -= m_x;
    y1 -= m_y;

    if (y0 == y1 || (y0 <= 0 && y1 <= 0) || (y0 >= m_h && y1 >= m_h)) {
        return;
    }

    // The part left of the rectangle is moved onto its left side: it covers all
    // the pixels right of it. The part right of the rectangle covers no pixel
    // of the rectangle.
    float splits[2];
    int numSplits = 0;
    if ((x0 < 0) != (x1 < 0)) {
        splits[numSplits++] = (0 - x0) / (x1 - x0);
    }
    if ((x0 > m_w) != (x1 > m_w)) {
        splits[numSplits++] = (m_w - x0) / (x1 - x0);
    }
    if (numSplits == 2 && splits[0] > splits[1]) {
        float t = splits[0];
        splits[0] = splits[1];
        splits[1] = t;
    }

    float px = x0;
    float py = y0;
    for (int i = 0; i <= numSplits; i++) {
        float qx = i < numSplits ? x0 + (x1 - x0) * splits[i] : x1;
        float qy = i < numSplits ? y0 + (y1 - y0) * splits[i] : y1;
        float mx = 0.5f * (px + qx);
        if (mx < 0) {
            addLineInside(0, py, 0, qy);
        } else if (mx <= m_w) {
            addLineInside(minf(maxf(px, 0), (float)m_w), py, minf(maxf(qx, 0), (float)m_w), qy);
        }
        px = qx;
        py = qy;
    }
}

// the line is inside the rectangle horizontally (0 <= x <= w)
void PolygonCoverage::addLineInside(float x0, float y0, float x1, float y1) {
    if (y0 == y1) {
        return;
    }

    float dir = 1.0f;
    if (y0 > y1) {
        float t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
        dir = -1.0f;
    }

    float dxdy = (x1 - x0) / (y1 - y0);

    int rowStart = y0 > 0 ? (int)y0 : 0;
    int rowEnd = y1 < m_h ? (int)ceilf(y1) : m_h;

    for (int row = rowStart; row < rowEnd; row++) {
        // the part of the line in this row
        float ya = maxf((float)row, y0);
        float yb = minf((float)(row + 1), y1);
        if (yb <= ya) {
            continue;
        }

        float xa = x0 + (ya - y0) * dxdy;
        float xb = x0 + (yb - y0) * dxdy;
        float xl = minf(maxf(minf(xa, xb), 0), (float)m_w);
        float xr = minf(maxf(maxf(xa, xb), 0), (float)m_w);
        float d = (yb - ya) * dir;

        float *cells = &m_cells[(size_t)row * m_stride];

        int il = (int)xl;
        int ir = (int)xr;

        if (il == ir) {
            // in one pixel, it covers the part right of its middle
            float xm = 0.5f * (xl + xr) - il;
            cells[il] += d * (1.0f - xm);
            cells[il + 1] += d * xm;
        } else {
            // coverage(j): the part of pixel j right of the line, cells[j] gets
            // coverage(j) - coverage(j - 1)
            float s = 1.0f / (xr - xl); // part of the height per pixel width
            float wl = il + 1 - xl; // width of the line in the first pixel
            float coverage = d * 0.5f * s * wl * wl;
            cells[il] += coverage;
            float previous = coverage;
            float heightLeft = wl * s; // part of the height left of the pixel j
            for (int j = il + 1; j < ir; j++) {
                coverage = d * (heightLeft + 0.5f * s);
                cells[j] += coverage - previous;
                previous = coverage;
                heightLeft += s;
            }
            float wr = xr - ir; // width of the line in the last pixel
            coverage = d * (heightLeft + wr * s * (1.0f - 0.5f * wr));
            cells[ir] += coverage - previous;
            cells[ir + 1] += d - coverage;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Coverage of lines
//
// Each segment is a capsule (round ends): a pixel gets the part of its width
// within half the line width from the segment, measured from its center. The
// coverage of all segments is combined with max, so the joins are round and
// the overlaps are not counted twice.

class LineCoverage {
public:
    void reset(int x, int y, int w, int h) {
        m_x = x;
        m_y = y;
        m_w = w;
        m_h = h;
        m_cells.assign((size_t)w * h, 0.0f);
    }

    void addSegment(float ax, float ay, float bx, float by, float halfWidth);

    const float *row(int i) const { return &m_cells[(size_t)i * m_w]; }

private:
    int m_x, m_y, m_w, m_h;
    std::vector<float> m_cells;
};

void LineCoverage::addSegment(float ax, float ay, float bx, float by, float halfWidth) {
    ax -= m_x;
    ay -= m_y;
    bx -= m_x;
    by -= m_y;

    // the coverage is 0 at the distance halfWidth + 0.5 and further
    float reach = halfWidth + 0.5f;

    int rowStart = maxi(0, (int)floorf(minf(ay, by) - reach));
    int rowEnd = mini(m_h - 1, (int)ceilf(maxf(ay, by) + reach));
    int colStart = maxi(0, (int)floorf(minf(ax, bx) - reach));
    int colEnd = mini(m_w - 1, (int)ceilf(maxf(ax, bx) + reach));

    float dx = bx - ax;
    float dy = by - ay;
    float length2 = dx * dx + dy * dy;
    float length = sqrtf(length2);

    for (int row = rowStart; row <= rowEnd; row++) {
        float py = row + 0.5f;

        int col1 = colStart;
        int col2 = colEnd;
        if (fabsf(dy) > 1E-6f * length) {
            // only the pixels within the reach from the line through a and b
            float xLine = ax + dx * (py - ay) / dy;
            float halfSpan = reach * length / fabsf(dy);
            col1 = maxi(col1, (int)floorf(xLine - halfSpan - 0.5f));
            col2 = mini(col2, (int)ceilf(xLine + halfSpan - 0.5f));
        }

        float *cells = &m_cells[(size_t)row * m_w];

        for (int col = col1; col <= col2; col++) {
            float px = col + 0.5f;

            // distance from the center of the pixel to the segment
            float t = length2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / length2 : 0;
            t = minf(maxf(t, 0), 1.0f);
            float ex = px - (ax + t * dx);
            float ey = py - (ay + t * dy);
            float distance = sqrtf(ex * ex + ey * ey);

            // the line is from distance - halfWidth to distance + halfWidth,
            // the pixel from -0.5 to 0.5
            float coverage = minf(distance + halfWidth, 0.5f) - maxf(distance - halfWidth, -0.5f);
            if (coverage > cells[col]) {
                cells[col] = coverage < 1.0f ? coverage : 1.0f;
            }
        }
    }
}

std::vector<uint32_t> g_span;

// Draws the row of the pixels (n from x) with the alpha of each pixel, without
// the pixels with alpha 0 at both ends.
void blendRow(Target &target, int x, int y, int n, uint32_t *pixels) {
    int first = 0;
    while (first < n && !(pixels[first] >> 24)) {
        first++;
    }
    int last = n - 1;
    while (last >= first && !(pixels[last] >> 24)) {
        last--;
    }
    if (first <= last) {
        target.blendSpan(x + first, y, last - first + 1, pixels + first);
    }
}

// Draws the color with the coverage of the pixels of the rectangle.
template <typename Coverage>
void blendCoverage(Target &target, const Coverage &coverage, int x, int y, int w, int h, uint32_t color) {
    g_span.resize(w);
    uint32_t *pixels = g_span.data();
    for (int row = 0; row < h; row++) {
        const float *rowCoverage = coverage.row(row);
        for (int i = 0; i < w; i++) {
            pixels[i] = colorWithAlpha(color, rowCoverage[i]);
        }
        blendRow(target, x, y + row, w, pixels);
    }
}

// the rectangle that contains the points, expanded by the margin and clipped
// to the target, false if empty
bool getBounds(Target &target, const float *points, size_t numPoints, float margin, int &x1, int &y1, int &x2, int &y2) {
    if (numPoints == 0) {
        return false;
    }

    float minX = points[0];
    float minY = points[1];
    float maxX = minX;
    float maxY = minY;
    for (size_t i = 1; i < numPoints; i++) {
        minX = minf(minX, points[2 * i]);
        minY = minf(minY, points[2 * i + 1]);
        maxX = maxf(maxX, points[2 * i]);
        maxY = maxf(maxY, points[2 * i + 1]);
    }

    // a NaN fails these, the points are then not drawn
    if (!(minX - margin < target.clipX2 + 1 && maxX + margin > target.clipX1 &&
        minY - margin < target.clipY2 + 1 && maxY + margin > target.clipY1)) {
        return false;
    }

    x1 = maxi(target.clipX1, (int)floorf(minX - margin));
    y1 = maxi(target.clipY1, (int)floorf(minY - margin));
    x2 = mini(target.clipX2, (int)ceilf(maxX + margin) - 1);
    y2 = mini(target.clipY2, (int)ceilf(maxY + margin) - 1);

    return x1 <= x2 && y1 <= y2;
}

PolygonCoverage g_polygonCoverage1;
PolygonCoverage g_polygonCoverage2;
LineCoverage g_lineCoverage;

} // namespace

////////////////////////////////////////////////////////////////////////////////

void fillPath(Target &target, const Path &path, uint32_t color) {
    if (!(color >> 24)) {
        return;
    }

    int x1, y1, x2, y2;
    if (!getBounds(target, path.m_points.data(), path.m_points.size() / 2, 0, x1, y1, x2, y2)) {
        return;
    }

    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;
    auto &coverage = g_polygonCoverage1;
    coverage.reset(x1, y1, w, h);

    for (auto &contour : path.m_contours) {
        const float *p = &path.m_points[2 * contour.start];
        for (uint32_t i = 0; i < contour.count; i++) {
            uint32_t j = i + 1 < contour.count ? i + 1 : 0;
            coverage.addLine(p[2 * i], p[2 * i + 1], p[2 * j], p[2 * j + 1]);
        }
    }

    coverage.resolve();
    blendCoverage(target, coverage, x1, y1, w, h, color);
}

void strokePath(Target &target, const Path &path, float lineWidth, uint32_t color) {
    if (!(color >> 24) || !(lineWidth > 0)) {
        return;
    }

    float halfWidth = lineWidth / 2;

    int x1, y1, x2, y2;
    if (!getBounds(target, path.m_points.data(), path.m_points.size() / 2, halfWidth + 1.0f, x1, y1, x2, y2)) {
        return;
    }

    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;
    auto &coverage = g_lineCoverage;
    coverage.reset(x1, y1, w, h);

    for (auto &contour : path.m_contours) {
        const float *p = &path.m_points[2 * contour.start];
        if (contour.count == 1) {
            // a dot
            coverage.addSegment(p[0], p[1], p[0], p[1], halfWidth);
            continue;
        }
        uint32_t numSegments = contour.closed ? contour.count : contour.count - 1;
        for (uint32_t i = 0; i < numSegments; i++) {
            uint32_t j = i + 1 < contour.count ? i + 1 : 0;
            coverage.addSegment(p[2 * i], p[2 * i + 1], p[2 * j], p[2 * j + 1], halfWidth);
        }
    }

    blendCoverage(target, coverage, x1, y1, w, h, color);
}

////////////////////////////////////////////////////////////////////////////////
// Rounded rectangle

namespace {

enum { TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT };

struct RoundedBox {
    float left, top, right, bottom;
    float rx[4];
    float ry[4];

    bool isEmpty() const { return !(right > left && bottom > top); }

    // radii too large for the box are reduced in proportion
    void limitRadii() {
        float width = right - left;
        float height = bottom - top;
        float k = 1.0f;
        auto limit = [&](float size, float r1, float r2) {
            if (r1 + r2 > size) {
                k = minf(k, size > 0 ? size / (r1 + r2) : 0);
            }
        };
        limit(width, rx[TOP_LEFT], rx[TOP_RIGHT]);
        limit(width, rx[BOTTOM_LEFT], rx[BOTTOM_RIGHT]);
        limit(height, ry[TOP_LEFT], ry[BOTTOM_LEFT]);
        limit(height, ry[TOP_RIGHT], ry[BOTTOM_RIGHT]);
        if (k < 1.0f) {
            for (int i = 0; i < 4; i++) {
                rx[i] *= k;
                ry[i] *= k;
            }
        }
    }

    // the box d larger on every side (smaller if d < 0), the rounded corners
    // with the radii d larger
    RoundedBox offset(float d) const {
        RoundedBox box;
        box.left = left - d;
        box.top = top - d;
        box.right = right + d;
        box.bottom = bottom + d;
        for (int i = 0; i < 4; i++) {
            bool rounded = rx[i] > 0 && ry[i] > 0;
            box.rx[i] = rounded ? maxf(rx[i] + d, 0) : 0;
            box.ry[i] = rounded ? maxf(ry[i] + d, 0) : 0;
        }
        box.limitRadii();
        return box;
    }

    // adds the outline (clockwise) to the coverage
    void addTo(PolygonCoverage &coverage) const {
        float firstX = 0, firstY = 0, lastX = 0, lastY = 0;
        bool first = true;

        auto point = [&](float x, float y) {
            if (first) {
                firstX = x;
                firstY = y;
                first = false;
            } else {
                coverage.addLine(lastX, lastY, x, y);
            }
            lastX = x;
            lastY = y;
        };

        auto corner = [&](int i, float cx, float cy, float a1) {
            if (rx[i] > 0 && ry[i] > 0) {
                int n = arcSegments(maxf(rx[i], ry[i]), PI / 2);
                for (int k = 0; k <= n; k++) {
                    float a = a1 + (PI / 2) * k / n;
                    point(cx + rx[i] * cosf(a), cy + ry[i] * sinf(a));
                }
            } else {
                point(cx, cy);
            }
        };

        corner(TOP_LEFT, left + rx[TOP_LEFT], top + ry[TOP_LEFT], PI);
        corner(TOP_RIGHT, right - rx[TOP_RIGHT], top + ry[TOP_RIGHT], 1.5f * PI);
        corner(BOTTOM_RIGHT, right - rx[BOTTOM_RIGHT], bottom - ry[BOTTOM_RIGHT], 0);
        corner(BOTTOM_LEFT, left + rx[BOTTOM_LEFT], bottom - ry[BOTTOM_LEFT], 0.5f * PI);

        coverage.addLine(lastX, lastY, firstX, firstY);
    }
};

} // namespace

void roundedRect(
    Target &target,
    int x1, int y1, int x2, int y2,
    float lineWidth,
    const float radii[8],
    uint32_t lineColor, uint32_t fillColor
) {
    float lw = lineWidth > 0 ? lineWidth : 0;
    bool drawLine = lw > 0 && (lineColor >> 24);
    bool fill = (fillColor >> 24) != 0;
    if (!drawLine && !fill) {
        return;
    }

    // the middle of the line
    RoundedBox middle;
    middle.left = x1 + lw / 2;
    middle.top = y1 + lw / 2;
    middle.right = x2 + 1 - lw / 2;
    middle.bottom = y2 + 1 - lw / 2;
    if (middle.right < middle.left) {
        middle.left = middle.right = (x1 + x2 + 1) / 2.0f;
    }
    if (middle.bottom < middle.top) {
        middle.top = middle.bottom = (y1 + y2 + 1) / 2.0f;
    }
    middle.rx[TOP_LEFT] = radii[0];
    middle.ry[TOP_LEFT] = radii[1];
    middle.rx[TOP_RIGHT] = radii[2];
    middle.ry[TOP_RIGHT] = radii[3];
    middle.rx[BOTTOM_RIGHT] = radii[4];
    middle.ry[BOTTOM_RIGHT] = radii[5];
    middle.rx[BOTTOM_LEFT] = radii[6];
    middle.ry[BOTTOM_LEFT] = radii[7];
    for (int i = 0; i < 4; i++) {
        middle.rx[i] = maxf(middle.rx[i], 0);
        middle.ry[i] = maxf(middle.ry[i], 0);
    }
    middle.limitRadii();

    // with the line: the line is between outer and inner, the fill is inside
    // inner; without the line: the fill is the middle box
    RoundedBox outer = middle.offset(lw / 2);
    RoundedBox inner = middle.offset(-lw / 2);
    bool hasInner = !inner.isEmpty();

    float lineAlpha = (lineColor >> 24) / 255.0f;
    float fillAlpha = (fillColor >> 24) / 255.0f;

    // draws the part of the rectangle x1, y1, x2, y2
    auto drawPart = [&](int px1, int py1, int px2, int py2) {
        px1 = maxi(px1, target.clipX1);
        py1 = maxi(py1, target.clipY1);
        px2 = mini(px2, target.clipX2);
        py2 = mini(py2, target.clipY2);
        if (px1 > px2 || py1 > py2) {
            return;
        }

        int w = px2 - px1 + 1;
        int h = py2 - py1 + 1;

        auto &outerCoverage = g_polygonCoverage1;
        auto &innerCoverage = g_polygonCoverage2;

        outerCoverage.reset(px1, py1, w, h);
        (drawLine ? outer : middle).addTo(outerCoverage);
        outerCoverage.resolve();

        bool useInner = drawLine && hasInner;
        if (useInner) {
            innerCoverage.reset(px1, py1, w, h);
            inner.addTo(innerCoverage);
            innerCoverage.resolve();
        }

        g_span.resize(w);
        uint32_t *pixels = g_span.data();

        for (int row = 0; row < h; row++) {
            const float *o = outerCoverage.row(row);
            const float *in = useInner ? innerCoverage.row(row) : nullptr;

            for (int i = 0; i < w; i++) {
                if (!drawLine) {
                    pixels[i] = colorWithAlpha(fillColor, o[i]);
                    continue;
                }

                float insideInner = in ? minf(in[i], o[i]) : 0;
                float lineCoverage = o[i] - insideInner;

                if (!fill || insideInner <= 0) {
                    pixels[i] = colorWithAlpha(lineColor, lineCoverage);
                    continue;
                }

                if (lineCoverage <= 0) {
                    pixels[i] = colorWithAlpha(fillColor, insideInner);
                    continue;
                }

                // the line and the fill cover different parts of the pixel
                float wl = lineCoverage * lineAlpha;
                float wf = insideInner * fillAlpha;
                float alpha = wl + wf;
                if (alpha <= 0) {
                    pixels[i] = 0;
                    continue;
                }
                float kl = wl / alpha;
                float kf = wf / alpha;
                uint32_t r = (uint32_t)((lineColor & 0xFF) * kl + (fillColor & 0xFF) * kf + 0.5f);
                uint32_t g = (uint32_t)(((lineColor >> 8) & 0xFF) * kl + ((fillColor >> 8) & 0xFF) * kf + 0.5f);
                uint32_t b = (uint32_t)(((lineColor >> 16) & 0xFF) * kl + ((fillColor >> 16) & 0xFF) * kf + 0.5f);
                uint32_t a = (uint32_t)(alpha * 255.0f + 0.5f);
                pixels[i] = r | (g << 8) | (b << 16) | (a << 24);
            }

            blendRow(target, px1, py1 + row, w, pixels);
        }
    };

    // Between the top and the bottom corners the sides are straight, there
    // only the strips along the sides are drawn with the coverage and the
    // rest is a plain fill (the bands and strips include the line and the
    // antialiased pixels).
    int lwPixels = (int)ceilf(lw);
    int topBand = (int)ceilf(maxf(middle.ry[TOP_LEFT], middle.ry[TOP_RIGHT])) + lwPixels + 2;
    int bottomBand = (int)ceilf(maxf(middle.ry[BOTTOM_LEFT], middle.ry[BOTTOM_RIGHT])) + lwPixels + 2;
    int strip = lwPixels + 2;
    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;

    if (topBand + bottomBand < h && 2 * strip < w) {
        int my1 = y1 + topBand;
        int my2 = y2 - bottomBand;

        drawPart(x1, y1, x2, my1 - 1);
        drawPart(x1, my2 + 1, x2, y2);
        drawPart(x1, my1, x1 + strip - 1, my2);
        drawPart(x2 - strip + 1, my1, x2, my2);

        if (fill) {
            int fx1 = maxi(x1 + strip, target.clipX1);
            int fy1 = maxi(my1, target.clipY1);
            int fx2 = mini(x2 - strip, target.clipX2);
            int fy2 = mini(my2, target.clipY2);
            if (fx1 <= fx2 && fy1 <= fy2) {
                target.fillRect(fx1, fy1, fx2, fy2, fillColor);
            }
        }
    } else {
        drawPart(x1, y1, x2, y2);
    }
}

} // namespace raster
} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
