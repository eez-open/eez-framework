/*
 * eez-framework tests: helpers of the widget tests (tests/widgets/).
 *
 * The pages, styles and variables are in projects/parts/widgets.py (prefix w_).
 * Colors are 0xRRGGBB values of the presented frame (harness::pixel), the
 * project colors (the assets have 32-bit colors).
 */

#pragma once

#include <stdint.h>

#include <initializer_list>
#include <string>
#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/core/value.h>
#include <eez/gui/gui.h>

#include "document.h"
#include "harness.h"
#include "test.h"

namespace eez {
namespace flow {
struct FlowState;
}
}

namespace wt {

using eez::Value;

// colors of the project (widgets.py COLORS and base.py)
static const uint32_t BLACK = 0x000000;
static const uint32_t WHITE = 0xF8FCF8;     // w_white #f8fcf8
static const uint32_t RED = 0xF80000;
static const uint32_t GREEN = 0x00FC00;
static const uint32_t BLUE = 0x0000F8;
static const uint32_t YELLOW = 0xF8FC00;
static const uint32_t GRAY = 0x808080;
// "text" (#ffffff), the color of the default style
static const uint32_t TEXT = 0xFFFFFF;

// color of the presented frame at x, y as 0xRRGGBB
uint32_t rgb(int x, int y);
std::string hex(uint32_t color);

// shows the page and runs frames until it is drawn
void show(int pageId);

// true if anything was drawn in the last frame
bool drawn();
// runs frames, returns in how many of them something was drawn
int framesWithDrawing(int frames);

// pixels of the rectangle with the color
int count(int x, int y, int w, int h, uint32_t color);
// rows of the rectangle with at least one pixel of the color
int rowsWith(int x, int y, int w, int h, uint32_t color);
// groups of rows with the color, separated by at least 3 rows without it
// (lines of text)
int textLines(int x, int y, int w, int h, uint32_t color);
// pixels of the rectangle with other colors
int countOther(int x, int y, int w, int h, uint32_t color);
// number of different pixels of two rectangles of the same size
int diffBlocks(int x1, int y1, int x2, int y2, int w, int h);
// pixels of the rectangle, and the number of pixels that differ from them now
std::vector<uint32_t> block(int x, int y, int w, int h);
int diffBlock(const std::vector<uint32_t> &pixels, int x, int y, int w, int h);

// The presented frame (after the incremental updates of the frames before)
// compared with a full redraw of the same state: refreshScreen, one frame.
// Returns the number of different pixels, the first one is described in where.
int diffWithFullRedraw(std::string *where = nullptr);

// touch: press and release with frames in between (the next touch is far
// enough in time not to be taken as an accidental double tap)
void tap(int x, int y);
void press(int x, int y);
void release();

// global variables of widgets.py (GLOBALS), in the same order, found after w_marker
enum Var {
    V_MARKER,
    V_A,
    V_B,
    V_C,
    V_D,
    V_E,
    V_INT,
    V_INT2,
    V_BOOL,
    V_BOOL2,
    V_STR,
    V_DOUBLE,
    V_SB,
    V_LC_POINTS,
    V_STRS
};

void set(Var var, const Value &value);
Value get(Var var);

Value str(const char *s);
Value intValue(int i);
Value floatValue(float f);
Value doubleValue(double d);
Value boolValue(bool b);
Value intArray(std::initializer_list<int> items);
Value doubleArray(std::initializer_list<double> items);
Value stringArray(std::initializer_list<const char *> items);
// array of ARRAY_TYPE_ANY with the given elements
Value anyArray(std::initializer_list<Value> items);

// widget asset: index-th widget of the page (in the order of widgets.py)
eez::gui::Widget *pageWidget(int pageId, int index);
eez::flow::FlowState *pageFlowState(int pageId);

////////////////////////////////////////////////////////////////////////////////
// native variables and actions (native.cpp)

struct YtData {
    uint8_t method = 2; // YT_GRAPH_UPDATE_METHOD_STATIC
    uint32_t position = 0;
    uint32_t size = 0;
    std::vector<float> values; // value of the value index 0 at the position
    float min = 0;
    float max = 100;
    float div = 25;
    float offset = 0;
    int vertDivisions = 4;
    int horzDivisions = 4;
    uint32_t refreshCounter = 0;
    int touchDrags = 0;
};

struct Native {
    std::string ell = "";
    uint16_t ellOptions = 0;
    std::string ref = "";
    std::string xscrollText = "";
    int xscroll = 0;
    int counterA = 0;
    int counterB = 0;
    YtData yt;
    int canvasRefresh = 0;
    int canvasDraws = 0;
    uint32_t canvasColor = 0;
    int rol = 0;
    int rolSets = 0;
    eez::Assets *probeAssets = nullptr;
    int probeCalls = 0;
    int clicks = 0;
    int clicks2 = 0;
    int counterArrGets = 0;
    int counterArrSize = 2;
};

extern Native g_native;

} // namespace wt
