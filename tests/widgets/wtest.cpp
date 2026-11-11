/*
 * eez-framework tests: helpers of the widget tests (wtest.h).
 */

#include "wtest.h"

#include <stdio.h>
#include <string.h>

#include <eez/gui/display.h>
#include <eez/flow/flow.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/private.h>

namespace eez {
namespace gui {
namespace display {
// set by every drawing function, cleared at the start of the frame (display.cpp)
extern bool g_dirty;
}
}
}

using namespace eez;
using namespace eez::gui;

namespace wt {

uint32_t rgb(int x, int y) {
    uint32_t p = harness::pixel(x, y);
    return ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF);
}

std::string hex(uint32_t color) {
    return test::format("#%06x", (unsigned)color);
}

void show(int pageId) {
    harness::appContext()->showPage(pageId);
    harness::frame(3);
}

bool drawn() {
    return display::g_dirty;
}

int framesWithDrawing(int frames) {
    int n = 0;
    for (int i = 0; i < frames; i++) {
        harness::frame();
        if (drawn()) {
            n++;
        }
    }
    return n;
}

int count(int x, int y, int w, int h, uint32_t color) {
    int n = 0;
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (rgb(i, j) == color) {
                n++;
            }
        }
    }
    return n;
}

int rowsWith(int x, int y, int w, int h, uint32_t color) {
    int n = 0;
    for (int j = y; j < y + h; j++) {
        if (count(x, j, w, 1, color) > 0) {
            n++;
        }
    }
    return n;
}

int textLines(int x, int y, int w, int h, uint32_t color) {
    int lines = 0;
    int emptyRows = 3;
    for (int j = y; j < y + h; j++) {
        if (count(x, j, w, 1, color) > 0) {
            if (emptyRows >= 3) {
                lines++;
            }
            emptyRows = 0;
        } else {
            emptyRows++;
        }
    }
    return lines;
}

int countOther(int x, int y, int w, int h, uint32_t color) {
    return w * h - count(x, y, w, h, color);
}

int diffBlocks(int x1, int y1, int x2, int y2, int w, int h) {
    int n = 0;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (rgb(x1 + i, y1 + j) != rgb(x2 + i, y2 + j)) {
                n++;
            }
        }
    }
    return n;
}

std::vector<uint32_t> block(int x, int y, int w, int h) {
    std::vector<uint32_t> pixels;
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            pixels.push_back(rgb(i, j));
        }
    }
    return pixels;
}

int diffBlock(const std::vector<uint32_t> &pixels, int x, int y, int w, int h) {
    int n = 0;
    size_t k = 0;
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (k >= pixels.size() || pixels[k] != rgb(i, j)) {
                n++;
            }
            k++;
        }
    }
    return n;
}

static std::vector<uint32_t> capture() {
    std::vector<uint32_t> pixels(DISPLAY_WIDTH * DISPLAY_HEIGHT);
    for (uint32_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint32_t x = 0; x < DISPLAY_WIDTH; x++) {
            pixels[y * DISPLAY_WIDTH + x] = rgb(x, y);
        }
    }
    return pixels;
}

int diffWithFullRedraw(std::string *where) {
    auto incremental = capture();
    refreshScreen();
    harness::frame();
    auto full = capture();
    int n = 0;
    for (size_t i = 0; i < full.size(); i++) {
        if (incremental[i] != full[i]) {
            if (n == 0 && where) {
                *where = test::format("first at (%d, %d): incremental %s, full redraw %s",
                    (int)(i % DISPLAY_WIDTH), (int)(i / DISPLAY_WIDTH),
                    hex(incremental[i]).c_str(), hex(full[i]).c_str());
            }
            n++;
        }
    }
    if (n > 0) {
        printf("full redraw differs in %d pixels, %s\n", n, where ? where->c_str() : "");
    }
    return n;
}

void press(int x, int y) {
    harness::touch(x, y);
    harness::frame(2);
}

void release() {
    harness::release();
    harness::frame(2);
}

void tap(int x, int y) {
    press(x, y);
    release();
    // the next touch is not an accidental double tap
    harness::frame(25);
}

////////////////////////////////////////////////////////////////////////////////

static int markerIndex() {
    static int g_index = -1;
    if (g_index == -1) {
        uint32_t count = g_mainAssets->flowDefinition->globalVariables.count;
        for (uint32_t i = 0; i < count; i++) {
            Value value = flow::getGlobalVariable(i);
            if (value.isString() && value.getString() && strcmp(value.getString(), "w_marker") == 0) {
                g_index = (int)i;
                break;
            }
        }
    }
    REQUIRE(g_index != -1);
    return g_index;
}

void set(Var var, const Value &value) {
    flow::setGlobalVariable(markerIndex() + var, value);
}

Value get(Var var) {
    return flow::getGlobalVariable(markerIndex() + var);
}

Value str(const char *s) {
    return Value::makeStringRef(s, -1, 0x5717e57a);
}

Value intValue(int i) {
    return Value(i, VALUE_TYPE_INT32);
}

Value floatValue(float f) {
    return Value(f, VALUE_TYPE_FLOAT);
}

Value doubleValue(double d) {
    return Value(d, VALUE_TYPE_DOUBLE);
}

Value boolValue(bool b) {
    return Value(b, VALUE_TYPE_BOOLEAN);
}

Value intArray(std::initializer_list<int> items) {
    Value array = Value::makeArrayRef((int)items.size(), flow::defs_v3::ARRAY_TYPE_INTEGER, 0x7a11a001);
    int i = 0;
    for (auto item : items) {
        array.getArray()->values[i++] = intValue(item);
    }
    return array;
}

Value doubleArray(std::initializer_list<double> items) {
    Value array = Value::makeArrayRef((int)items.size(), flow::defs_v3::ARRAY_TYPE_DOUBLE, 0x7a11a002);
    int i = 0;
    for (auto item : items) {
        array.getArray()->values[i++] = doubleValue(item);
    }
    return array;
}

Value stringArray(std::initializer_list<const char *> items) {
    Value array = Value::makeArrayRef((int)items.size(), flow::defs_v3::ARRAY_TYPE_STRING, 0x7a11a003);
    int i = 0;
    for (auto item : items) {
        array.getArray()->values[i++] = str(item);
    }
    return array;
}

Value anyArray(std::initializer_list<Value> items) {
    Value array = Value::makeArrayRef((int)items.size(), flow::defs_v3::ARRAY_TYPE_ANY, 0x7a11a004);
    int i = 0;
    for (auto &item : items) {
        array.getArray()->values[i++] = item;
    }
    return array;
}

Widget *pageWidget(int pageId, int index) {
    auto page = getPageAsset(pageId);
    return (Widget *)static_cast<const Widget *>(page->widgets[index]);
}

flow::FlowState *pageFlowState(int pageId) {
    return flow::getPageFlowState(g_mainAssets, pageId - 1, WidgetCursor());
}

} // namespace wt
