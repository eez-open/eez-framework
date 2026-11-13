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
#include <cmath>

#include <eez/gui/raster.h>

namespace eez {
namespace gui {
namespace raster {

static const float PI = 3.14159265358979f;

// largest distance (pixels) of an arc from the lines that replace it
static const float ARC_TOLERANCE = 0.05f;

// most lines that replace an arc
static const int MAX_ARC_SEGMENTS = 1024;

// The coordinates of the points (and half the line width) are limited to this
// many pixels, an infinite coordinate is this far away. So the arithmetic and
// the conversions to int stay in range.
static const float COORD_LIMIT = 1e6f;

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }

static inline float limitCoord(float v) {
    return v < -COORD_LIMIT ? -COORD_LIMIT : v > COORD_LIMIT ? COORD_LIMIT : v;
}

// The point i of the points (x, y), with the coordinates limited to COORD_LIMIT.
// A point with a NaN coordinate is not drawn: returns false.
static inline bool getPoint(const float *points, size_t i, float &x, float &y) {
    x = points[2 * i];
    y = points[2 * i + 1];
    if (std::isnan(x) || std::isnan(y)) {
        return false;
    }
    x = limitCoord(x);
    y = limitCoord(y);
    return true;
}

static int arcSegments(float radius, float sweep) {
    // a line over the angle t is radius * (1 - cos(t / 2)) = 2 * radius * sin(t / 4)^2
    // away from the arc (the second form keeps the precision for large radii)
    float t = radius > ARC_TOLERANCE ? 4.0f * asinf(sqrtf(ARC_TOLERANCE / (2.0f * radius))) : PI;
    float n = ceilf(fabsf(sweep) / t);
    if (!(n >= 1.0f)) {
        // also a NaN radius or sweep
        return 1;
    }
    return n < MAX_ARC_SEGMENTS ? (int)n : MAX_ARC_SEGMENTS;
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
// Strips
//
// A shape is drawn in strips of rows: the coverage of the rows of a strip is
// computed in cells, the rows are drawn and their cells are set back to 0 for
// the next strip. So the memory is the cells of a few rows (MAX_STRIP_CELLS),
// not of the rectangle of the shape. Every coverage is computed in the
// coordinates of the whole rectangle, so the pixels don't depend on the strips.
//
// Each row keeps the ranges of the columns whose cells got a value. Only these
// cells are converted to pixels and set back to 0: between the ranges the cells
// are 0, the coverage is the same as at the end of the previous range, and the
// pixels there are drawn as a run of one color (a long run with fillRect).

namespace {

// cells of the strips of a shape (all its coverages together): as many rows as
// fit (at least one, at most MAX_STRIP_ROWS)
static const int MAX_STRIP_CELLS = 8192;
static const int MAX_STRIP_ROWS = 32;

// a run of pixels of one color at least this long is drawn with fillRect, the
// ranges of a row with a shorter gap between them are joined
static const int MIN_RUN = 16;

// most ranges of a row, the two with the smallest gap are joined when there
// are more
static const int MAX_RANGES = 4;

// The ranges of columns (inclusive) of a row, ordered, with gaps of at least
// MIN_RUN columns between them.
struct Ranges {
    int n;
    int start[MAX_RANGES + 1];
    int end[MAX_RANGES + 1];

    // adds the columns a .. b (a <= b)
    void add(int a, int b) {
        // the first range that doesn't end MIN_RUN or more columns before a
        int i = 0;
        while (i < n && end[i] + MIN_RUN < a) {
            i++;
        }

        if (i < n && start[i] <= b + MIN_RUN) {
            // joined with the range i and the next ones it reaches
            if (a < start[i]) {
                start[i] = a;
            }
            if (b > end[i]) {
                end[i] = b;
            }
            int j = i + 1;
            while (j < n && start[j] <= end[i] + MIN_RUN) {
                if (end[j] > end[i]) {
                    end[i] = end[j];
                }
                j++;
            }
            int removed = j - (i + 1);
            if (removed > 0) {
                for (int k = j; k < n; k++) {
                    start[k - removed] = start[k];
                    end[k - removed] = end[k];
                }
                n -= removed;
            }
            return;
        }

        // a new range before the range i
        for (int k = n; k > i; k--) {
            start[k] = start[k - 1];
            end[k] = end[k - 1];
        }
        start[i] = a;
        end[i] = b;

        if (++n > MAX_RANGES) {
            int best = 0;
            for (int k = 1; k + 1 < n; k++) {
                if (start[k + 1] - end[k] < start[best + 1] - end[best]) {
                    best = k;
                }
            }
            end[best] = end[best + 1];
            for (int k = best + 1; k + 1 < n; k++) {
                start[k] = start[k + 1];
                end[k] = end[k + 1];
            }
            n--;
        }
    }
};

// The rows of the shape's rectangle that are computed now: rows from first.
struct Strip {
    int first;
    int rows;
    Ranges ranges[MAX_STRIP_ROWS];

    void set(int first_, int rows_) {
        first = first_;
        rows = rows_;
        for (int i = 0; i < rows; i++) {
            ranges[i].n = 0;
        }
    }
};

Strip g_strip;

// the cells of the coverages, all 0 when no shape is drawn
std::vector<float> g_cells;

// n cells, all 0
float *getCells(size_t n) {
    if (g_cells.size() < n) {
        g_cells.resize(n, 0.0f);
    }
    return g_cells.data();
}

// sets the cells of the ranges back to 0
void clearCells(float *cells, const Ranges &ranges) {
    for (int k = 0; k < ranges.n; k++) {
        for (int i = ranges.start[k]; i <= ranges.end[k]; i++) {
            cells[i] = 0;
        }
    }
}

// rows of a strip with cellsPerRow cells in each row, for a shape of h rows
int getStripRows(int cellsPerRow, int h) {
    int rows = MAX_STRIP_CELLS / cellsPerRow;
    rows = rows < 1 ? 1 : rows > MAX_STRIP_ROWS ? MAX_STRIP_ROWS : rows;
    return rows < h ? rows : h;
}

} // namespace

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
    // an edge at x == w also writes the cells w and w + 1
    static int getStride(int w) { return w + 2; }

    // the rectangle of pixels w x h at (x, y), the cells for the rows of the
    // strip (all 0)
    void reset(int x, int y, int w, int h, float *cells, Strip *strip) {
        m_x = x;
        m_y = y;
        m_w = w;
        m_h = h;
        m_stride = getStride(w);
        m_cells = cells;
        m_strip = strip;
    }

    // adds the line to the rows of the strip
    void addLine(float x0, float y0, float x1, float y1);

    // the cells of the row i of the strip
    float *row(int i) const { return m_cells + (size_t)i * m_stride; }

private:
    void addLineInside(float x0, float y0, float x1, float y1);

    int m_x, m_y, m_w, m_h;
    int m_stride;
    float *m_cells;
    Strip *m_strip;
};

void PolygonCoverage::addLine(float x0, float y0, float x1, float y1) {
    x0 -= m_x;
    y0 -= m_y;
    x1 -= m_x;
    y1 -= m_y;

    // the strip is inside the rows 0 .. h
    float stripTop = (float)m_strip->first;
    float stripBottom = (float)(m_strip->first + m_strip->rows);
    if (y0 == y1 || (y0 <= stripTop && y1 <= stripTop) || (y0 >= stripBottom && y1 >= stripBottom)) {
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

    // only the rows of the strip
    int first = m_strip->first;
    rowStart = maxi(rowStart, first);
    rowEnd = mini(rowEnd, first + m_strip->rows);

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

        float *cells = this->row(row - first);

        int il = (int)xl;
        int ir = (int)xr;

        // the cells il .. ir + 1 are written
        m_strip->ranges[row - first].add(il, ir + 1);

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
    // the rectangle of pixels w wide at (x, y) (the strip is inside its rows),
    // the cells for the rows of the strip (all 0, w in each row)
    void reset(int x, int y, int w, float *cells, Strip *strip) {
        m_x = x;
        m_y = y;
        m_w = w;
        m_cells = cells;
        m_strip = strip;
    }

    // adds the segment to the rows of the strip
    void addSegment(float ax, float ay, float bx, float by, float halfWidth);

    // the cells of the row i of the strip
    float *row(int i) const { return m_cells + (size_t)i * m_w; }

private:
    int m_x, m_y, m_w;
    float *m_cells;
    Strip *m_strip;
};

void LineCoverage::addSegment(float ax, float ay, float bx, float by, float halfWidth) {
    ax -= m_x;
    ay -= m_y;
    bx -= m_x;
    by -= m_y;

    // the coverage is 0 at the distance halfWidth + 0.5 and further
    float reach = halfWidth + 0.5f;

    // only the rows of the strip (a segment outside of them is left out before
    // the conversions to int)
    int first = m_strip->first;
    int last = first + m_strip->rows - 1;
    float top = minf(ay, by) - reach;
    float bottom = maxf(ay, by) + reach;
    if (!(bottom > (float)(first - 1) && top < (float)(last + 1))) {
        return;
    }
    int rowStart = maxi(first, (int)floorf(top));
    int rowEnd = mini(last, (int)ceilf(bottom));
    if (rowStart > rowEnd) {
        return;
    }

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
            // (limited to the columns before the conversion to int)
            float xLine = ax + dx * (py - ay) / dy;
            float halfSpan = reach * length / fabsf(dy);
            float columnMin = (float)(colStart - 1);
            float columnMax = (float)(colEnd + 1);
            col1 = maxi(col1, (int)floorf(minf(maxf(xLine - halfSpan - 0.5f, columnMin), columnMax)));
            col2 = mini(col2, (int)ceilf(minf(maxf(xLine + halfSpan - 0.5f, columnMin), columnMax)));
        }

        if (col1 > col2) {
            continue;
        }

        float *cells = this->row(row - first);
        m_strip->ranges[row - first].add(col1, col2);

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

// Draws the rows of a shape: the pixels with blendSpan, the runs of one color
// with fillRect. A run in the same columns and color as the run of the row
// above makes that rectangle taller (a shape draws every pixel once, so the
// order of the drawing doesn't matter).
class RowDrawer {
public:
    explicit RowDrawer(Target &target) : m_target(target) {}
    ~RowDrawer() { flush(); }

    // Draws the row y, the pixels x .. x + w - 1 (g_span has w pixels), from
    // the ranges of its cells: row.pixel(i) is the pixel of the column i of a
    // range (called for the columns in order), row.constant() the pixel of the
    // columns after the last column given to pixel (before the first one: the
    // pixel of no coverage).
    template <typename Row>
    void drawRow(int x, int y, int w, const Ranges &ranges, Row &row);

    // draws the rectangle that is not drawn yet
    void flush() {
        if (m_hasRect) {
            m_target.fillRect(m_rectX1, m_rectY1, m_rectX2, m_rectY2, m_rectColor);
            m_hasRect = false;
        }
    }

private:
    void run(int x1, int x2, int y, uint32_t color);

    Target &m_target;
    bool m_hasRect = false;
    int m_rectX1 = 0;
    int m_rectY1 = 0;
    int m_rectX2 = 0;
    int m_rectY2 = 0;
    uint32_t m_rectColor = 0;
};

template <typename Row>
void RowDrawer::drawRow(int x, int y, int w, const Ranges &ranges, Row &row) {
    uint32_t *pixels = g_span.data();

    // the pixels from the column spanStart are not drawn yet (-1: none)
    int spanStart = -1;

    auto endSpan = [&](int end) {
        if (spanStart >= 0) {
            blendRow(m_target, x + spanStart, y, end - spanStart, pixels + spanStart);
            spanStart = -1;
        }
    };

    // the columns from .. to between the ranges
    auto constant = [&](int from, int to) {
        if (from > to) {
            return;
        }
        uint32_t pixel = row.constant();
        if (to - from + 1 >= MIN_RUN) {
            endSpan(from);
            run(x + from, x + to, y, pixel);
            return;
        }
        if (spanStart < 0) {
            if (!(pixel >> 24)) {
                return;
            }
            spanStart = from;
        }
        for (int i = from; i <= to; i++) {
            pixels[i] = pixel;
        }
    };

    int next = 0; // the first column not drawn
    for (int k = 0; k < ranges.n && ranges.start[k] < w; k++) {
        int a = ranges.start[k];
        int b = mini(ranges.end[k], w - 1);
        constant(next, a - 1);
        if (spanStart < 0) {
            spanStart = a;
        }
        for (int i = a; i <= b; i++) {
            pixels[i] = row.pixel(i);
        }
        next = b + 1;
    }
    constant(next, w - 1);
    endSpan(w);
}

void RowDrawer::run(int x1, int x2, int y, uint32_t color) {
    if (!(color >> 24)) {
        return;
    }
    if (m_hasRect && x1 == m_rectX1 && x2 == m_rectX2 && y == m_rectY2 + 1 && color == m_rectColor) {
        m_rectY2 = y;
        return;
    }
    flush();
    m_hasRect = true;
    m_rectX1 = x1;
    m_rectY1 = y;
    m_rectX2 = x2;
    m_rectY2 = y;
    m_rectColor = color;
}

// A row of a polygon: the running sum of the cells is the coverage.
struct PolygonRow {
    PolygonRow(const float *cells_, uint32_t color_) : cells(cells_), color(color_) {}

    uint32_t pixel(int i) {
        sum += cells[i];
        float c = fabsf(sum);
        coverage = c < 1.0f ? c : 1.0f;
        return colorWithAlpha(color, coverage);
    }

    uint32_t constant() const { return colorWithAlpha(color, coverage); }

    const float *cells;
    uint32_t color;
    float sum = 0;
    float coverage = 0;
};

// A row of a line: the cells are the coverage, 0 between the ranges.
struct LineRow {
    LineRow(const float *cells_, uint32_t color_) : cells(cells_), color(color_) {}

    uint32_t pixel(int i) const { return colorWithAlpha(color, cells[i]); }

    uint32_t constant() const { return 0; }

    const float *cells;
    uint32_t color;
};

// the rectangle that contains the points (see getPoint), expanded by the margin
// and clipped to the target, false if empty
bool getBounds(Target &target, const float *points, size_t numPoints, float margin, int &x1, int &y1, int &x2, int &y2) {
    bool hasPoint = false;
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    for (size_t i = 0; i < numPoints; i++) {
        float x, y;
        if (!getPoint(points, i, x, y)) {
            continue;
        }
        if (!hasPoint) {
            minX = maxX = x;
            minY = maxY = y;
            hasPoint = true;
        } else {
            minX = minf(minX, x);
            minY = minf(minY, y);
            maxX = maxf(maxX, x);
            maxY = maxf(maxY, y);
        }
    }

    if (!hasPoint) {
        return false;
    }

    if (!(minX - margin < target.clipX2 + 1 && maxX + margin > target.clipX1 &&
        minY - margin < target.clipY2 + 1 && maxY + margin > target.clipY1)) {
        return false;
    }

    // the coordinates and the margin are limited to COORD_LIMIT, so they fit into int
    x1 = maxi(target.clipX1, (int)floorf(minX - margin));
    y1 = maxi(target.clipY1, (int)floorf(minY - margin));
    x2 = mini(target.clipX2, (int)ceilf(maxX + margin) - 1);
    y2 = mini(target.clipY2, (int)ceilf(maxY + margin) - 1);

    return x1 <= x2 && y1 <= y2;
}

// Adds the edges of the contour (count points from p, closed) to the coverage.
// The points that are not drawn (see getPoint) are left out of the contour, it
// is closed with the others.
void addContour(PolygonCoverage &coverage, const float *p, size_t count) {
    bool hasPoint = false;
    float firstX = 0, firstY = 0, lastX = 0, lastY = 0;
    for (size_t i = 0; i < count; i++) {
        float x, y;
        if (!getPoint(p, i, x, y)) {
            continue;
        }
        if (!hasPoint) {
            firstX = x;
            firstY = y;
            hasPoint = true;
        } else {
            coverage.addLine(lastX, lastY, x, y);
        }
        lastX = x;
        lastY = y;
    }
    if (hasPoint) {
        coverage.addLine(lastX, lastY, firstX, firstY);
    }
}

// Adds the segments of the contour (count points from p, closed: with the
// segment from the last point to the first one) to the coverage, one point is
// a dot. A segment to or from a point that is not drawn (see getPoint) is left
// out.
void addContour(LineCoverage &coverage, const float *p, size_t count, bool closed, float halfWidth) {
    if (count == 0) {
        return;
    }
    if (count == 1) {
        float x, y;
        if (getPoint(p, 0, x, y)) {
            coverage.addSegment(x, y, x, y, halfWidth);
        }
        return;
    }
    size_t numSegments = closed ? count : count - 1;
    float ax, ay;
    bool hasA = getPoint(p, 0, ax, ay);
    for (size_t i = 0; i < numSegments; i++) {
        size_t j = i + 1 < count ? i + 1 : 0;
        float bx, by;
        bool hasB = getPoint(p, j, bx, by);
        if (hasA && hasB) {
            coverage.addSegment(ax, ay, bx, by, halfWidth);
        }
        ax = bx;
        ay = by;
        hasA = hasB;
    }
}

// Draws the line through the points: addSegments(coverage, halfWidth) adds the
// segments (for every strip).
template <typename AddSegments>
void stroke(Target &target, const float *points, size_t numPoints, float lineWidth, uint32_t color, AddSegments addSegments) {
    if (!(color >> 24) || !(lineWidth > 0)) {
        return;
    }

    float halfWidth = minf(lineWidth / 2, COORD_LIMIT);

    int x1, y1, x2, y2;
    if (!getBounds(target, points, numPoints, halfWidth + 1.0f, x1, y1, x2, y2)) {
        return;
    }

    int w = x2 - x1 + 1;
    int h = y2 - y1 + 1;
    int rows = getStripRows(w, h);
    LineCoverage coverage;
    coverage.reset(x1, y1, w, getCells((size_t)w * rows), &g_strip);
    g_span.resize(w);

    RowDrawer drawer(target);
    for (int first = 0; first < h; first += rows) {
        g_strip.set(first, mini(rows, h - first));
        addSegments(coverage, halfWidth);
        for (int i = 0; i < g_strip.rows; i++) {
            LineRow row(coverage.row(i), color);
            drawer.drawRow(x1, y1 + first + i, w, g_strip.ranges[i], row);
            clearCells(coverage.row(i), g_strip.ranges[i]);
        }
    }
}

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
    int stride = PolygonCoverage::getStride(w);
    int rows = getStripRows(stride, h);
    PolygonCoverage coverage;
    coverage.reset(x1, y1, w, h, getCells((size_t)stride * rows), &g_strip);
    g_span.resize(w);

    RowDrawer drawer(target);
    for (int first = 0; first < h; first += rows) {
        g_strip.set(first, mini(rows, h - first));
        for (auto &contour : path.m_contours) {
            addContour(coverage, &path.m_points[2 * contour.start], contour.count);
        }
        for (int i = 0; i < g_strip.rows; i++) {
            PolygonRow row(coverage.row(i), color);
            drawer.drawRow(x1, y1 + first + i, w, g_strip.ranges[i], row);
            clearCells(coverage.row(i), g_strip.ranges[i]);
        }
    }
}

void strokePath(Target &target, const Path &path, float lineWidth, uint32_t color) {
    stroke(target, path.m_points.data(), path.m_points.size() / 2, lineWidth, color, [&](LineCoverage &coverage, float halfWidth) {
        for (auto &contour : path.m_contours) {
            addContour(coverage, &path.m_points[2 * contour.start], contour.count, contour.closed, halfWidth);
        }
    });
}

void strokePolyline(Target &target, const float *points, size_t numPoints, float lineWidth, uint32_t color) {
    stroke(target, points, numPoints, lineWidth, color, [&](LineCoverage &coverage, float halfWidth) {
        addContour(coverage, points, numPoints, false, halfWidth);
    });
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

    // the points of the outline (clockwise, x and y of each point)
    void getOutline(std::vector<float> &points) const {
        points.clear();

        auto corner = [&](int i, float cx, float cy, float a1) {
            if (rx[i] > 0 && ry[i] > 0) {
                int n = arcSegments(maxf(rx[i], ry[i]), PI / 2);
                for (int k = 0; k <= n; k++) {
                    float a = a1 + (PI / 2) * k / n;
                    points.push_back(cx + rx[i] * cosf(a));
                    points.push_back(cy + ry[i] * sinf(a));
                }
            } else {
                points.push_back(cx);
                points.push_back(cy);
            }
        };

        corner(TOP_LEFT, left + rx[TOP_LEFT], top + ry[TOP_LEFT], PI);
        corner(TOP_RIGHT, right - rx[TOP_RIGHT], top + ry[TOP_RIGHT], 1.5f * PI);
        corner(BOTTOM_RIGHT, right - rx[BOTTOM_RIGHT], bottom - ry[BOTTOM_RIGHT], 0);
        corner(BOTTOM_LEFT, left + rx[BOTTOM_LEFT], bottom - ry[BOTTOM_LEFT], 0.5f * PI);
    }
};

// adds the edges of the outline (see RoundedBox::getOutline) to the coverage
void addOutline(PolygonCoverage &coverage, const std::vector<float> &points) {
    size_t n = points.size() / 2;
    for (size_t i = 1; i < n; i++) {
        coverage.addLine(points[2 * i - 2], points[2 * i - 1], points[2 * i], points[2 * i + 1]);
    }
    coverage.addLine(points[2 * n - 2], points[2 * n - 1], points[0], points[1]);
}

// A row of a rounded rectangle: the coverage of the outer (or the middle) box
// and of the inner box (useInner) as in PolygonRow, combined by pixelOf.
template <typename PixelOf>
struct RoundedRow {
    RoundedRow(const float *outer_, const float *inner_, const PixelOf &pixelOf_)
        : outer(outer_), inner(inner_), pixelOf(pixelOf_) {}

    uint32_t pixel(int i) {
        outerSum += outer[i];
        float c = fabsf(outerSum);
        outerCoverage = c < 1.0f ? c : 1.0f;
        if (inner) {
            innerSum += inner[i];
            c = fabsf(innerSum);
            innerCoverage = c < 1.0f ? c : 1.0f;
        }
        return pixelOf(outerCoverage, innerCoverage);
    }

    uint32_t constant() const { return pixelOf(outerCoverage, innerCoverage); }

    const float *outer;
    const float *inner; // nullptr without the inner box
    const PixelOf &pixelOf;
    float outerSum = 0;
    float outerCoverage = 0;
    float innerSum = 0;
    float innerCoverage = 0;
};

// the outlines of the boxes of a rounded rectangle
std::vector<float> g_outerOutline;
std::vector<float> g_innerOutline;

} // namespace

// Draws the rounded rectangle x1, y1, x2, y2 from its boxes: with drawLine the
// line is between outerBox and inner (useInner, else the whole outerBox) and the
// fill inside inner; without the line the fill is outerBox. The bands at the
// top and the bottom and the strips along the sides include the curves, the
// line and the antialiased pixels; between them is a plain fill.
static void drawRoundedBoxes(
    Target &target,
    int x1, int y1, int x2, int y2,
    const RoundedBox &outerBox, const RoundedBox &inner, bool useInner,
    bool drawLine, bool fill,
    uint32_t lineColor, uint32_t fillColor,
    int topBand, int bottomBand, int strip
);

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

    bool useInner = drawLine && hasInner;

    // Between the top and the bottom corners the sides are straight, there
    // only the strips along the sides are drawn with the coverage and the
    // rest is a plain fill (the bands and strips include the line and the
    // antialiased pixels). In the bands only the corners get the coverage of
    // each pixel, between them the rows are runs of one color (see Strips).
    int lwPixels = (int)ceilf(lw);
    int topBand = (int)ceilf(maxf(middle.ry[TOP_LEFT], middle.ry[TOP_RIGHT])) + lwPixels + 2;
    int bottomBand = (int)ceilf(maxf(middle.ry[BOTTOM_LEFT], middle.ry[BOTTOM_RIGHT])) + lwPixels + 2;
    int strip = lwPixels + 2;

    drawRoundedBoxes(
        target, x1, y1, x2, y2,
        drawLine ? outer : middle, inner, useInner,
        drawLine, fill, lineColor, fillColor,
        topBand, bottomBand, strip
    );
}

void roundedRectSides(
    Target &target,
    int x1, int y1, int x2, int y2,
    const float lineWidths[4],
    const float radii[8],
    uint32_t lineColor, uint32_t fillColor
) {
    float wt = maxf(lineWidths[0], 0);
    float wr = maxf(lineWidths[1], 0);
    float wb = maxf(lineWidths[2], 0);
    float wl = maxf(lineWidths[3], 0);

    bool drawLine = (wt > 0 || wr > 0 || wb > 0 || wl > 0) && (lineColor >> 24);
    bool fill = (fillColor >> 24) != 0;
    if (!drawLine && !fill) {
        return;
    }

    // the widths of the sides at the ends of each corner
    float wx[4];
    float wy[4];
    wx[TOP_LEFT] = wl;
    wy[TOP_LEFT] = wt;
    wx[TOP_RIGHT] = wr;
    wy[TOP_RIGHT] = wt;
    wx[BOTTOM_RIGHT] = wr;
    wy[BOTTOM_RIGHT] = wb;
    wx[BOTTOM_LEFT] = wl;
    wy[BOTTOM_LEFT] = wb;

    float rx[4] = { radii[0], radii[2], radii[4], radii[6] };
    float ry[4] = { radii[1], radii[3], radii[5], radii[7] };
    static_assert(TOP_LEFT == 0 && TOP_RIGHT == 1 && BOTTOM_RIGHT == 2 && BOTTOM_LEFT == 3, "corner order");

    // the middle of the line of each side, the radii are there
    RoundedBox middle;
    middle.left = x1 + wl / 2;
    middle.top = y1 + wt / 2;
    middle.right = x2 + 1 - wr / 2;
    middle.bottom = y2 + 1 - wb / 2;
    if (middle.right < middle.left) {
        middle.left = middle.right = (x1 + x2 + 1) / 2.0f;
    }
    if (middle.bottom < middle.top) {
        middle.top = middle.bottom = (y1 + y2 + 1) / 2.0f;
    }
    for (int i = 0; i < 4; i++) {
        middle.rx[i] = maxf(rx[i], 0);
        middle.ry[i] = maxf(ry[i], 0);
    }
    middle.limitRadii();

    // the outer edge of the line: the radii half the side width larger, the
    // inner edge: half the side width smaller
    RoundedBox outer;
    outer.left = (float)x1;
    outer.top = (float)y1;
    outer.right = (float)(x2 + 1);
    outer.bottom = (float)(y2 + 1);

    RoundedBox inner;
    inner.left = x1 + wl;
    inner.top = y1 + wt;
    inner.right = x2 + 1 - wr;
    inner.bottom = y2 + 1 - wb;

    for (int i = 0; i < 4; i++) {
        bool rounded = middle.rx[i] > 0 && middle.ry[i] > 0;
        outer.rx[i] = rounded ? middle.rx[i] + wx[i] / 2 : 0;
        outer.ry[i] = rounded ? middle.ry[i] + wy[i] / 2 : 0;
        inner.rx[i] = rounded ? maxf(middle.rx[i] - wx[i] / 2, 0) : 0;
        inner.ry[i] = rounded ? maxf(middle.ry[i] - wy[i] / 2, 0) : 0;
    }
    outer.limitRadii();
    inner.limitRadii();

    bool useInner = drawLine && !inner.isEmpty();

    int topBand = (int)ceilf(maxf(maxf(outer.ry[TOP_LEFT], outer.ry[TOP_RIGHT]), wt)) + 2;
    int bottomBand = (int)ceilf(maxf(maxf(outer.ry[BOTTOM_LEFT], outer.ry[BOTTOM_RIGHT]), wb)) + 2;
    int strip = (int)ceilf(maxf(wl, wr)) + 2;

    drawRoundedBoxes(
        target, x1, y1, x2, y2,
        drawLine ? outer : middle, inner, useInner,
        drawLine, fill, lineColor, fillColor,
        topBand, bottomBand, strip
    );
}

static void drawRoundedBoxes(
    Target &target,
    int x1, int y1, int x2, int y2,
    const RoundedBox &outerBox, const RoundedBox &inner, bool useInner,
    bool drawLine, bool fill,
    uint32_t lineColor, uint32_t fillColor,
    int topBand, int bottomBand, int strip
) {
    // the outlines are computed once, every part of the rectangle adds them
    // to its coverages (for each strip)
    outerBox.getOutline(g_outerOutline);
    if (useInner) {
        inner.getOutline(g_innerOutline);
    }

    float lineAlpha = (lineColor >> 24) / 255.0f;
    float fillAlpha = (fillColor >> 24) / 255.0f;

    // the pixel of the coverage of the outer (or the middle) box and of the
    // inner box
    auto pixelOf = [&](float o, float in) -> uint32_t {
        if (!drawLine) {
            return colorWithAlpha(fillColor, o);
        }

        float insideInner = useInner ? minf(in, o) : 0;
        float lineCoverage = o - insideInner;

        if (!fill || insideInner <= 0) {
            return colorWithAlpha(lineColor, lineCoverage);
        }

        if (lineCoverage <= 0) {
            return colorWithAlpha(fillColor, insideInner);
        }

        // the line and the fill cover different parts of the pixel
        float wl = lineCoverage * lineAlpha;
        float wf = insideInner * fillAlpha;
        float alpha = wl + wf;
        if (alpha <= 0) {
            return 0;
        }
        float kl = wl / alpha;
        float kf = wf / alpha;
        uint32_t r = (uint32_t)((lineColor & 0xFF) * kl + (fillColor & 0xFF) * kf + 0.5f);
        uint32_t g = (uint32_t)(((lineColor >> 8) & 0xFF) * kl + ((fillColor >> 8) & 0xFF) * kf + 0.5f);
        uint32_t b = (uint32_t)(((lineColor >> 16) & 0xFF) * kl + ((fillColor >> 16) & 0xFF) * kf + 0.5f);
        uint32_t a = (uint32_t)(alpha * 255.0f + 0.5f);
        return r | (g << 8) | (b << 16) | (a << 24);
    };

    RowDrawer drawer(target);

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

        int stride = PolygonCoverage::getStride(w);
        int numCoverages = useInner ? 2 : 1;
        int rows = getStripRows(stride * numCoverages, h);
        float *cells = getCells((size_t)stride * rows * numCoverages);

        PolygonCoverage outerCoverage;
        PolygonCoverage innerCoverage;
        outerCoverage.reset(px1, py1, w, h, cells, &g_strip);
        if (useInner) {
            innerCoverage.reset(px1, py1, w, h, cells + (size_t)stride * rows, &g_strip);
        }

        g_span.resize(w);

        for (int first = 0; first < h; first += rows) {
            g_strip.set(first, mini(rows, h - first));
            addOutline(outerCoverage, g_outerOutline);
            if (useInner) {
                addOutline(innerCoverage, g_innerOutline);
            }
            for (int i = 0; i < g_strip.rows; i++) {
                RoundedRow<decltype(pixelOf)> row(outerCoverage.row(i), useInner ? innerCoverage.row(i) : nullptr, pixelOf);
                drawer.drawRow(px1, py1 + first + i, w, g_strip.ranges[i], row);
                clearCells(outerCoverage.row(i), g_strip.ranges[i]);
                if (useInner) {
                    clearCells(innerCoverage.row(i), g_strip.ranges[i]);
                }
            }
        }
    };

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
