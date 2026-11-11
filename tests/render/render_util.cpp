/*
 * eez-framework tests: helpers of the drawing tests (render_util.h) and the
 * native variables of the render part (projects/parts/render.py).
 */

#include "render_util.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL.h>
#include <SDL_image.h>

#include <eez/conf-internal.h>
#include <eez/core/memory.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>

#include "document.h"
#include "harness.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;

////////////////////////////////////////////////////////////////////////////////
// native variables render_text_a .. render_text_f

namespace {

std::string g_texts[6] = { "text a", "text b", "text c", "text d", "text e", "text f" };

void getText(int index, DataOperationEnum operation, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value::makeStringRef(g_texts[index].c_str(), (int)g_texts[index].size(), 0x52454e00 + index);
    }
}

} // namespace

namespace eez {
namespace gui {

void data_render_text_a(DataOperationEnum operation, const WidgetCursor &, Value &value) { getText(0, operation, value); }
void data_render_text_b(DataOperationEnum operation, const WidgetCursor &, Value &value) { getText(1, operation, value); }
void data_render_text_c(DataOperationEnum operation, const WidgetCursor &, Value &value) { getText(2, operation, value); }
void data_render_text_d(DataOperationEnum operation, const WidgetCursor &, Value &value) { getText(3, operation, value); }
void data_render_text_e(DataOperationEnum operation, const WidgetCursor &, Value &value) { getText(4, operation, value); }
void data_render_text_f(DataOperationEnum operation, const WidgetCursor &, Value &value) { getText(5, operation, value); }

namespace display {
// the two buffers drawn into in turn (display.cpp)
extern VideoBuffer g_renderBuffer1;
extern VideoBuffer g_renderBuffer2;
} // namespace display

} // namespace gui
} // namespace eez

namespace render {

void setText(int index, const char *text) {
    g_texts[index] = text;
}

////////////////////////////////////////////////////////////////////////////////
// capture

Image32 captureBuffer(const uint32_t *buffer, int x, int y, int w, int h) {
    Image32 image(w, h);
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            image.at(xx, yy) = buffer[(y + yy) * (int)DISPLAY_WIDTH + x + xx] | 0xFF000000u;
        }
    }
    return image;
}

Image32 captureRegion(int x, int y, int w, int h) {
    return captureBuffer(display::getBufferPointer(), x, y, w, h);
}

Image32 captureFrame() {
    return captureRegion(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// PNG

bool savePng(const std::string &path, const Image32 &image) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)image.px.data(), image.w, image.h, 32, image.w * 4, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        fprintf(stderr, "SDL_CreateRGBSurfaceWithFormatFrom: %s\n", SDL_GetError());
        return false;
    }
    int result = IMG_SavePNG(surface, path.c_str());
    SDL_FreeSurface(surface);
    if (result != 0) {
        fprintf(stderr, "IMG_SavePNG %s: %s\n", path.c_str(), IMG_GetError());
        return false;
    }
    return true;
}

bool loadPng(const std::string &path, Image32 &image) {
    SDL_Surface *loaded = IMG_Load(path.c_str());
    if (!loaded) {
        return false;
    }
    SDL_Surface *surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loaded);
    if (!surface) {
        return false;
    }
    image = Image32(surface->w, surface->h);
    SDL_LockSurface(surface);
    for (int y = 0; y < surface->h; y++) {
        memcpy(&image.px[(size_t)y * surface->w], (uint8_t *)surface->pixels + y * surface->pitch, surface->w * 4);
    }
    SDL_UnlockSurface(surface);
    SDL_FreeSurface(surface);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// compare

static bool pixelDiffers(uint32_t a, uint32_t b, int tolerance) {
    return abs(R(a) - R(b)) > tolerance || abs(G(a) - G(b)) > tolerance || abs(B(a) - B(b)) > tolerance;
}

int countDiff(const Image32 &a, const Image32 &b, int tolerance) {
    if (a.w != b.w || a.h != b.h) {
        return INT_MAX;
    }
    int count = 0;
    for (size_t i = 0; i < a.px.size(); i++) {
        if (pixelDiffers(a.px[i], b.px[i], tolerance)) {
            count++;
        }
    }
    return count;
}

static void makeDirs(const std::string &path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') {
            mkdir(path.substr(0, i).c_str(), 0755);
        }
    }
}

static std::string sourceDir() {
    std::string file = __FILE__;
    return file.substr(0, file.rfind('/'));
}

static std::string buildDir() {
    char path[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) {
        return ".";
    }
    path[n] = 0;
    std::string exe = path;
    return exe.substr(0, exe.rfind('/'));
}

static std::string safeName(const std::string &name) {
    std::string result = name;
    for (auto &c : result) {
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') {
            c = '_';
        }
    }
    return result;
}

std::string failureDir(const std::string &name) {
    std::string dir = buildDir() + "/render-failures/" + safeName(name);
    makeDirs(dir);
    return dir;
}

std::string writeMismatch(const std::string &name, const Image32 &actual, const Image32 &expected, int tolerance) {
    std::string dir = failureDir(name);
    savePng(dir + "/actual.png", actual);
    savePng(dir + "/expected.png", expected);
    if (actual.w == expected.w && actual.h == expected.h) {
        Image32 diff(actual.w, actual.h);
        for (size_t i = 0; i < actual.px.size(); i++) {
            uint32_t e = expected.px[i];
            if (pixelDiffers(actual.px[i], e, tolerance)) {
                diff.px[i] = rgb(255, 0, 0);
            } else {
                int v = (R(e) + G(e) + B(e)) / 12;
                diff.px[i] = rgb(v, v, v);
            }
        }
        savePng(dir + "/diff.png", diff);
    }
    return dir;
}

// a pixel differs when R, G or B differs by more than this
static const int GOLDEN_TOLERANCE = 3;
// number of differing pixels that is still a match (libm differences in the
// antialiased edges on other machines)
static const int GOLDEN_MAX_DIFFERING_PIXELS = 8;

void checkGolden(const char *file, int line, const char *name, const Image32 &image) {
    std::string goldenDir = sourceDir() + "/golden";
    std::string path = goldenDir + "/" + name + ".png";

    const char *update = getenv("EEZ_TESTS_UPDATE_GOLDEN");
    if (update && update[0] && strcmp(update, "0") != 0) {
        makeDirs(goldenDir);
        if (savePng(path, image)) {
            printf("    updated golden image %s\n", path.c_str());
        } else {
            test::fail(file, line, std::string("can't write golden image ") + path);
        }
        return;
    }

    Image32 expected;
    if (!loadPng(path, expected)) {
        std::string dir = failureDir(name);
        savePng(dir + "/actual.png", image);
        test::fail(file, line, std::string("no golden image ") + path +
            " (run with EEZ_TESTS_UPDATE_GOLDEN=1 to create it), actual: " + dir + "/actual.png");
        return;
    }

    if (expected.w != image.w || expected.h != image.h) {
        std::string dir = writeMismatch(name, image, expected, GOLDEN_TOLERANCE);
        test::fail(file, line, test::format("golden image %s: size %dx%d, expected %dx%d (images in %s)",
            name, image.w, image.h, expected.w, expected.h, dir.c_str()));
        return;
    }

    int differing = countDiff(image, expected, GOLDEN_TOLERANCE);
    if (differing > GOLDEN_MAX_DIFFERING_PIXELS) {
        std::string dir = writeMismatch(name, image, expected, GOLDEN_TOLERANCE);
        test::fail(file, line, test::format("golden image %s: %d pixels differ (images in %s)",
            name, differing, dir.c_str()));
    }
}

////////////////////////////////////////////////////////////////////////////////

void showPage(int pageId, int frames) {
    harness::appContext()->showPage(pageId);
    harness::frame(frames);
}

void pushPage(int pageId, int frames) {
    harness::appContext()->pushPage(pageId);
    harness::frame(frames);
}

void frameToBlink(bool on) {
    for (int i = 0; i < 200; i++) {
        uint32_t phase = harness::now() % 800;
        if (on ? (phase >= 440 && phase <= 560) : (phase >= 40 && phase <= 160)) {
            break;
        }
        harness::frame();
    }
    harness::frame(2);
}

int compareWithFullRedraw(const char *name, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        x = 0;
        y = 0;
        w = DISPLAY_WIDTH;
        h = DISPLAY_HEIGHT;
    }
    Image32 incremental = captureRegion(x, y, w, h);
    refreshScreen();
    harness::frame();
    Image32 full = captureRegion(x, y, w, h);
    int differing = countDiff(incremental, full);
    if (differing) {
        std::string dir = writeMismatch(std::string("redraw_") + name, incremental, full, 0);
        printf("    incremental frame vs full redraw: %d pixels differ (actual = incremental, expected = full redraw, in %s)\n",
            differing, dir.c_str());
    }
    return differing;
}

void resetDrawState() {
    display::restoreClipRect({ 0, 0, (int)DISPLAY_WIDTH - 1, (int)DISPLAY_HEIGHT - 1 });
    display::setOpacity(255);
    g_widgetCursor = WidgetCursor();
    g_widgetCursor.assets = g_mainAssets;
    g_widgetCursor.refreshed = true;
}

void clearRect(int x1, int y1, int x2, int y2, uint32_t color) {
    auto savedColor = display::getColor();
    display::setColorByValue(color);
    auto savedOpacity = display::setOpacity(255);
    display::fillRect(x1, y1, x2, y2);
    display::setOpacity(savedOpacity);
    display::setColorByValue(savedColor);
}

const Style *style(int styleId) {
    return getStyle(styleId);
}

////////////////////////////////////////////////////////////////////////////////
// damage markers

static const uint32_t MARKER = 0xFF03FE01; // R 1, G 254, B 3

static uint32_t *nextRenderBuffer() {
    uint32_t *presented = display::getBufferPointer();
    uint32_t *buffer1 = display::g_renderBuffer1;
    uint32_t *buffer2 = display::g_renderBuffer2;
    return presented == buffer1 ? buffer2 : buffer1;
}

void putMarkers(int step) {
    uint32_t *buffer = nextRenderBuffer();
    for (int y = step / 2; y < (int)DISPLAY_HEIGHT; y += step) {
        for (int x = step / 2; x < (int)DISPLAY_WIDTH; x += step) {
            buffer[y * DISPLAY_WIDTH + x] = MARKER;
        }
    }
}

int countMarkers(int step) {
    const uint32_t *buffer = display::getBufferPointer();
    int count = 0;
    for (int y = step / 2; y < (int)DISPLAY_HEIGHT; y += step) {
        for (int x = step / 2; x < (int)DISPLAY_WIDTH; x += step) {
            if (buffer[y * DISPLAY_WIDTH + x] == MARKER) {
                count++;
            }
        }
    }
    return count;
}

int numMarkers(int step) {
    int nx = 0, ny = 0;
    for (int x = step / 2; x < (int)DISPLAY_WIDTH; x += step) nx++;
    for (int y = step / 2; y < (int)DISPLAY_HEIGHT; y += step) ny++;
    return nx * ny;
}

} // namespace render
