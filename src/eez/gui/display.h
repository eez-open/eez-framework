/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <stdint.h>

#if defined(EEZ_PLATFORM_STM32)
	typedef uint16_t *VideoBuffer;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__) || defined(EEZ_PLATFORM_CM5_RASPI)
	typedef uint32_t *VideoBuffer;
#endif

#if EEZ_OPTION_GUI_ANIMATIONS
#include <eez/gui/animation.h>
#endif

#include <eez/gui/font.h>
#include <eez/gui/geometry.h>
#include <eez/gui/image.h>
#include <eez/gui/raster.h>

static const int CURSOR_WIDTH = 2;

namespace eez {
namespace gui {
namespace display {

#define TRANSPARENT_COLOR_INDEX 0xFFFF

extern VideoBuffer g_renderBuffer;

void init();

void turnOn();
void turnOff();
bool isOn();

void onThemeChanged();
void clearScreenTransparent();
void onLuminocityChanged();
void updateBrightness();

void update();

#if EEZ_OPTION_GUI_ANIMATIONS
void animate(Buffer startBuffer, void (*callback)(float t, VideoBuffer bufferOld, VideoBuffer bufferNew, VideoBuffer bufferDst), float duration = -1);
#endif

void beginRendering();
int beginBufferRendering();
void endBufferRendering(int bufferIndex, int x, int y, int width, int height, bool withShadow, uint8_t opacity, int xOffset, int yOffset, gui::Rect *backdrop);
void endRendering();

// The background of the render buffer currently drawn into (call it right after
// beginBufferRendering) is what is below it, i.e. the render buffers drawn before
// it composed in the same way as in endRendering, plus its own backdrop. Used by
// the page with transparent background. Returns true if the background changed
// since the previous frame, so the whole buffer must be drawn again.
bool setBufferBackgroundBelow(int x, int y, int width, int height, gui::Rect *backdrop);
// draws the background set with setBufferBackgroundBelow (does nothing if not set)
void drawBufferBackground(int x1, int y1, int x2, int y2);

// All drawing to the render buffer is clipped to the clip rectangle, which is
// the whole display by default (x2 and y2 are inclusive).
struct ClipRect {
    int x1;
    int y1;
    int x2;
    int y2;
};
// Sets the clip rectangle to the intersection of the current clip rectangle and
// the given rectangle. Returns the previous clip rectangle, which must be restored
// with restoreClipRect.
ClipRect intersectClipRect(int x, int y, int width, int height);
void restoreClipRect(const ClipRect &previousClipRect);
const ClipRect &getClipRect();

// Collects the bounding rectangle (inclusive, empty when x1 > x2) of everything
// drawn into the current buffer between beginDamageProbe and endDamageProbe.
// Probes can be nested, what is collected by the inner probe is also added to
// the outer probe.
struct DamageProbe {
    int x1;
    int y1;
    int x2;
    int y2;

    bool isEmpty() const { return x1 > x2 || y1 > y2; }

    // internal
    DamageProbe *parent;
    const void *target;
};
void beginDamageProbe(DamageProbe &probe);
void endDamageProbe(DamageProbe &probe);

// The rectangle (inclusive) of the current buffer is drawn into directly (not
// with the drawing functions): it is added to the damage, inside the clip
// rectangle.
void addDamageRect(int x1, int y1, int x2, int y2);

VideoBuffer getBufferPointer();

// What the last frame cost (for tests and tuning): the pixels composed from
// the render buffers into the frame, transferred to the screen and of the
// backdrop blurred, and the number of rectangles composed and transferred.
struct FrameStats {
    uint32_t composedPixels;
    uint32_t composedRects;
    uint32_t presentedPixels;
    uint32_t presentedRects;
    uint32_t blurredPixels;
};
const FrameStats &getFrameStats();

// The RGB pixels (3 bytes each) of the frame of the device app context,
// getScreenshotWidth() x getScreenshotHeight(). Valid until releaseScreenshot.
const uint8_t *takeScreenshot();
int getScreenshotWidth();
int getScreenshotHeight();
void releaseScreenshot();

#ifdef GUI_CALC_FPS
extern bool g_calcFpsEnabled;
extern bool g_drawFpsGraphEnabled;
extern uint32_t g_fpsAvg;
void drawFpsGraph(int x, int y, int w, int h, const Style *style);
#endif

inline int getDisplayWidth() { return DISPLAY_WIDTH; }
inline int getDisplayHeight() { return DISPLAY_HEIGHT;  }

#if DISPLAY_BPP == 16
typedef uint16_t Color;
#else
typedef uint32_t Color;
#endif

// The color with its alpha (theme colors of assets with 32-bit colors can have
// one; RGB565 colors have alpha 255). TRANSPARENT_COLOR_INDEX is 0, a fully
// transparent color.
Color getColorFromIndex(uint16_t colorIndex);

// False for TRANSPARENT_COLOR_INDEX and for a color with alpha: what is below
// a background in such a color must be drawn first, the color is blended over
// it.
bool isColorOpaque(uint16_t colorIndex);

// True for TRANSPARENT_COLOR_INDEX and for a color with alpha 0.
bool isColorTransparent(uint16_t colorIndex);

struct ColorRGBA {
	uint8_t r, g, b, a;
};
void getColorRGBAFromIndex(uint16_t color, ColorRGBA *colorRGBA);

// get foreground color
Color getColor();

// set foreground color by index
void setColor(uint16_t colorIndex, bool ignoreLuminocity = false);

// set foreground color by value
void setColor(uint8_t r, uint8_t g, uint8_t b);
void setColorByValue(Color color);

Color getBackColor();

// set background color by index
void setBackColor(uint16_t colorIndex, bool ignoreLuminocity = false);

// set background color by value
void setBackColor(uint8_t r, uint8_t g, uint8_t b);

uint8_t setOpacity(uint8_t opacity);
uint8_t getOpacity();

void getPixel(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b);

// these are the basic drawing operations
void startPixelsDraw();
void drawPixel(int x, int y);
void drawPixel(int x, int y, uint8_t opacity);
void endPixelsDraw();
void fillRect(int x1, int y1, int x2, int y2);
void bitBlt(int x1, int y1, int x2, int y2, int x, int y);
void drawBitmap(Image *image, int x, int y);

// used by animation
void fillRect(void *dst, int x1, int y1, int x2, int y2);
void bitBlt(void *src, int x1, int y1, int x2, int y2);
void bitBlt(void *src, void *dst, int x1, int y1, int x2, int y2);
void bitBlt(void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, uint8_t opacity); // also used for buffer rendering (see endRendering)

// used by ThorVG
void bitBlt(void *src, int srcBpp, uint32_t srcLineOffset, uint16_t *dst, int x, int y, int width, int height);

// these are implemented by calling basic drawing operations
void drawHLine(int x, int y, int l);
void drawVLine(int x, int y, int l);
void drawRect(int x1, int y1, int x2, int y2);
void drawFocusFrame(int x, int y, int w, int h);

// Antialiased drawing (see raster.h), with the current opacity, clipped to the
// clip rectangle.

// Rectangle with rounded corners (see raster::roundedRect): the line in the
// foreground color if drawLine, the fill in the background color if fill. The
// optional clip rectangle (inclusive) limits the drawing further.
void fillRoundedRect(
	int x1, int y1, int x2, int y2,
	int lineWidth,
	int rtlx, int rtly, int rtrx, int rtry,
	int rbrx, int rbry, int rblx, int rbly,
	bool drawLine, bool fill,
	int clip_x1 = -1, int clip_y1 = -1, int clip_x2 = -1, int clip_y2 = -1
);

void fillRoundedRect(
	int x1, int y1, int x2, int y2,
	int lineWidth,
	int r,
	bool drawLine, bool fill,
	int clip_x1 = -1, int clip_y1 = -1, int clip_x2 = -1, int clip_y2 = -1
);

// fillRoundedRect with a line of its own width on each side (see
// raster::roundedRectSides). lineColors: the color index of each side (top,
// right, bottom, left; TRANSPARENT_COLOR_INDEX: no line there), nullptr: the
// current color everywhere.
void fillRoundedRectSides(
	int x1, int y1, int x2, int y2,
	int lineWidthTop, int lineWidthRight, int lineWidthBottom, int lineWidthLeft,
	int rtlx, int rtly, int rtrx, int rtry,
	int rbrx, int rbry, int rblx, int rbly,
	bool drawLine, bool fill,
	const uint16_t *lineColors = nullptr, bool ignoreLuminocity = false
);

// Keeps the corners of a box with rounded corners: what is drawn between
// beginRoundedClip and endRoundedClip outside of the rounded corners of the box
// left, top, right, bottom (pixel edges, right and bottom exclusive) with the
// radii (rtlx, rtly, rtrx, rtry, rbrx, rbry, rblx, rbly) is undone, the
// antialiased edge in part. The pairs nest. Only on displays with a 32-bit
// buffer, elsewhere it does nothing.
//
// The corners are blended with what is below the content of the box (its
// background), so it must be known also when only the content is drawn again:
// with backgroundIsFresh the box itself is just drawn and the corner pixels are
// its background, they are kept for the next times (by the box and the owner,
// e.g. its style), when the corner pixels are the last frame.
struct RoundedClip {
	float left, top, right, bottom;
	float radii[8];
	const void *owner;

	// set by beginRoundedClip
	int level; // -1: not active
	void *buffer;
	DamageProbe probe;
};
void beginRoundedClip(RoundedClip &clip, bool backgroundIsFresh);
void endRoundedClip(RoundedClip &clip);

// only the line of the rectangle with rounded corners, in the foreground color,
// with any line width and radii (see raster::roundedRect)
void strokeRoundedRect(int x1, int y1, int x2, int y2, float lineWidth, const float radii[8]);

// only the line of the rectangle with rounded corners, in the foreground color
void drawRoundedRect(
	int x1, int y1, int x2, int y2,
	int lineWidth,
	int rtlx, int rtly, int rtrx, int rtry,
	int rbrx, int rbry, int rblx, int rbly
);

using Path = raster::Path;

// fills the path in the foreground color
void fillPath(const Path &path);

// line through the points of the path, with round joins and caps, in the
// foreground color
void strokePath(const Path &path, float lineWidth);

// line through the points (x and y of each point), as strokePath
void drawPolyline(const float *points, int numPoints, float lineWidth);

void drawStr(const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2, int clip_y2, gui::font::Font &font, int cursorPosition, int startFontIndex = 0);
int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font);
int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font);
int8_t measureGlyph(int32_t encoding, gui::font::Font &font);
int measureStr(const char *text, int textLength, gui::font::Font &font, int max_width = 0, int startFontIndex = 0);
int getFontIndexAtEnd(const char *text, int textLength, int fontIndex);

// Decodes the UTF-8 character at text into codePoint and returns the text after
// it. At the terminating NUL codePoint is 0 and text is returned. A byte that
// doesn't start a valid UTF-8 sequence, also a sequence cut short by the NUL, is
// U+FFFD and one byte long, so the decoding never steps over the NUL.
const char *decodeUtf8(const char *text, int32_t &codePoint);

// Returns the text after the character at text, as decodeUtf8, but a font tag
// that starts a font (see drawStr) is one character with its font index.
const char *skipChar(const char *text);

enum DisplayState {
    OFF,
    TURNING_ON,
    ON,
    TURNING_OFF
};

extern DisplayState g_displayState;

void markRenderBufferDirty(void);
// render buffer is dirty if something was drawn since the last clear
bool isRenderBufferDirty(void);
void clearRenderBufferDirty(void);

} // namespace display
} // namespace gui
} // namespace eez
