/*
 * eez-framework tests: helpers of the drawing tests (render/).
 *
 * Golden images: CHECK_GOLDEN(name, image) compares the image with
 * render/golden/<name>.png. With EEZ_TESTS_UPDATE_GOLDEN=1 in the environment
 * the baseline is (re)written instead and the check passes; a missing baseline
 * is a failure otherwise. On a mismatch the actual image, the baseline and a
 * diff image are written to <build dir>/render-failures/<name>/ and the paths
 * are printed.
 *
 * Images are RGBA with the bytes R, G, B, A in memory (0xAABBGGRR, the 32-bit
 * display color); the alpha of captured frames is set to 255 (the display
 * ignores it) and comparisons use R, G and B.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include <eez/core/assets.h>

namespace render {

struct Image32 {
    int w = 0;
    int h = 0;
    std::vector<uint32_t> px;

    Image32() {}
    Image32(int w_, int h_, uint32_t fill = 0xFF000000) : w(w_), h(h_), px((size_t)w_ * h_, fill) {}

    uint32_t at(int x, int y) const { return px[(size_t)y * w + x]; }
    uint32_t &at(int x, int y) { return px[(size_t)y * w + x]; }
};

inline uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}
inline int R(uint32_t c) { return c & 0xFF; }
inline int G(uint32_t c) { return (c >> 8) & 0xFF; }
inline int B(uint32_t c) { return (c >> 16) & 0xFF; }
inline int A(uint32_t c) { return (c >> 24) & 0xFF; }
inline uint32_t opaque(uint32_t c) { return c | 0xFF000000u; }

// the presented frame (the render buffer after harness::frame), or a part of it
Image32 captureFrame();
Image32 captureRegion(int x, int y, int w, int h);
// a region of any 800-px wide video buffer
Image32 captureBuffer(const uint32_t *buffer, int x, int y, int w, int h);

bool savePng(const std::string &path, const Image32 &image);
bool loadPng(const std::string &path, Image32 &image);

// pixels where any of R, G, B differs by more than tolerance
int countDiff(const Image32 &a, const Image32 &b, int tolerance = 0);

// folder for the images of failed checks (created on demand)
std::string failureDir(const std::string &name);
// writes actual, expected and diff images into failureDir(name), returns the folder
std::string writeMismatch(const std::string &name, const Image32 &actual, const Image32 &expected, int tolerance);

void checkGolden(const char *file, int line, const char *name, const Image32 &image);

#define CHECK_GOLDEN(NAME, IMAGE) render::checkGolden(__FILE__, __LINE__, NAME, IMAGE)

// pages
void showPage(int pageId, int frames = 3);
void pushPage(int pageId, int frames = 3);

// native variables render_text_a .. render_text_f (index 0 .. 5)
void setText(int index, const char *text);

// Runs frames until the blinking widgets are faded in (on: the blink opacity
// is 255) or out (off: 0), see g_blinkOpacity.
void frameToBlink(bool on);

// The presented frame compared with a full redraw of the same state
// (refreshScreen + frame), in the rectangle (the whole frame if w <= 0).
// Returns the number of differing pixels; on a mismatch the images are
// written with writeMismatch("redraw_" + name). sameTime: the full redraw at
// the time of the presented frame (the clock goes back one frame), for what
// changes in every frame (e.g. the blink opacity).
int compareWithFullRedraw(const char *name, int x = 0, int y = 0, int w = 0, int h = 0, bool sameTime = false);

// For direct drawing: full clip rectangle, opacity 255, a fresh widget cursor
// (refreshed, main assets), so the draw functions don't use the background
// stack of the last enumeration.
void resetDrawState();
// fills a rectangle of the render buffer with an opaque color (the current
// foreground color and opacity are kept)
void clearRect(int x1, int y1, int x2, int y2, uint32_t color = 0xFF000000);

// font of the style (main assets)
const eez::gui::Style *style(int styleId);

// Damage measuring: puts a marker pixel every `step` pixels in both directions
// into the video buffer the next frame will be rendered into (the one that is
// not presented now). After the frame, countMarkers() tells how many of them
// survived in the presented frame: the composed part of the frame is where
// they were overwritten.
void putMarkers(int step = 8);
int countMarkers(int step = 8);
int numMarkers(int step = 8);

} // namespace render
