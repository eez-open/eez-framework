/*
 * eez-framework tests: Canvas (native data), LineChart, Gauge, QRCode widgets
 * (pages w_canvas, w_lchart*, w_gauge, w_qr).
 */

#include <math.h>
#include <unistd.h>

#include "wtest.h"

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

const int CV[] = { 10, 10, 100, 100 };

// w_lchart: margins top 10, right 10, bottom 30, left 50; line red
const int LC[] = { 10, 10, 400, 200 };
// w_lchart_legend: the same chart at the top and lower on the page
const int LC_TOP[] = { 10, 10, 400, 150 };
const int LC_LOW[] = { 10, 300, 400, 150 };

// w_gauge: center (150, 192), bar ring radius 92 .. 112
const int GA[] = { 10, 10, 300, 200 };

const int QR[] = { 10, 10, 200, 200 };

#define RECT(R) R[0], R[1], R[2], R[3]

// point of the gauge bar ring at the angle (degrees, counterclockwise from the right)
void gaugeBarPoint(float angleDeg, int &x, int &y) {
    float a = angleDeg * 3.14159265f / 180.0f;
    x = GA[0] + (int)lroundf(150 + 102 * cosf(a));
    y = GA[1] + (int)lroundf(192 - 102 * sinf(a));
}

bool gaugeBarAt(float angleDeg) {
    int x, y;
    gaugeBarPoint(angleDeg, x, y);
    return rgb(x, y) == GREEN;
}

void gaugeValues(Value value, Value min, Value max, Value unit) {
    set(V_A, value);
    set(V_B, min);
    set(V_C, max);
    set(V_D, intValue(1000)); // threshold outside of the range
    set(V_E, unit);
}

// points [x, y] for the line charts
void chartPoints() {
    set(V_LC_POINTS, anyArray({ doubleArray({ 0, 1 }), doubleArray({ 1, 3 }), doubleArray({ 2, 2 }) }));
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// Canvas

TEST("widgets/canvas: drawn by the draw function of the data") {
    g_native.canvasColor = RED;
    show(PAGE_ID_W_CANVAS);
    CHECK(g_native.canvasDraws >= 1);
    CHECK_EQ(countOther(RECT(CV), RED), 0);
}

TEST("widgets/canvas: drawn again only when the refresh state changes") {
    g_native.canvasColor = RED;
    show(PAGE_ID_W_CANVAS);
    int draws = g_native.canvasDraws;
    CHECK_EQ(framesWithDrawing(5), 0);
    CHECK_EQ(g_native.canvasDraws, draws);
    g_native.canvasColor = GREEN;
    g_native.canvasRefresh++;
    harness::frame();
    CHECK_EQ(g_native.canvasDraws, draws + 1);
    CHECK_EQ(countOther(RECT(CV), GREEN), 0);
}

////////////////////////////////////////////////////////////////////////////////
// LineChart (points from the page open flow: Evaluate w_lc_points -> value input)

TEST("widgets/linechart: the line of the points is drawn") {
    chartPoints();
    show(PAGE_ID_W_LCHART);
    harness::frame(3);
    CHECK_EQ(harness::flowErrors().size(), 0u);
    // in the grid area
    CHECK(count(LC[0] + 50, LC[1] + 10, 340, 160, RED) > 10);
    CHECK_EQ(framesWithDrawing(3), 0);
}

TEST("widgets/linechart: without points nothing is drawn") {
    // no execution state before the first input value
    set(V_LC_POINTS, anyArray({}));
    show(PAGE_ID_W_LCHART);
    harness::frame(3);
    CHECK_EQ(count(RECT(LC), RED), 0);
}

TEST("widgets/linechart: a plot lower than one y label is drawn") {
    // grid height 50 - 10 - 30 = 10 px: no y tick fits (W5: the tick delta was
    // infinite and the grid loop never ended)
    alarm(5);
    chartPoints();
    show(PAGE_ID_W_LCHART_SMALL);
    harness::frame(3);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_LCHART_SMALL);
}

TEST_BUG("widgets/linechart: the legend is drawn also lower on the page", "W16") {
    chartPoints();
    show(PAGE_ID_W_LCHART_LEGEND);
    harness::frame(3);
    // legend icons (line color) right of the grid
    int top = count(LC_TOP[0] + 362, LC_TOP[1], 24, LC_TOP[3], RED);
    int low = count(LC_LOW[0] + 362, LC_LOW[1], 24, LC_LOW[3], RED);
    CHECK(top > 0);
    // the legend y (absolute) is compared with the grid bottom (relative)
    CHECK_EQ(low, top);
}

////////////////////////////////////////////////////////////////////////////////
// Gauge

TEST("widgets/gauge: the bar goes from the left to the angle of the value") {
    gaugeValues(intValue(50), intValue(0), intValue(100), str("V"));
    show(PAGE_ID_W_GAUGE);
    CHECK(gaugeBarAt(170));
    CHECK(gaugeBarAt(100));
    CHECK(!gaugeBarAt(80));
    CHECK(!gaugeBarAt(10));
    CHECK_EQ(framesWithDrawing(3), 0);
    set(V_A, intValue(100));
    harness::frame();
    CHECK(gaugeBarAt(10));
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/gauge: NaN value and min above max draw an empty bar") {
    gaugeValues(Value(NAN, VALUE_TYPE_FLOAT), intValue(0), intValue(100), str("V"));
    show(PAGE_ID_W_GAUGE);
    CHECK(!gaugeBarAt(170));
    gaugeValues(intValue(50), intValue(100), intValue(0), str("V"));
    harness::frame(2);
    CHECK(!gaugeBarAt(170));
}

TEST("widgets/gauge: a gauge without a unit is drawn") {
    // the unit string is kept while it is used (W4: it came from a temporary
    // freed at the end of the statement)
    gaugeValues(intValue(50), intValue(0), intValue(100), Value());
    show(PAGE_ID_W_GAUGE);
    CHECK(gaugeBarAt(170));
}

TEST("widgets/gauge: a narrow range at a large value is drawn") {
    // tick step 2 is below the float spacing (8) at 1e8 (W5: the tick loop
    // never ended)
    alarm(5);
    gaugeValues(Value(100000004.0f, VALUE_TYPE_FLOAT), Value(100000000.0f, VALUE_TYPE_FLOAT),
        Value(100000008.0f, VALUE_TYPE_FLOAT), str("V"));
    show(PAGE_ID_W_GAUGE);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_GAUGE);
}

////////////////////////////////////////////////////////////////////////////////
// QRCode

TEST("widgets/qrcode: dark and light modules with a border") {
    set(V_A, str("hello"));
    show(PAGE_ID_W_QR);
    CHECK(count(RECT(QR), WHITE) > 1000);
    CHECK(count(RECT(QR), BLACK) > 1000);
    // the border (one module) is the background
    CHECK_EQ(countOther(QR[0], QR[1], QR[2], 5, BLACK), 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    auto hello = block(RECT(QR));
    set(V_A, str("other"));
    harness::frame();
    CHECK(drawn());
    CHECK(diffBlock(hello, RECT(QR)) > 0);
}

TEST("widgets/qrcode: data that is not a string draws an empty code") {
    // a value that is not a string is encoded as its text (W7: encodeText got
    // nullptr from data.getString())
    set(V_A, Value());
    show(PAGE_ID_W_QR);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_W_QR);
}
