/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <stdint.h>
#include <vector>

// Antialiased drawing of polygons, lines and rounded rectangles.
//
// Coordinates are in pixels: the pixel (x, y) is the square from (x, y) to
// (x + 1, y + 1), so (x + 0.5, y + 0.5) is the center of the pixel. A color is
// 0xAABBGGRR (the 32-bit display color), its alpha is the opacity of the shape.
//
// A filled shape gets the exact part of each pixel it covers (the area of the
// polygon inside the pixel). A line gets, in each pixel, the part of the pixel
// width that is within half the line width from the line, so the line through
// the points has round joins and round caps.

namespace eez {
namespace gui {
namespace raster {

// Where the shapes are drawn.
struct Target {
    // the pixels outside of this rectangle (inclusive) are not drawn
    int clipX1;
    int clipY1;
    int clipX2;
    int clipY2;

    // Draws n pixels starting at (x, y), inside the clip rectangle: pixels[i]
    // is drawn over the pixel x + i, blended with its alpha (0 leaves the
    // pixel unchanged).
    virtual void blendSpan(int x, int y, int n, const uint32_t *pixels) = 0;

    // Draws the color over the rectangle (inclusive, inside the clip rectangle).
    virtual void fillRect(int x1, int y1, int x2, int y2, uint32_t color) = 0;
};

// Polygon (one or more contours) or line through the points.
class Path {
public:
    void moveTo(float x, float y);
    // line from the current point, a moveTo if there is none
    void lineTo(float x, float y);
    // Elliptical arc around (cx, cy) from the angle a1 to the angle a2 (radians,
    // 0 is to the right and pi / 2 is down, a2 can be less than a1), with a line
    // from the current point to its start.
    void arc(float cx, float cy, float rx, float ry, float a1, float a2);
    // the line from the last point to the first point of the contour
    void close();
    void clear();

    bool isEmpty() const { return m_points.empty(); }

private:
    friend void fillPath(Target &target, const Path &path, uint32_t color);
    friend void strokePath(Target &target, const Path &path, float lineWidth, uint32_t color);

    struct Contour {
        uint32_t start; // index of the first point
        uint32_t count; // number of points
        bool closed;
    };

    std::vector<float> m_points; // x, y
    std::vector<Contour> m_contours;
};

// Fills the path (every contour closed, nonzero winding).
void fillPath(Target &target, const Path &path, uint32_t color);

// Draws the line of the given width through the points of the path.
void strokePath(Target &target, const Path &path, float lineWidth, uint32_t color);

// Rectangle x1, y1, x2, y2 (pixels, inclusive) with rounded corners: radii are
// rtlx, rtly, rtrx, rtry, rbrx, rbry, rblx, rbly. The line (if lineWidth > 0
// and the alpha of lineColor is not 0) is inside the rectangle and the radii
// are at its middle: the outer edge of a rounded corner has the radius plus
// half the line width, the inner edge the radius minus half the line width,
// a corner with radius 0 is square. The fill (if the alpha of fillColor is not
// 0) is inside the line, without the line it is the rectangle inset by half the
// line width. Radii too large for the rectangle are reduced in proportion.
void roundedRect(
    Target &target,
    int x1, int y1, int x2, int y2,
    float lineWidth,
    const float radii[8],
    uint32_t lineColor, uint32_t fillColor
);

} // namespace raster
} // namespace gui
} // namespace eez
