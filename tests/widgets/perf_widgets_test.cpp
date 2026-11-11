/*
 * eez-framework tests: per-frame work of the widgets (WP2, NEW-widgets-1,
 * CP3): hidden widgets, the Input string, List data with a scrollbar state and
 * the YTChart trace expressions.
 *
 * The YTChart has no page in the test project: its widget, flow component and
 * traces are built in memory (YtChart below) and its updateState is called
 * directly.
 */

#include <cmath>
#include <string.h>

#include "wtest.h"

#include <eez/flow/flow.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/hooks.h>
#include <eez/flow/private.h>
#include <eez/flow/components/yt_chart_widget.h>
#include <eez/gui/widgets/yt_chart.h>

#if defined(__SANITIZE_ADDRESS__)
// sanitizer/allocator_interface.h (the header is not always installed)
extern "C" int __sanitizer_install_malloc_and_free_hooks(
    void (*mallocHook)(const volatile void *, size_t),
    void (*freeHook)(const volatile void *)
);
#endif

using namespace eez;
using namespace eez::gui;
using namespace wt;

namespace {

// w_text
const int T_DATA[] = { 10, 60, 200, 40 };

#define RECT(R) R[0], R[1], R[2], R[3]

// visible of a widget asset, restored at the end of the test
struct VisibleOverride {
    Widget *widget;
    int16_t saved;

    VisibleOverride(Widget *widget_, int16_t visible) : widget(widget_), saved(widget_->visible) {
        widget->visible = visible;
    }

    ~VisibleOverride() {
        widget->visible = saved;
    }
};

#if defined(__SANITIZE_ADDRESS__)
int g_mallocs;
bool g_countMallocs;

void mallocHook(const volatile void *, size_t) {
    if (g_countMallocs) {
        g_mallocs++;
    }
}

void freeHook(const volatile void *) {
}
#endif

////////////////////////////////////////////////////////////////////////////////
// YTChart built in memory

int globalVariableIndex(Var var) {
    uint32_t count = g_mainAssets->flowDefinition->globalVariables.count;
    for (uint32_t i = 0; i < count; i++) {
        Value value = flow::getGlobalVariable(i);
        if (value.isString() && value.getString() && strcmp(value.getString(), "w_marker") == 0) {
            return (int)i + var;
        }
    }
    REQUIRE(false);
    return -1;
}

// index of a constant of the flow definition with a number that is not 0
int findNumberConstant(float &number) {
    auto &constants = g_mainAssets->flowDefinition->constants;
    for (uint32_t i = 0; i < constants.count; i++) {
        const Value *value = constants[i];
        if ((value->isInt32OrLess() || value->isFloat() || value->isDouble()) && value->toFloat() != 0) {
            number = value->toFloat();
            return (int)i;
        }
    }
    REQUIRE(false);
    return -1;
}

uint16_t pushConstant(int index) {
    return EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT | (uint16_t)index;
}

uint16_t pushGlobal(Var var) {
    return EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR | (uint16_t)globalVariableIndex(var);
}

// a native variable is pushed as a global variable after the global variables
uint16_t pushNative(int dataId) {
    return EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR | (uint16_t)(g_mainAssets->flowDefinition->globalVariables.count + dataId - 1);
}

uint32_t g_ytPosition;
float g_ytPeriod = 0.001f;

bool ytHistory(uint32_t, uint32_t, float *values, uint32_t count, uint32_t &position, float &period) {
    for (uint32_t i = 0; i < count; i++) {
        values[i] = 0.5f;
    }
    position = g_ytPosition;
    period = g_ytPeriod;
    return true;
}

struct YtChart {
    static const int MAX_TRACES = 3;

    // ListOfAssetsPtr: count and the offset of the items from the offset field
    struct RawList {
        uint32_t count;
        int32_t offset;
    };

    // assets of the widget and its flow, in one block (the offsets are small)
    struct Memory {
        YTChartWidget widget;
        Flow flow;
        AssetsPtr<Component> componentItems[1];
        flow::YTChartWidgetComponent component;
        AssetsPtr<flow::YTChartTrace> traceItems[MAX_TRACES];
        flow::YTChartTrace traces[MAX_TRACES];
        // min, max and visible of each trace: [instruction] [END]
        uint8_t expressions[MAX_TRACES][3][4];
    };

    Memory *m;
    flow::FlowState *flowState;
    YTChartWidgetState *state;
    bool hasPreviousState = false;
    decltype(flow::getModuleFieldYtHistoryHook) savedHook;

    static void setList(void *list, void *items, uint32_t count) {
        auto raw = (RawList *)list;
        raw->count = count;
        raw->offset = (int32_t)((uint8_t *)items - (uint8_t *)&raw->offset);
    }

    static void setExpression(uint8_t *bytes, uint16_t instruction) {
        bytes[0] = (uint8_t)instruction;
        bytes[1] = (uint8_t)(instruction >> 8);
        bytes[2] = (uint8_t)EXPR_EVAL_INSTRUCTION_TYPE_END;
        bytes[3] = (uint8_t)(EXPR_EVAL_INSTRUCTION_TYPE_END >> 8);
    }

    YtChart() {
        m = new Memory();

        m->widget.type = WIDGET_TYPE_YT_CHART;
        m->widget.width = 100;
        m->widget.height = 50;
        m->widget.style = 1;
        m->widget.mode = 0; // no data: scroll mode
        m->widget.componentIndex = 0;

        setList(&m->flow.components, m->componentItems, 1);
        m->componentItems[0] = &m->component;
        setList(&m->component.traces, m->traceItems, 0);
        for (int i = 0; i < MAX_TRACES; i++) {
            m->traceItems[i] = &m->traces[i];
            m->traces[i].bit = -1;
            m->traces[i].type = YT_CHART_TRACE_TYPE_FIELD;
            m->traces[i].min = m->expressions[i][0];
            m->traces[i].max = m->expressions[i][1];
            m->traces[i].visible = m->expressions[i][2];
        }

        flowState = (flow::FlowState *)calloc(1, sizeof(flow::FlowState));
        flowState->assets = g_mainAssets;
        flowState->flow = &m->flow;

        state = new YTChartWidgetState();

        savedHook = flow::getModuleFieldYtHistoryHook;
        flow::getModuleFieldYtHistoryHook = ytHistory;
        g_ytPosition = 100;
    }

    ~YtChart() {
        flow::getModuleFieldYtHistoryHook = savedHook;
        delete state;
        ::free(flowState);
        delete m;
    }

    void trace(int i, uint16_t min, uint16_t max, uint16_t visible, int8_t bit = -1) {
        setExpression(m->expressions[i][0], min);
        setExpression(m->expressions[i][1], max);
        setExpression(m->expressions[i][2], visible);
        m->traces[i].bit = bit;
        auto raw = (RawList *)&m->component.traces;
        if ((int)raw->count < i + 1) {
            raw->count = i + 1;
        }
    }

    // updateState of the widget as in a frame, true if it is drawn again
    bool update() {
        WidgetCursor saved = g_widgetCursor;
        g_widgetCursor = WidgetCursor(g_mainAssets, harness::appContext(), &m->widget, 0, 0, state, false, hasPreviousState);
        g_widgetCursor.w = m->widget.width;
        g_widgetCursor.h = m->widget.height;
        g_widgetCursor.flowState = flowState;
        bool refresh = state->updateState();
        g_widgetCursor = saved;
        hasPreviousState = true;
        return refresh;
    }

    int framesWithRefresh(int frames) {
        int n = 0;
        for (int i = 0; i < frames; i++) {
            if (update()) {
                n++;
            }
        }
        return n;
    }
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// hidden widgets

TEST("widgets/visible: a widget shows the data changed while it was hidden") {
    // for this test visible of text/data (w_a) is the data of text/fit (w_b)
    VisibleOverride visible(pageWidget(PAGE_ID_W_TEXT, 1), pageWidget(PAGE_ID_W_TEXT, 5)->data);
    set(V_A, str("one"));
    set(V_B, boolValue(true));
    show(PAGE_ID_W_TEXT);
    CHECK(count(RECT(T_DATA), YELLOW) > 0);
    auto one = block(RECT(T_DATA));

    set(V_B, boolValue(false));
    harness::frame();
    CHECK_EQ(countOther(RECT(T_DATA), BLACK), 0);

    // the hidden widget is not drawn
    set(V_A, str("two two"));
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(countOther(RECT(T_DATA), BLACK), 0);

    set(V_B, boolValue(true));
    harness::frame();
    CHECK(count(RECT(T_DATA), YELLOW) > 0);
    CHECK(diffBlock(one, RECT(T_DATA)) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

TEST("widgets/visible: a hidden container is drawn again with its children when it is visible") {
    set(V_BOOL, boolValue(true));
    show(PAGE_ID_W_CONT);
    // cont/static: gray container with a text
    auto shown = block(10, 10, 300, 100);
    set(V_BOOL, boolValue(false));
    harness::frame();
    CHECK_EQ(countOther(10, 10, 300, 100, BLACK), 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    set(V_BOOL, boolValue(true));
    harness::frame();
    CHECK_EQ(diffBlock(shown, 10, 10, 300, 100), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// Input

TEST("widgets/input: a text input with an unchanged string doesn't allocate in a frame") {
    set(V_A, str("text"));
    set(V_B, str("secret"));
    set(V_C, doubleValue(1.5));
    set(V_D, str("other"));
    show(PAGE_ID_W_INPUT);
    CHECK_EQ(framesWithDrawing(3), 0);
#if defined(__SANITIZE_ADDRESS__)
    // All the mallocs are counted with the sanitizer hooks (not in the build
    // without the sanitizers). The three text inputs allocated two blocks each
    // in every frame, the GUI message queue (std::deque) of the PC build
    // allocates a block now and then.
    __sanitizer_install_malloc_and_free_hooks(mallocHook, freeHook);
    g_mallocs = 0;
    g_countMallocs = true;
    harness::frame(20);
    g_countMallocs = false;
    CHECK(g_mallocs < 5);
#endif
}

TEST("widgets/input: a string changed in place in the same buffer is drawn again") {
    // the string of the data is compared, not the pointer
    static char buffer[16] = "abc";
    set(V_A, Value(buffer, VALUE_TYPE_STRING));
    show(PAGE_ID_W_INPUT);
    auto before = block(10, 10, 200, 40);
    strcpy(buffer, "xyz");
    harness::frame();
    CHECK(drawn());
    CHECK(diffBlock(before, 10, 10, 200, 40) > 0);
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// List

TEST("widgets/list: a list with a scrollbar state takes the position from it") {
    // list/v with a scrollbar state (100 items) as its data
    Value sb = get(V_SB);
    REQUIRE(sb.isArray());
    set(V_A, sb);
    show(PAGE_ID_W_LIST);
    CHECK_EQ(framesWithDrawing(3), 0);
    sb.getArray()->values[flow::defs_v3::SYSTEM_STRUCTURE_SCROLLBAR_STATE_FIELD_POSITION] = intValue(3);
    harness::frame();
    CHECK(drawn());
    CHECK_EQ(framesWithDrawing(3), 0);
    CHECK_EQ(diffWithFullRedraw(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// YTChart

TEST("widgets/ytchart: min and max of a hidden trace and of a bit trace are not evaluated") {
    float number;
    int constant = findNumberConstant(number);
    set(V_BOOL, boolValue(false));

    YtChart chart;
    // value trace, bit trace, hidden value trace: min and max count the evaluations
    chart.trace(0, pushNative(DATA_ID_W_COUNTER_A), pushConstant(constant), pushNative(DATA_ID_W_COUNTER_B));
    chart.trace(1, pushNative(DATA_ID_W_COUNTER_A), pushNative(DATA_ID_W_COUNTER_A), pushConstant(constant), 3);
    chart.trace(2, pushNative(DATA_ID_W_COUNTER_A), pushNative(DATA_ID_W_COUNTER_A), pushGlobal(V_BOOL));

    CHECK(chart.update());
    int a = g_native.counterA;
    int b = g_native.counterB;
    CHECK_EQ(chart.framesWithRefresh(5), 0);
    // only min of the first trace, its max is a constant
    CHECK_EQ(g_native.counterA - a, 5);
    CHECK_EQ(g_native.counterB - b, 5);

    // the hidden trace is visible: its min and max are evaluated, the chart is drawn again
    set(V_BOOL, boolValue(true));
    a = g_native.counterA;
    CHECK(chart.update());
    CHECK_EQ(g_native.counterA - a, 3);
    CHECK(chart.state->traceVisible[2]);
    CHECK_EQ(chart.framesWithRefresh(3), 0);
}

TEST("widgets/ytchart: a constant min is used when a hidden trace is visible") {
    float number;
    int constant = findNumberConstant(number);
    set(V_BOOL, boolValue(false));

    YtChart chart;
    chart.trace(0, pushConstant(constant), pushConstant(constant), pushGlobal(V_BOOL));
    CHECK(chart.update());
    CHECK_EQ(chart.framesWithRefresh(3), 0);
    CHECK(!chart.state->traceVisible[0]);

    set(V_BOOL, boolValue(true));
    CHECK(chart.update());
    CHECK(chart.state->traceVisible[0]);
    CHECK_EQ(chart.state->traceMin[0], number);
    CHECK_EQ(chart.state->traceMax[0], number);
    CHECK_EQ(chart.framesWithRefresh(3), 0);
}

TEST("widgets/ytchart: NaN max and NaN period are not drawn again in every frame") {
    float number;
    int constant = findNumberConstant(number);
    set(V_DOUBLE, doubleValue(NAN));

    YtChart chart;
    chart.trace(0, pushConstant(constant), pushGlobal(V_DOUBLE), pushConstant(constant));
    CHECK(chart.update());
    CHECK(std::isnan(chart.state->traceMax[0]));
    CHECK_EQ(chart.framesWithRefresh(5), 0);

    g_ytPeriod = NAN;
    CHECK(chart.update());
    CHECK_EQ(chart.framesWithRefresh(5), 0);
    g_ytPeriod = 0.001f;

    // a change of max and of the position still draws the chart again
    set(V_DOUBLE, doubleValue(10));
    CHECK(chart.update());
    CHECK_EQ(chart.state->traceMax[0], 10.0f);
    g_ytPosition++;
    CHECK(chart.update());
    CHECK_EQ(chart.framesWithRefresh(3), 0);
}
