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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <eez/core/alloc.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/yt_chart.h>

#include <eez/flow/expression.h>
#include <eez/flow/hooks.h>
#include <eez/flow/components/yt_chart_widget.h>

// cleared columns after the cursor in the scan mode
#define BLANK_COLUMNS_AFTER_CURSOR 10

namespace eez {
namespace gui {

// Pixels of the graph, see YTChartWidgetState::buffer
struct YTChartCanvas {
    display::Color *pixels;
    int w;
    int h;

    void fill(display::Color color) {
        for (int i = 0; i < w * h; i++) {
            pixels[i] = color;
        }
    }

    // vertical line from y1 to y2, both included
    void drawVLine(int x, int y1, int y2, display::Color color) {
        if (x < 0 || x >= w) {
            return;
        }
        if (y1 < 0) {
            y1 = 0;
        }
        if (y2 > h - 1) {
            y2 = h - 1;
        }
        for (int y = y1; y <= y2; y++) {
            pixels[y * w + x] = color;
        }
    }

    // moves all the pixels n columns to the left
    void scrollLeft(int n) {
        if (n <= 0 || n >= w) {
            return;
        }
        for (int y = 0; y < h; y++) {
            auto row = pixels + y * w;
            memmove(row, row + n, (w - n) * sizeof(display::Color));
        }
    }
};

static flow::YTChartWidgetComponent *getComponent(const WidgetCursor &widgetCursor, const YTChartWidget *widget) {
    if (!widgetCursor.flowState) {
        return nullptr;
    }
    return (flow::YTChartWidgetComponent *)widgetCursor.flowState->flow->components[widget->componentIndex];
}

// as DibModule.field: slot of the module (assets reserved) and field index
static uint32_t getModuleFieldId(const WidgetCursor &widgetCursor, uint16_t fieldIndex) {
    uint16_t slotIndex = (uint16_t)widgetCursor.flowState->assets->reserved;
    return ((uint32_t)slotIndex << 16) | fieldIndex;
}

// The position of the newest YT value of the trace and the seconds per value,
// false if it has no YT history. A product has the values both fields have.
static bool getTraceNewest(const WidgetCursor &widgetCursor, const flow::YTChartTrace *trace, uint32_t &position, float &period) {
    if (trace->type != YT_CHART_TRACE_TYPE_FIELD && trace->type != YT_CHART_TRACE_TYPE_PRODUCT) {
        return false;
    }
    if (!flow::getModuleFieldYtHistoryHook(getModuleFieldId(widgetCursor, trace->fieldIndex), flow::YT_HISTORY_NEWEST, nullptr, 0, position, period)) {
        return false;
    }
    if (trace->type == YT_CHART_TRACE_TYPE_PRODUCT) {
        uint32_t position2;
        float period2;
        if (!flow::getModuleFieldYtHistoryHook(getModuleFieldId(widgetCursor, trace->field2Index), flow::YT_HISTORY_NEWEST, nullptr, 0, position2, period2)) {
            return false;
        }
        if (position2 < position) {
            position = position2;
        }
    }
    return true;
}

// The count YT values of the trace that end before endPosition, NAN where
// there is no value. values2 has room for count values (the second field of a
// product). Returns false if the trace has no YT history.
static bool getTraceValues(const WidgetCursor &widgetCursor, const flow::YTChartTrace *trace, uint32_t endPosition, float *values, float *values2, uint32_t count) {
    if (trace->type != YT_CHART_TRACE_TYPE_FIELD && trace->type != YT_CHART_TRACE_TYPE_PRODUCT) {
        return false;
    }
    uint32_t position;
    float period;
    if (!flow::getModuleFieldYtHistoryHook(getModuleFieldId(widgetCursor, trace->fieldIndex), endPosition, values, count, position, period)) {
        return false;
    }
    if (trace->type == YT_CHART_TRACE_TYPE_PRODUCT) {
        if (!flow::getModuleFieldYtHistoryHook(getModuleFieldId(widgetCursor, trace->field2Index), endPosition, values2, count, position, period)) {
            return false;
        }
        for (uint32_t i = 0; i < count; i++) {
            values[i] *= values2[i];
        }
    }
    return true;
}

// y of the value, INT_MIN if there is no value, can be outside the canvas
static int getY(float value, float min, float max, int h) {
    if (isNaN(value) || !(max > min)) {
        return INT_MIN;
    }
    return h - 1 - (int)roundf((h - 1) * (value - min) / (max - min));
}

// Column of a trace: the value and the line from the value in the previous
// column.
static void drawTraceColumn(YTChartCanvas &canvas, int x, int yPrev, int y, display::Color color) {
    if (y == INT_MIN) {
        return;
    }

    int y1, y2;
    if (yPrev == INT_MIN || abs(yPrev - y) <= 1) {
        y1 = y;
        y2 = y;
    } else if (yPrev < y) {
        y1 = yPrev + 1;
        y2 = y;
    } else {
        y1 = y;
        y2 = yPrev - 1;
    }

    if (y2 < 0 || y1 >= canvas.h) {
        return;
    }

    canvas.drawVLine(x, y1, y2, color);
}

#define BIT_LEVEL_NONE -1
#define BIT_LEVEL_LOW 0
#define BIT_LEVEL_HIGH 1
// the bit changed in the period of the value
#define BIT_LEVEL_CHANGED 2

// Level of the bit in a YT value of a bit field (AND of the samples in bits
// 0..7, OR in bits 8..15).
static int getBitLevel(float value, int bit) {
    if (isNaN(value) || bit < 0 || bit > 7) {
        return BIT_LEVEL_NONE;
    }
    uint32_t v = (uint32_t)value;
    uint32_t andBit = (v >> bit) & 1;
    uint32_t orBit = (v >> (bit + 8)) & 1;
    if (andBit != orBit) {
        return BIT_LEVEL_CHANGED;
    }
    return andBit ? BIT_LEVEL_HIGH : BIT_LEVEL_LOW;
}

// Column of a bit trace in its lane: the level, the edge from the level in
// the previous column, the whole swing if the bit changed in the period.
static void drawBitColumn(YTChartCanvas &canvas, int x, int levelPrev, int level, int laneTop, int laneHeight, display::Color color) {
    if (level == BIT_LEVEL_NONE) {
        return;
    }

    int margin = laneHeight >= 5 ? laneHeight / 5 : 0;
    int yHigh = laneTop + margin;
    int yLow = laneTop + laneHeight - 1 - margin;

    bool edge = (levelPrev == BIT_LEVEL_LOW || levelPrev == BIT_LEVEL_HIGH) && levelPrev != level;
    if (level == BIT_LEVEL_CHANGED || edge) {
        canvas.drawVLine(x, yHigh, yLow, color);
    } else {
        int y = level == BIT_LEVEL_HIGH ? yHigh : yLow;
        canvas.drawVLine(x, y, y, color);
    }
}

YTChartWidgetState::~YTChartWidgetState() {
    eez::free(buffer);
}

// NaN never compares equal: it would be taken as a change in every frame
static bool isSameFloat(float a, float b) {
    return a == b || (isNaN(a) && isNaN(b));
}

// the expression is only a constant: [PUSH_CONSTANT index] [END]
static bool isConstantExpression(const uint8_t *instructions) {
    uint16_t instruction = instructions[0] + (instructions[1] << 8);
    uint16_t nextInstruction = instructions[2] + (instructions[3] << 8);
    return
        (instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK) == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT &&
        nextInstruction == EXPR_EVAL_INSTRUCTION_TYPE_END;
}

bool YTChartWidgetState::updateState() {
    WIDGET_STATE_START(YTChartWidget);

    // new widget or a new size
    if (!hasPreviousState) {
        redrawAll = true;
    }

    WIDGET_STATE(flags.active, g_isActiveWidget);

    uint8_t newMode = get(widgetCursor, widget->mode).toInt32() == YT_CHART_MODE_SCAN ? YT_CHART_MODE_SCAN : YT_CHART_MODE_SCROLL;
    if (!hasPreviousState || newMode != mode) {
        mode = newMode;
        redrawAll = true;
        hasPreviousState = false;
    }

    auto component = getComponent(widgetCursor, widget);

    uint8_t newNumTraces = 0;
    if (component) {
        newNumTraces = component->traces.count > YT_CHART_MAX_TRACES ? YT_CHART_MAX_TRACES : (uint8_t)component->traces.count;
    }
    if (!hasPreviousState || newNumTraces != numTraces) {
        numTraces = newNumTraces;
        redrawAll = true;
        hasPreviousState = false;
    }

    // The trace expressions are evaluated in every frame (what they depend on
    // is not known), except an expression that is only a constant (evaluated
    // for a new state or after a change) and min and max of a trace that
    // doesn't use them, a hidden trace and a bit trace (they are evaluated
    // again when the trace is visible, the change redraws the chart).
    for (uint8_t i = 0; i < numTraces; i++) {
        auto trace = component->traces[i];

        Value value;

        bool visible = traceVisible[i];
        if (!hasPreviousState || !isConstantExpression(trace->visible)) {
            visible = true;
            if (flow::evalExpression(widgetCursor.flowState, widget->componentIndex, trace->visible, value, flow::FlowError::Plain("Failed to evaluate trace visible in YTChart widget"), nullptr, widgetCursor.iterators)) {
                visible = value.toBool(nullptr);
            }
        }

        int8_t bit = trace->type == YT_CHART_TRACE_TYPE_FIELD && trace->bit >= 0 && trace->bit <= 7 ? trace->bit : -1;

        bool changed = !hasPreviousState || visible != traceVisible[i] || bit != traceBit[i];

        float min = traceMin[i];
        float max = traceMax[i];
        if (visible && bit == -1) {
            if (changed || !isConstantExpression(trace->min)) {
                min = 0;
                if (flow::evalExpression(widgetCursor.flowState, widget->componentIndex, trace->min, value, flow::FlowError::Plain("Failed to evaluate trace min in YTChart widget"), nullptr, widgetCursor.iterators)) {
                    min = value.toFloat(nullptr);
                }
            }

            if (changed || !isConstantExpression(trace->max)) {
                max = 1.0f;
                if (flow::evalExpression(widgetCursor.flowState, widget->componentIndex, trace->max, value, flow::FlowError::Plain("Failed to evaluate trace max in YTChart widget"), nullptr, widgetCursor.iterators)) {
                    max = value.toFloat(nullptr);
                }
            }
        }

        if (changed || !isSameFloat(min, traceMin[i]) || !isSameFloat(max, traceMax[i])) {
            traceVisible[i] = visible;
            traceMin[i] = min;
            traceMax[i] = max;
            traceBit[i] = bit;
            redrawAll = true;
            hasPreviousState = false;
        }
    }

    // the newest value all the visible traces have
    uint32_t newPosition = 0;
    float newPeriod = 0;
    if (component && flow::getModuleFieldYtHistoryHook) {
        bool first = true;
        for (uint8_t i = 0; i < numTraces; i++) {
            if (!traceVisible[i]) {
                continue;
            }
            uint32_t tracePosition;
            float tracePeriod;
            if (getTraceNewest(widgetCursor, component->traces[i], tracePosition, tracePeriod)) {
                if (first || tracePosition < newPosition) {
                    newPosition = tracePosition;
                }
                if (first) {
                    newPeriod = tracePeriod;
                }
                first = false;
            }
        }
    }

    if (!hasPreviousState || !isSameFloat(newPeriod, period)) {
        period = newPeriod;
        redrawAll = true;
        hasPreviousState = false;
    }

    WIDGET_STATE(position, newPosition);

    WIDGET_STATE_END()
}

void YTChartWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const YTChartWidget *)widgetCursor.widget;
    const Style *style = getStyle(widget->style);

    int w = widgetCursor.w;
    int h = widgetCursor.h;
    if (w <= 0 || h <= 0) {
        return;
    }

    // pixels, then the values of the drawn columns (and the column before) of
    // each trace and of the second field of a product trace
    if (!buffer || bufferWidth != w || bufferHeight != h) {
        eez::free(buffer);
        buffer = eez::alloc(w * h * sizeof(display::Color) + (YT_CHART_MAX_TRACES + 1) * (w + 1) * sizeof(float), 0x7e1b5a63);
        if (!buffer) {
            bufferWidth = 0;
            bufferHeight = 0;
            return;
        }
        bufferWidth = w;
        bufferHeight = h;
        redrawAll = true;
    }

    YTChartCanvas canvas = { (display::Color *)buffer, w, h };
    float *values = (float *)(canvas.pixels + w * h);
    float *values2 = values + YT_CHART_MAX_TRACES * (w + 1);

    // the values before bufferPosition are in the buffer already (render can
    // be called again for the same state, e.g. when overlapped)
    uint32_t startPosition;
    if (redrawAll || bufferPosition > position || position - bufferPosition > (uint32_t)w) {
        startPosition = position > (uint32_t)w ? position - w : 0;
        redrawAll = true;
    } else {
        startPosition = bufferPosition;
    }
    uint32_t numColumns = position - startPosition;

    display::Color backgroundColor = display::getColorFromIndex(style->backgroundColor);

    if (redrawAll || numColumns > 0) {
        // buffer could be still in use by the previous drawBitmap
        display::startPixelsDraw();

        if (redrawAll) {
            canvas.fill(backgroundColor);
        }

        // values of the positions startPosition - 1 .. position - 1
        auto component = getComponent(widgetCursor, widget);
        for (uint8_t i = 0; i < numTraces; i++) {
            float *traceValues = values + i * (w + 1);
            if (
                !traceVisible[i] ||
                !component ||
                !flow::getModuleFieldYtHistoryHook ||
                !getTraceValues(widgetCursor, component->traces[i], position, traceValues, values2, numColumns + 1)
            ) {
                for (uint32_t j = 0; j <= numColumns; j++) {
                    traceValues[j] = NAN;
                }
            }
        }

        if (mode == YT_CHART_MODE_SCROLL && !redrawAll) {
            canvas.scrollLeft(numColumns);
        }

        // lanes of the visible bit traces
        int numLanes = 0;
        int traceLane[YT_CHART_MAX_TRACES];
        for (uint8_t i = 0; i < numTraces; i++) {
            traceLane[i] = traceVisible[i] && traceBit[i] != -1 ? numLanes++ : -1;
        }

        for (uint32_t j = 0; j < numColumns; j++) {
            int x = mode == YT_CHART_MODE_SCROLL ? w - numColumns + j : (startPosition + j) % w;

            canvas.drawVLine(x, 0, h - 1, backgroundColor);

            for (uint8_t i = 0; i < numTraces; i++) {
                if (!traceVisible[i]) {
                    continue;
                }
                float *traceValues = values + i * (w + 1);
                display::Color color = display::getColorFromIndex(component->traces[i]->color);
                if (traceLane[i] != -1) {
                    int laneTop = h * traceLane[i] / numLanes;
                    int laneHeight = h * (traceLane[i] + 1) / numLanes - laneTop;
                    drawBitColumn(
                        canvas, x,
                        getBitLevel(traceValues[j], traceBit[i]),
                        getBitLevel(traceValues[j + 1], traceBit[i]),
                        laneTop, laneHeight,
                        color
                    );
                } else {
                    drawTraceColumn(
                        canvas, x,
                        getY(traceValues[j], traceMin[i], traceMax[i], h),
                        getY(traceValues[j + 1], traceMin[i], traceMax[i], h),
                        color
                    );
                }
            }
        }

        if (mode == YT_CHART_MODE_SCAN) {
            // cursor at the column of the next value, then a few clear columns
            int x = position % w;
            canvas.drawVLine(x, 0, h - 1, display::getColorFromIndex(style->color));
            for (int i = 1; i <= BLANK_COLUMNS_AFTER_CURSOR; i++) {
                canvas.drawVLine((x + i) % w, 0, h - 1, backgroundColor);
            }
        }

        bufferPosition = position;
        redrawAll = false;
    }

    Image image;
    image.width = w;
    image.height = h;
    image.bpp = DISPLAY_BPP;
    image.lineOffset = 0;
    image.pixels = (uint8_t *)buffer;
    display::drawBitmap(&image, widgetCursor.x, widgetCursor.y);
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
