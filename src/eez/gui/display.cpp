/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <algorithm>
#include <assert.h>
#include <atomic>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include <eez/core/utf8.h>
#include <eez/core/util.h>
#if OPTION_KEYBOARD
#include <eez/core/keyboard.h>
#endif
#if OPTION_MOUSE
#include <eez/core/mouse.h>
#endif
#include <eez/core/debug.h>
#include <eez/core/memory.h>
#include <eez/core/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>
#include <eez/gui/touch.h>
#include <eez/gui/scrolling.h>
#include <eez/gui/vector_font.h>

#if defined(EEZ_NEMA_GFX)
#include <eez/gui/nema_hal_stm32.h>
#include <nema_graphics.h>
#include <nema_blender.h>
#include <tsi_malloc.h>
#endif

#define CONF_BACKDROP_OPACITY 128

#if EEZ_USE_SDL
#define EEZ_SW_BACKDROP_BLUR 1
#include <vector>

// backdrop blur: downscale -> N x box blur (approximates gaussian) -> bilinear upscale + tint
#define CONF_BACKDROP_BLUR_DOWNSCALE 2
#define CONF_BACKDROP_BLUR_RADIUS 2 // in downscaled pixels
#define CONF_BACKDROP_BLUR_PASSES 3
#define CONF_BACKDROP_BLUR_TINT_OPACITY 80
#endif

// Full redraw: a frame in which something changed is drawn from the start,
// every widget in order (see update). Without it only the widgets that
// changed are drawn, over the previous frame.
#if EEZ_USE_SDL
#define EEZ_FULL_REDRAW 1
#else
#define EEZ_FULL_REDRAW 0
#endif

#if defined(EEZ_PLATFORM_STM32)

#include <dma2d.h>
#include <ltdc.h>

#ifdef EEZ_PLATFORM_STM32F469I_DISCO
#include "stm32469i_discovery_lcd.h"
extern "C" LTDC_HandleTypeDef hltdc_eval;
extern "C" DMA2D_HandleTypeDef hdma2d_eval;
#define hltdc hltdc_eval
#define hdma2d hdma2d_eval
#endif

#endif

#if EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
#include <SDL.h>
#endif

#if DISPLAY_BPP == 16
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x0400
#define COLOR_BLUE 0x001F
#else
#define COLOR_BLACK 0xFF000000
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_RED 0xFF0000FF
#define COLOR_GREEN 0xFF00FF00
#define COLOR_BLUE 0xFFFF0000
#endif

#define RGB565_COLOR_TO_R(C) (uint8_t(((C) >> 11) << 3))
#define RGB565_COLOR_TO_G(C) (uint8_t((((C) >> 5) << 2) & 0xFF))
#define RGB565_COLOR_TO_B(C) (uint8_t(((C) << 3) & 0xFF))

#if DISPLAY_BPP == 16
// C: rrrrrggggggbbbbb
#define RGB_TO_COLOR(R, G, B) (uint16_t((R)&0xF8) << 8) | (uint16_t((G)&0xFC) << 3) | (((B)&0xF8) >> 3)

#define COLOR_TO_R(C) RGB565_COLOR_TO_R(C)
#define COLOR_TO_G(C) RGB565_COLOR_TO_G(C)
#define COLOR_TO_B(C) RGB565_COLOR_TO_B(C)
#define COLOR_TO_A(C) 255
#else
// C: aaaaaaaabbbbbbbbggggggggrrrrrrrr
#define RGB_TO_COLOR(R, G, B) (Color(((R) << 0) | ((G) << 8) | ((B) << 16) | (255 << 24)))

#define COLOR_TO_R(C) (uint8_t((C) & 0xFF))
#define COLOR_TO_G(C) (uint8_t(((C) >> 8) & 0xFF))
#define COLOR_TO_B(C) (uint8_t(((C) >> 16) & 0xFF))
#define COLOR_TO_A(C) (uint8_t(((C) >> 24) & 0xFF))
#endif

using namespace eez::gui;

namespace eez {
namespace gui {
namespace display {

// rectangle in display coordinates (inclusive), empty when x1 > x2
struct DamageRect {
    int x1 = 0;
    int y1 = 0;
    int x2 = -1;
    int y2 = -1;

    bool isEmpty() const { return x1 > x2 || y1 > y2; }

    void clear() {
        x1 = y1 = 0;
        x2 = y2 = -1;
    }

    void setFull() {
        x1 = y1 = 0;
        x2 = DISPLAY_WIDTH - 1;
        y2 = DISPLAY_HEIGHT - 1;
    }

    void add(int ax1, int ay1, int ax2, int ay2) {
        if (ax1 > ax2 || ay1 > ay2) {
            return;
        }
        if (isEmpty()) {
            x1 = ax1;
            y1 = ay1;
            x2 = ax2;
            y2 = ay2;
        } else {
            x1 = std::min(x1, ax1);
            y1 = std::min(y1, ay1);
            x2 = std::max(x2, ax2);
            y2 = std::max(y2, ay2);
        }
    }

    void add(const DamageRect &r) { add(r.x1, r.y1, r.x2, r.y2); }

    void intersect(int ax1, int ay1, int ax2, int ay2) {
        x1 = std::max(x1, ax1);
        y1 = std::max(y1, ay1);
        x2 = std::min(x2, ax2);
        y2 = std::min(y2, ay2);
    }

    bool contains(int ax1, int ay1, int ax2, int ay2) const {
        return ax1 >= x1 && ay1 >= y1 && ax2 <= x2 && ay2 <= y2;
    }

    bool intersects(int ax1, int ay1, int ax2, int ay2) const {
        return ax1 <= x2 && ax2 >= x1 && ay1 <= y2 && ay2 >= y1;
    }

    uint32_t area() const {
        return isEmpty() ? 0 : (uint32_t)(x2 - x1 + 1) * (uint32_t)(y2 - y1 + 1);
    }

    bool operator==(const DamageRect &other) const {
        return x1 == other.x1 && y1 == other.y1 && x2 == other.x2 && y2 == other.y2;
    }
};

struct RenderBuffer {
    VideoBuffer bufferPointer;
    VideoBuffer previousBuffer;
    DamageRect damage; // the bounding rectangle of what is drawn into this buffer in the current frame
    DamageRect *previousDamage;
    int x;
    int y;
    int width;
    int height;
    bool withShadow;
    uint8_t opacity;
    int xOffset;
    int yOffset;
    gui::Rect *backdrop;
    int previousBufferIndex;
    bool rendering; // between beginBufferRendering and endBufferRendering
    bool hasBackgroundBelow; // see setBufferBackgroundBelow
    bool hadBackgroundBelow; // hasBackgroundBelow in the previous frame
};

DisplayState g_displayState;

VideoBuffer g_renderBuffer1;
VideoBuffer g_renderBuffer2;

VideoBuffer g_syncedBuffer;
VideoBuffer g_renderBuffer;

#if EEZ_OPTION_GUI_ANIMATIONS
VideoBuffer g_animationBuffer1;
VideoBuffer g_animationBuffer2;
VideoBuffer g_animationBuffer;
#endif

// takeScreenshot asks the GUI thread to copy the frame (see update), the GUI
// thread clears it when the copy is done
static std::atomic<bool> g_takeScreenshot;

// The screenshot buffer is in use, from the copy until releaseScreenshot. It is
// also an animation buffer, so the animations are not drawn while it is set.
static std::atomic<bool> g_screenshotAllocated;

#if EEZ_OPTION_THREADS && !EEZ_EMSCRIPTEN_NO_THREADS
// One screenshot at a time: taken in takeScreenshot, given back in
// releaseScreenshot. Not a mutex, because the screenshot can be released by
// another thread than the one that took it.
static std::atomic<bool> g_screenshotLocked;
#endif

// the RGB copy of the frame (see takeScreenshot)
static uint8_t *g_screenshotBuffer;
static int g_screenshotWidth;
static int g_screenshotHeight;

Color g_fc, g_bc;
uint8_t g_opacity = 255;

// the colors of the selected theme and the custom colors of the main assets:
// 32-bit colors if g_colors32, RGB565 otherwise (see colorFromAssets)
static const void *g_themeColors;
static uint32_t g_themeColorsCount;
static const void *g_colors;
static uint32_t g_colorsCount;
static bool g_colors32;

bool g_dirty;
inline void clearDirty() { g_dirty = false; }
inline void setDirty() { 
    g_dirty = true; 
}
inline bool isDirty() { return g_dirty; }

// The render buffers, one for each page and overlay drawn in a frame (see
// beginBufferRendering). NUM_AUX_BUFFERS of them are allocated by init, the
// others when they are needed the first time, and then kept.
#ifndef NUM_AUX_BUFFERS
#if defined(EEZ_PLATFORM_STM32)
// allocated at the start, before the heap is fragmented
#define NUM_AUX_BUFFERS 6
#else
#define NUM_AUX_BUFFERS 0
#endif
#endif

// the most layers drawn in a frame, a layer above them is drawn into the
// current buffer
#ifndef MAX_RENDER_BUFFERS
#define MAX_RENDER_BUFFERS 32
#endif

static_assert(NUM_AUX_BUFFERS <= MAX_RENDER_BUFFERS, "NUM_AUX_BUFFERS is greater than MAX_RENDER_BUFFERS");

RenderBuffer g_renderBuffers[MAX_RENDER_BUFFERS];
static VideoBuffer g_mainBufferPointer;
static int g_numBuffersToDraw;
// render buffer currently drawn into (-1 for the main buffer)
static int g_currentBufferIndex = -1;

// what is drawn into the buffer currently drawn into in this frame (nullptr when not collected)
static DamageRect *g_damage;
// drawn directly into the main render buffer in this frame
static DamageRect g_mainDamage;
// the render buffer to be drawn next is not the same as the synced buffer
static bool g_renderBufferStale = true;
// the synced buffer must be transferred to the screen
static bool g_presentNeeded = true;

// Dry run (see update): the drawing functions draw nothing, they only note
// that something would be drawn (isDirty).
static bool g_dryRun;

bool isDryRun() {
    return g_dryRun;
}

#if EEZ_FULL_REDRAW
// the next frame is drawn also if nothing changed
static bool g_redrawNeeded = true;
// The screen is transparent where no widget draws (see clearScreenTransparent):
// every frame is drawn into cleared buffers, so what is blended over the
// transparent screen is not blended over the previous frame.
static bool g_transparentScreen;
#endif

static inline void addDamage(int x1, int y1, int x2, int y2) {
    if (g_damage) {
        g_damage->add(x1, y1, x2, y2);
    }
}

// buffers composed in the previous frame, if they are not the same in this frame everything has to be composed
struct LayerInfo {
    int x;
    int y;
    int width;
    int height;
    int xOffset;
    int yOffset;
    bool withShadow;
    uint8_t opacity;
    bool hasBackdrop;

    bool operator==(const LayerInfo &other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height &&
            xOffset == other.xOffset && yOffset == other.yOffset && withShadow == other.withShadow &&
            opacity == other.opacity && hasBackdrop == other.hasBackdrop;
    }
};
static LayerInfo g_lastLayers[MAX_RENDER_BUFFERS];
static int g_lastNumLayers = -1;
// layers changed in the previous frame
static bool g_lastLayersChanged = true;

static LayerInfo getLayerInfo(const RenderBuffer &renderBuffer) {
    LayerInfo layer;
    layer.x = renderBuffer.x;
    layer.y = renderBuffer.y;
    layer.width = renderBuffer.width;
    layer.height = renderBuffer.height;
    layer.xOffset = renderBuffer.xOffset;
    layer.yOffset = renderBuffer.yOffset;
    layer.withShadow = renderBuffer.withShadow;
    layer.opacity = renderBuffer.opacity;
    layer.hasBackdrop = renderBuffer.backdrop != nullptr;
    return layer;
}

static void setFullDamage() {
    g_renderBufferStale = true;
    g_presentNeeded = true;
}

// the frame of a backdrop: its rectangle inside the display
static DamageRect getBackdropFrame(int x, int y, int w, int h) {
    DamageRect frame;
    frame.add(x, y, x + w - 1, y + h - 1);
    frame.intersect(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    return frame;
}

static DamageRect getBackdropFrame(const Rect &backdrop) {
    return getBackdropFrame(backdrop.x, backdrop.y, backdrop.w, backdrop.h);
}

// The backdrop of the render buffer can be seen: false also if the render
// buffer is opaque and covers the frame of its backdrop, so the blurred
// backdrop is overwritten by the render buffer when composed.
static bool isBackdropVisible(const RenderBuffer &renderBuffer) {
    if (!renderBuffer.backdrop) {
        return false;
    }
    DamageRect frame = getBackdropFrame(*renderBuffer.backdrop);
    if (frame.isEmpty()) {
        return false;
    }
    if (renderBuffer.opacity != 255) {
        return true;
    }
    int x1 = renderBuffer.x + renderBuffer.xOffset;
    int y1 = renderBuffer.y + renderBuffer.yOffset;
    int x2 = x1 + renderBuffer.width - 1;
    int y2 = y1 + renderBuffer.height - 1;
    return !(x1 <= frame.x1 && y1 <= frame.y1 && x2 >= frame.x2 && y2 >= frame.y2);
}

#if !EEZ_FULL_REDRAW
// the visible part (inside its rectangle) of what is drawn into the render
// buffer in this frame, in display coordinates
static DamageRect getLayerDamage(const RenderBuffer &renderBuffer) {
    DamageRect damage = renderBuffer.damage;
    damage.intersect(renderBuffer.x, renderBuffer.y, renderBuffer.x + renderBuffer.width - 1, renderBuffer.y + renderBuffer.height - 1);
    if (!damage.isEmpty()) {
        damage.x1 += renderBuffer.xOffset;
        damage.y1 += renderBuffer.yOffset;
        damage.x2 += renderBuffer.xOffset;
        damage.y2 += renderBuffer.yOffset;
    }
    return damage;
}
#endif

////////////////////////////////////////////////////////////////////////////////

static void initDriver();
static bool updateLayers();
static bool isOverlayDirty();
static void syncBuffer();
static void copySyncedBufferToScreenshotBuffer();
static void drawStrInit();
static void resetClipRect();
static void drawGlyph(const uint8_t *src, uint32_t srcLineOffset, int x, int y, int width, int height);

////////////////////////////////////////////////////////////////////////////////

#if EEZ_USE_SDL || (defined(EEZ_PLATFORM_STM32) && DISPLAY_BPP == 16)
static uint32_t color16to32(uint16_t color, uint8_t opacity = 255) {
    uint32_t color32;
    ((uint8_t *)&color32)[0] = RGB565_COLOR_TO_R(color);
    ((uint8_t *)&color32)[1] = RGB565_COLOR_TO_G(color);
    ((uint8_t *)&color32)[2] = RGB565_COLOR_TO_B(color);
    ((uint8_t *)&color32)[3] = opacity;
    return color32;
}
#endif

#if DISPLAY_BPP == 16
static uint16_t color32to16(uint32_t color) {
    auto pcolor = (uint8_t *)&color;
    return RGB_TO_COLOR(pcolor[0], pcolor[1], pcolor[1]);
}
#endif

// blends fgColor with opacity a (0..255) over the opaque bgColor, using integer arithmetic only
static inline uint32_t blendColorOpaque(uint32_t fgColor, uint32_t bgColor, uint32_t a) {
    uint32_t ia = 255 - a;

    // R and B channels in parallel, G separately; (x + 128 + ((x + 128) >> 8)) >> 8 == round(x / 255)
    uint32_t rb = (fgColor & 0x00FF00FF) * a + (bgColor & 0x00FF00FF) * ia + 0x00800080;
    rb = ((rb + ((rb >> 8) & 0x00FF00FF)) >> 8) & 0x00FF00FF;

    uint32_t g = ((fgColor >> 8) & 0xFF) * a + ((bgColor >> 8) & 0xFF) * ia + 0x80;
    g = ((g + (g >> 8)) >> 8) & 0xFF;

    return 0xFF000000 | rb | (g << 8);
}

static inline uint32_t blendColor(uint32_t fgColor, uint32_t bgColor) {
    uint32_t fgAlpha = fgColor >> 24;
    if (fgAlpha == 255) {
        return fgColor;
    }
    if (fgAlpha == 0) {
        return bgColor;
    }
    if ((bgColor >> 24) == 255) {
        return blendColorOpaque(fgColor, bgColor, fgAlpha);
    }

    uint8_t *fg = (uint8_t *)&fgColor;
    uint8_t *bg = (uint8_t *)&bgColor;

    float alphaMult = fg[3] * bg[3] / 255.0f;
    float alphaOut = fg[3] + bg[3] - alphaMult;

    float r = (fg[0] * fg[3] + bg[0] * bg[3] - bg[0] * alphaMult) / alphaOut;
    float g = (fg[1] * fg[3] + bg[1] * bg[3] - bg[1] * alphaMult) / alphaOut;
    float b = (fg[2] * fg[3] + bg[2] * bg[3] - bg[2] * alphaMult) / alphaOut;

    r = clamp(r, 0.0f, 255.0f);
    g = clamp(g, 0.0f, 255.0f);
    b = clamp(b, 0.0f, 255.0f);

    uint32_t result;
    uint8_t *presult = (uint8_t *)&result;
    presult[0] = (uint8_t)r;
    presult[1] = (uint8_t)g;
    presult[2] = (uint8_t)b;
    presult[3] = (uint8_t)alphaOut;

    return result;
}

// The opacity a color is drawn with: the current opacity (setOpacity) and the
// alpha of the color (theme colors of assets with 32-bit colors can have one).
// Every drawing operation takes the opacity of its color from here.
static inline uint32_t getDrawOpacity(Color color) {
    uint32_t alpha = COLOR_TO_A(color);
    if (alpha == 255) {
        return g_opacity;
    }
    return (g_opacity * alpha + 127) / 255;
}

////////////////////////////////////////////////////////////////////////////////

static uint32_t getVideoBufferSize() {
    return DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP / 8;
}

// the pixels of the render buffer, false if there is not enough memory
static bool allocRenderBuffer(int bufferIndex) {
    RenderBuffer &renderBuffer = g_renderBuffers[bufferIndex];
    if (!renderBuffer.bufferPointer) {
        renderBuffer.bufferPointer = (VideoBuffer)allocBuffer(getVideoBufferSize());
    }
    return renderBuffer.bufferPointer != nullptr;
}

void init() {
    onThemeChanged();

    uint32_t videoBufferSize = getVideoBufferSize();

    g_renderBuffer1 = (VideoBuffer)allocBuffer(videoBufferSize);
    g_renderBuffer2 = (VideoBuffer)allocBuffer(videoBufferSize);
    assert(g_renderBuffer1 && g_renderBuffer2);

#if EEZ_OPTION_GUI_ANIMATIONS
    // one block: the screenshot is copied into it (see update), the RGB copy
    // of a 16-bit frame doesn't fit into one animation buffer
    uint8_t *animationBuffers = allocBuffer(2 * videoBufferSize);
    assert(animationBuffers);
    g_animationBuffer1 = (VideoBuffer)animationBuffers;
    g_animationBuffer2 = (VideoBuffer)(animationBuffers + videoBufferSize);
    g_screenshotBuffer = animationBuffers;
#else
    g_screenshotBuffer = allocBuffer(2 * videoBufferSize);
    assert(g_screenshotBuffer);
#endif

    for (int i = 0; i < NUM_AUX_BUFFERS; i++) {
        allocRenderBuffer(i);
    }

    initDriver();

    // start with the black screen
    setColor(0, 0, 0);
    g_renderBuffer = g_renderBuffer1;
    fillRect(0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
    g_renderBuffer = g_renderBuffer2;
    fillRect(0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);

#if EEZ_OPTION_GUI_ANIMATIONS
    g_animationBuffer = g_animationBuffer1;
#endif

    g_syncedBuffer = g_renderBuffer1;
    setFullDamage();
    syncBuffer();
}

// Studio's page editor (the editor mode of the Studio WASM runtime) shows its
// own background where no widget draws, so the screen starts transparent
// (followed by refreshScreen, which draws all the widgets again)
void clearScreenTransparent() {
    memset((void *)g_renderBuffer1, 0, getVideoBufferSize());
    memset((void *)g_renderBuffer2, 0, getVideoBufferSize());
    // and the buffers of the pages: where a transparent page draws nothing
    // (e.g. outside of rounded corners) they keep what was drawn before
    for (int i = 0; i < MAX_RENDER_BUFFERS; i++) {
        if (g_renderBuffers[i].bufferPointer) {
            memset((void *)g_renderBuffers[i].bufferPointer, 0, getVideoBufferSize());
        }
    }
    setFullDamage();
#if EEZ_FULL_REDRAW
    g_redrawNeeded = true;
    g_transparentScreen = true;
#endif
}

void turnOn() {
    if (g_displayState != ON && g_displayState != TURNING_ON) {
		g_hooks.turnOnDisplayStart();
    }
}

bool isOn() {
    return g_displayState == ON || g_displayState == TURNING_ON;
}

void turnOff() {
    if (g_displayState != OFF && g_displayState != TURNING_OFF) {
		g_hooks.turnOffDisplayStart();
    }
}

////////////////////////////////////////////////////////////////////////////////

#ifdef GUI_CALC_FPS
bool g_calcFpsEnabled = true;
bool g_drawFpsGraphEnabled = true;
uint32_t g_fpsValues[NUM_FPS_VALUES];
uint32_t g_fpsAvg;
static uint32_t g_fpsTotal;
static uint32_t g_lastTimeFPS;

void calcFPS() {
    // calculate last FPS value
	g_fpsTotal -= g_fpsValues[0];

	for (size_t i = 1; i < NUM_FPS_VALUES; i++) {
		g_fpsValues[i - 1] = g_fpsValues[i];
	}

	uint32_t time = millis();
	auto diff = time - g_lastTimeFPS;

	auto fps = diff ? 1000 / diff : 0;

    static int dither;
    dither = (dither + 1) % NUM_FPS_VALUES;
    if (dither == 0 && fps > 5) fps = 0;

    g_fpsValues[NUM_FPS_VALUES - 1] = fps;

	g_fpsTotal += g_fpsValues[NUM_FPS_VALUES - 1];
	g_fpsAvg = g_fpsTotal / NUM_FPS_VALUES;
}

void drawFpsGraph(int x, int y, int w, int h, const Style *style) {
	int x1 = x;
	int y1 = y;
	int x2 = x + w - 1;
	int y2 = y + h - 1;
	drawBorderAndBackground(x1, y1, x2, y2, style, style->backgroundColor);

	x1++;
	y1++;
	x2--;
	y2--;

	bool isRed = false;
	display::setColor(style->color);

	x = x1;
	for (size_t i = 0; i < NUM_FPS_VALUES && x <= x2; i++, x++) {
		int y = y2 - (g_fpsValues[i] <= 60 ? g_fpsValues[i] : 60) * (y2 - y1) / 60;
		if (y < y1) {
			y = y1;
		}

		if (g_fpsValues[i] < 40) {
			if (!isRed) {
				display::setColorByValue(COLOR_RED);
				isRed = true;
			}
		} else {
			if (isRed) {
				display::setColor(style->color);
				isRed = false;
			}
		}

		display::drawVLine(x, y, y2 - y);
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////

#if EEZ_OPTION_GUI_ANIMATIONS
static void finishAnimation() {
    g_animationState.enabled = false;

    if (g_renderBuffer == g_renderBuffer1) {
        g_renderBuffer = g_renderBuffer2;
        bitBlt(g_renderBuffer1, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
    } else {
        g_renderBuffer = g_renderBuffer1;
        bitBlt(g_renderBuffer2, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
    }

    g_syncedBuffer = g_renderBuffer1;
    setFullDamage();
    syncBuffer();
}
#endif

#if EEZ_OPTION_GUI_ANIMATIONS
void animate(Buffer startBuffer, void (*callback)(float t, VideoBuffer bufferOld, VideoBuffer bufferNew, VideoBuffer bufferDst), float duration) {
    if (g_animationState.enabled) {
        display::finishAnimation();
    }

    g_animationState.enabled = true;
    g_animationState.startTime = 0;
    g_animationState.duration = duration != -1 ? duration : g_hooks.getDefaultAnimationDuration();
    g_animationState.startBuffer = startBuffer;
    g_animationState.callback = callback;
    g_animationState.easingRects = remapOutQuad;
    g_animationState.easingOpacity = remapOutCubic;
}

static void animateStep() {
    uint32_t time = millis();
    if (time == 0) {
        time = 1;
    }
    if (g_animationState.startTime == 0) {
        g_animationState.startTime = time;
    }
    float t = (time - g_animationState.startTime) / (1000.0f * g_animationState.duration);
    if (t < 1.0f) {
		if (g_syncedBuffer == g_animationBuffer1) {
			g_animationBuffer = g_animationBuffer2;
		} else {
			g_animationBuffer = g_animationBuffer1;
		}

        if (g_renderBuffer == g_renderBuffer1) {
            g_animationState.callback(t, g_renderBuffer2, g_renderBuffer1, g_animationBuffer);
        } else {
            g_animationState.callback(t, g_renderBuffer1, g_renderBuffer2, g_animationBuffer);
        }

        g_syncedBuffer = g_animationBuffer;
        setFullDamage();
        syncBuffer();
    } else {
    	finishAnimation();
    }
}
#endif

void update() {
    if (g_displayState == TURNING_ON) {
		g_hooks.turnOnDisplayTick();
    } else if (g_displayState == TURNING_OFF) {
		g_hooks.turnOffDisplayTick();
    } else if (g_displayState == OFF) {
		#if EEZ_OPTION_GUI_ANIMATIONS
			if (g_animationState.enabled) {
				display::finishAnimation();
			}
		#endif

#if !EEZ_USE_SDL
		#if EEZ_OPTION_THREADS
        	sleepMs(16);
        	sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC, 0, 0);
		#endif

        return;
#endif
    }

#ifdef GUI_CALC_FPS
	g_lastTimeFPS = millis();
#endif

    touchScrollTick();

    display::beginRendering();

#if EEZ_FULL_REDRAW
    // Dry run: the widgets are updated as when only the widgets that changed
    // are drawn, but nothing is drawn. It tells if something changed.
    g_dryRun = true;
    updateScreen();
    g_dryRun = false;

    bool changed = isDirty() || g_redrawNeeded;
    if (!updateLayers()) {
        // page opened/closed, moved or changed opacity
        changed = true;
    }
    if (isOverlayDirty()) {
        changed = true;
    }

    if (changed) {
        // every widget is drawn, in order
        g_redrawNeeded = false;
        display::beginRendering();
        if (g_transparentScreen) {
            memset((void *)g_renderBuffer, 0, getVideoBufferSize());
        }
        updateScreen(true);
        setDirty();
    } else {
        clearDirty();
    }
#else
    updateScreen();
#endif

    display::endRendering();

#ifdef GUI_CALC_FPS
    if (g_calcFpsEnabled) {
        calcFPS();
    }
#endif

#if EEZ_OPTION_GUI_ANIMATIONS
    if (!g_screenshotAllocated && g_animationState.enabled) {
        animateStep();
    } else {
#endif
        g_syncedBuffer = g_renderBuffer;
		syncBuffer();

        if (g_takeScreenshot) {
            copySyncedBufferToScreenshotBuffer();

            // allocated before the request is done, so no animation step
            // overwrites the copy before releaseScreenshot
            g_screenshotAllocated = true;
            g_takeScreenshot = false;
        }
#if EEZ_OPTION_GUI_ANIMATIONS
    }
#endif
}

const uint8_t *takeScreenshot() {
#if !EEZ_OPTION_THREADS || EEZ_EMSCRIPTEN_NO_THREADS
    copySyncedBufferToScreenshotBuffer();
#else
    // waits until the screenshot taken before is released
    while (g_screenshotLocked.exchange(true)) {
        sleepMs(1);
    }

    // the GUI thread copies the frame in update
    g_takeScreenshot = true;
    while (g_takeScreenshot) {
        sleepMs(1);
    }
#endif

    return g_screenshotBuffer;
}

int getScreenshotWidth() {
    return g_screenshotWidth;
}

int getScreenshotHeight() {
    return g_screenshotHeight;
}

void releaseScreenshot() {
#if !EEZ_OPTION_THREADS || EEZ_EMSCRIPTEN_NO_THREADS
    g_screenshotAllocated = false;
#else
    // (only after takeScreenshot got the copy)
    if (g_screenshotAllocated.exchange(false)) {
        g_screenshotLocked = false;
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////

VideoBuffer getBufferPointer() {
    return g_renderBuffer;
}

void setBufferPointer(VideoBuffer buffer) {
    g_renderBuffer = buffer;
}

void beginRendering() {
    if (g_syncedBuffer == g_renderBuffer1) {
        g_renderBuffer = g_renderBuffer2;
    } else if (g_syncedBuffer == g_renderBuffer2) {
        g_renderBuffer = g_renderBuffer1;
    }

    clearDirty();

    resetClipRect();

    g_mainBufferPointer = getBufferPointer();
    g_numBuffersToDraw = 0;
    g_currentBufferIndex = -1;

    g_mainDamage.clear();
    g_damage = &g_mainDamage;
}

static int g_maxNumBuffersToDraw = 0;

int beginBufferRendering() {
    if (g_numBuffersToDraw >= MAX_RENDER_BUFFERS || !allocRenderBuffer(g_numBuffersToDraw)) {
        // No render buffer (more layers than MAX_RENDER_BUFFERS or not enough
        // memory): it is drawn into the current buffer, endBufferRendering with
        // -1 does nothing.
        return -1;
    }

    int bufferIndex = g_numBuffersToDraw++;
    if (g_numBuffersToDraw > g_maxNumBuffersToDraw) {
        g_maxNumBuffersToDraw = g_numBuffersToDraw;
    }
	g_renderBuffers[bufferIndex].previousBuffer = getBufferPointer();
    setBufferPointer(g_renderBuffers[bufferIndex].bufferPointer);

#if EEZ_FULL_REDRAW
    if (g_transparentScreen && !g_dryRun) {
        memset((void *)g_renderBuffers[bufferIndex].bufferPointer, 0, getVideoBufferSize());
    }
#endif

    g_renderBuffers[bufferIndex].damage.clear();
    g_renderBuffers[bufferIndex].previousDamage = g_damage;
    g_damage = &g_renderBuffers[bufferIndex].damage;

    g_renderBuffers[bufferIndex].previousBufferIndex = g_currentBufferIndex;
    g_currentBufferIndex = bufferIndex;

    g_renderBuffers[bufferIndex].rendering = true;
    g_renderBuffers[bufferIndex].hadBackgroundBelow = g_renderBuffers[bufferIndex].hasBackgroundBelow;
    g_renderBuffers[bufferIndex].hasBackgroundBelow = false;

    return bufferIndex;
}

#if defined(EEZ_NEMA_GFX)
static void blurBackdrop(int x, int y, int w, int h) {
    const int DOWNSCALE_FACTOR = 4;

    if (w <= 0 || h <= 0) {
        return;
    }

    int displayWidth = getDisplayWidth();
    int displayHeight = getDisplayHeight();

    // Clamp blur rect to the visible display area.
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > displayWidth) {
        w = displayWidth - x;
    }
    if (y + h > displayHeight) {
        h = displayHeight - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    uint8_t *srcBuffer = (uint8_t *)g_renderBuffer;

    int dsW = (w + DOWNSCALE_FACTOR - 1) / DOWNSCALE_FACTOR;
    int dsH = (h + DOWNSCALE_FACTOR - 1) / DOWNSCALE_FACTOR;

#if DISPLAY_BPP == 16
    const uint32_t nemaFormat = NEMA_RGB565;
    int stride = displayWidth * 2;
    int tmpStride = dsW * 2;
    int tmpSize = tmpStride * dsH;
#else
    const uint32_t nemaFormat = NEMA_ARGB8888;
    int stride = displayWidth * 4;
    int tmpStride = dsW * 4;
    int tmpSize = tmpStride * dsH;
#endif

    static uint8_t *tmpBuffer = nullptr;
    static int tmpBufferSize = 0;
    if (tmpBuffer == nullptr || tmpBufferSize < tmpSize) {
        if (tmpBuffer) {
            tsi_free(tmpBuffer);
        }
        tmpBuffer = (uint8_t *)tsi_malloc(tmpSize);
        tmpBufferSize = tmpBuffer ? tmpSize : 0;
    }
    if (tmpBuffer == nullptr) {
        return;
    }

    uintptr_t srcPhys = tsi_virt2phys((void *)srcBuffer);
    if (srcPhys == 0) {
        srcPhys = (uintptr_t)srcBuffer;
    }

    uintptr_t tmpPhys = tsi_virt2phys((void *)tmpBuffer);
    if (tmpPhys == 0) {
        tmpPhys = (uintptr_t)tmpBuffer;
    }

    nema_cl_rewind(&nema_hal_stm32_cmd_list);
    nema_cl_bind(&nema_hal_stm32_cmd_list);

    nema_set_blend_blit(NEMA_BL_SRC);
    nema_set_clip(0, 0, dsW, dsH);
    nema_bind_src_tex(srcPhys + y * stride + x * (stride / displayWidth),
                      w, h, nemaFormat, stride, NEMA_FILTER_BL | NEMA_TEX_CLAMP);
    nema_bind_dst_tex(tmpPhys, dsW, dsH, nemaFormat, tmpStride);
    nema_blit_rect_fit(0, 0, dsW, dsH);
    nema_cl_submit(&nema_hal_stm32_cmd_list);
    nema_cl_wait(&nema_hal_stm32_cmd_list);

    nema_cl_rewind(&nema_hal_stm32_cmd_list);
    nema_cl_bind(&nema_hal_stm32_cmd_list);

    nema_set_blend_blit(NEMA_BL_SRC);
    nema_set_clip(x, y, w, h);
    nema_bind_src_tex(tmpPhys, dsW, dsH, nemaFormat, tmpStride, NEMA_FILTER_BL | NEMA_TEX_CLAMP);
    nema_bind_dst_tex(srcPhys, displayWidth, getDisplayHeight(), nemaFormat, stride);
    nema_blit_rect_fit(x, y, w, h);
    nema_cl_submit(&nema_hal_stm32_cmd_list);
    nema_cl_wait(&nema_hal_stm32_cmd_list);
}
#endif

#if defined(EEZ_SW_BACKDROP_BLUR)
// box blur along "numLines" lines of "length" packed RGB pixels; "step" and "lineStep" are byte strides along/between lines
static void boxBlurLines(const uint8_t *src, uint8_t *dst, int length, int numLines, int step, int lineStep, int r) {
    const int div = 2 * r + 1;
    const int last = length - 1;
    for (int line = 0; line < numLines; line++) {
        const uint8_t *in = src + line * lineStep;
        uint8_t *out = dst + line * lineStep;
        for (int c = 0; c < 3; c++) {
            int sum = 0;
            for (int i = -r; i <= r; i++) {
                sum += in[std::min(std::max(i, 0), last) * step + c];
            }
            for (int i = 0; i < length; i++) {
                out[i * step + c] = (uint8_t)((sum + div / 2) / div);
                sum += in[std::min(i + r + 1, last) * step + c] - in[std::max(i - r, 0) * step + c];
            }
        }
    }
}

// The blur of a backdrop: its frame (the backdrop rectangle inside the display)
// is downscaled (block average), blurred with box blur passes and upscaled
// bilinearly, mixed with the backdrop tint color.

// source coordinate (8.8 fixed point) of the destination pixel center: (p + 0.5) / F - 0.5
static inline void blurSrcCoord(int p, int maxIndex, int &i0, int &i1, int &f) {
    const int F = CONF_BACKDROP_BLUR_DOWNSCALE;
    int u = ((2 * p + 1) * 256) / (2 * F) - 128;
    u = std::min(std::max(u, 0), maxIndex * 256);
    i0 = u >> 8;
    i1 = std::min(i0 + 1, maxIndex);
    f = u & 255;
}

// blurs the frame of the buffer
static void blurBackdropFrame(uint32_t *buffer, const DamageRect &frame) {
    const int F = CONF_BACKDROP_BLUR_DOWNSCALE;

    int displayWidth = getDisplayWidth();

    int w = frame.x2 - frame.x1 + 1;
    int h = frame.y2 - frame.y1 + 1;
    int dsW = (w + F - 1) / F;
    int dsH = (h + F - 1) / F;

    static std::vector<uint8_t> bufA;
    static std::vector<uint8_t> bufB;
    static std::vector<int> colX0;
    static std::vector<int> colX1;
    static std::vector<int> colFx;
    bufA.resize(dsW * dsH * 3);
    bufB.resize(dsW * dsH * 3);
    colX0.resize(w);
    colX1.resize(w);
    colFx.resize(w);

    // 1. downscale (block average)
    for (int dy = 0; dy < dsH; dy++) {
        int y0 = dy * F;
        int y1 = std::min(y0 + F, h);
        for (int dx = 0; dx < dsW; dx++) {
            int x0 = dx * F;
            int x1 = std::min(x0 + F, w);
            uint32_t r = 0, g = 0, b = 0;
            for (int sy = y0; sy < y1; sy++) {
                const uint8_t *p = (const uint8_t *)(buffer + (frame.y1 + sy) * displayWidth + frame.x1 + x0);
                for (int sx = x0; sx < x1; sx++, p += 4) {
                    r += p[0];
                    g += p[1];
                    b += p[2];
                }
            }
            uint32_t n = (y1 - y0) * (x1 - x0);
            uint8_t *d = &bufA[(dy * dsW + dx) * 3];
            d[0] = (uint8_t)((r + n / 2) / n);
            d[1] = (uint8_t)((g + n / 2) / n);
            d[2] = (uint8_t)((b + n / 2) / n);
        }
    }

    // 2. separable box blur passes, the lines are clamped at the edges of the frame
    for (int pass = 0; pass < CONF_BACKDROP_BLUR_PASSES; pass++) {
        boxBlurLines(bufA.data(), bufB.data(), dsW, dsH, 3, dsW * 3, CONF_BACKDROP_BLUR_RADIUS);
        boxBlurLines(bufB.data(), bufA.data(), dsH, dsW, dsW * 3, 3, CONF_BACKDROP_BLUR_RADIUS);
    }

    // 3. bilinear upscale back into the buffer, mixed with the backdrop tint color
    auto savedColor = g_fc;
    setColor(EEZ_COLOR_ID_BACKDROP);
    const int ta = CONF_BACKDROP_BLUR_TINT_OPACITY;
    const int tr = COLOR_TO_R(g_fc) * ta;
    const int tg = COLOR_TO_G(g_fc) * ta;
    const int tb = COLOR_TO_B(g_fc) * ta;
    g_fc = savedColor;

    for (int px = 0; px < w; px++) {
        blurSrcCoord(px, dsW - 1, colX0[px], colX1[px], colFx[px]);
    }

    for (int py = 0; py < h; py++) {
        int y0, y1, fy;
        blurSrcCoord(py, dsH - 1, y0, y1, fy);
        const uint8_t *row0 = &bufA[y0 * dsW * 3];
        const uint8_t *row1 = &bufA[y1 * dsW * 3];
        uint32_t *dst = buffer + (frame.y1 + py) * displayWidth + frame.x1;
        for (int px = 0; px < w; px++) {
            const uint8_t *p00 = row0 + colX0[px] * 3;
            const uint8_t *p01 = row0 + colX1[px] * 3;
            const uint8_t *p10 = row1 + colX0[px] * 3;
            const uint8_t *p11 = row1 + colX1[px] * 3;
            int fx = colFx[px];

            uint32_t value = 0xFF000000;
            for (int c = 0; c < 3; c++) {
                int top = p00[c] * (256 - fx) + p01[c] * fx;
                int bottom = p10[c] * (256 - fx) + p11[c] * fx;
                int v = (top * (256 - fy) + bottom * fy) >> 16;
                int t = c == 0 ? tr : c == 1 ? tg : tb;
                v = (v * (256 - ta) + t) >> 8;
                value |= (uint32_t)v << (8 * c);
            }
            dst[px] = value;
        }
    }
}

static void blurBackdrop(int x, int y, int w, int h) {
    DamageRect frame = getBackdropFrame(x, y, w, h);
    if (!frame.isEmpty()) {
        blurBackdropFrame((uint32_t *)g_renderBuffer, frame);
    }
}
#endif

void endBufferRendering(int bufferIndex, int x, int y, int width, int height, bool withShadow, uint8_t opacity, int xOffset, int yOffset, Rect *backdrop) {
    if (bufferIndex < 0) {
        // beginBufferRendering had no free buffer
        return;
    }

    // the buffers are ended in the reverse order of beginBufferRendering
    assert(bufferIndex == g_currentBufferIndex);

    RenderBuffer &renderBuffer = g_renderBuffers[bufferIndex];

	renderBuffer.x = x;
	renderBuffer.y = y;
	renderBuffer.width = width;
	renderBuffer.height = height;
	renderBuffer.withShadow = withShadow;
	renderBuffer.opacity = opacity;
	renderBuffer.xOffset = xOffset;
	renderBuffer.yOffset = yOffset;
	renderBuffer.backdrop = backdrop;

    setBufferPointer(renderBuffer.previousBuffer);
    g_damage = renderBuffer.previousDamage;

    g_currentBufferIndex = renderBuffer.previousBufferIndex;
    renderBuffer.rendering = false;
}

static ClipRect g_clipRect = { 0, 0, (int)DISPLAY_WIDTH - 1, (int)DISPLAY_HEIGHT - 1 };

static void resetClipRect() {
    g_clipRect.x1 = 0;
    g_clipRect.y1 = 0;
    g_clipRect.x2 = getDisplayWidth() - 1;
    g_clipRect.y2 = getDisplayHeight() - 1;
}

ClipRect intersectClipRect(int x, int y, int width, int height) {
    ClipRect previousClipRect = g_clipRect;
    g_clipRect.x1 = MAX(g_clipRect.x1, x);
    g_clipRect.y1 = MAX(g_clipRect.y1, y);
    g_clipRect.x2 = MIN(g_clipRect.x2, x + width - 1);
    g_clipRect.y2 = MIN(g_clipRect.y2, y + height - 1);
    return previousClipRect;
}

void restoreClipRect(const ClipRect &previousClipRect) {
    g_clipRect = previousClipRect;
}

const ClipRect &getClipRect() {
    return g_clipRect;
}

static inline bool isInsideClipRect(int x, int y) {
    return x >= g_clipRect.x1 && x <= g_clipRect.x2 && y >= g_clipRect.y1 && y <= g_clipRect.y2;
}

// returns false if nothing is left
static inline bool clipToClipRect(int &x1, int &y1, int &x2, int &y2) {
    if (x1 < g_clipRect.x1) x1 = g_clipRect.x1;
    if (y1 < g_clipRect.y1) y1 = g_clipRect.y1;
    if (x2 > g_clipRect.x2) x2 = g_clipRect.x2;
    if (y2 > g_clipRect.y2) y2 = g_clipRect.y2;
    return x1 <= x2 && y1 <= y2;
}

void addDamageRect(int x1, int y1, int x2, int y2) {
    if (clipToClipRect(x1, y1, x2, y2)) {
        if (!g_dryRun) {
            addDamage(x1, y1, x2, y2);
        }
        setDirty();
    }
}

static void composeBackdrop(const Rect &backdrop) {
#if (0 && defined(EEZ_NEMA_GFX)) || defined(EEZ_SW_BACKDROP_BLUR)
    // only the part inside the clip rectangle
    int x1 = backdrop.x, y1 = backdrop.y, x2 = backdrop.x + backdrop.w - 1, y2 = backdrop.y + backdrop.h - 1;
    if (clipToClipRect(x1, y1, x2, y2)) {
        blurBackdrop(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    }
#else
    // opacity backdrop
    auto savedOpacity = setOpacity(CONF_BACKDROP_OPACITY);
    setColor(EEZ_COLOR_ID_BACKDROP);
    fillRect(backdrop.x, backdrop.y, backdrop.x + backdrop.w - 1, backdrop.y + backdrop.h - 1);
    setOpacity(savedOpacity);
#endif
}

// Composes the render buffer into the buffer currently drawn into, only the
// part inside the clip rectangle, with its backdrop if it can be seen.
static void composeRenderBuffer(const RenderBuffer &renderBuffer) {
    int sx = renderBuffer.x;
    int sy = renderBuffer.y;

    int x1 = renderBuffer.x + renderBuffer.xOffset;
    int y1 = renderBuffer.y + renderBuffer.yOffset;
    int x2 = x1 + renderBuffer.width - 1;
    int y2 = y1 + renderBuffer.height - 1;

    if (isBackdropVisible(renderBuffer)) {
        composeBackdrop(*renderBuffer.backdrop);
    }

    if (renderBuffer.withShadow) {
        drawShadow(x1, y1, x2, y2);
    }

    // copy only the part inside the clip rectangle
    int dx1 = x1, dy1 = y1, dx2 = x2, dy2 = y2;
    if (clipToClipRect(dx1, dy1, dx2, dy2)) {
        bitBlt(renderBuffer.bufferPointer, nullptr, sx + dx1 - x1, sy + dy1 - y1, dx2 - dx1 + 1, dy2 - dy1 + 1, dx1, dy1, renderBuffer.opacity);
    }
}

#if !EEZ_FULL_REDRAW
static bool isBufferBackgroundChanged(int bufferIndex, int x, int y, int width, int height, Rect *backdrop, bool withShadow) {
    const RenderBuffer &renderBuffer = g_renderBuffers[bufferIndex];

    if (g_lastLayersChanged || !renderBuffer.hadBackgroundBelow || bufferIndex >= g_lastNumLayers) {
        return true;
    }

    const LayerInfo &lastLayer = g_lastLayers[bufferIndex];
    if (lastLayer.x != x || lastLayer.y != y || lastLayer.width != width || lastLayer.height != height || lastLayer.hasBackdrop != (backdrop != nullptr) || lastLayer.withShadow != withShadow) {
        return true;
    }

    for (int i = 0; i < bufferIndex; i++) {
        const RenderBuffer &below = g_renderBuffers[i];

        // (buffer still being rendered, i.e. the one this buffer is inside of, gets its layer info at the end)
        if (!below.rendering && !(getLayerInfo(below) == g_lastLayers[i])) {
            return true;
        }

        if (getLayerDamage(below).intersects(x, y, x + width - 1, y + height - 1)) {
            return true;
        }
    }

    return false;
}
#endif

bool setBufferBackgroundBelow(int x, int y, int width, int height, Rect *backdrop, bool withShadow) {
    if (g_currentBufferIndex == -1) {
        return false;
    }

    int bufferIndex = g_currentBufferIndex;
    RenderBuffer &renderBuffer = g_renderBuffers[bufferIndex];

#if EEZ_FULL_REDRAW
    // a change below is a change of the frame, the buffer is drawn again in any case
    bool changed = false;
#else
    bool changed = isBufferBackgroundChanged(bufferIndex, x, y, width, height, backdrop, withShadow);
#endif

    renderBuffer.hasBackgroundBelow = true;
    // also set in endBufferRendering, but it is needed before (see drawBufferBackground)
    renderBuffer.x = x;
    renderBuffer.y = y;
    renderBuffer.width = width;
    renderBuffer.height = height;
    renderBuffer.backdrop = backdrop;
    renderBuffer.withShadow = withShadow;

    return changed;
}

// draws the buffers below, own backdrop and shadow, only the part inside the
// clip rectangle
static void composeBufferBackground(const RenderBuffer &renderBuffer) {
    auto savedColor = g_fc;
    auto savedOpacity = setOpacity(255);

    // compose the buffers below in the same way as in endRendering
    for (int bufferIndex = 0; bufferIndex < g_currentBufferIndex; bufferIndex++) {
        composeRenderBuffer(g_renderBuffers[bufferIndex]);
    }

    // own backdrop is composed (in endRendering) under this buffer, so it must be in the background
    if (renderBuffer.backdrop) {
        composeBackdrop(*renderBuffer.backdrop);
    }

    // own shadow too, also below the page: it shows outside of the rounded
    // corners of the page
    if (renderBuffer.withShadow) {
        drawShadow(renderBuffer.x, renderBuffer.y, renderBuffer.x + renderBuffer.width - 1, renderBuffer.y + renderBuffer.height - 1, true);
    }

    setOpacity(savedOpacity);
    g_fc = savedColor;
}

void drawBufferBackground(int x1, int y1, int x2, int y2) {
    if (g_currentBufferIndex == -1) {
        return;
    }

    const RenderBuffer &renderBuffer = g_renderBuffers[g_currentBufferIndex];
    if (!renderBuffer.hasBackgroundBelow || g_dryRun) {
        return;
    }

    auto savedClipRect = intersectClipRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

    if (g_clipRect.x1 <= g_clipRect.x2 && g_clipRect.y1 <= g_clipRect.y2) {
#if defined(EEZ_SW_BACKDROP_BLUR)
        if (renderBuffer.backdrop) {
            // The blur of a part of the backdrop is not the same as the part
            // of the blurred backdrop, so the whole backdrop is blurred (as in
            // endRendering) and there is no seam at the edge of the buffer.
            // What is composed outside of the rectangle of the buffer is not
            // shown.
            resetClipRect();
            const Rect &backdrop = *renderBuffer.backdrop;
            intersectClipRect(
                MIN(backdrop.x, renderBuffer.x),
                MIN(backdrop.y, renderBuffer.y),
                MAX(backdrop.x + backdrop.w, renderBuffer.x + renderBuffer.width) - MIN(backdrop.x, renderBuffer.x),
                MAX(backdrop.y + backdrop.h, renderBuffer.y + renderBuffer.height) - MIN(backdrop.y, renderBuffer.y)
            );
        }
#endif
        composeBufferBackground(renderBuffer);
    }

    restoreClipRect(savedClipRect);
}

// returns false if the buffers to compose are not the same as in the previous frame
static bool updateLayers() {
    bool same = g_numBuffersToDraw == g_lastNumLayers;

    for (int bufferIndex = 0; bufferIndex < g_numBuffersToDraw; bufferIndex++) {
        RenderBuffer &renderBuffer = g_renderBuffers[bufferIndex];

        LayerInfo layer = getLayerInfo(renderBuffer);

        if (same && !(layer == g_lastLayers[bufferIndex])) {
            same = false;
        }
        g_lastLayers[bufferIndex] = layer;
    }

    g_lastNumLayers = g_numBuffersToDraw;

    return same;
}

// composes the layers in the clip rectangle
static void composeLayers() {
    for (int bufferIndex = 0; bufferIndex < g_numBuffersToDraw; bufferIndex++) {
        composeRenderBuffer(g_renderBuffers[bufferIndex]);
    }
}

// the keyboard focus, the mouse pointer or the FPS graph must be drawn again
static bool isOverlayDirty() {
    bool dirty = false;

#if OPTION_KEYBOARD
    if (keyboard::isDisplayDirty()) {
        dirty = true;
    }
#endif

#if OPTION_MOUSE
    if (mouse::isDisplayDirty()) {
        dirty = true;
    }
#endif

#if defined(GUI_CALC_FPS)
    if (g_drawFpsGraphEnabled) {
        dirty = true;
    }
#endif

    return dirty;
}

void endRendering() {
    setBufferPointer(g_mainBufferPointer);

    resetClipRect();

    // composing below is not collected
    g_damage = nullptr;

#if !EEZ_FULL_REDRAW
    if (isOverlayDirty()) {
        setDirty();
    }
#endif

    g_lastLayersChanged = !updateLayers();
    if (g_lastLayersChanged) {
        // page opened/closed, moved or changed opacity
        setDirty();
    }

    if (isDirty()) {
        // the whole frame is composed
        composeLayers();

#if defined(GUI_CALC_FPS)
        if (g_drawFpsGraphEnabled) {
            drawFpsGraph(getDisplayWidth() - NUM_FPS_VALUES - 4, 4, NUM_FPS_VALUES, 32, getStyle(EEZ_STYLE_ID_FPS_GRAPH));
        }
#endif

#if OPTION_KEYBOARD
        keyboard::updateDisplay();
#endif

#if OPTION_MOUSE
        mouse::updateDisplay();
#endif

        // the other render buffer (the synced buffer) misses this frame
        g_renderBufferStale = true;
        g_presentNeeded = true;
    } else {
        // Nothing was drawn: the render buffer is made the same as the synced
        // buffer (once, the next idle frames have nothing to copy). During an
        // animation it is not known what is in the render buffer.
        bool isRenderBufferTracked = g_syncedBuffer != g_renderBuffer && (g_syncedBuffer == g_renderBuffer1 || g_syncedBuffer == g_renderBuffer2);
        if (isRenderBufferTracked && g_renderBufferStale) {
            bitBlt(g_syncedBuffer, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
            g_renderBufferStale = false;
        }
        clearDirty();
    }
}

////////////////////////////////////////////////////////////////////////////////

void onThemeChanged() {
    auto selectedThemeIndex = g_hooks.getSelectedThemeIndex();
    g_themeColors = getThemeColors(selectedThemeIndex);
    g_themeColorsCount = getThemeColorsCount(selectedThemeIndex);
    g_colors = getColors();
    g_colorsCount = g_mainAssets->colorsDefinition->colors.count;
    g_colors32 = g_mainAssets->projectMinorVersion == 1;
}

////////////////////////////////////////////////////////////////////////////////

// Color i of a color list of the assets. Assets with 32-bit colors (Studio's
// "Color BPP" 32, projectMinorVersion 1) store R, G, B and alpha (R in the low
// byte), the others RGB565.
static Color colorFromAssets(const void *colors, uint32_t i, bool colors32) {
    if (colors32) {
        uint32_t color = ((const uint32_t *)colors)[i];
#if DISPLAY_BPP == 16
        return RGB_TO_COLOR(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
#else
        return color;
#endif
    }

    uint16_t color = ((const uint16_t *)colors)[i];
#if DISPLAY_BPP == 16
    return color;
#else
    return RGB_TO_COLOR(RGB565_COLOR_TO_R(color), RGB565_COLOR_TO_G(color), RGB565_COLOR_TO_B(color));
#endif
}

static Color g_runtimeColors[NUM_RUNTIME_COLORS];

// A color index below -1 (as int16_t) is a color of external (i.e. not main)
// assets: first the colors defined in the external assets, their values are in
// the themes (one theme for each main assets theme, in the same order), then
// the custom colors. The color list and the index in it, in the theme
// themeIndex of the main assets, false if there is no such color.
static bool findExternalColor(Assets *assets, uint16_t colorIndex, uint32_t themeIndex, const void *&colors, uint32_t &index) {
    index = -((int16_t)colorIndex) - 2;

    auto &themes = assets->colorsDefinition->themes;
    if (themes.count > 0) {
        if (themeIndex >= themes.count) {
            themeIndex = 0;
        }
        auto &themeColors = themes[themeIndex]->colors;
        if (index < themeColors.count) {
            colors = themeColors.items;
            return true;
        }
        index -= themeColors.count;
    }

    auto &customColors = assets->colorsDefinition->colors;
    if (index >= customColors.count) {
        return false;
    }
    colors = customColors.items;
    return true;
}

Color getColorFromIndex(uint16_t colorIndex) {
    if (colorIndex == TRANSPARENT_COLOR_INDEX) {
        return 0;
    }

    if ((int16_t)colorIndex < -1) {
        Assets *assets = g_widgetCursor.assets;
        const void *colors;
        uint32_t index;
        if (!findExternalColor(assets, colorIndex, g_hooks.getSelectedThemeIndex(), colors, index)) {
            return 0;
        }
        return colorFromAssets(colors, index, assets->projectMinorVersion == 1);
    }

    if (colorIndex >= RUNTIME_COLOR_INDEX_FIRST && colorIndex < RUNTIME_COLOR_INDEX_FIRST + NUM_RUNTIME_COLORS) {
        return g_runtimeColors[colorIndex - RUNTIME_COLOR_INDEX_FIRST];
    }

    colorIndex = g_hooks.transformColor(colorIndex);

    if (colorIndex < g_themeColorsCount) {
        return colorFromAssets(g_themeColors, colorIndex, g_colors32);
    }

    if (colorIndex - g_themeColorsCount < g_colorsCount) {
        return colorFromAssets(g_colors, colorIndex - g_themeColorsCount, g_colors32);
    }

    return 0;
}

bool isColorOpaque(uint16_t colorIndex) {
    return colorIndex != TRANSPARENT_COLOR_INDEX && COLOR_TO_A(getColorFromIndex(colorIndex)) == 255;
}

bool isColorTransparent(uint16_t colorIndex) {
    return colorIndex == TRANSPARENT_COLOR_INDEX || COLOR_TO_A(getColorFromIndex(colorIndex)) == 0;
}

void getColorRGBAFromIndex(uint16_t colorIndex, ColorRGBA *colorRGBA) {
    auto color = getColorFromIndex(colorIndex);
    colorRGBA->r = COLOR_TO_R(color);
    colorRGBA->g = COLOR_TO_G(color);
    colorRGBA->b = COLOR_TO_B(color);
    colorRGBA->a = COLOR_TO_A(color);
}

bool getThemeColorRGBA(Assets *assets, uint16_t colorIndex, int themeIndex, ColorRGBA *colorRGBA) {
    if (colorIndex == TRANSPARENT_COLOR_INDEX) {
        return false;
    }

    const void *colors;
    uint32_t index;
    bool colors32;
    if ((int16_t)colorIndex < -1) {
        if (!assets || !findExternalColor(assets, colorIndex, themeIndex < 0 ? 0 : (uint32_t)themeIndex, colors, index)) {
            return false;
        }
        colors32 = assets->projectMinorVersion == 1;
    } else {
        if (themeIndex < 0 || themeIndex >= getThemesCount()) {
            themeIndex = 0;
        }
        uint32_t themeColorsCount = getThemeColorsCount(themeIndex);
        if (colorIndex < themeColorsCount) {
            colors = getThemeColors(themeIndex);
            index = colorIndex;
        } else if (colorIndex - themeColorsCount < g_mainAssets->colorsDefinition->colors.count) {
            colors = getColors();
            index = colorIndex - themeColorsCount;
        } else {
            return false;
        }
        colors32 = g_mainAssets->projectMinorVersion == 1;
    }

    if (colors32) {
        uint32_t color = ((const uint32_t *)colors)[index];
        colorRGBA->r = color & 0xFF;
        colorRGBA->g = (color >> 8) & 0xFF;
        colorRGBA->b = (color >> 16) & 0xFF;
        colorRGBA->a = color >> 24;
    } else {
        uint16_t color = ((const uint16_t *)colors)[index];
        colorRGBA->r = RGB565_COLOR_TO_R(color);
        colorRGBA->g = RGB565_COLOR_TO_G(color);
        colorRGBA->b = RGB565_COLOR_TO_B(color);
        colorRGBA->a = 255;
    }
    return true;
}

// get foreground color
Color getColor() {
    return g_fc;
}

// set foreground color by index
void setColor(uint16_t colorIndex) {
    g_fc = getColorFromIndex(colorIndex);
}

// set foreground color by RGB value
void setColor(uint8_t r, uint8_t g, uint8_t b) {
    g_fc = RGB_TO_COLOR(r, g, b);
}

uint16_t setRuntimeColor(int i, uint8_t r, uint8_t g, uint8_t b) {
    if (i < 0 || i >= NUM_RUNTIME_COLORS) {
        return TRANSPARENT_COLOR_INDEX;
    }
    g_runtimeColors[i] = RGB_TO_COLOR(r, g, b);
    return RUNTIME_COLOR_INDEX_FIRST + i;
}

// set foreground color by value
void setColorByValue(Color color) {
    g_fc = color;
}

Color getBackColor() {
    return g_bc;
}

// set background color by index
void setBackColor(uint16_t colorIndex) {
	g_bc = getColorFromIndex(colorIndex);
}

// set background color by value
void setBackColor(uint8_t r, uint8_t g, uint8_t b) {
    g_bc = RGB_TO_COLOR(r, g, b);
}

uint8_t setOpacity(uint8_t opacity) {
    uint8_t savedOpacity = g_opacity;
    g_opacity = opacity;
    return savedOpacity;
}

uint8_t getOpacity() {
    return g_opacity;
}

////////////////////////////////////////////////////////////////////////////////

void drawHLine(int x, int y, int l) {
    fillRect(x, y, x + l, y);
}

void drawVLine(int x, int y, int l) {
    fillRect(x, y, x, y + l);
}

void drawRect(int x1, int y1, int x2, int y2) {
    drawHLine(x1, y1, x2 - x1);
    drawHLine(x1, y2, x2 - x1);
    drawVLine(x1, y1, y2 - y1);
    drawVLine(x2, y1, y2 - y1);
}

void drawFocusFrame(int x, int y, int w, int h) {
    int lineWidth = MIN(MIN(3, w), h);

    setColorByValue(RGB_TO_COLOR(255, 0, 255));

    // top
    fillRect(x, y, x + w - 1, y + lineWidth - 1);

    // left
    fillRect(x, y + lineWidth, x + lineWidth - 1, y + h - lineWidth - 1);

    // right
    fillRect(x + w - lineWidth, y + lineWidth, x + w - 1, y + h - lineWidth - 1);

    // bottom
    fillRect(x, y + h - lineWidth, x + w - 1, y + h - 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////

// The target of the antialiased drawing (raster.h): the render buffer inside
// the clip rectangle and the optional clip rectangle (clip_x1 != -1).
struct RenderBufferTarget : raster::Target {
    RenderBufferTarget(int clip_x1 = -1, int clip_y1 = -1, int clip_x2 = -1, int clip_y2 = -1) {
        clipX1 = g_clipRect.x1;
        clipY1 = g_clipRect.y1;
        clipX2 = g_clipRect.x2;
        clipY2 = g_clipRect.y2;
        if (clip_x1 != -1) {
            clipX1 = MAX(clipX1, clip_x1);
            clipY1 = MAX(clipY1, clip_y1);
            clipX2 = MIN(clipX2, clip_x2);
            clipY2 = MIN(clipY2, clip_y2);
        }
        startPixelsDraw();
    }

    ~RenderBufferTarget() {
        endPixelsDraw();
    }

    bool isEmpty() const {
        return clipX1 > clipX2 || clipY1 > clipY2;
    }

    void blendSpan(int x, int y, int n, const uint32_t *pixels) override {
#if EEZ_USE_SDL
        if (g_dryRun) {
            setDirty();
            return;
        }
        uint32_t *dst = g_renderBuffer + y * (int)DISPLAY_WIDTH + x;
        for (int i = 0; i < n; i++) {
            if (pixels[i] >> 24) {
                dst[i] = blendColor(pixels[i], dst[i]);
            }
        }
        addDamage(x, y, x + n - 1, y);
        setDirty();
#else
        auto savedColor = g_fc;
        for (int i = 0; i < n; i++) {
            uint32_t pixel = pixels[i];
            if (pixel >> 24) {
                g_fc = RGB_TO_COLOR(pixel & 0xFF, (pixel >> 8) & 0xFF, (pixel >> 16) & 0xFF);
                drawPixel(x + i, y, pixel >> 24);
            }
        }
        g_fc = savedColor;
#endif
    }

    void fillRect(int x1, int y1, int x2, int y2, uint32_t color) override {
        auto savedColor = g_fc;
        auto savedOpacity = g_opacity;
        g_fc = RGB_TO_COLOR(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
        g_opacity = color >> 24;
        display::fillRect(x1, y1, x2, y2);
        g_fc = savedColor;
        g_opacity = savedOpacity;
    }
};

// the color with the opacity it is drawn with, as a color of raster.h
static uint32_t getRasterColor(Color color) {
    return (uint32_t)COLOR_TO_R(color) | ((uint32_t)COLOR_TO_G(color) << 8) | ((uint32_t)COLOR_TO_B(color) << 16) | (getDrawOpacity(color) << 24);
}

void fillRoundedRect(
    int x1, int y1, int x2, int y2,
    int lineWidth,
    int rtlx, int rtly, int rtrx, int rtry,
    int rbrx, int rbry, int rblx, int rbly,
    bool drawLine, bool fill,
    int clip_x1, int clip_y1, int clip_x2, int clip_y2
) {
    RenderBufferTarget target(clip_x1, clip_y1, clip_x2, clip_y2);
    if (target.isEmpty()) {
        return;
    }

    float radii[8] = {
        (float)rtlx, (float)rtly, (float)rtrx, (float)rtry,
        (float)rbrx, (float)rbry, (float)rblx, (float)rbly
    };

    raster::roundedRect(
        target,
        x1, y1, x2, y2,
        (float)lineWidth,
        radii,
        drawLine ? getRasterColor(g_fc) : 0,
        fill ? getRasterColor(g_bc) : 0
    );
}

void fillRoundedRect(
    int x1, int y1, int x2, int y2,
    int lineWidth,
    int r,
    bool drawLine, bool fill,
    int clip_x1, int clip_y1, int clip_x2, int clip_y2
) {
    fillRoundedRect(x1, y1, x2, y2, lineWidth, r, r, r, r, r, r, r, r, drawLine, fill, clip_x1, clip_y1, clip_x2, clip_y2);
}

void fillRoundedRectSides(
	int x1, int y1, int x2, int y2,
	int lineWidthTop, int lineWidthRight, int lineWidthBottom, int lineWidthLeft,
	int rtlx, int rtly, int rtrx, int rtry,
	int rbrx, int rbry, int rblx, int rbly,
	bool drawLine, bool fill,
	const uint16_t *lineColors
) {
    RenderBufferTarget target;
    if (target.isEmpty()) {
        return;
    }

    float lineWidths[4] = {
        (float)lineWidthTop, (float)lineWidthRight, (float)lineWidthBottom, (float)lineWidthLeft
    };

    float radii[8] = {
        (float)rtlx, (float)rtly, (float)rtrx, (float)rtry,
        (float)rbrx, (float)rbry, (float)rblx, (float)rbly
    };

    // the colors of the sides as the current color would be set to them
    uint32_t sideColors[4];
    if (lineColors) {
        for (int i = 0; i < 4; i++) {
            if (lineColors[i] == TRANSPARENT_COLOR_INDEX) {
                sideColors[i] = 0;
            } else {
                Color color = getColorFromIndex(lineColors[i]);
                sideColors[i] = getRasterColor(color);
            }
        }
    }

    raster::roundedRectSides(
        target,
        x1, y1, x2, y2,
        lineWidths,
        radii,
        drawLine ? getRasterColor(g_fc) : 0,
        fill ? getRasterColor(g_bc) : 0,
        drawLine && lineColors ? sideColors : nullptr
    );
}

////////////////////////////////////////////////////////////////////////////////

#if EEZ_USE_SDL
// the saved corner pixels of the active rounded clips, by level
static std::vector<std::vector<uint32_t>> g_roundedClipPixels;
static int g_roundedClipLevel;

// The rectangle of the pixels (inclusive) of corner i (top left, top right,
// bottom right, bottom left) of the clip, inside the display; false if there is
// none.
static bool getRoundedClipCorner(const RoundedClip &clip, int i, int &x1, int &y1, int &x2, int &y2) {
    float rx = clip.radii[2 * i];
    float ry = clip.radii[2 * i + 1];
    if (rx <= 0 || ry <= 0) {
        return false;
    }

    bool left = i == 0 || i == 3;
    bool top = i == 0 || i == 1;

    x1 = left ? (int)floorf(clip.left) : (int)floorf(clip.right - rx);
    x2 = left ? (int)ceilf(clip.left + rx) - 1 : (int)ceilf(clip.right) - 1;
    y1 = top ? (int)floorf(clip.top) : (int)floorf(clip.bottom - ry);
    y2 = top ? (int)ceilf(clip.top + ry) - 1 : (int)ceilf(clip.bottom) - 1;

    x1 = MAX(x1, 0);
    y1 = MAX(y1, 0);
    x2 = MIN(x2, (int)DISPLAY_WIDTH - 1);
    y2 = MIN(y2, (int)DISPLAY_HEIGHT - 1);

    return x1 <= x2 && y1 <= y2;
}

// the part of the pixel x, y inside the rounded corner i of the clip (in the
// corner rectangle)
static float getRoundedClipCoverage(const RoundedClip &clip, int i, int x, int y) {
    float rx = clip.radii[2 * i];
    float ry = clip.radii[2 * i + 1];

    bool left = i == 0 || i == 3;
    bool top = i == 0 || i == 1;

    float cx = left ? clip.left + rx : clip.right - rx;
    float cy = top ? clip.top + ry : clip.bottom - ry;

    float dx = x + 0.5f - cx;
    float dy = y + 0.5f - cy;

    if ((left ? dx >= 0 : dx <= 0) || (top ? dy >= 0 : dy <= 0)) {
        // beside the curve, inside the box
        return 1.0f;
    }

    // signed distance to the ellipse, approximated by its value over its gradient
    float nx = dx / rx;
    float ny = dy / ry;
    float f = nx * nx + ny * ny - 1.0f;
    float gx = nx / rx;
    float gy = ny / ry;
    float g = 2.0f * sqrtf(gx * gx + gy * gy);
    float distance = g > 0 ? f / g : 0;

    float coverage = 0.5f - distance;
    return coverage < 0 ? 0 : coverage > 1.0f ? 1.0f : coverage;
}
#endif

void beginRoundedClip(RoundedClip &clip) {
#if EEZ_USE_SDL
    if (g_dryRun) {
        clip.level = -1;
        return;
    }

    clip.level = g_roundedClipLevel++;
    clip.buffer = g_renderBuffer;

    if ((int)g_roundedClipPixels.size() <= clip.level) {
        g_roundedClipPixels.resize(clip.level + 1);
    }
    auto &pixels = g_roundedClipPixels[clip.level];
    pixels.clear();

    auto buffer = (uint32_t *)g_renderBuffer;
    for (int i = 0; i < 4; i++) {
        int x1, y1, x2, y2;
        if (getRoundedClipCorner(clip, i, x1, y1, x2, y2)) {
            for (int y = y1; y <= y2; y++) {
                auto row = buffer + y * (int)DISPLAY_WIDTH;
                pixels.insert(pixels.end(), row + x1, row + x2 + 1);
            }
        }
    }
#else
    clip.level = -1;
#endif
}

void endRoundedClip(RoundedClip &clip) {
#if EEZ_USE_SDL
    if (clip.level < 0) {
        return;
    }

    g_roundedClipLevel = clip.level;

    if (clip.buffer != g_renderBuffer) {
        // drawn into another buffer
        return;
    }

    auto &pixels = g_roundedClipPixels[clip.level];
    size_t k = 0;

    auto buffer = (uint32_t *)g_renderBuffer;
    for (int i = 0; i < 4; i++) {
        int x1, y1, x2, y2;
        if (!getRoundedClipCorner(clip, i, x1, y1, x2, y2)) {
            continue;
        }

        for (int y = y1; y <= y2; y++) {
            auto row = buffer + y * (int)DISPLAY_WIDTH;
            for (int x = x1; x <= x2; x++, k++) {
                uint32_t saved = pixels[k];
                uint32_t current = row[x];
                if (saved == current) {
                    continue;
                }

                float coverage = getRoundedClipCoverage(clip, i, x, y);
                if (coverage >= 1.0f) {
                    continue;
                }
                if (coverage <= 0) {
                    row[x] = saved;
                    continue;
                }

                // what is drawn over the part of the pixel inside the corner
                int a = (int)(coverage * 256.0f + 0.5f);
                uint32_t result = 0;
                for (int shift = 0; shift < 32; shift += 8) {
                    int s = (saved >> shift) & 0xFF;
                    int c = (current >> shift) & 0xFF;
                    result |= (uint32_t)((s + (((c - s) * a) >> 8)) & 0xFF) << shift;
                }
                row[x] = result;
            }
        }
    }
#endif
}

void strokeRoundedRect(int x1, int y1, int x2, int y2, float lineWidth, const float radii[8]) {
    RenderBufferTarget target;
    if (!target.isEmpty()) {
        raster::roundedRect(target, x1, y1, x2, y2, lineWidth, radii, getRasterColor(g_fc), 0);
    }
}

void drawRoundedRect(
    int x1, int y1, int x2, int y2,
    int lineWidth,
    int rtlx, int rtly, int rtrx, int rtry,
    int rbrx, int rbry, int rblx, int rbly
) {
    fillRoundedRect(
        x1, y1, x2, y2,
        lineWidth,
        rtlx, rtly, rtrx, rtry,
        rbrx, rbry, rblx, rbly,
        true, false
    );
}

void fillPath(const Path &path) {
    RenderBufferTarget target;
    if (!target.isEmpty()) {
        raster::fillPath(target, path, getRasterColor(g_fc));
    }
}

void strokePath(const Path &path, float lineWidth) {
    RenderBufferTarget target;
    if (!target.isEmpty()) {
        raster::strokePath(target, path, lineWidth, getRasterColor(g_fc));
    }
}

void drawPolyline(const float *points, int numPoints, float lineWidth) {
    RenderBufferTarget target;
    if (!target.isEmpty() && numPoints > 0) {
        raster::strokePolyline(target, points, (size_t)numPoints, lineWidth, getRasterColor(g_fc));
    }
}

////////////////////////////////////////////////////////////////////////////////

// Text can contain font tags, see convertFontTags in Studio:
// FONT_START_CHAR followed by a character whose code point is the font index
// switches to that font, FONT_END_CHAR switches back to the default font.
// FONT_START_EXTERNAL_CHAR is FONT_START_CHAR for a font of the external
// assets: the code point is minus the font index.
static const utf8_int32_t FONT_START_CHAR = 0xE000;
static const utf8_int32_t FONT_END_CHAR = 0xE001;
static const utf8_int32_t FONT_START_EXTERNAL_CHAR = 0xE002;

// fontIndex as in getFontData: > 0 main assets font, < 0 external assets font
static gui::font::Font getFontByIndex(int fontIndex, gui::font::Font &defaultFont) {
    const gui::FontData *fontData = nullptr;
    if (fontIndex < 0 || (g_mainAssets && fontIndex <= (int)g_mainAssets->fonts.count)) {
        fontData = getFontData(fontIndex);
    }
    return fontData ? gui::font::Font(fontData) : defaultFont;
}

static bool isFontTag(utf8_int32_t encoding) {
    return encoding == FONT_START_CHAR || encoding == FONT_END_CHAR || encoding == FONT_START_EXTERNAL_CHAR;
}

const char *decodeUtf8(const char *text, int32_t &codePoint) {
    const uint8_t *s = (const uint8_t *)text;
    uint8_t c = s[0];

    if (c < 0x80) {
        codePoint = c;
        return c ? text + 1 : text;
    }

#if UTF8_SUPPORT
    // the number of continuation bytes, the bits of the lead byte and the range
    // of the first continuation byte (without overlong forms, surrogates and
    // code points above U+10FFFF)
    int n;
    int32_t value;
    uint8_t min = 0x80;
    uint8_t max = 0xBF;
    if (c >= 0xC2 && c <= 0xDF) {
        n = 1;
        value = c & 0x1F;
    } else if (c >= 0xE0 && c <= 0xEF) {
        n = 2;
        value = c & 0x0F;
        if (c == 0xE0) {
            min = 0xA0;
        } else if (c == 0xED) {
            max = 0x9F;
        }
    } else if (c >= 0xF0 && c <= 0xF4) {
        n = 3;
        value = c & 0x07;
        if (c == 0xF0) {
            min = 0x90;
        } else if (c == 0xF4) {
            max = 0x8F;
        }
    } else {
        // a continuation byte or a byte that is never in UTF-8
        codePoint = 0xFFFD;
        return text + 1;
    }

    for (int k = 1; k <= n; k++) {
        // the bytes are checked one by one, so the NUL ends the sequence
        uint8_t b = s[k];
        if (b < min || b > max) {
            codePoint = 0xFFFD;
            return text + 1;
        }
        value = (value << 6) | (b & 0x3F);
        min = 0x80;
        max = 0xBF;
    }

    codePoint = value;
    return text + 1 + n;
#else
    codePoint = c;
    return text + 1;
#endif
}

const char *skipChar(const char *text) {
    int32_t encoding;
    const char *next = decodeUtf8(text, encoding);
    if (encoding == FONT_START_CHAR || encoding == FONT_START_EXTERNAL_CHAR) {
        next = decodeUtf8(next, encoding);
    }
    return next;
}

// Returns true if encoding is a font tag marker and applies it to font. In case
// of FONT_START_CHAR and FONT_START_EXTERNAL_CHAR the font index character is
// consumed from the text; such a tag at the end of the text (without the font
// index) changes nothing.
static bool handleFontTag(utf8_int32_t encoding, const char *&text, int &i, int textLength, gui::font::Font &defaultFont, gui::font::Font &font) {
    if (encoding == FONT_START_CHAR || encoding == FONT_START_EXTERNAL_CHAR) {
        if (textLength == -1 || i + 1 < textLength) {
            utf8_int32_t index;
            const char *next = decodeUtf8(text, index);
            if (index) {
                text = next;
                ++i;
                font = getFontByIndex(encoding == FONT_START_CHAR ? (int)index : -(int)index, defaultFont);
            }
        }
        return true;
    }
    if (encoding == FONT_END_CHAR) {
        font = defaultFont;
        return true;
    }
    return false;
}

// Returns font index (as in getFontData) that is active at the end of the
// text, 0 for default font.
int getFontIndexAtEnd(const char *text, int textLength, int fontIndex) {
    for (int i = 0; textLength == -1 || i < textLength; ++i) {
        utf8_int32_t encoding;
        text = decodeUtf8(text, encoding);
        if (!encoding) {
            break;
        }
        if (encoding == FONT_START_CHAR || encoding == FONT_START_EXTERNAL_CHAR) {
            // as in handleFontTag
            if (textLength == -1 || i + 1 < textLength) {
                utf8_int32_t index;
                const char *next = decodeUtf8(text, index);
                if (index) {
                    text = next;
                    ++i;
                    fontIndex = encoding == FONT_START_CHAR ? (int)index : -(int)index;
                }
            }
        } else if (encoding == FONT_END_CHAR) {
            fontIndex = 0;
        }
    }
    return fontIndex;
}

// A glyph of laid out text (see layoutText).
struct TextGlyph {
    // the index of the first character of the glyph, characters counted as by
    // the text functions: every code point, also of the font tags
    int charIndex;
    // the pen position, in pixels from the start of the text
    int x;
    // to the pen position of the next glyph
    int advance;
    // the 8-bit coverage image, nullptr if there is none (a space, a missing
    // glyph): width * height bytes, left from the pen position, top above the
    // baseline
    const uint8_t *pixels;
    int width;
    int height;
    int left;
    int top;
    // the font of the glyph (font tags change it)
    gui::font::Font font;
};

// The letter spacing of laid out text: added before every glyph with an
// advance, except the first one, so it is only between the characters.
struct LetterSpacing {
    int spacing; // 1/64 px
    bool started;

    // the pen (1/64 px) moved by the spacing before a glyph with this advance
    void beforeGlyph(int64_t &pen, int advance) {
        if (spacing != 0 && advance != 0) {
            if (started) {
                pen += spacing;
            }
            started = true;
        }
    }
};

#if EEZ_OPTION_GUI_VECTOR_FONTS
// The characters of the text at p in the vector font up to a font tag or the
// end, shaped. Calls onChar for every character, before the glyph it is in,
// and onGlyph for every glyph; i and the pen (1/64 px) are moved past the run.
template <typename OnChar, typename OnGlyph>
static bool layoutVectorRun(const char *&p, int &i, int textLength, int64_t &pen, LetterSpacing &letterSpacing, gui::font::Font &font, bool ligatures, OnChar &onChar, OnGlyph &onGlyph) {
    const char *runStart = p;
    int runFirstChar = i;

    // the byte offsets of the characters in the run
    uint32_t smallOffsets[64];
    std::vector<uint32_t> largeOffsets;
    uint32_t *charOffsets = smallOffsets;
    int numChars = 0;

    const char *q = p;
    while (textLength == -1 || i + numChars < textLength) {
        utf8_int32_t encoding;
        const char *next = decodeUtf8(q, encoding);
        if (!encoding || isFontTag(encoding) || encoding == DIGIT_GROUP_GAP_CHAR) {
            break;
        }
        if (numChars == 64) {
            largeOffsets.assign(smallOffsets, smallOffsets + 64);
        }
        if (numChars >= 64) {
            largeOffsets.push_back((uint32_t)(q - runStart));
            charOffsets = largeOffsets.data();
        } else {
            smallOffsets[numChars] = (uint32_t)(q - runStart);
        }
        numChars++;
        q = next;
    }

    p = q;
    i += numChars;

    uint32_t numGlyphs;
    auto glyphs = vector_font::shape(font.getVectorFontData(), runStart, (int)(q - runStart), ligatures, numGlyphs);

    // a glyph is at the rounded pen position

    // characters before this one are announced (the glyphs are in visual order,
    // in left to right text the clusters grow)
    int nextChar = 0;

    for (uint32_t k = 0; k < numGlyphs; k++) {
        auto &shapedGlyph = glyphs[k];

        int charInRun = nextChar;
        while (charInRun < numChars && charOffsets[charInRun] < shapedGlyph.cluster) {
            charInRun++;
        }

        letterSpacing.beforeGlyph(pen, shapedGlyph.advance);

        int glyphX = (int)((pen + 32) >> 6);
        while (nextChar < numChars && charOffsets[nextChar] <= shapedGlyph.cluster) {
            if (onChar(runFirstChar + nextChar, glyphX)) {
                return true;
            }
            nextChar++;
        }

        int64_t penNext = pen + shapedGlyph.advance;

        TextGlyph glyph;
        glyph.charIndex = runFirstChar + (charInRun < numChars ? charInRun : numChars - 1);
        glyph.x = glyphX;
        glyph.advance = (int)((penNext + 32) >> 6) - glyphX;
        glyph.font = font;

        auto image = vector_font::getGlyphImage(font.getVectorFontData(), shapedGlyph.glyphIndex);
        if (image) {
            glyph.pixels = image->pixels;
            glyph.width = image->width;
            glyph.height = image->height;
            glyph.left = image->left + (int)((shapedGlyph.xOffset + 32) >> 6);
            glyph.top = image->top + (int)((shapedGlyph.yOffset + 32) >> 6);
        } else {
            glyph.pixels = nullptr;
            glyph.width = 0;
            glyph.height = 0;
            glyph.left = 0;
            glyph.top = 0;
        }

        if (onGlyph(glyph)) {
            return true;
        }

        pen = penNext;
    }

    // the run ends on a whole pixel
    int x = (int)((pen + 32) >> 6);
    pen = (int64_t)x * 64;

    // the characters after the last glyph (no glyphs, right to left text)
    while (nextChar < numChars) {
        if (onChar(runFirstChar + nextChar, x)) {
            return true;
        }
        nextChar++;
    }

    return false;
}
#endif

// Lays out the text (textLength characters, -1: up to the NUL) in the font,
// from the font of startFontIndex if it isn't 0. Calls onChar(charIndex, x)
// for every character (the font tags too) before its glyph, and at the end with
// the number of characters, and onGlyph(glyph) for every glyph (also a missing
// glyph of a bitmap font, with advance 0). A callback that returns true stops
// the layout. Without ligatures every glyph of a vector font is one character
// (editable text). The letter spacing of the default font is between all the
// characters. Any thread can call it.
template <typename OnChar, typename OnGlyph>
static void layoutText(const char *text, int textLength, gui::font::Font &defaultFont, int startFontIndex, bool ligatures, OnChar onChar, OnGlyph onGlyph) {
#if EEZ_OPTION_GUI_VECTOR_FONTS
    vector_font::Lock lock;
#endif

    gui::font::Font font = startFontIndex ? getFontByIndex(startFontIndex, defaultFont) : defaultFont;

    // the pen in 1/64 px, a glyph is at the rounded pen position
    int64_t pen = 0;
    LetterSpacing letterSpacing = { defaultFont.letterSpacing, false };

    int i = 0;

    while (textLength == -1 || i < textLength) {
        utf8_int32_t encoding;
        const char *next = decodeUtf8(text, encoding);
        if (!encoding) {
            break;
        }

        if (encoding == DIGIT_GROUP_GAP_CHAR) {
            text = next;

            int x = (int)((pen + 32) >> 6);

            if (onChar(i, x)) {
                return;
            }

            TextGlyph glyph;
            glyph.charIndex = i;
            glyph.x = x;
            glyph.advance = getDigitGroupGapWidth(font);
            glyph.pixels = nullptr;
            glyph.width = 0;
            glyph.height = 0;
            glyph.left = 0;
            glyph.top = 0;
            glyph.font = font;

            if (onGlyph(glyph)) {
                return;
            }

            pen = (int64_t)(x + glyph.advance) * 64;
            ++i;
            continue;
        }

#if EEZ_OPTION_GUI_VECTOR_FONTS
        if (font.isVector() && !isFontTag(encoding)) {
            if (layoutVectorRun(text, i, textLength, pen, letterSpacing, font, ligatures, onChar, onGlyph)) {
                return;
            }
            continue;
        }
#endif

        text = next;

        if (isFontTag(encoding)) {
            if (onChar(i, (int)((pen + 32) >> 6))) {
                return;
            }
            handleFontTag(encoding, text, i, textLength, defaultFont, font);
            ++i;
            continue;
        }

        auto glyphData = font.getGlyph(encoding);

        letterSpacing.beforeGlyph(pen, glyphData ? glyphData->dx : 0);

        int x = (int)((pen + 32) >> 6);

        if (onChar(i, x)) {
            return;
        }

        TextGlyph glyph;
        glyph.charIndex = i;
        glyph.x = x;
        glyph.font = font;

        if (glyphData) {
            glyph.advance = glyphData->dx;
            glyph.pixels = glyphData->pixels;
            glyph.width = glyphData->width;
            glyph.height = glyphData->height;
            glyph.left = glyphData->x;
            glyph.top = glyphData->y + glyphData->height;
        } else {
            glyph.advance = 0;
            glyph.pixels = nullptr;
            glyph.width = 0;
            glyph.height = 0;
            glyph.left = 0;
            glyph.top = 0;
        }

        if (onGlyph(glyph)) {
            return;
        }

        pen += (int64_t)glyph.advance * 64;
        ++i;
    }

    onChar(i, (int)((pen + 32) >> 6));
}

int getDigitGroupGapWidth(gui::font::Font &font) {
    // about a fifth of the size of the font, as a thin space
    return font ? MAX((font.getHeight() + 3) / 6, 1) : 0;
}

void groupDecimals(char *text, size_t size) {
    // the decimals: the digits after the first point that is after a digit
    size_t first = 0;
    while (text[first] && !(text[first] == '.' && first > 0 && text[first - 1] >= '0' && text[first - 1] <= '9')) {
        first++;
    }
    if (!text[first]) {
        return;
    }
    first++;

    size_t numDecimals = 0;
    while (text[first + numDecimals] >= '0' && text[first + numDecimals] <= '9') {
        numDecimals++;
    }
    if (numDecimals <= 4) {
        return;
    }

    size_t numGaps = (numDecimals - 1) / 3;
    size_t length = strlen(text);
    if (length + numGaps + 1 > size) {
        return;
    }

    // from the end, so every character is moved once
    size_t from = length;
    size_t to = length + numGaps;
    text[to] = 0;
    while (numGaps > 0) {
        text[--to] = text[--from];
        if (from > first && from < first + numDecimals && (from - first) % 3 == 0) {
            text[--to] = DIGIT_GROUP_GAP_CHAR;
            numGaps--;
        }
    }
}

int8_t measureGlyph(int32_t encoding, gui::font::Font &font) {
    if (!font) {
        return 0;
    }

    if (encoding == DIGIT_GROUP_GAP_CHAR) {
        return (int8_t)getDigitGroupGapWidth(font);
    }

    if (!font.isVector()) {
        auto glyph = font.getGlyph(encoding);
        if (!glyph) {
            return 0;
        }
        return glyph->dx;
    }

    char text[5];
    char *end = utf8catcodepoint(text, encoding, sizeof(text) - 1);
    *(end ? end : text) = 0;
    return (int8_t)measureStr(text, -1, font);
}

int measureStr(const char *text, int textLength, gui::font::Font &font, int max_width, int startFontIndex) {
    int width = 0;

    layoutText(text, textLength, font, startFontIndex, true,
        [](int, int) { return false; },
        [&](const TextGlyph &glyph) {
            if (max_width > 0 && glyph.x + glyph.advance > max_width) {
                width = max_width;
                return true;
            }
            width = glyph.x + glyph.advance;
            return false;
        }
    );

    return width;
}

int measureStrInkAbove(const char *text, int textLength, gui::font::Font &font) {
    int inkAbove = 0;

    layoutText(text, textLength, font, 0, true,
        [](int, int) { return false; },
        [&](const TextGlyph &glyph) {
            if (glyph.pixels && glyph.top > inkAbove) {
                inkAbove = glyph.top;
            }
            return false;
        }
    );

    return inkAbove;
}

void drawStr(const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2, int clip_y2, gui::font::Font &font, int cursorPosition, int startFontIndex) {
    drawStrInit();

    int xCursor = x;

    // glyphs are clipped to the given clip rectangle and to the current clip rectangle
    int glyph_clip_x1 = clip_x1;
    int glyph_clip_y1 = clip_y1;
    int glyph_clip_x2 = clip_x2;
    int glyph_clip_y2 = clip_y2;
    if (!clipToClipRect(glyph_clip_x1, glyph_clip_y1, glyph_clip_x2, glyph_clip_y2)) {
        return;
    }

    // editable text (with a cursor) has no ligatures
    layoutText(text, textLength, font, startFontIndex, cursorPosition == -1,
        [&](int charIndex, int charX) {
            if (charIndex == cursorPosition) {
                xCursor = x + charX;
            }
            return false;
        },
        [&](const TextGlyph &glyph) {
            if (!glyph.pixels) {
                return false;
            }

            int x_glyph = x + glyph.x + glyph.left;
            int y_glyph = y + glyph.font.getAscent() - glyph.top;

            // draw glyph pixels
            int iStartByte = 0;
            if (x_glyph < glyph_clip_x1) {
                int dx_off = glyph_clip_x1 - x_glyph;
                iStartByte = dx_off;
                x_glyph = glyph_clip_x1;
            }

            if (iStartByte < glyph.width) {
                int offset = 0;
                int glyphHeight = glyph.height;
                if (y_glyph < glyph_clip_y1) {
                    int dy_off = glyph_clip_y1 - y_glyph;
                    offset += dy_off * glyph.width;
                    glyphHeight -= dy_off;
                    y_glyph = glyph_clip_y1;
                }

                int width;
                if (x_glyph + (glyph.width - iStartByte) - 1 > glyph_clip_x2) {
                    width = glyph_clip_x2 - x_glyph + 1;
                } else {
                    width = (glyph.width - iStartByte);
                }

                int height;
                if (y_glyph + glyphHeight - 1 > glyph_clip_y2) {
                    height = glyph_clip_y2 - y_glyph + 1;
                } else {
                    height = glyphHeight;
                }

                if (width > 0 && height > 0) {
                    drawGlyph(glyph.pixels + offset + iStartByte, glyph.width - width, x_glyph, y_glyph, width, height);
                }
            }

            return false;
        }
    );

    if (cursorPosition != -1 && xCursor - CURSOR_WIDTH / 2 >= clip_x1 && xCursor + CURSOR_WIDTH / 2 - 1 <= clip_x2) {
        auto d = MAX(((clip_y2 - clip_y1) - font.getHeight()) / 2, 0);
        fillRect(xCursor - CURSOR_WIDTH / 2, clip_y1 + d, xCursor + CURSOR_WIDTH / 2 - 1, clip_y2 - d);
    }

    setDirty();
}

int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font) {
    int result = 0;

    layoutText(text, textLength, font, 0, false,
        [&](int charIndex, int) {
            // the number of characters at the end
            result = charIndex;
            return false;
        },
        [&](const TextGlyph &glyph) {
            if (xPos < x + glyph.x + glyph.advance / 2) {
                result = glyph.charIndex;
                return true;
            }
            return false;
        }
    );

    return result;
}

int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font) {
    int result = x;

    layoutText(text, textLength, font, 0, false,
        [&](int charIndex, int charX) {
            result = x + charX;
            return charIndex == cursorPosition;
        },
        [](const TextGlyph &) { return false; }
    );

    return result;
}

#if EEZ_USE_SDL
#if !defined(__EMSCRIPTEN__)
static SDL_Window *g_mainWindow;
static SDL_Renderer *g_renderer;
static SDL_Texture *g_texture;
static bool g_textureInitialized;
#endif
#endif

#if defined(EEZ_PLATFORM_STM32)
#define DMA2D_WAIT if (g_waitDMA) { while (HAL_DMA2D_PollForTransfer(&hdma2d, 1000) != HAL_OK); g_waitDMA = false; }

static bool g_waitDMA;
#endif

static void initDriver() {
#if EEZ_USE_SDL

#if EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL video could not initialize! SDL Error: %s\n", SDL_GetError());
        return;
    }

    // Set texture filtering to linear
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        printf("Warning: Linear texture filtering not enabled!");
    }

    // Create window
#if defined(EEZ_PLATFORM_CM5_RASPI)
    g_mainWindow = SDL_CreateWindow(TITLE, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS);
#else
    g_mainWindow = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_HIDDEN);
#endif

    if (g_mainWindow == NULL) {
        printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }

// #if defined(EEZ_PLATFORM_CM5_RASPI)
//     SDL_SetWindowPosition(g_mainWindow, 0, 0);
// #endif

    // Create renderer
    g_renderer = SDL_CreateRenderer(g_mainWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) {
        // e.g. the video driver "dummy" (BB3+ tests without a window)
        g_renderer = SDL_CreateRenderer(g_mainWindow, -1, SDL_RENDERER_SOFTWARE);
    }
    if (g_renderer == NULL) {
		g_mainWindow = NULL;
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }

    SDL_RendererInfo rendererInfo;
    if (SDL_GetRendererInfo(g_renderer, &rendererInfo) == 0) {
        printf("SDL renderer: %s (%s)\n", rendererInfo.name, (rendererInfo.flags & SDL_RENDERER_PRESENTVSYNC) ? "VSYNC enabled" : "VSYNC unavailable");
    }

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (g_texture == NULL) {
        printf("Streaming texture could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }
    SDL_SetTextureBlendMode(g_texture, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    SDL_ShowWindow(g_mainWindow);

#if OPTION_MOUSE
    if (mouse::isMouseEnabled()) {
        SDL_ShowCursor(SDL_DISABLE);
        SDL_CaptureMouse(SDL_TRUE);
    }
#endif
#endif

#endif

#if defined(EEZ_PLATFORM_STM32)
    __HAL_RCC_DMA2D_CLK_ENABLE();
#endif

#if defined(EEZ_NEMA_GFX)
    nema_hal_stm32_init();
#endif
}

#if EEZ_USE_SDL && !EEZ_EMSCRIPTEN_NO_THREADS
// The shortest time between two frames. The next VSYNC message is queued
// right after the frame is presented: when the present doesn't wait for the
// vertical sync (a renderer without vsync, a hidden window) the GUI loop would
// run the frames one after the other without a pause.
#define CONF_MIN_FRAME_INTERVAL_MS 16

static uint32_t g_lastFrameTime;

static void waitForFrameInterval() {
    uint32_t elapsed = millis() - g_lastFrameTime;
    if (elapsed < CONF_MIN_FRAME_INTERVAL_MS) {
        sleepMs(CONF_MIN_FRAME_INTERVAL_MS - elapsed);
    }
    g_lastFrameTime = millis();
}
#endif

#if EEZ_USE_SDL && defined(__EMSCRIPTEN__)
static std::atomic<VideoBuffer> g_presentedFrame;
static std::atomic<uint32_t> g_presentedFrameCounter;

VideoBuffer getPresentedFrame(uint32_t &counter) {
    counter = g_presentedFrameCounter;
    return g_presentedFrame;
}
#endif

static void syncBuffer() {
#if EEZ_USE_SDL

#if EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
    if (!g_mainWindow || !g_texture) {
		return;
    }

    if (!g_textureInitialized) {
        g_presentNeeded = true;
    }

    // the frame is uploaded only when it changed, if that fails it is uploaded with the next frame
    if (g_presentNeeded) {
        if (SDL_UpdateTexture(g_texture, nullptr, g_syncedBuffer, DISPLAY_WIDTH * sizeof(uint32_t)) == 0) {
            g_textureInitialized = true;
            g_presentNeeded = false;
        } else {
            printf("Unable to update streaming texture! SDL Error: %s\n", SDL_GetError());
        }
    }

    if (g_textureInitialized && SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr) != 0) {
        printf("Unable to render streaming texture! SDL Error: %s\n", SDL_GetError());
    }
    SDL_RenderPresent(g_renderer);
#endif

#if defined(__EMSCRIPTEN__)
    // no window here: the web page draws the frame it gets with getPresentedFrame
    if (g_presentNeeded) {
        g_presentedFrame = g_syncedBuffer;
        g_presentedFrameCounter++;
        g_presentNeeded = false;
    }
#endif

#if !EEZ_EMSCRIPTEN_NO_THREADS
    waitForFrameInterval();
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC, 0, 0);
#endif

    touch::tick();

#endif

#if defined(EEZ_PLATFORM_STM32)

    DMA2D_WAIT;
#ifdef EEZ_CONF_DCACHE_ENABLED
    SCB_CleanDCache();
#endif
    static const uint32_t LINE_INTERRUPT_POSITION = (LTDC->AWCR & 0x7FF) - 1;
    HAL_LTDC_ProgramLineEvent(&hltdc, LINE_INTERRUPT_POSITION);
#endif
}    

#if defined(EEZ_PLATFORM_STM32)
static void bitBltRGB888(uint16_t *src, uint8_t *dst, int x, int y, int width, int height);
#endif

static void copySyncedBufferToScreenshotBuffer() {
#if EEZ_USE_SDL

    // the rectangle of the device app context (in the simulator a part of the
    // window)
    auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    int x1 = MAX(appContext->rect.x, 0);
    int y1 = MAX(appContext->rect.y, 0);
    int x2 = MIN(appContext->rect.x + appContext->rect.w, (int)DISPLAY_WIDTH);
    int y2 = MIN(appContext->rect.y + appContext->rect.h, (int)DISPLAY_HEIGHT);
    g_screenshotWidth = MAX(x2 - x1, 0);
    g_screenshotHeight = MAX(y2 - y1, 0);

    // the presented frame (the render buffer before the first frame)
    VideoBuffer frame = g_syncedBuffer ? g_syncedBuffer : g_renderBuffer;
    uint8_t *dst = g_screenshotBuffer;

    for (int y = 0; y < g_screenshotHeight; y++) {
        uint8_t *src = (uint8_t *)(frame + (y1 + y) * DISPLAY_WIDTH + x1);
        for (int x = 0; x < g_screenshotWidth; x++) {
            uint8_t r = *src++;
            uint8_t g = *src++;
            uint8_t b = *src++;
            src++;

            *dst++ = r;
            *dst++ = g;
            *dst++ = b;
        }
    }

#endif

#if defined(EEZ_PLATFORM_STM32)

    g_screenshotWidth = DISPLAY_WIDTH;
    g_screenshotHeight = DISPLAY_HEIGHT;
    bitBltRGB888(g_syncedBuffer, g_screenshotBuffer, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    DMA2D_WAIT;

#endif
}

#if defined(EEZ_PLATFORM_STM32)

inline uint32_t vramOffset(uint16_t *vram, int x, int y) {
#if DISPLAY_BPP == 32
    return (uint32_t)((uint32_t *)vram + y * DISPLAY_WIDTH + x);
#elif DISPLAY_BPP == 24
    return (uint32_t)((uint8_t *)vram + (y * DISPLAY_WIDTH + x) * (DISPLAY_BPP / 8));
#else
    return (uint32_t)(vram + y * DISPLAY_WIDTH + x);
#endif
}

inline uint32_t vramOffsetRGB888(uint8_t *vram, int x, int y) {
    return (uint32_t)(vram + (y * DISPLAY_WIDTH  + x) * 3);
}

inline uint32_t vramOffset(uint32_t *vram, int x, int y) {
    return (uint32_t)(vram + y * DISPLAY_WIDTH + x);
}

#if DISPLAY_BPP == 32
    #define DMA2D_DISPLAY_OUTPUT_MODE DMA2D_OUTPUT_ARGB8888
#elif DISPLAY_BPP == 24
    #define DMA2D_DISPLAY_OUTPUT_MODE DMA2D_OUTPUT_RGB888
#else
    #define DMA2D_DISPLAY_OUTPUT_MODE DMA2D_OUTPUT_RGB565
#endif

#if DISPLAY_BPP == 32
    #define DMA2D_DISPLAY_INPUT_MODE DMA2D_INPUT_ARGB8888
#elif DISPLAY_BPP == 24
    #define DMA2D_DISPLAY_INPUT_MODE DMA2D_INPUT_RGB888
#else
    #define DMA2D_DISPLAY_INPUT_MODE DMA2D_INPUT_RGB565
#endif

void fillRect(uint16_t *dst, int x, int y, int width, int height, Color color) {
    if (g_opacity == 255) {
        hdma2d.Init.Mode = DMA2D_R2M;
        hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
        hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

        uint32_t colorBGRA;
        uint8_t *pcolorBGRA = (uint8_t *)&colorBGRA;
        pcolorBGRA[0] = COLOR_TO_B(color);
        pcolorBGRA[1] = COLOR_TO_G(color);
        pcolorBGRA[2] = COLOR_TO_R(color);
        pcolorBGRA[3] = 255;

        DMA2D_WAIT;
        HAL_DMA2D_Init(&hdma2d);
        HAL_DMA2D_Start(&hdma2d, colorBGRA, vramOffset(dst, x, y), width, height);
        g_waitDMA = true;
    } else {
        // fill aux. buffer with BGRA color
        uint32_t colorBGRA;
        uint8_t *pcolorBGRA = (uint8_t *)&colorBGRA;
        pcolorBGRA[0] = COLOR_TO_B(color);
        pcolorBGRA[1] = COLOR_TO_G(color);
        pcolorBGRA[2] = COLOR_TO_R(color);
        pcolorBGRA[3] = g_opacity;

        // blend aux. buffer with dst buffer
        hdma2d.Init.Mode = DMA2D_M2M_BLEND;
        hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
        hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

        hdma2d.LayerCfg[0].InputOffset = DISPLAY_WIDTH - width;
        hdma2d.LayerCfg[0].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
        hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[0].InputAlpha = 0;

        hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_A8;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_REPLACE_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = colorBGRA;

        auto dstOffset = vramOffset(dst, x, y);

        DMA2D_WAIT;
        HAL_DMA2D_Init(&hdma2d);
        HAL_DMA2D_ConfigLayer(&hdma2d, 1);
        HAL_DMA2D_ConfigLayer(&hdma2d, 0);
        HAL_DMA2D_BlendingStart(&hdma2d, dstOffset, dstOffset, dstOffset, width, height);
        g_waitDMA = true;
    }
}

void bitBlt(void *src, int srcBpp, uint32_t srcLineOffset, uint16_t *dst, int x, int y, int width, int height) {
    if (srcBpp == 32) {
        hdma2d.Init.Mode = DMA2D_M2M_BLEND;
        hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
        hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

        hdma2d.LayerCfg[0].InputOffset = DISPLAY_WIDTH - width;
        hdma2d.LayerCfg[0].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
        hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[0].InputAlpha = 0;

        hdma2d.LayerCfg[1].InputOffset = srcLineOffset;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = 0;
    } else if (srcBpp == 24) {
#if DMA2D_DISPLAY_OUTPUT_MODE == DMA2D_OUTPUT_RGB888
        hdma2d.Init.Mode = DMA2D_M2M;
#else
        hdma2d.Init.Mode = DMA2D_M2M_PFC;
#endif
        hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
        hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;        
#ifndef EEZ_PLATFORM_STM32F469I_DISCO
        hdma2d.Init.RedBlueSwap = DMA2D_RB_SWAP;
#endif

        hdma2d.LayerCfg[1].InputOffset = srcLineOffset;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB888;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = 0;
    } else {
#if DMA2D_DISPLAY_OUTPUT_MODE == DMA2D_INPUT_RGB565
        hdma2d.Init.Mode = DMA2D_M2M;
#else
        hdma2d.Init.Mode = DMA2D_M2M_PFC;
#endif
        hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
        hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

        hdma2d.LayerCfg[1].InputOffset = srcLineOffset;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = 0;
    }

    auto dstOffset = vramOffset(dst, x, y);

    DMA2D_WAIT;
    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    if (srcBpp == 32) {
        HAL_DMA2D_ConfigLayer(&hdma2d, 0);
        HAL_DMA2D_BlendingStart(&hdma2d, (uint32_t)src, dstOffset, dstOffset, width, height);
    } else {
        HAL_DMA2D_Start(&hdma2d, (uint32_t)src, dstOffset, width, height);
    }
    g_waitDMA = true;

#ifndef EEZ_PLATFORM_STM32F469I_DISCO
    if (srcBpp == 24) {
        hdma2d.Init.RedBlueSwap = DMA2D_RB_REGULAR;
    }
#endif
}

void bitBlt(uint16_t *src, uint16_t *dst, int x, int y, int width, int height) {
    hdma2d.Init.Mode = DMA2D_M2M;
    hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0;

    DMA2D_WAIT;
    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    HAL_DMA2D_Start(&hdma2d, vramOffset(src, x, y), vramOffset(dst, x, y), width, height);
    g_waitDMA = true;
}

static void bitBltRGB888(uint16_t *src, uint8_t *dst, int x, int y, int width, int height) {
    hdma2d.Init.Mode = DMA2D_M2M_PFC;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB888;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;
#ifndef EEZ_PLATFORM_STM32F469I_DISCO
    hdma2d.Init.RedBlueSwap = DMA2D_RB_SWAP;
#endif

    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0;

    DMA2D_WAIT;
    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    HAL_DMA2D_Start(&hdma2d, vramOffset(src, x, y), vramOffsetRGB888(dst, x, y), width, height);
    g_waitDMA = true;

#ifndef EEZ_PLATFORM_STM32F469I_DISCO
    hdma2d.Init.RedBlueSwap = DMA2D_RB_REGULAR;
#endif
}

void bitBlt(uint16_t *src, uint16_t *dst, int x, int y, int width, int height, int dstx, int dsty) {
    hdma2d.Init.Mode = DMA2D_M2M;
    hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0;

    DMA2D_WAIT;
    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    HAL_DMA2D_Start(&hdma2d, vramOffset(src, x, y), vramOffset(dst, dstx, dsty), width, height);
    g_waitDMA = true;
}

#endif

void getPixel(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
#if EEZ_USE_SDL
    uint8_t *dest = (uint8_t *)(g_renderBuffer + y * (int)DISPLAY_WIDTH + x);
    *r = dest[0];
    *g = dest[1];
    *b = dest[2];
#endif

#if defined(EEZ_PLATFORM_STM32)
    void *buff = (void *)vramOffset(g_renderBuffer, x, y);

    #if DISPLAY_BPP == 24 || DISPLAY_BPP == 32
        *r = COLOR_TO_R(((uint8_t *)buff)[0]);
        *g = COLOR_TO_G(((uint8_t *)buff)[1]);
        *b = COLOR_TO_B(((uint8_t *)buff)[2]);    
    #else
        *r = COLOR_TO_R(*((uint16_t *)buff));
        *g = COLOR_TO_G(*((uint16_t *)buff));
        *b = COLOR_TO_B(*((uint16_t *)buff));
    #endif
#endif
}


void startPixelsDraw() {
#if EEZ_USE_SDL
#endif

#if defined(EEZ_PLATFORM_STM32)
    DMA2D_WAIT;
#endif
}

void drawPixel(int x, int y) {
    if (!isInsideClipRect(x, y)) {
        return;
    }

    if (g_dryRun) {
        setDirty();
        return;
    }

#if EEZ_USE_SDL
    auto dest = g_renderBuffer + y * (int)DISPLAY_WIDTH + x;
    uint32_t opacity = getDrawOpacity(g_fc);
    if (opacity == 255) {
        *dest = g_fc;
    } else if (opacity != 0) {
        *dest = blendColor((g_fc & 0x00FFFFFF) | (opacity << 24), *dest);
    }
    addDamage(x, y, x, y);
    setDirty();
#endif

#if defined(EEZ_PLATFORM_STM32)
    void *buff = (void *)vramOffset(g_renderBuffer, x, y);

    #if DISPLAY_BPP == 32
        *((uint32_t *)buff) = g_fc;
    #elif DISPLAY_BPP == 24
        ((uint8_t *)buff)[0] = COLOR_TO_R(g_fc);
        ((uint8_t *)buff)[1] = COLOR_TO_G(g_fc);
        ((uint8_t *)buff)[2] = COLOR_TO_B(g_fc);
    #else    
        *((uint16_t *)buff) = g_fc;
    #endif
#endif
}

void drawPixel(int x, int y, uint8_t opacity) {
    if (!isInsideClipRect(x, y)) {
        return;
    }

    if (g_dryRun) {
        setDirty();
        return;
    }

#if EEZ_USE_SDL
    auto dest = g_renderBuffer + y * (int)DISPLAY_WIDTH + x;

    // draw g_fc with the given opacity (and its own) over the pixel
    uint32_t alpha = (opacity * getDrawOpacity(g_fc) + 127) / 255;
    *dest = blendColor((g_fc & 0x00FFFFFF) | (alpha << 24), *dest);
    addDamage(x, y, x, y);
    setDirty();
#endif

#if defined(EEZ_PLATFORM_STM32)
    void *buff = (void *)vramOffset(g_renderBuffer, x, y);
#if DISPLAY_BPP == 32
    uint32_t *dest = (uint32_t *)buff;
    uint32_t srcColor = (g_fc & 0x00FFFFFF) | (opacity << 24);
    uint32_t dstColor = (*dest & 0x00FFFFFF) | ((255 - opacity) << 24);
    *dest = blendColor(srcColor, dstColor);
#elif DISPLAY_BPP == 24
    uint8_t *dest = (uint8_t *)buff;
    uint32_t srcColor = (g_fc & 0x00FFFFFF) | (opacity << 24);
    uint32_t dstColor = (dest[0] | (dest[1] << 8) | (dest[2] << 16)) | ((255 - opacity) << 24);
    uint32_t c = blendColor(srcColor, dstColor);

    dest[0] = c & 0xFF;
    dest[1] = (c >> 8) & 0xFF;
    dest[2] = (c >> 16) & 0xFF;
#else
    *((uint16_t *)buff) = color32to16(
        blendColor(
            color16to32(g_fc, opacity),
            color16to32(*((uint16_t *)buff), 255 - opacity)
        )
    );
#endif

#endif    
}

void endPixelsDraw() {
    setDirty();
}

void fillRect(int x1, int y1, int x2, int y2) {
    if (!clipToClipRect(x1, y1, x2, y2)) {
        return;
    }

    if (g_dryRun) {
        setDirty();
        return;
    }

    addDamage(x1, y1, x2, y2);

#if EEZ_USE_SDL
	uint32_t *dst = g_renderBuffer + y1 * (int)DISPLAY_WIDTH + x1;
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;
    uint32_t opacity = getDrawOpacity(g_fc);
    if (opacity == 255) {
        for (int y = 0; y < height; y++, dst += DISPLAY_WIDTH) {
            std::fill_n(dst, width, g_fc);
        }
    } else if (opacity != 0) {
        uint32_t fc = (g_fc & 0x00FFFFFF) | (opacity << 24);
        for (int y = 0; y < height; y++, dst += DISPLAY_WIDTH) {
            for (int x = 0; x < width; x++) {
                dst[x] = blendColor(fc, dst[x]);
            }
        }
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    auto width = x2 - x1 + 1;
    if (width <= 0) {
        return;
    }

    auto height = y2 - y1 + 1;
    if (height <= 0) {
        return;
    }

    fillRect(g_renderBuffer, x1, y1, width, height, g_fc);
#endif    

    setDirty();
}

void fillRect(void *dstBuffer, int x1, int y1, int x2, int y2) {
    if (g_dryRun) {
        setDirty();
        return;
    }

    if (dstBuffer == g_renderBuffer) {
        addDamage(x1, y1, x2, y2);
    }

#if EEZ_USE_SDL
    uint32_t *dst = (uint32_t *)dstBuffer + y1 * (int)DISPLAY_WIDTH + x1;
    int width = x2 - x1 + 1;
    if (width > 0) {
        for (int y = y1; y <= y2; y++) {
            std::fill_n(dst, width, g_fc);
            dst += DISPLAY_WIDTH;
        }
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    fillRect((uint16_t *)dstBuffer, x1, y1, x2 - x1 + 1, y2 - y1 + 1, g_fc);
#endif    

    setDirty();
}

void bitBlt(int x1, int y1, int x2, int y2, int dstx, int dsty) {
    // clip destination rectangle and move source rectangle along
    int dx1 = dstx;
    int dy1 = dsty;
    int dx2 = dstx + x2 - x1;
    int dy2 = dsty + y2 - y1;
    if (!clipToClipRect(dx1, dy1, dx2, dy2)) {
        return;
    }
    if (g_dryRun) {
        setDirty();
        return;
    }

    x1 += dx1 - dstx;
    y1 += dy1 - dsty;
    x2 = x1 + dx2 - dx1;
    y2 = y1 + dy2 - dy1;
    dstx = dx1;
    dsty = dy1;

    addDamage(dx1, dy1, dx2, dy2);

#if EEZ_USE_SDL
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;

    // source and destination are in the same buffer and can overlap,
    // so copy rows bottom-up when moving down and use memmove within a row
    int rowStep = dsty > y1 ? -(int)DISPLAY_WIDTH : (int)DISPLAY_WIDTH;
    int firstRow = dsty > y1 ? height - 1 : 0;
    uint32_t *src = g_renderBuffer + (y1 + firstRow) * (int)DISPLAY_WIDTH + x1;
    uint32_t *dst = g_renderBuffer + (dsty + firstRow) * (int)DISPLAY_WIDTH + dstx;

    for (int y = 0; y < height; y++, src += rowStep, dst += rowStep) {
        memmove(dst, src, width * sizeof(uint32_t));
        for (int x = 0; x < width; x++) {
            dst[x] |= 0xFF000000;
        }
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    bitBlt(g_renderBuffer, g_renderBuffer, x1, y1, x2-x1+1, y2-y1+1, dstx, dsty);
#endif    

    setDirty();
}

void bitBlt(void *src, int x1, int y1, int x2, int y2) {
    bitBlt(src, g_renderBuffer, x1, y1, x2, y2);
    setDirty();
}

void bitBlt(void *src, void *dst, int x1, int y1, int x2, int y2) {
    if (g_dryRun) {
        setDirty();
        return;
    }

    if (dst == g_renderBuffer) {
        addDamage(x1, y1, x2, y2);
    }

#if EEZ_USE_SDL
    if (src != dst) {
        int width = x2 - x1 + 1;
        if (width > 0) {
            uint32_t *srcRow = (uint32_t *)src + y1 * (int)DISPLAY_WIDTH + x1;
            uint32_t *dstRow = (uint32_t *)dst + y1 * (int)DISPLAY_WIDTH + x1;
            for (int y = y1; y <= y2; y++) {
                memcpy(dstRow, srcRow, width * sizeof(uint32_t));
                srcRow += DISPLAY_WIDTH;
                dstRow += DISPLAY_WIDTH;
            }
        }
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    bitBlt((uint16_t *)src, (uint16_t *)dst, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
#endif    

    setDirty();
}

void bitBlt(void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, uint8_t opacity) {
    if (g_dryRun) {
        setDirty();
        return;
    }

    if (dst == nullptr) {
        dst = g_renderBuffer;
    }

    if (dst == g_renderBuffer) {
        addDamage(dx, dy, dx + sw - 1, dy + sh - 1);
    }

#if EEZ_USE_SDL
    if (sw > 0 && sh > 0 && opacity != 0 && !(src == dst && sx == dx && sy == dy)) {
        // when copying within the same buffer, go bottom-up if moving down so the rows don't overlap
        bool bottomUp = src == dst && dy > sy;
        int rowStep = bottomUp ? -(int)DISPLAY_WIDTH : (int)DISPLAY_WIDTH;
        int firstRow = bottomUp ? sh - 1 : 0;
        const uint32_t *srcRow = (const uint32_t *)src + (sy + firstRow) * (int)DISPLAY_WIDTH + sx;
        uint32_t *dstRow = (uint32_t *)dst + (dy + firstRow) * (int)DISPLAY_WIDTH + dx;

        if (opacity == 255) {
            for (int y = 0; y < sh; y++, srcRow += rowStep, dstRow += rowStep) {
                memmove(dstRow, srcRow, sw * sizeof(uint32_t));
            }
        } else {
            uint32_t alpha = (uint32_t)opacity << 24;
            for (int y = 0; y < sh; y++, srcRow += rowStep, dstRow += rowStep) {
                for (int x = 0; x < sw; x++) {
                    dstRow[x] = blendColor((srcRow[x] & 0x00FFFFFF) | alpha, dstRow[x]);
                }
            }
        }
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    auto srcOffset = vramOffset((uint16_t *)src, sx, sy);
    auto dstOffset = vramOffset((uint16_t *)dst, dx, dy);

    if (opacity == 255) {
        hdma2d.Init.Mode = DMA2D_M2M;
        hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
        hdma2d.Init.OutputOffset = DISPLAY_WIDTH - sw;

        hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - sw;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = 0;

        DMA2D_WAIT;
        HAL_DMA2D_Init(&hdma2d);
        HAL_DMA2D_ConfigLayer(&hdma2d, 1);
        HAL_DMA2D_Start(&hdma2d, srcOffset, dstOffset, sw, sh);
        g_waitDMA = true;
    } else {
        hdma2d.Init.Mode = DMA2D_M2M_BLEND;
        hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
        hdma2d.Init.OutputOffset = DISPLAY_WIDTH - sw;

        hdma2d.LayerCfg[0].InputOffset = DISPLAY_WIDTH - sw;
        hdma2d.LayerCfg[0].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
        hdma2d.LayerCfg[0].AlphaMode = DMA2D_COMBINE_ALPHA;
        hdma2d.LayerCfg[0].InputAlpha = 0xFF;

        hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - sw;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_COMBINE_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = opacity;

        DMA2D_WAIT;
        HAL_DMA2D_Init(&hdma2d);
        HAL_DMA2D_ConfigLayer(&hdma2d, 1);
        HAL_DMA2D_ConfigLayer(&hdma2d, 0);
        HAL_DMA2D_BlendingStart(&hdma2d, srcOffset, dstOffset, dstOffset, sw, sh);
        g_waitDMA = true;
    }
#endif    

    setDirty();
}

void drawBitmap(Image *image, int x, int y) {
    int x1 = x;
    int y1 = y;
    int x2 = x + (int)image->width - 1;
    int y2 = y + (int)image->height - 1;
    if (!clipToClipRect(x1, y1, x2, y2)) {
        return;
    }

    if (g_dryRun) {
        setDirty();
        return;
    }

    Image clippedImage;
    if (x1 != x || y1 != y || x2 != x + (int)image->width - 1 || y2 != y + (int)image->height - 1) {
        // draw only the visible part of the image
        int bytesPerPixel = image->bpp / 8;
        clippedImage.width = x2 - x1 + 1;
        clippedImage.height = y2 - y1 + 1;
        clippedImage.bpp = image->bpp;
        clippedImage.lineOffset = image->lineOffset + (image->width - clippedImage.width);
        clippedImage.pixels = image->pixels + ((y1 - y) * (image->width + image->lineOffset) + (x1 - x)) * bytesPerPixel;
        image = &clippedImage;
        x = x1;
        y = y1;
    }

    addDamage(x1, y1, x2, y2);

#if EEZ_USE_SDL
    uint32_t *dst = g_renderBuffer + y * (int)DISPLAY_WIDTH + x;
    int nlDst = DISPLAY_WIDTH - image->width;

    if (image->bpp == 32) {
        uint32_t *src = (uint32_t *)image->pixels;
        int nlSrc = image->lineOffset;

        uint32_t opacity = g_opacity;

        for (uint32_t *srcEnd = src + (image->width + nlSrc) * image->height; src != srcEnd; src += nlSrc, dst += nlDst) {
            if (opacity == 255) {
                for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src++, dst++) {
                    *dst = blendColor(*src, *dst);
                }
            } else {
                for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src++, dst++) {
                    uint32_t pixel = *src;
                    uint32_t alpha = (pixel >> 24) * opacity / 255;
                    *dst = blendColor((pixel & 0x00FFFFFF) | (alpha << 24), *dst);
                }
            }
        }
    } else if (image->bpp == 24) {
        uint8_t *src = (uint8_t *)image->pixels;
        int nlSrc = 3 * image->lineOffset;

        // a row is 3 * width bytes, then nlSrc (already in bytes) to the next row
        for (uint8_t *srcEnd = src + (3 * image->width + nlSrc) * image->height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src += 3, dst++) {
                *dst = 0xFF000000 | src[0] | (src[1] << 8) | (src[2] << 16);
            }
        }
    } else {
        uint16_t *src = (uint16_t *)image->pixels;
        int nlSrc = image->lineOffset;

        for (uint16_t *srcEnd = src + (image->width + nlSrc) * image->height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src++, dst++) {
                *dst = color16to32(*src);
            }
        }
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    bitBlt(image->pixels, image->bpp, image->lineOffset, g_renderBuffer, x, y, image->width, image->height);
#endif    

    setDirty();
}

static void drawStrInit() {
#if EEZ_USE_SDL
#endif

#if defined(EEZ_PLATFORM_STM32)
    auto color = g_fc;

    // initialize everything except lineOffset

    hdma2d.Init.Mode = DMA2D_M2M_BLEND;
    hdma2d.Init.ColorMode = DMA2D_DISPLAY_OUTPUT_MODE;
    hdma2d.Init.OutputOffset = 0;

    // background
    hdma2d.LayerCfg[0].InputColorMode = DMA2D_DISPLAY_INPUT_MODE;
    hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[0].InputAlpha = 0;
    hdma2d.LayerCfg[0].InputOffset = 0;

    // foreground
    uint32_t colorBGRA;
    uint8_t *pcolorBGRA = (uint8_t *)&colorBGRA;
    pcolorBGRA[0] = COLOR_TO_B(color);
    pcolorBGRA[1] = COLOR_TO_G(color);
    pcolorBGRA[2] = COLOR_TO_R(color);
    pcolorBGRA[3] = g_opacity;

    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_A8;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_COMBINE_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = colorBGRA;
    hdma2d.LayerCfg[1].InputOffset = 0;

    DMA2D_WAIT;
    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 0);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    g_waitDMA = true;
#endif    
}

static void drawGlyph(const uint8_t *src, uint32_t srcLineOffset, int x_glyph, int y_glyph, int width, int height) {
    if (g_dryRun) {
        setDirty();
        return;
    }

    addDamage(x_glyph, y_glyph, x_glyph + width - 1, y_glyph + height - 1);

#if EEZ_USE_SDL
    // glyph->pixels + offset + iStartByte, glyph->width - width, x_glyph, y_glyph, width,height
    // const gui::GlyphData &glyph, int x_glyph, int y_glyph, int width, int height, int offset, int iStartByte

    uint32_t color = g_fc & 0x00FFFFFF;
    uint32_t opacity = getDrawOpacity(g_fc);

    uint32_t *dst = g_renderBuffer + y_glyph * (int)DISPLAY_WIDTH + x_glyph;

    for (int y = 0; y < height; y++, src += width + srcLineOffset, dst += DISPLAY_WIDTH) {
        for (int x = 0; x < width; x++) {
            uint32_t alpha = src[x];
            if (alpha) {
                if (opacity != 255) {
                    alpha = alpha * opacity / 255;
                }
                dst[x] = blendColor(color | (alpha << 24), dst[x]);
            }
        }
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    uint32_t lineOffset = DISPLAY_WIDTH - width;
    uint32_t dst = vramOffset(g_renderBuffer, x_glyph, y_glyph);

    DMA2D_WAIT;
    // initialize lineOffset
    WRITE_REG(hdma2d.Instance->OOR, lineOffset);
    WRITE_REG(hdma2d.Instance->BGOR, lineOffset);
    WRITE_REG(hdma2d.Instance->FGOR, srcLineOffset);
    HAL_DMA2D_BlendingStart(&hdma2d, (uint32_t)src, dst, dst, width, height);
    g_waitDMA = true;
#endif    
}

void markRenderBufferDirty(void) {
    g_dirty = true;
}

bool isRenderBufferDirty(void) {
    return g_dirty;
}

void clearRenderBufferDirty(void) {
    g_dirty = false;
}

} // namespace display
} // namespace gui
} // namespace eez

#if defined(EEZ_PLATFORM_STM32)
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *phltdc) {
    using namespace eez::gui;
    using namespace eez::gui::display;

    LTDC_LAYER(phltdc, 0)->CFBAR = (uint32_t)g_syncedBuffer;
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(phltdc);

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC, 0, 0);
}
#endif

#if defined(__EMSCRIPTEN__)

EM_PORT_API(uint8_t*) getSyncedBuffer() {
    using namespace eez::gui;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC, 0, 0);
	return (uint8_t*)display::g_syncedBuffer;
}

#endif

#endif // EEZ_OPTION_GUI
