/*
 * eez-framework tests: the native variables and actions of widgets.py
 * (data_w_* and action_w_*), their state is wt::g_native.
 */

#include <math.h>

#include "wtest.h"

#include <eez/gui/display.h>
#include <eez/gui/widgets/yt_graph.h>
#include <eez/flow/flow_defs_v3.h>

namespace wt {

Native g_native;

static float ytValue(uint32_t position, uint8_t valueIndex, float *max) {
    float value = NAN;
    if (valueIndex == 0 && position < g_native.yt.values.size()) {
        value = g_native.yt.values[position];
    }
    if (max) {
        *max = value;
    }
    return value;
}

static void canvasDraw(const eez::gui::WidgetCursor &widgetCursor) {
    g_native.canvasDraws++;
    eez::gui::display::setColor((uint8_t)(g_native.canvasColor >> 16), (uint8_t)(g_native.canvasColor >> 8), (uint8_t)g_native.canvasColor);
    eez::gui::display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w - 1, widgetCursor.y + widgetCursor.h - 1);
}

} // namespace wt

namespace eez {
namespace gui {

using wt::g_native;

void data_w_ell(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value::makeStringRef(g_native.ell.c_str(), -1, 0xe11e11e1);
        value.options = g_native.ellOptions;
    }
}

void data_w_ref(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value::makeStringRef(g_native.ref.c_str(), -1, 0x4ef4ef01);
    }
}

void data_w_xscroll(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value::makeStringRef(g_native.xscrollText.c_str(), -1, 0x5c4011aa);
    } else if (operation == DATA_OPERATION_GET_X_SCROLL) {
        value = Value(g_native.xscroll, VALUE_TYPE_INT32);
    }
}

void data_w_counter_a(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        g_native.counterA++;
        value = Value(1, VALUE_TYPE_INT32);
    }
}

void data_w_counter_b(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        g_native.counterB++;
        value = Value(2, VALUE_TYPE_INT32);
    }
}

void data_w_yt(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    auto &yt = g_native.yt;
    switch (operation) {
    case DATA_OPERATION_GET:
        value = Value(0, VALUE_TYPE_INT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER:
        value = Value(yt.refreshCounter, VALUE_TYPE_UINT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD:
        value = Value(yt.method, VALUE_TYPE_UINT8);
        break;
    case DATA_OPERATION_YT_DATA_GET_POSITION:
        value = Value(yt.position, VALUE_TYPE_UINT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_SIZE:
        value = Value(yt.size, VALUE_TYPE_UINT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC:
        value = Value(wt::ytValue);
        break;
    case DATA_OPERATION_YT_DATA_GET_STYLE:
        value = Value(STYLE_ID_W_TRACE, VALUE_TYPE_UINT16);
        break;
    case DATA_OPERATION_YT_DATA_GET_MIN:
        value = Value(yt.min, VALUE_TYPE_FLOAT);
        break;
    case DATA_OPERATION_YT_DATA_GET_MAX:
        value = Value(yt.max, VALUE_TYPE_FLOAT);
        break;
    case DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS:
        value = Value(yt.vertDivisions, VALUE_TYPE_INT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS:
        value = Value(yt.horzDivisions, VALUE_TYPE_INT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_DIV:
        value = Value(yt.div, VALUE_TYPE_FLOAT);
        break;
    case DATA_OPERATION_YT_DATA_GET_OFFSET:
        value = Value(yt.offset, VALUE_TYPE_FLOAT);
        break;
    case DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE:
        value = Value(value.getUInt8() == 0 ? 1 : 0, VALUE_TYPE_INT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_SHOW_LABELS:
        value = Value(0, VALUE_TYPE_INT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_SELECTED_VALUE_INDEX:
        value = Value(-1, VALUE_TYPE_INT32);
        break;
    case DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE:
        value = Value(0, VALUE_TYPE_INT32);
        break;
    case DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET:
        value = Value(0, VALUE_TYPE_UINT32);
        break;
    case DATA_OPERATION_YT_DATA_TOUCH_DRAG:
        yt.touchDrags++;
        break;
    default:
        break;
    }
}

void data_w_canvas(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((void *)wt::canvasDraw, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CANVAS_REFRESH_STATE) {
        value = Value(g_native.canvasRefresh, VALUE_TYPE_INT32);
    }
}

void data_w_rol(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_native.rol, VALUE_TYPE_INT32);
    } else if (operation == DATA_OPERATION_SET) {
        g_native.rol = value.getInt();
        g_native.rolSets++;
    }
}

void data_w_probe(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        g_native.probeAssets = widgetCursor.assets;
        g_native.probeCalls++;
        value = Value::makeStringRef("P", -1, 0x9b0be001);
    }
}

void data_w_counter_arr(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        g_native.counterArrGets++;
        // the same array every time (arrays are compared by reference)
        static Value g_array;
        if (g_array.getType() == VALUE_TYPE_UNDEFINED || (int)g_array.getArray()->arraySize != g_native.counterArrSize) {
            g_array = Value::makeArrayRef(g_native.counterArrSize, flow::defs_v3::ARRAY_TYPE_INTEGER, 0xc0c0c0c0);
            for (int i = 0; i < g_native.counterArrSize; i++) {
                g_array.getArray()->values[i] = Value(i, VALUE_TYPE_INT32);
            }
        }
        value = g_array;
    }
}

void action_w_on_click() {
    g_native.clicks++;
}

void action_w_on_click2() {
    g_native.clicks2++;
}

} // namespace gui
} // namespace eez
