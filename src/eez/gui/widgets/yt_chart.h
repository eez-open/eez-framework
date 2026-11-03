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

namespace eez {
namespace gui {

// YTChart widget (flow projects): the YT histories of DIB fields of the module
// (see flow::getModuleFieldYtHistoryHook), one value per pixel column, the
// traces are in the flow component (flow::YTChartWidgetComponent). The value
// traces use the whole height (their min .. max), the visible bit traces
// split it into lanes, the first one on the top.

#define YT_CHART_MODE_SCROLL 0
#define YT_CHART_MODE_SCAN 1

#define YT_CHART_MAX_TRACES 8

struct YTChartWidget : public Widget {
    // data item, YT_CHART_MODE_...
    int16_t mode;
    uint16_t componentIndex;
};

struct YTChartWidgetState : public WidgetState {
    WidgetStateFlags flags;
    uint8_t mode;

    // the traces evaluated from the flow component
    uint8_t numTraces;
    bool traceVisible[YT_CHART_MAX_TRACES];
    float traceMin[YT_CHART_MAX_TRACES];
    float traceMax[YT_CHART_MAX_TRACES];
    // bit of a bit trace, -1 for a value trace
    int8_t traceBit[YT_CHART_MAX_TRACES];

    // position of the newest value all the visible traces have and the
    // seconds per value
    uint32_t position;
    float period;

    // graph pixels, drawn incrementally (only the new values) and copied to
    // the display, so the graph can be rendered again at any time (e.g. when
    // it is overlapped by some other widget)
    void *buffer = nullptr;
    int bufferWidth = 0;
    int bufferHeight = 0;
    // the values before this position are in the buffer
    uint32_t bufferPosition = 0;
    // the whole graph must be drawn again
    bool redrawAll = true;

    ~YTChartWidgetState() override;

    bool updateState() override;
    void render() override;
};

} // gui
} // eez
