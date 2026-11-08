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

#include <eez/flow/private.h>

namespace eez {
namespace flow {

// YTChartTrace.type
#define YT_CHART_TRACE_TYPE_FIELD 0   // the YT history of the field
#define YT_CHART_TRACE_TYPE_PRODUCT 1 // the product of the YT histories of field and field2 (e.g. P = U * I)

// Trace of the YTChart widget: from the YT histories of the DIB fields of the
// module (see getModuleFieldYtHistoryHook).
struct YTChartTrace {
    // index of the DIB field in the module metadata (as DibModule.field)
    uint16_t fieldIndex;
    uint16_t color;
    // bit 0..7 of a bit field (its YT history value is the AND of the samples
    // in bits 0..7 and the OR in bits 8..15), drawn as a digital trace in a
    // lane of its own, -1 for a value trace
    int8_t bit;
    // YT_CHART_TRACE_TYPE_...
    uint8_t type;
    // YT_CHART_TRACE_TYPE_PRODUCT: index of the second DIB field
    uint16_t field2Index;
    // expressions, y range of the trace (not used by a bit trace) and if it
    // is drawn
    AssetsPtr<uint8_t> min;
    AssetsPtr<uint8_t> max;
    AssetsPtr<uint8_t> visible;
};

struct YTChartWidgetComponent : public Component {
    ListOfAssetsPtr<YTChartTrace> traces;
};

} // flow
} // eez
