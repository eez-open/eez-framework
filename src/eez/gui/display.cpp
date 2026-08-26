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

#include <math.h>
#include <stdio.h>
#include <string.h>

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

#if defined(EEZ_NEMA_GFX)
#include <eez/gui/nema_hal_stm32.h>
#include <nema_graphics.h>
#include <nema_blender.h>
#include <tsi_malloc.h>
#endif

#define CONF_BACKDROP_OPACITY 128

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

#if defined(EEZ_PLATFORM_SIMULATOR) && EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
#include <SDL.h>
#include <SDL_image.h>
#include <string>
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
#else
// C: rrrrrggggggbbbbb
#define RGB_TO_COLOR(R, G, B) (Color(((R) << 0) | ((G) << 8) | ((B) << 16) | (255 << 24)))

#define COLOR_TO_R(C) (uint8_t((C) & 0xFF))
#define COLOR_TO_G(C) (uint8_t(((C) >> 8) & 0xFF))
#define COLOR_TO_B(C) (uint8_t(((C) >> 16) & 0xFF))
#endif

using namespace eez::gui;

namespace eez {
namespace gui {
namespace display {

struct RenderBuffer {
    VideoBuffer bufferPointer;
    VideoBuffer previousBuffer;
    int x;
    int y;
    int width;
    int height;
    bool withShadow;
    uint8_t opacity;
    int xOffset;
    int yOffset;
    gui::Rect *backdrop;
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

bool g_takeScreenshot;

Color g_fc, g_bc;
uint8_t g_opacity = 255;

gui::font::Font g_font;

static uint8_t g_colorCache[256][4];

#define FLOAT_TO_COLOR_COMPONENT(F) ((F) < 0 ? 0 : (F) > 255 ? 255 : (uint8_t)(F))
#define RGB_TO_HIGH_BYTE(R, G, B) (((R) & 248) | (G) >> 5)
#define RGB_TO_LOW_BYTE(R, G, B) (((G) & 28) << 3 | (B) >> 3)

static const uint16_t *g_themeColors;
static uint32_t g_themeColorsCount;
static const uint16_t *g_colors;

bool g_dirty;
inline void clearDirty() { g_dirty = false; }
inline void setDirty() { g_dirty = true; }
inline bool isDirty() { return g_dirty; }

static const int NUM_BUFFERS = 6;

RenderBuffer g_renderBuffers[NUM_BUFFERS];
static VideoBuffer g_mainBufferPointer;
static int g_numBuffersToDraw;

bool g_screenshotAllocated;

////////////////////////////////////////////////////////////////////////////////

static void initDriver();
static void syncBuffer();
static void copySyncedBufferToScreenshotBuffer();
static void drawStrInit();
static void drawGlyph(const uint8_t *src, uint32_t srcLineOffset, int x, int y, int width, int height);

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__) || (defined(EEZ_PLATFORM_STM32) && DISPLAY_BPP == 16)
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

static uint32_t blendColor(uint32_t fgColor, uint32_t bgColor) {
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

////////////////////////////////////////////////////////////////////////////////

void init() {
    onLuminocityChanged();
    onThemeChanged();

    g_renderBuffer1 = (VideoBuffer)VRAM_BUFFER1_START_ADDRESS;
    g_renderBuffer2 = (VideoBuffer)VRAM_BUFFER2_START_ADDRESS;

#if EEZ_OPTION_GUI_ANIMATIONS
    g_animationBuffer1 = (VideoBuffer)VRAM_ANIMATION_BUFFER1_START_ADDRESS;
    g_animationBuffer2 = (VideoBuffer)VRAM_ANIMATION_BUFFER2_START_ADDRESS;
#endif

    for (size_t i = 0; i < NUM_AUX_BUFFERS; i++) {
        g_renderBuffers[i].bufferPointer = (VideoBuffer)(VRAM_AUX_BUFFER_START_ADDRESSES[i]);
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
    syncBuffer();
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

#if !defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
		#if EEZ_OPTION_THREADS
        	osDelay(16);
        	sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC, 0, 0);
		#endif

        return;
#endif
    }

#ifdef GUI_CALC_FPS
	g_lastTimeFPS = millis();
#endif

    display::beginRendering();
    updateScreen();
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

            g_takeScreenshot = false;
            g_screenshotAllocated = true;
        }
#if EEZ_OPTION_GUI_ANIMATIONS
    }
#endif
}

const uint8_t *takeScreenshot() {
#if !EEZ_OPTION_THREADS || defined(__EMSCRIPTEN__)
    copySyncedBufferToScreenshotBuffer();
#else
    while (g_screenshotAllocated) {
    }

	g_takeScreenshot = true;

	do {
		osDelay(0);
	} while (g_takeScreenshot);

#endif

    return SCREENSHOOT_BUFFER_START_ADDRESS;
}

void releaseScreenshot() {
    g_screenshotAllocated = false;
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

    g_mainBufferPointer = getBufferPointer();
    g_numBuffersToDraw = 0;
}

static int g_maxNumBuffersToDraw = 0;

int beginBufferRendering() {
    int bufferIndex = g_numBuffersToDraw++;
    if (g_numBuffersToDraw > g_maxNumBuffersToDraw) {
        g_maxNumBuffersToDraw = g_numBuffersToDraw;
    }
	g_renderBuffers[bufferIndex].previousBuffer = getBufferPointer();
    setBufferPointer(g_renderBuffers[bufferIndex].bufferPointer);
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

void endBufferRendering(int bufferIndex, int x, int y, int width, int height, bool withShadow, uint8_t opacity, int xOffset, int yOffset, Rect *backdrop) {
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
}

void endRendering() {
    setBufferPointer(g_mainBufferPointer);

#if OPTION_KEYBOARD
    if (keyboard::isDisplayDirty()) {
    	setDirty();
    }
#endif

#if OPTION_MOUSE
    if (mouse::isDisplayDirty()) {
    	setDirty();
    }
#endif

#if defined(GUI_CALC_FPS)
    if (g_drawFpsGraphEnabled) {
	    setDirty();
    }
#endif

    if (isDirty()) {
        for (int bufferIndex = 0; bufferIndex < g_numBuffersToDraw; bufferIndex++) {
            RenderBuffer &renderBuffer = g_renderBuffers[bufferIndex];

            int sx = renderBuffer.x;
            int sy = renderBuffer.y;

            int x1 = renderBuffer.x + renderBuffer.xOffset;
            int y1 = renderBuffer.y + renderBuffer.yOffset;
            int x2 = x1 + renderBuffer.width - 1;
            int y2 = y1 + renderBuffer.height - 1;

            if (renderBuffer.backdrop) {
#if 0 && defined(EEZ_NEMA_GFX)
                blurBackdrop(renderBuffer.backdrop->x, renderBuffer.backdrop->y, renderBuffer.backdrop->w, renderBuffer.backdrop->h);
#else
                // opacity backdrop
                auto savedOpacity = setOpacity(CONF_BACKDROP_OPACITY);
                setColor(EEZ_COLOR_ID_BACKDROP);
                fillRect(renderBuffer.backdrop->x, renderBuffer.backdrop->y, renderBuffer.backdrop->x + renderBuffer.backdrop->w - 1, renderBuffer.backdrop->y + renderBuffer.backdrop->h - 1);
                setOpacity(savedOpacity);
#endif
            }

            if (renderBuffer.withShadow) {
                drawShadow(x1, y1, x2, y2);
            }

            bitBlt(g_renderBuffers[bufferIndex].bufferPointer, nullptr, sx, sy, x2 - x1 + 1, y2 - y1 + 1, x1, y1, renderBuffer.opacity);
        }

#if defined(GUI_CALC_FPS)
        if (g_drawFpsGraphEnabled) {
            drawFpsGraph(getDisplayWidth() - NUM_FPS_VALUES - 4, 4, NUM_FPS_VALUES, 32, getStyle(STYLE_ID_FPS_GRAPH));
        }
#endif

#if OPTION_KEYBOARD
        keyboard::updateDisplay();
#endif

#if OPTION_MOUSE
        mouse::updateDisplay();
#endif
    } else {
        if (g_syncedBuffer == g_renderBuffer1) {
            bitBlt(g_renderBuffer1, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
        } else if (g_syncedBuffer == g_renderBuffer2) {
            bitBlt(g_renderBuffer2, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void onThemeChanged() {
    auto selectedThemeIndex = g_hooks.getSelectedThemeIndex();
    g_themeColors = getThemeColors(selectedThemeIndex);
    g_themeColorsCount = getThemeColorsCount(selectedThemeIndex);
    g_colors = getColors();
}

void onLuminocityChanged() {
    // invalidate cache
    for (int i = 0; i < 256; ++i) {
        g_colorCache[i][0] = 0;
        g_colorCache[i][1] = 0;
        g_colorCache[i][2] = 0;
        g_colorCache[i][3] = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////

#define swap(type, i, j) {type t = i; i = j; j = t;}

void rgbToHsl(float r, float g, float b, float &h, float &s, float &l) {
    r /= 255;
    g /= 255;
    b /= 255;

    float min = r;
    float mid = g;
    float max = b;

    if (min > mid) {
        swap(float, min, mid);
    }
    if (mid > max) {
        swap(float, mid, max);
    }
    if (min > mid) {
        swap(float, min, mid);
    }

    l = (max + min) / 2;

    if (max == min) {
        h = s = 0; // achromatic
    } else {
        float d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);

        if (max == r) {
            h = (g - b) / d + (g < b ? 6 : 0);
        } else if (max == g) {
            h = (b - r) / d + 2;
        } else if (max == b) {
            h = (r - g) / d + 4;
        }

        h /= 6;
    }
}

float hue2rgb(float p, float q, float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f/6) return p + (q - p) * 6 * t;
    if (t < 1.0f/2) return q;
    if (t < 2.0f/3) return p + (q - p) * (2.0f/3 - t) * 6;
    return p;
}

void hslToRgb(float h, float s, float l, float &r, float &g, float &b) {
    if (s == 0) {
        r = g = b = l; // achromatic
    } else {
        float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;

        r = hue2rgb(p, q, h + 1.0f/3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0f/3);
    }

    r *= 255;
    g *= 255;
    b *= 255;
}

void adjustColor(Color &c) {
#if DISPLAY_BPP == 16
    if (g_hooks.getDisplayBackgroundLuminosityStep() == DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT) {
        return;
    }

	uint8_t ch = c >> 8;
	uint8_t cl = c & 0xFF;

    int i = (ch & 0xF0) | (cl & 0x0F);
    if (ch == g_colorCache[i][0] && cl == g_colorCache[i][1]) {
        // cache hit!
		c = (g_colorCache[i][2] << 8) | g_colorCache[i][3];
        return;
    }

    uint8_t r, g, b;
    r = ch & 248;
    g = ((ch << 5) | (cl >> 3)) & 252;
    b = cl << 3;

    float h, s, l;
    rgbToHsl(r, g, b, h, s, l);

    float a = l < 0.5 ? l : 1 - l;
    if (a > 0.3f) {
        a = 0.3f;
    }
    float lmin = l - a;
    float lmax = l + a;

    float lNew = remap((float)g_hooks.getDisplayBackgroundLuminosityStep(),
        (float)DISPLAY_BACKGROUND_LUMINOSITY_STEP_MIN,
        lmin,
        (float)DISPLAY_BACKGROUND_LUMINOSITY_STEP_MAX,
        lmax);

    float floatR, floatG, floatB;
    hslToRgb(h, s, lNew, floatR, floatG, floatB);

    r = FLOAT_TO_COLOR_COMPONENT(floatR);
    g = FLOAT_TO_COLOR_COMPONENT(floatG);
    b = FLOAT_TO_COLOR_COMPONENT(floatB);

    uint8_t chNew = RGB_TO_HIGH_BYTE(r, g, b);
    uint8_t clNew = RGB_TO_LOW_BYTE(r, g, b);

    // store new color in the cache
    g_colorCache[i][0] = ch;
    g_colorCache[i][1] = cl;
    g_colorCache[i][2] = chNew;
    g_colorCache[i][3] = clNew;

	c = (chNew << 8) | clNew;
#endif
}

Color getColorFromIndex(uint16_t colorIndex) {
    uint16_t color;
    if ((int16_t)colorIndex < -1) {
        // color is from external (i.e. not main) assets data
        Assets *assets = g_widgetCursor.assets;

        auto& colors = assets->colorsDefinition->colors;

        uint32_t externalColorIndex = -((int16_t)colorIndex) - 2;
        if (externalColorIndex >= colors.count) {
            return 0;
        }
        
        {
            uint16_t *colors = static_cast<uint16_t *>(assets->colorsDefinition->colors.items);
            if (assets->projectMinorVersion == 1) {
                return ((uint32_t *)colors)[externalColorIndex];
            }
            color = colors[externalColorIndex];
        }        
    } else {
        colorIndex = g_hooks.transformColor(colorIndex);

        if (g_mainAssets->projectMinorVersion == 1) {
            return colorIndex < g_themeColorsCount ? ((uint32_t *)g_themeColors)[colorIndex] : ((uint32_t *)g_colors)[colorIndex - g_themeColorsCount];    
        }
    	color = colorIndex < g_themeColorsCount ? g_themeColors[colorIndex] : g_colors[colorIndex - g_themeColorsCount];
    }

#if DISPLAY_BPP == 16
    return color;
#else
    return RGB_TO_COLOR(RGB565_COLOR_TO_R(color), RGB565_COLOR_TO_G(color), RGB565_COLOR_TO_B(color));
#endif
}

void getColorRGBAFromIndex(uint16_t colorIndex, ColorRGBA *colorRGBA) {
    auto color = getColorFromIndex(colorIndex);
    colorRGBA->r = COLOR_TO_R(color);
    colorRGBA->g = COLOR_TO_G(color);
    colorRGBA->b = COLOR_TO_B(color);
    colorRGBA->a = 255;
}

// get foreground color
Color getColor() {
    return g_fc;
}

// set foreground color by index
void setColor(uint16_t colorIndex, bool ignoreLuminocity) {
    g_fc = getColorFromIndex(colorIndex);
    if (!ignoreLuminocity) {
        adjustColor(g_fc);
    }
}

// set foreground color by RGB value
void setColor(uint8_t r, uint8_t g, uint8_t b) {
    g_fc = RGB_TO_COLOR(r, g, b);
	adjustColor(g_fc);
}

// set foreground color by value
void setColorByValue(Color color) {
    g_fc = color;
    adjustColor(g_fc);
}

Color getBackColor() {
    return g_bc;
}

// set background color by index
void setBackColor(uint16_t colorIndex, bool ignoreLuminocity) {
	g_bc = getColorFromIndex(colorIndex);
    if (!ignoreLuminocity) {
	    adjustColor(g_bc);
    }
}

// set background color by value
void setBackColor(uint8_t r, uint8_t g, uint8_t b) {
    g_bc = RGB_TO_COLOR(r, g, b);
	adjustColor(g_bc);
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

void aggInit(AggDrawing& aggDrawing) {
	aggDrawing.rbuf.attach((uint8_t *)getBufferPointer(), getDisplayWidth(), getDisplayHeight(), getDisplayWidth() * DISPLAY_BPP / 8);
	aggDrawing.graphics.attach(aggDrawing.rbuf.buf(), aggDrawing.rbuf.width(), aggDrawing.rbuf.height(), aggDrawing.rbuf.stride());
}

void drawRoundedRect(
    AggDrawing& aggDrawing,
    int x1, int y1, int x2, int y2,
    int lineWidth,
	int rtlx, int rtly, int rtrx, int rtry,
	int rbrx, int rbry, int rblx, int rbly
) {
    fillRoundedRect(
        aggDrawing,
        x1, y1, x2, y2,
        lineWidth,
        rtlx, rtly, rtrx, rtry,
	    rbrx, rbry, rblx, rbly,
        true, false
    );
}

void fillRect(
    int x1, int y1, int x2, int y2,
    int clip_x1, int clip_y1, int clip_x2, int clip_y2
) {
    if (clip_x1 != -1) {
        x1 = MAX(x1, clip_x1);
        x2 = MIN(x2, clip_x2);
        y1 = MAX(y1, clip_y1);
        y2 = MIN(y2, clip_y2);
    }
    fillRect(x1, y1, x2, y2);
}

void fillRoundedRect(
    AggDrawing& aggDrawing,
    int x1, int y1, int x2, int y2,
    int lineWidth,
    int rtlx, int rtly, int rtrx, int rtry,
    int rbrx, int rbry, int rblx, int rbly,
    bool drawLine, bool fill,
    int clip_x1, int clip_y1, int clip_x2, int clip_y2
) {
#ifdef CONF_FAST_ROUND_RECT
	if (
		rtlx == rtly && rtly == rtrx && rtrx == rtry && rtry == rbrx && rbrx == rbry && rbry == rblx && rblx == rbly // all radiuses are the same
		// && clip_x1 == -1 // no clipping
	) {
		int r = rtlx;
		int border = lineWidth;

		if (border == 0) {
			drawLine = 0;
		}

		int w = x2 - x1 + 1;
		int h = y2 - y1 + 1;

		int x = MIN(w, h);
		if (r > x / 2.0f) {
			r = floorf(x / 2.0f);
		}

		int r_inner = r - border;

		int xc1 = x2 - r + 1;
		int yc1 = y1 + r;

		int xc2 = x1 + r;
		int yc2 = y1 + r;

		int xc3 = x1 + r;
		int yc3 = y2 - r + 1;

		int xc4 = x2 - r + 1;
		int yc4 = y2 - r + 1;

		auto fc_save = g_fc;

		uint8_t fc[3] = { COLOR_TO_R(g_fc), COLOR_TO_G(g_fc), COLOR_TO_B(g_fc) };
		uint8_t bc[3] = { COLOR_TO_R(g_bc), COLOR_TO_G(g_bc), COLOR_TO_B(g_bc) };

		float op = g_opacity / 255.0f;
		float a1_op;
		float a2_op;
		float a3_op;
		float r1, g1, b1;
		uint8_t dest_r, dest_g, dest_b;

		#define DRAW_PIXEL(x, y, c1, a1, c2, a2) \
			getPixel(x, y, &dest_r, &dest_g, &dest_b); \
			a1_op = a1 * op; \
			a2_op = a2 * op; \
			a3_op = 1 - (a1 + a2); \
			r1 = (c1)[0] * a1_op + (c2)[0] * a2_op + dest_r * a3_op; \
			g1 = (c1)[1] * a1_op + (c2)[1] * a2_op + dest_g * a3_op; \
			b1 = (c1)[2] * a1_op + (c2)[2] * a2_op + dest_b * a3_op; \
			g_fc = RGB_TO_COLOR((int)r1, (int)g1, (int)b1); \
			if (clip_x1 == -1 || (x >= clip_x1 && x <= clip_x2 && y >= clip_y1 && y <= clip_y2)) drawPixel(x, y) \

		#define DRAW_4(x, y, a1, a2) \
			DRAW_PIXEL(xc1  + (x)     , yc1 -  (y)     , drawLine ? fc : bc, (a1), bc, (a2)); \
			DRAW_PIXEL(xc2 - ((x) + 1), yc2 -  (y)     , drawLine ? fc : bc, (a1), bc, (a2)); \
			DRAW_PIXEL(xc3 - ((x) + 1), yc3 + ((y) - 1), drawLine ? fc : bc, (a1), bc, (a2)); \
			DRAW_PIXEL(xc4  + (x)     , yc4 + ((y) - 1), drawLine ? fc : bc, (a1), bc, (a2)); \

		display::startPixelsDraw();

		int ffd = roundf(r / sqrtf(2.0f));
		for (int x = 0; x < ffd; x++) {
			float yr = sqrtf(r * r - (x + 0.5f) * (x + 0.5f));
			int y = ceilf(yr);
			float a1 = 1 - (y - yr);

			float yr_inner = drawLine && x < r_inner ? sqrtf(r_inner * r_inner - (x + 0.5f) * (x + 0.5f)) : 0;
			int y_inner = ceilf(yr_inner);
			float a2 = 1 - (y_inner - yr_inner);

			if (y > 0) {
                DRAW_4(x, y, a1, 0);
            }
			DRAW_4(y - 1, x + 1, a1, 0);

			for (y = y - 1; y > y_inner; y--) {
				DRAW_4(x, y, 1.0, 0);
				DRAW_4(y - 1, x + 1, 1.0, 0);
			}

			if (y > 0) {
				DRAW_4(x, y, 1 - a2, fill ? a2 : 0);
				DRAW_4(y - 1, x + 1, 1 - a2, fill ? a2 : 0);

                if (fill) {
                    for (y = y - 1; y > 0; y--) {
                        DRAW_4(x, y, 0.0, 1.0);
                        DRAW_4(y - 1, x + 1, 0.0, 1.0);
                    }
                }
			}
		}

		display::endPixelsDraw();

		g_fc = g_bc;

		// background
		if (fill) {
			if (drawLine) {
				fillRect(x1 + r, y1 + border, x2 - r, y2 - border, clip_x1, clip_y1, clip_x2, clip_y2); // from top to bottom
				fillRect(x1 + border, y1 + r, x1 + r - 1, y2 - r, clip_x1, clip_y1, clip_x2, clip_y2); // left
				fillRect(x2 - r + 1, y1 + r, x2 - border, y2 - r, clip_x1, clip_y1, clip_x2, clip_y2); // right
			} else {
				fillRect(x1 + r, y1, x2 - r, y2); // from top to bottom
				fillRect(x1, y1 + r, x1 + r - 1, y2 - r, clip_x1, clip_y1, clip_x2, clip_y2); // left
				fillRect(x2 - r + 1, y1 + r, x2, y2 - r, clip_x1, clip_y1, clip_x2, clip_y2); // right
			}
		}

		g_fc = fc_save;

		// border
		if (drawLine) {
			fillRect(x1 + r, y1, x2 - r, y1 + border - 1, clip_x1, clip_y1, clip_x2, clip_y2); // top
			fillRect(x1 + r, y2 - border + 1, x2 - r, y2, clip_x1, clip_y1, clip_x2, clip_y2); // bottom
			fillRect(x1, y1 + r, x1 + border - 1, y2 - r, clip_x1, clip_y1, clip_x2, clip_y2); // left
			fillRect(x2 - border + 1, y1 + r, x2, y2 - r, clip_x1, clip_y1, clip_x2, clip_y2); // right
		}
	} else {
#endif
        // use AGG, slower

        auto &graphics = aggDrawing.graphics;

        if (clip_x1 != -1) {
            graphics.clipBox(clip_x1, clip_y1, clip_x2 + 1, clip_y2 + 1);
        } else {
            graphics.clipBox(x1, y1, x2 + 1, y2 + 1);
        }
        graphics.masterAlpha(g_opacity / 255.0);
        graphics.translate(x1, y1);
        graphics.lineWidth(lineWidth);
        if (lineWidth > 0 && drawLine) {
            graphics.lineColor(COLOR_TO_R(g_fc), COLOR_TO_G(g_fc), COLOR_TO_B(g_fc));
        } else {
            graphics.noLine();
        }
        if (fill) {
            graphics.fillColor(COLOR_TO_R(g_bc), COLOR_TO_G(g_bc), COLOR_TO_B(g_bc));
        } else {
            graphics.noFill();
        }
        auto w = x2 - x1 + 1;
        auto h = y2 - y1 + 1;
        graphics.roundedRect(
            lineWidth / 2.0, lineWidth / 2.0, w - lineWidth, h - lineWidth,
            rtlx, rtly, rtrx, rtry, rbrx, rbry, rblx, rbly
        );

        graphics.translate(-x1, -y1);
        graphics.clipBox(0, 0, aggDrawing.rbuf.width(), aggDrawing.rbuf.height());
#ifdef CONF_FAST_ROUND_RECT
    }
#endif
}

void fillRoundedRect(
    AggDrawing& aggDrawing,
	int x1, int y1, int x2, int y2,
	int lineWidth,
	int r,
	bool drawLine, bool fill,
	int clip_x1, int clip_y1, int clip_x2, int clip_y2
) {
	fillRoundedRect(aggDrawing, x1, y1, x2, y2, lineWidth, r, r, r, r, r, r, r, r, drawLine, fill, clip_x1, clip_y1, clip_x2, clip_y2);
}

////////////////////////////////////////////////////////////////////////////////

static int8_t measureGlyph(int32_t encoding) {
    auto glyph = g_font.getGlyph(encoding);
    if (!glyph)
        return 0;

    return glyph->dx;
}

int8_t measureGlyph(int32_t encoding, gui::font::Font &font) {
    auto glyph = font.getGlyph(encoding);
    if (!glyph)
        return 0;

    return glyph->dx;
}

int measureStr(const char *text, int textLength, gui::font::Font &font, int max_width) {
    g_font = font;

    int width = 0;

    if (textLength == -1) {
        while (true) {
            utf8_int32_t encoding;
            text = utf8codepoint(text, &encoding);
            if (!encoding) {
                break;
            }
            int glyph_width = measureGlyph(encoding);
            if (max_width > 0 && width + glyph_width > max_width) {
                return max_width;
            }
            width += glyph_width;
        }
    } else {
        for (int i = 0; i < textLength; ++i) {
            utf8_int32_t encoding;
            text = utf8codepoint(text, &encoding);
            if (!encoding) {
                break;
            }
            int glyph_width = measureGlyph(encoding);
            if (max_width > 0 && width + glyph_width > max_width) {
                return max_width;
            }
            width += glyph_width;
        }
    }

    return width;
}

void drawStr(const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2, int clip_y2, gui::font::Font &font, int cursorPosition) {
    g_font = font;

    drawStrInit();

    if (textLength == -1) {
        textLength = utf8len(text);
    }

    int xCursor = x;

    int i;

    for (i = 0; i < textLength; ++i) {
        utf8_int32_t encoding;
        text = utf8codepoint(text, &encoding);
        if (!encoding) {
            break;
        }

        if (i == cursorPosition) {
            xCursor = x;
        }

        auto x1 = x;
        auto y1 = y;

        auto glyph = g_font.getGlyph(encoding);
        if (glyph) {
            int x_glyph = x1 + glyph->x;
            int y_glyph = y1 + g_font.getAscent() - (glyph->y + glyph->height);

            // draw glyph pixels
            int iStartByte = 0;
            if (x_glyph < clip_x1) {
                int dx_off = clip_x1 - x_glyph;
                iStartByte = dx_off;
                x_glyph = clip_x1;
            }

			if (iStartByte < glyph->width) {
				int offset = 0;
				int glyphHeight = glyph->height;
				if (y_glyph < clip_y1) {
					int dy_off = clip_y1 - y_glyph;
					offset += dy_off * glyph->width;
					glyphHeight -= dy_off;
					y_glyph = clip_y1;
				}

				int width;
				if (x_glyph + (glyph->width - iStartByte) - 1 > clip_x2) {
					width = clip_x2 - x_glyph + 1;
				} else {
					width = (glyph->width - iStartByte);
				}

				int height;
				if (y_glyph + glyphHeight - 1 > clip_y2) {
					height = clip_y2 - y_glyph + 1;
				} else {
					height = glyphHeight;
				}

				if (width > 0 && height > 0) {
					drawGlyph(glyph->pixels + offset + iStartByte, glyph->width - width, x_glyph, y_glyph, width, height);
				}
			}

			x += glyph->dx;
		}
    }

    if (i == cursorPosition) {
        xCursor = x;
    }

    if (cursorPosition != -1 && xCursor - CURSOR_WIDTH / 2 >= clip_x1 && xCursor + CURSOR_WIDTH / 2 - 1 <= clip_x2) {
        auto d = MAX(((clip_y2 - clip_y1) - font.getHeight()) / 2, 0);
        fillRect(xCursor - CURSOR_WIDTH / 2, clip_y1 + d, xCursor + CURSOR_WIDTH / 2 - 1, clip_y2 - d);
    }

    setDirty();
}

int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font) {
    if (textLength == -1) {
        textLength = utf8len(text);
    }

    int i;

    for (i = 0; i < textLength; ++i) {
        utf8_int32_t encoding;
        text = utf8codepoint(text, &encoding);
        if (!encoding) {
            break;
        }

        auto glyph = font.getGlyph(encoding);
        auto dx = 0;
        if (glyph) {
            dx = glyph->dx;
        }
        if (xPos < x + dx / 2) {
            return i;
        }
        x += dx;
    }

    return i;
}

int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font) {
    if (textLength == -1) {
        textLength = utf8len(text);
    }

    for (int i = 0; i < textLength; ++i) {
        utf8_int32_t encoding;
        text = utf8codepoint(text, &encoding);
        if (!encoding) {
            break;
        }

        if (i == cursorPosition) {
            return x;
        }

        auto glyph = font.getGlyph(encoding);
        if (glyph) {
            x += glyph->dx;
        }
    }

    return x;
}

#if defined(EEZ_PLATFORM_SIMULATOR) && EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
static SDL_Window *g_mainWindow;
static SDL_Renderer *g_renderer;

// heuristics to find resource file
std::string getFullPath(std::string category, std::string path) {
    std::string fullPath = category + "/" + path;
    for (int i = 0; i < 5; ++i) {
        FILE *fp = fopen(fullPath.c_str(), "r");
        if (fp) {
            fclose(fp);
            return fullPath;
        }
        fullPath = std::string("../") + fullPath;
    }
    return path;
}
#endif

#if defined(EEZ_PLATFORM_STM32)
#define DMA2D_WAIT if (g_waitDMA) { while (HAL_DMA2D_PollForTransfer(&hdma2d, 1000) != HAL_OK); g_waitDMA = false; }

static bool g_waitDMA;
#endif

static void initDriver() {
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)

#if EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
    // Set texture filtering to linear
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        printf("Warning: Linear texture filtering not enabled!");
    }

    // Create window
    g_mainWindow = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_HIDDEN);

    if (g_mainWindow == NULL) {
        printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }

    // Create renderer
    g_renderer = SDL_CreateRenderer(g_mainWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) {
		g_mainWindow = NULL;
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    // Initialize PNG loading
    int imgFlags = IMG_INIT_PNG;
    if ((IMG_Init(imgFlags) & imgFlags) != imgFlags) {
        printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
    } else {
#if !defined(__EMSCRIPTEN__)
        // Set icon
        SDL_Surface *iconSurface = IMG_Load(getFullPath("images", ICON).c_str());
        if (!iconSurface) {
            printf("Failed to load icon! SDL Error: %s\n", SDL_GetError());
        } else {
            SDL_SetWindowIcon(g_mainWindow, iconSurface);
            SDL_FreeSurface(iconSurface);
        }
#endif
    }

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

static void syncBuffer() {
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)

#if EEZ_USE_SDL && !defined(__EMSCRIPTEN__)
    if (!g_mainWindow) {
		return;
    }

    SDL_Surface *rgbSurface = SDL_CreateRGBSurfaceFrom(
        (uint32_t *)g_syncedBuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, 32, 4 * DISPLAY_WIDTH, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (rgbSurface != NULL) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, rgbSurface);
        if (texture != NULL) {
            SDL_Rect srcRect = { 0, 0, (int)DISPLAY_WIDTH, (int)DISPLAY_HEIGHT };
            SDL_Rect dstRect = { 0, 0, (int)DISPLAY_WIDTH, (int)DISPLAY_HEIGHT };

            SDL_RenderCopyEx(g_renderer, texture, &srcRect, &dstRect, 0.0, NULL, SDL_FLIP_NONE);

            SDL_DestroyTexture(texture);
        } else {
            printf("Unable to create texture from image buffer! SDL Error: %s\n", SDL_GetError());
        }
        SDL_FreeSurface(rgbSurface);
    } else {
        printf("Unable to render text surface! SDL Error: %s\n", SDL_GetError());
    }
    SDL_RenderPresent(g_renderer);
#endif

#if !defined(__EMSCRIPTEN__)
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)

    auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    uint8_t *src = (uint8_t *)(g_renderBuffer + appContext->rect.y * DISPLAY_WIDTH + appContext->rect.x);
    uint8_t *dst = SCREENSHOOT_BUFFER_START_ADDRESS;

    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
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

    bitBltRGB888(g_syncedBuffer, SCREENSHOOT_BUFFER_START_ADDRESS, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    uint8_t *dest = (uint8_t *)(g_renderBuffer + y * DISPLAY_WIDTH + x);
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
#endif

#if defined(EEZ_PLATFORM_STM32)
    DMA2D_WAIT;
#endif
}

void drawPixel(int x, int y) {
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    *(g_renderBuffer + y * DISPLAY_WIDTH + x) = g_fc;
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    auto dest = g_renderBuffer + y * DISPLAY_WIDTH + x;

    uint32_t srcColor = (g_fc & 0x00FFFFFF) | (opacity << 24);
    uint32_t dstColor = (*dest & 0x00FFFFFF) | ((255 - opacity) << 24);
    *dest = blendColor(srcColor, dstColor);
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
	uint32_t *dst = g_renderBuffer + y1 * DISPLAY_WIDTH + x1;
    int width = x2 - x1 + 1;
    if (width <= 0) {
        return;
    }
    int height = y2 - y1 + 1;
    if (height <= 0) {
        return;
    }
    int nl = DISPLAY_WIDTH - width;
    if (g_opacity == 255) {
        for (uint32_t *dstEnd = dst + height * DISPLAY_WIDTH; dst != dstEnd; dst += nl) {
            for (uint32_t *lineEnd = dst + width; dst != lineEnd; dst++) {
                *dst = g_fc;
            }
        }
    } else {
        uint32_t fc = (g_fc & 0x00FFFFFF) | (g_opacity << 24);
        for (uint32_t *dstEnd = dst + height * DISPLAY_WIDTH; dst != dstEnd; dst += nl) {
            for (uint32_t *lineEnd = dst + width; dst != lineEnd; dst++) {
                *dst = blendColor(fc, *dst);
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    uint32_t *dst = (uint32_t *)dstBuffer + y1 * DISPLAY_WIDTH + x1;
    int nl = DISPLAY_WIDTH - (x2 - x1 + 1);
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            *dst++ = g_fc;
        }
        dst += nl;
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    fillRect((uint16_t *)dstBuffer, x1, y1, x2 - x1 + 1, y2 - y1 + 1, g_fc);
#endif    

    setDirty();
}

void bitBlt(int x1, int y1, int x2, int y2, int dstx, int dsty) {
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    int width = x2 - x1 + 1;

    uint32_t *src = g_renderBuffer + y1 * DISPLAY_WIDTH + x1;
    uint32_t *dst = g_renderBuffer + dsty * DISPLAY_WIDTH + dstx;
    int nl = DISPLAY_WIDTH - width;

    for (int y = y1; y <= y2; y++, src += nl, dst += nl) {
        for (uint32_t *lineEnd = dst + width; dst != lineEnd; dst++, src++) {
            uint8_t *src8 = (uint8_t *)src;
            *dst = RGB_TO_COLOR(src8[0], src8[1], src8[2]);
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            int i = y * DISPLAY_WIDTH + x;
            ((uint32_t *)dst)[i] = ((uint32_t *)src)[i];
        }
    }
#endif

#if defined(EEZ_PLATFORM_STM32)
    bitBlt((uint16_t *)src, (uint16_t *)dst, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
#endif    

    setDirty();
}

void bitBlt(void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, uint8_t opacity) {
    if (dst == nullptr) {
        dst = g_renderBuffer;
    }

#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    if (opacity == 255) {
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < sw; ++x) {
                ((uint32_t *)dst)[(dy + y) * DISPLAY_WIDTH + dx + x] = ((uint32_t *)src)[(sy + y) * DISPLAY_WIDTH + sx + x];
            }
        }
    } else {
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < sw; ++x) {
                uint8_t *p = (uint8_t *)&((uint32_t *)src)[(sy + y) * DISPLAY_WIDTH + sx + x];
                p[3] = opacity;
                ((uint32_t *)dst)[(dy + y) * DISPLAY_WIDTH + dx + x] = blendColor(
                    ((uint32_t *)src)[(sy + y) * DISPLAY_WIDTH + sx + x],
                    ((uint32_t *)dst)[(dy + y) * DISPLAY_WIDTH + dx + x]
                );
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    uint32_t *dst = g_renderBuffer + y * DISPLAY_WIDTH + x;
    int nlDst = DISPLAY_WIDTH - image->width;

    if (image->bpp == 32) {
        uint32_t *src = (uint32_t *)image->pixels;
        int nlSrc = image->lineOffset;

        uint32_t pixel;
        uint8_t *pixelAlpha = ((uint8_t *)&pixel) + 3;

        for (uint32_t *srcEnd = src + (image->width + nlSrc) * image->height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src++, dst++) {
                pixel = *src;
                *pixelAlpha = *pixelAlpha * g_opacity / 255;
                *dst = blendColor(pixel, *dst);
            }
        }
    } else if (image->bpp == 24) {
        uint8_t *src = (uint8_t *)image->pixels;
        int nlSrc = 3 * image->lineOffset;

        for (uint8_t *srcEnd = src + 3 * (image->width + nlSrc) * image->height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src += 3, dst++) {
                ((uint8_t *)dst)[0] = ((uint8_t *)src)[0];
                ((uint8_t *)dst)[1] = ((uint8_t *)src)[1];
                ((uint8_t *)dst)[2] = ((uint8_t *)src)[2];
                ((uint8_t *)dst)[3] = 255;
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
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
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    // glyph->pixels + offset + iStartByte, glyph->width - width, x_glyph, y_glyph, width,height
    // const gui::GlyphData &glyph, int x_glyph, int y_glyph, int width, int height, int offset, int iStartByte

    uint32_t pixel;
    ((uint8_t *)&pixel)[0] = COLOR_TO_R(g_fc);
    ((uint8_t *)&pixel)[1] = COLOR_TO_G(g_fc);
    ((uint8_t *)&pixel)[2] = COLOR_TO_B(g_fc);
    uint8_t *pixelAlpha = ((uint8_t *)&pixel) + 3;

    uint32_t *dst = g_renderBuffer + y_glyph * DISPLAY_WIDTH + x_glyph;
    int nlDst = DISPLAY_WIDTH - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            *pixelAlpha = *src * g_opacity / 255;
            *dst = blendColor(pixel, *dst);
            src++;
            dst++;
        }
        src += srcLineOffset;
        dst += nlDst;
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
