/*
 * eez-framework tests: the native variables expr_nat_* of parts/expr_extra.py.
 *
 * They give the expression cases values that the project's own types can't
 * hold: a uint32 above 2^31, an int64, the narrow integer types, a float with
 * a unit, undefined and a dynamic array.
 */

#include <string.h>
#include <new>

#include <eez/conf-internal.h>
#include <eez/core/value.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/gui/gui.h>

#include "document.h"

namespace eez {
namespace gui {

// A Value as its constructor leaves it when it is built in memory that holds
// 0xCC bytes: the constructors set all 8 bytes of the union (C2: the narrow
// ones set only their member). The copy keeps all 8 bytes.
template <typename T>
static Value patternValue(T v, ValueType type) {
    alignas(Value) unsigned char buffer[sizeof(Value)];
    memset(buffer, 0xCC, sizeof(buffer));
    Value *value = new (buffer) Value(v, type);
    Value result = *value;
    value->~Value();
    return result;
}

void data_expr_nat_u32(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = patternValue((uint32_t)3000000000u, VALUE_TYPE_UINT32);
    }
}

void data_expr_nat_i64(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((int64_t)5000000000LL, VALUE_TYPE_INT64);
    }
}

void data_expr_nat_u8(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = patternValue((uint8_t)200, VALUE_TYPE_UINT8);
    }
}

void data_expr_nat_i8(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = patternValue((int8_t)-5, VALUE_TYPE_INT8);
    }
}

void data_expr_nat_i16(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = patternValue((int16_t)-300, VALUE_TYPE_INT16);
    }
}

void data_expr_nat_volt(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(2.5f, UNIT_VOLT);
    }
}

void data_expr_nat_undef(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value();
    }
}

static Value dynGetElement(uint32_t elementIndex) {
    return Value((int)((elementIndex + 1) * 10), VALUE_TYPE_INT32);
}

static void dynSetElement(uint32_t, const Value &) {
}

static const DynamicArrayValue g_dyn = { 3, flow::defs_v3::ARRAY_TYPE_INTEGER, dynGetElement, dynSetElement };

void data_expr_nat_dyn(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value::makeDynamicArray(&g_dyn);
    }
}

} // namespace gui
} // namespace eez
