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
static uint32_t getDibFieldId(const WidgetCursor &widgetCursor, uint16_t fieldIndex) {
    uint16_t slotIndex = (uint16_t)widgetCursor.flowState->assets->reserved;
    return ((uint32_t)slotIndex << 16) | fieldIndex;
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

YTChartWidgetState::~YTChartWidgetState() {
    eez::free(buffer);
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

    for (uint8_t i = 0; i < numTraces; i++) {
        auto trace = component->traces[i];

        Value value;

        bool visible = true;
        if (flow::evalExpression(widgetCursor.flowState, widget->componentIndex, trace->visible, value, flow::FlowError::Plain("Failed to evaluate trace visible in YTChart widget"), nullptr, widgetCursor.iterators)) {
            visible = value.toBool(nullptr);
        }

        float min = 0;
        if (flow::evalExpression(widgetCursor.flowState, widget->componentIndex, trace->min, value, flow::FlowError::Plain("Failed to evaluate trace min in YTChart widget"), nullptr, widgetCursor.iterators)) {
            min = value.toFloat(nullptr);
        }

        float max = 1.0f;
        if (flow::evalExpression(widgetCursor.flowState, widget->componentIndex, trace->max, value, flow::FlowError::Plain("Failed to evaluate trace max in YTChart widget"), nullptr, widgetCursor.iterators)) {
            max = value.toFloat(nullptr);
        }

        if (!hasPreviousState || visible != traceVisible[i] || min != traceMin[i] || max != traceMax[i]) {
            traceVisible[i] = visible;
            traceMin[i] = min;
            traceMax[i] = max;
            redrawAll = true;
            hasPreviousState = false;
        }
    }

    // the newest value all the visible traces have
    uint32_t newPosition = 0;
    float newPeriod = 0;
    if (component && flow::getDibFieldYtHistoryHook) {
        bool first = true;
        for (uint8_t i = 0; i < numTraces; i++) {
            if (!traceVisible[i]) {
                continue;
            }
            uint32_t tracePosition;
            float tracePeriod;
            if (flow::getDibFieldYtHistoryHook(getDibFieldId(widgetCursor, component->traces[i]->fieldIndex), flow::YT_HISTORY_NEWEST, nullptr, 0, tracePosition, tracePeriod)) {
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

    if (!hasPreviousState || newPeriod != period) {
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
    // each trace
    if (!buffer || bufferWidth != w || bufferHeight != h) {
        eez::free(buffer);
        buffer = eez::alloc(w * h * sizeof(display::Color) + YT_CHART_MAX_TRACES * (w + 1) * sizeof(float), 0x7e1b5a63);
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
            uint32_t tracePosition;
            float tracePeriod;
            if (
                !traceVisible[i] ||
                !component ||
                !flow::getDibFieldYtHistoryHook ||
                !flow::getDibFieldYtHistoryHook(getDibFieldId(widgetCursor, component->traces[i]->fieldIndex), position, traceValues, numColumns + 1, tracePosition, tracePeriod)
            ) {
                for (uint32_t j = 0; j <= numColumns; j++) {
                    traceValues[j] = NAN;
                }
            }
        }

        if (mode == YT_CHART_MODE_SCROLL && !redrawAll) {
            canvas.scrollLeft(numColumns);
        }

        for (uint32_t j = 0; j < numColumns; j++) {
            int x = mode == YT_CHART_MODE_SCROLL ? w - numColumns + j : (startPosition + j) % w;

            canvas.drawVLine(x, 0, h - 1, backgroundColor);

            for (uint8_t i = 0; i < numTraces; i++) {
                if (traceVisible[i]) {
                    float *traceValues = values + i * (w + 1);
                    drawTraceColumn(
                        canvas, x,
                        getY(traceValues[j], traceMin[i], traceMax[i], h),
                        getY(traceValues[j + 1], traceMin[i], traceMax[i], h),
                        display::getColorFromIndex(component->traces[i]->color)
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
