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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include <string>

#include <eez/core/os.h>
#include <eez/core/value.h>
#include <eez/core/util.h>
#include <eez/core/utf8.h>

#include <eez/flow/flow.h>
#include <eez/flow/operations.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/date.h>
#include <eez/flow/hooks.h>

#if defined(EEZ_DASHBOARD_API)
#include <eez/flow/dashboard_api.h>
#endif

#if defined(EEZ_FOR_LVGL)
#include <eez/flow/lvgl_api.h>
#endif

#if EEZ_FOR_LVGL_SHA256_OPTION
extern "C" {
#include <eez/libs/sha256/sha256.h>
}
#endif

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

int g_eezFlowLvlgMeterTickIndex = 0;

namespace eez {
namespace flow {

Value op_add(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (a.isBlob() || b.isBlob()) {
        if (a.isBlob()) {
            if (b.isUndefinedOrNull()) {
                return a;
            }
            if (!b.isBlob()) {
                return Value::makeError();
            }
        } else {
            if (a.isUndefinedOrNull()) {
                return b;
            }
            return Value::makeError();
        }

        auto aBlob = a.getBlob();
        auto bBlob = b.getBlob();

        return Value::makeBlobRef(aBlob->blob, aBlob->len, bBlob->blob, bBlob->len, 0xc622dd24);
    }

    auto a_valid = a.isString() || a.isDouble() || a.isFloat() || a.isInt64() || a.isInt32OrLess();
    auto b_valid = b.isString() || b.isDouble() || b.isFloat() || b.isInt64() || b.isInt32OrLess();

    if (!a_valid && !b_valid) {
        return Value::makeError();
    }

    if (a.isString() || b.isString()) {
        Value value1 = a.toString(0x84eafaa8);
        Value value2 = b.toString(0xd273cab6);
        auto res = Value::concatenateString(value1, value2);

        char str1[128];
        res.toText(str1, sizeof(str1));

        return res;
    }

    if (a.isDouble() || b.isDouble()) {
        return Value(a.toDouble() + b.toDouble(), VALUE_TYPE_DOUBLE);
    }

    if (a.isFloat() || b.isFloat()) {
        return Value(a.toFloat() + b.toFloat(), VALUE_TYPE_FLOAT);
    }

    if (a.isInt64() || b.isInt64()) {
        return Value(a.toInt64() + b.toInt64(), VALUE_TYPE_INT64);
    }

    return Value((int)(a.int32Value + b.int32Value), VALUE_TYPE_INT32);
}

Value op_sub(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isDouble() || a.isFloat() || a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isDouble() || b.isFloat() || b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isDouble() || b.isDouble()) {
        return Value(a.toDouble() - b.toDouble(), VALUE_TYPE_DOUBLE);
    }

    if (a.isFloat() || b.isFloat()) {
        return Value(a.toFloat() - b.toFloat(), VALUE_TYPE_FLOAT);
    }

    if (a.isInt64() || b.isInt64()) {
        return Value(a.toInt64() - b.toInt64(), VALUE_TYPE_INT64);
    }

    return Value((int)(a.int32Value - b.int32Value), VALUE_TYPE_INT32);
}

Value op_mul(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isDouble() || a.isFloat() || a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isDouble() || b.isFloat() || b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isDouble() || b.isDouble()) {
        return Value(a.toDouble() * b.toDouble(), VALUE_TYPE_DOUBLE);
    }

    if (a.isFloat() || b.isFloat()) {
        return Value(a.toFloat() * b.toFloat(), VALUE_TYPE_FLOAT);
    }

    if (a.isInt64() || b.isInt64()) {
        return Value(a.toInt64() * b.toInt64(), VALUE_TYPE_INT64);
    }

    return Value((int)(a.int32Value * b.int32Value), VALUE_TYPE_INT32);
}

Value op_div(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isDouble() || a.isFloat() || a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isDouble() || b.isFloat() || b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isDouble() || b.isDouble()) {
        return Value(a.toDouble() / b.toDouble(), VALUE_TYPE_DOUBLE);
    }

    if (a.isFloat() || b.isFloat()) {
        return Value(a.toFloat() / b.toFloat(), VALUE_TYPE_FLOAT);
    }

    if (a.isInt64() || b.isInt64()) {
        auto d = b.toInt64();
        if (d == 0) {
            return Value::makeError();
        }

        return Value(1.0 * a.toInt64() / d, VALUE_TYPE_DOUBLE);
    }

    if (b.int32Value == 0) {
        return Value::makeError();
    }

    return Value(1.0 * a.int32Value / b.int32Value, VALUE_TYPE_DOUBLE);
}

Value op_mod(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isDouble() || a.isFloat() || a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isDouble() || b.isFloat() || b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isDouble() || b.isDouble()) {
        return Value(a.toDouble() - floor(a.toDouble() / b.toDouble()) * b.toDouble(), VALUE_TYPE_DOUBLE);
    }

    if (a.isFloat() || b.isFloat()) {
        return Value(a.toFloat() - floor(a.toFloat() / b.toFloat()) * b.toFloat(), VALUE_TYPE_FLOAT);
    }

    if (a.isInt64() || b.isInt64()) {
        auto d = b.toInt64();
        if (d == 0) {
            return Value::makeError();
        }

        return Value(a.toInt64() % d, VALUE_TYPE_INT64);
    }

    if (b.int32Value == 0) {
        return Value::makeError();
    }

    return Value((int)(a.int32Value % b.int32Value), VALUE_TYPE_INT32);
}

Value op_left_shift(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isInt64() || b.isInt64()) {
        return Value(a.toInt64() << b.toInt64(), VALUE_TYPE_INT64);
    }

    return Value((int)(a.toInt32() << b.toInt32()), VALUE_TYPE_INT32);
}

Value op_right_shift(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isInt64() || b.isInt64()) {
        return Value(a.toInt64() >> b.toInt64(), VALUE_TYPE_INT64);
    }

    return Value((int)(a.toInt32() >> b.toInt32()), VALUE_TYPE_INT32);

}

Value op_binary_and(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isInt64() || b.isInt64()) {
        return Value(a.toInt64() & b.toInt64(), VALUE_TYPE_INT64);
    }

    return Value((int)(a.toInt32() & b.toInt32()), VALUE_TYPE_INT32);
}

Value op_binary_or(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isInt64() || b.isInt64()) {
        return Value(a.toInt64() | b.toInt64(), VALUE_TYPE_INT64);
    }

    return Value((int)(a.toInt32() | b.toInt32()), VALUE_TYPE_INT32);
}

Value op_binary_xor(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    auto a = a1.getValue();
    auto b = b1.getValue();

    if (!(a.isInt64() || a.isInt32OrLess())) {
        return Value::makeError();
    }

    if (!(b.isInt64() || b.isInt32OrLess())) {
        return Value::makeError();
    }

    if (a.isInt64() || b.isInt64()) {
        return Value(a.toInt64() ^ b.toInt64(), VALUE_TYPE_INT64);
    }

    return Value((int)(a.toInt32() ^ b.toInt32()), VALUE_TYPE_INT32);
}

static bool is_equal(const Value& a1, const Value& b1) {
    auto a = a1.getValue();
    auto b = b1.getValue();

    auto aIsUndefinedOrNull = a.getType() == VALUE_TYPE_UNDEFINED || a.getType() == VALUE_TYPE_NULL;
    auto bIsUndefinedOrNull = b.getType() == VALUE_TYPE_UNDEFINED || b.getType() == VALUE_TYPE_NULL;

    if (aIsUndefinedOrNull) {
        return bIsUndefinedOrNull;
    } else if (bIsUndefinedOrNull) {
        return false;
    }

    if (a.isString() && b.isString()) {
        const char *aStr = a.getString();
        const char *bStr = b.getString();
        if (!aStr && !bStr) {
            return true;
        }
        if (!aStr || !bStr) {
            return false;
        }
        return strcmp(aStr, bStr) == 0;
    }

    if (a.isBlob() && b.isBlob()) {
        auto aBlobRef = a.getBlob();
        auto bBlobRef = b.getBlob();
        if (!aBlobRef && !bBlobRef) {
            return true;
        }
        if (!aBlobRef || !bBlobRef) {
            return false;
        }
        if (aBlobRef->len != bBlobRef->len) {
            return false;
        }
        return memcmp(aBlobRef->blob, bBlobRef->blob, aBlobRef->len) == 0;

    }

    if (a.getType() == b.getType()) {
        return a == b;
    }

    if (a.isInt32OrLess() == b.isInt32OrLess()) {
        return a.toInt32() == b.toInt32();
    }

    return a.toDouble() == b.toDouble();
}

static bool is_less(const Value& a1, const Value& b1) {
    auto a = a1.getValue();
    auto b = b1.getValue();

    if (a.isString() && b.isString()) {
        const char *aStr = a.getString();
        const char *bStr = b.getString();
        if (!aStr || !bStr) {
            return false;
        }
        return strcmp(aStr, bStr) < 0;
    }

    return a.toDouble() < b.toDouble();
}

static bool is_great(const Value& a1, const Value& b1) {
    return !is_less(a1, b1) && !is_equal(a1, b1);
}

Value op_eq(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    return Value(is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_neq(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    return Value(!is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_less(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    return Value(is_less(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_great(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    return Value(is_great(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_less_eq(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    return Value(is_less(a1, b1) || is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_great_eq(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

    return Value(!is_less(a1, b1), VALUE_TYPE_BOOLEAN);
}

static void do_OPERATION_TYPE_ADD(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_add(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_SUB(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_sub(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_MUL(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_mul(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_DIV(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_div(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_MOD(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_mod(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_LEFT_SHIFT(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_left_shift(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_RIGHT_SHIFT(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_right_shift(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_BINARY_AND(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_binary_and(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_BINARY_OR(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_binary_or(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_BINARY_XOR(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();

    auto result = op_binary_xor(a, b);

    if (result.getType() == VALUE_TYPE_UNDEFINED) {
        result = Value::makeError();
    }

    stack.push(result);
}

static void do_OPERATION_TYPE_EQUAL(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();
    stack.push(op_eq(a, b));
}

static void do_OPERATION_TYPE_NOT_EQUAL(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();
    stack.push(op_neq(a, b));
}

static void do_OPERATION_TYPE_LESS(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();
    stack.push(op_less(a, b));
}

static void do_OPERATION_TYPE_GREATER(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();
    stack.push(op_great(a, b));
}

static void do_OPERATION_TYPE_LESS_OR_EQUAL(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();
    stack.push(op_less_eq(a, b));
}

static void do_OPERATION_TYPE_GREATER_OR_EQUAL(EvalStack &stack) {
    auto b = stack.pop();
    auto a = stack.pop();
    stack.push(op_great_eq(a, b));
}

static void do_OPERATION_TYPE_LOGICAL_AND(EvalStack &stack) {
    auto bValue = stack.pop().getValue();
    auto aValue = stack.pop().getValue();

    if (aValue.isError()) {
        stack.push(aValue);
        return;
    }

    if (!aValue.toBool()) {
        stack.push(Value(false, VALUE_TYPE_BOOLEAN));
        return;
    }

    if (bValue.isError()) {
        stack.push(bValue);
        return;
    }

    stack.push(Value(bValue.toBool(), VALUE_TYPE_BOOLEAN));
}

static void do_OPERATION_TYPE_LOGICAL_OR(EvalStack &stack) {
    auto bValue = stack.pop().getValue();
    auto aValue = stack.pop().getValue();

    if (aValue.isError()) {
        stack.push(aValue);
        return;
    }

    if (aValue.toBool()) {
        stack.push(Value(true, VALUE_TYPE_BOOLEAN));
        return;
    }

    if (bValue.isError()) {
        stack.push(bValue);
        return;
    }

    stack.push(Value(bValue.toBool(), VALUE_TYPE_BOOLEAN));
}

static void do_OPERATION_TYPE_UNARY_PLUS(EvalStack &stack) {
    auto a = stack.pop().getValue();

    if (a.isDouble()) {
        stack.push(Value(a.getDouble(), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(a.toFloat(), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt64()) {
        stack.push(Value((int64_t)a.getInt64(), VALUE_TYPE_INT64));
        return;
    }

    if (a.isInt32()) {
        stack.push(Value((int)a.getInt32(), VALUE_TYPE_INT32));
        return;
    }

    if (a.isInt16()) {
        stack.push(Value((int16_t)a.getInt16(), VALUE_TYPE_INT16));
        return;
    }

    if (a.isInt8()) {
        stack.push(Value((int8_t)a.getInt8(), VALUE_TYPE_INT8));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_UNARY_MINUS(EvalStack &stack) {
    auto a = stack.pop().getValue();

    if (a.isDouble()) {
        stack.push(Value(-a.getDouble(), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(-a.toFloat(), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt64()) {
        stack.push(Value((int64_t)-a.getInt64(), VALUE_TYPE_INT64));
        return;
    }

    if (a.isInt32()) {
        stack.push(Value((int)-a.getInt32(), VALUE_TYPE_INT32));
        return;
    }

    if (a.isInt16()) {
        stack.push(Value((int16_t)-a.getInt16(), VALUE_TYPE_INT16));
        return;
    }

    if (a.isInt8()) {
        stack.push(Value((int8_t)-a.getInt8(), VALUE_TYPE_INT8));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_BINARY_ONE_COMPLEMENT(EvalStack &stack) {
    auto a = stack.pop().getValue();

    if (a.isInt64()) {
        stack.push(Value(~a.uint64Value, VALUE_TYPE_UINT64));
        return;
    }

    if (a.isInt32()) {
        stack.push(Value(~a.uint32Value, VALUE_TYPE_UINT32));
        return;
    }

    if (a.isInt16()) {
        stack.push(Value(~a.uint16Value, VALUE_TYPE_UINT16));
        return;
    }

    if (a.isInt8()) {
        stack.push(Value(~a.uint8Value, VALUE_TYPE_UINT8));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_NOT(EvalStack &stack) {
    auto aValue = stack.pop();

    if (aValue.isError()) {
        stack.push(aValue);
        return;
    }

    int err;
    auto a = aValue.toBool(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(Value(!a, VALUE_TYPE_BOOLEAN));
}

static void do_OPERATION_TYPE_CONDITIONAL(EvalStack &stack) {
    auto alternate = stack.pop();
    auto consequent = stack.pop();
    auto conditionValue = stack.pop();

    if (conditionValue.isError()) {
        stack.push(conditionValue);
        return;
    }

    int err;
    auto condition = conditionValue.toBool(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(condition ? consequent : alternate);
}

static void do_OPERATION_TYPE_SYSTEM_GET_TICK(EvalStack &stack) {
    stack.push(Value(millis(), VALUE_TYPE_UINT32));
}

static void do_OPERATION_TYPE_FLOW_INDEX(EvalStack &stack) {
    if (!stack.iterators) {
        stack.push(Value::makeError());
        return;
    }

    auto a = stack.pop();

    int err;
    auto iteratorIndex = a.toInt32(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

    if (iteratorIndex < 0 || iteratorIndex >= (int)MAX_ITERATORS) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(stack.iterators[iteratorIndex]);
}

static void do_OPERATION_TYPE_FLOW_IS_PAGE_ACTIVE(EvalStack &stack) {
#if EEZ_OPTION_GUI
    bool isActive = false;

    auto pageIndex = getPageIndex(stack.flowState);
    if (pageIndex >= 0) {
        int16_t pageId = (int16_t)(pageIndex + 1);
        if (stack.flowState->assets == g_externalAssets) {
            pageId = -pageId;
        }

        for (int16_t appContextId = 0; ; appContextId++) {
            auto appContext = getAppContextFromId(appContextId);
            if (!appContext) {
                break;
            }

            if (appContext->isPageOnStack(pageId)) {
                isActive = true;
                break;
            }
        }
    }

    stack.push(Value(isActive, VALUE_TYPE_BOOLEAN));
#elif defined(EEZ_FOR_LVGL)
    auto pageIndex = getPageIndex(stack.flowState);
    stack.push(Value(pageIndex == g_currentScreen, VALUE_TYPE_BOOLEAN));
#else
    stack.push(Value::makeError());
#endif // EEZ_OPTION_GUI
}

static void do_OPERATION_TYPE_FLOW_PAGE_TIMELINE_POSITION(EvalStack &stack) {
    stack.push(Value(stack.flowState->timelinePosition, VALUE_TYPE_FLOAT));
}

static void do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE(EvalStack &stack) {
    auto arrayTypeValue = stack.pop();
    if (arrayTypeValue.isError()) {
        stack.push(arrayTypeValue);
        return;
    }

    auto arraySizeValue = stack.pop();
    if (arraySizeValue.isError()) {
        stack.push(arraySizeValue);
        return;
    }

    auto numInitElementsValue = stack.pop();
    if (numInitElementsValue.isError()) {
        stack.push(numInitElementsValue);
        return;
    }

    int arrayType = arrayTypeValue.getInt();

    int err;
    int arraySize = arraySizeValue.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    int numInitElements = numInitElementsValue.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

#if defined(EEZ_DASHBOARD_API)
    if (arrayType == VALUE_TYPE_JSON) {
        Value jsonValue = operationJsonMake();

        for (int i = 0; i < arraySize; i += 2) {
            Value propertyName = stack.pop().getValue();
            if (!propertyName.isString()) {
                stack.push(Value::makeError());
                return;
            }

            Value propertyValue = stack.pop().getValue();
            if (propertyValue.isError()) {
                stack.push(propertyValue);
                return;
            }

            operationJsonSet(jsonValue.getInt(), propertyName.getString(), &propertyValue);
        }

        stack.push(jsonValue);

        return;
    }
#endif

    auto arrayValue = Value::makeArrayRef(arraySize, arrayType, 0x837260d4);

    auto array = arrayValue.getArray();

    for (int i = 0; i < arraySize; i++) {
        if (i < numInitElements) {
            array->values[i] = stack.pop().getValue();
        } else {
            array->values[i] = Value();
        }
    }

    stack.push(arrayValue);
}

static void do_OPERATION_TYPE_FLOW_LANGUAGES(EvalStack &stack) {
    auto &languages = stack.flowState->assets->languages;

    auto arrayValue = Value::makeArrayRef(languages.count, VALUE_TYPE_STRING, 0xff4787fc);

    auto array = arrayValue.getArray();

    for (uint32_t i = 0; i < languages.count; i++) {
        array->values[i] = Value((const char *)(languages[i]->languageID));
    }

    stack.push(arrayValue);
}

static void do_OPERATION_TYPE_FLOW_TRANSLATE(EvalStack &stack) {
    auto textResourceIndexValue = stack.pop();

    int err;
    int textResourceIndex = textResourceIndexValue.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    int languageIndex = g_selectedLanguage;

    auto &languages = stack.flowState->assets->languages;
    if (languageIndex >= 0 && languageIndex < (int)languages.count) {
        auto &translations = languages[languageIndex]->translations;
        if (textResourceIndex >= 0 && textResourceIndex < (int)translations.count) {
            stack.push(translations[textResourceIndex]);
            return;
        }
    }

    stack.push("");
}

static void do_OPERATION_TYPE_FLOW_THEMES(EvalStack &stack) {
    auto &themes = stack.flowState->assets->colorsDefinition->themes;

    auto arrayValue = Value::makeArrayRef(themes.count, VALUE_TYPE_STRING, 0x03906e8f);

    auto array = arrayValue.getArray();

    for (uint32_t i = 0; i < themes.count; i++) {
        array->values[i] = Value((const char *)(themes[i]->name));
    }

    stack.push(arrayValue);
}

static void do_OPERATION_TYPE_FLOW_PARSE_INTEGER(EvalStack &stack) {
    auto str = stack.pop();
    if (str.isError()) {
        stack.push(str);
        return;
    }

    int err;
    auto value = str.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(Value((int)value, VALUE_TYPE_INT32));
}

static void do_OPERATION_TYPE_FLOW_PARSE_FLOAT(EvalStack &stack) {
    auto str = stack.pop();
    if (str.isError()) {
        stack.push(str);
        return;
    }

    int err;
    auto value = str.toFloat(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(Value(value, VALUE_TYPE_FLOAT));
}

static void do_OPERATION_TYPE_FLOW_PARSE_DOUBLE(EvalStack &stack) {
    auto str = stack.pop();
    if (str.isError()) {
        stack.push(str);
        return;
    }

    int err;
    auto value = str.toDouble(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(Value(value, VALUE_TYPE_DOUBLE));
}

static void do_OPERATION_TYPE_FLOW_TO_INTEGER(EvalStack &stack) {
    auto str = stack.pop();
    if (str.isError()) {
        stack.push(str);
        return;
    }

    int err;
    int value = str.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(Value(value, VALUE_TYPE_INT32));
}

static void do_OPERATION_TYPE_FLOW_GET_BITMAP_INDEX(EvalStack &stack) {
#if EEZ_OPTION_GUI
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    Value bitmapName = a.toString(0x244c1880);

    int bitmapId = getBitmapIdByName(bitmapName.getString());

    stack.push(Value(bitmapId, VALUE_TYPE_INT32));
#else
    stack.push(Value::makeError());
#endif // EEZ_OPTION_GUI
}

static void do_OPERATION_TYPE_FLOW_GET_BITMAP_AS_DATA_URL(EvalStack &stack) {
#if defined(EEZ_DASHBOARD_API)
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    Value bitmapName = a.toString(0xcdc34cc3);
    stack.push(getBitmapAsDataURL(bitmapName.getString()));
#else
    stack.push(Value::makeError());
#endif // EEZ_OPTION_GUI
}

static void do_OPERATION_TYPE_DATE_NOW(EvalStack &stack) {
    stack.push(Value((double)date::now(), VALUE_TYPE_DATE));
}

static void do_OPERATION_TYPE_DATE_TO_STRING(EvalStack &stack) {
#ifndef ARDUINO
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    char str[128];
    date::toString(a.getDouble(), str, sizeof(str));
    stack.push(Value::makeStringRef(str, -1, 0xbe440ec8));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_DATE_TO_LOCALE_STRING(EvalStack &stack) {
#ifndef ARDUINO
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    char str[128];
    date::toLocaleString(a.getDouble(), str, sizeof(str));
    stack.push(Value::makeStringRef(str, -1, 0xbe440ec8));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_DATE_FROM_STRING(EvalStack &stack) {
#ifndef ARDUINO
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    Value dateStrValue = a.toString(0x99cb1a93);

    auto date = (double)date::fromString(dateStrValue.getString());
    stack.push(Value(date, VALUE_TYPE_DATE));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_DATE_GET_YEAR(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(date::getYear(a.getDouble()));
}

static void do_OPERATION_TYPE_DATE_GET_MONTH(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(date::getMonth(a.getDouble()));
}

static void do_OPERATION_TYPE_DATE_GET_DAY(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(date::getDay(a.getDouble()));
}

static void do_OPERATION_TYPE_DATE_GET_HOURS(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(date::getHours(a.getDouble()));
}

static void do_OPERATION_TYPE_DATE_GET_MINUTES(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(date::getMinutes(a.getDouble()));
}

static void do_OPERATION_TYPE_DATE_GET_SECONDS(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(date::getSeconds(a.getDouble()));
}

static void do_OPERATION_TYPE_DATE_GET_MILLISECONDS(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(date::getMilliseconds(a.getDouble()));
}

static void do_OPERATION_TYPE_DATE_MAKE(EvalStack &stack) {
    int err;
    Value value;

    value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }
    int year = value.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }
    int month = value.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }
    int day = value.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }
    int hours = value.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }
    int minutes = value.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }
    int seconds = value.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }
    int milliseconds = value.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    auto date = (double)date::makeDate(year, month, day, hours, minutes, seconds, milliseconds);
    stack.push(Value(date, VALUE_TYPE_DATE));
}

static void do_OPERATION_TYPE_MATH_SIN(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (a.isDouble()) {
        stack.push(Value(sin(a.getDouble()), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(sinf(a.toFloat()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt64()) {
        stack.push(Value(sin(a.toInt64()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt32OrLess()) {
        stack.push(Value(sinf(a.int32Value), VALUE_TYPE_FLOAT));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_MATH_COS(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (a.isDouble()) {
        stack.push(Value(cos(a.getDouble()), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(cosf(a.toFloat()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt64()) {
        stack.push(Value(cos(a.toInt64()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt32OrLess()) {
        stack.push(Value(cosf(a.int32Value), VALUE_TYPE_FLOAT));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_MATH_POW(EvalStack &stack) {
    auto baseValue = stack.pop().getValue();
    if (baseValue.isError()) {
        stack.push(Value::makeError());
        return;
    }
    if (!baseValue.isInt32OrLess() && !baseValue.isFloat() && !baseValue.isDouble()) {
        stack.push(Value::makeError());
        return;
    }

    auto exponentValue = stack.pop().getValue();
    if (exponentValue.isError()) {
        stack.push(Value::makeError());
        return;
    }
    if (!exponentValue.isInt32OrLess() && !exponentValue.isFloat() && !exponentValue.isDouble()) {
        stack.push(Value::makeError());
        return;
    }

    if (baseValue.isFloat() && (exponentValue.isFloat() || exponentValue.isInt32OrLess())) {
        int err;

        float base = baseValue.toFloat(&err);
        if (err) {
            stack.push(Value::makeError());
            return;
        }

        float exponent = exponentValue.toFloat(&err);
        if (err) {
            stack.push(Value::makeError());
            return;
        }

        float result = powf(base, exponent);

        stack.push(Value(result, VALUE_TYPE_FLOAT));
    } else {
        int err;

        double base = baseValue.toDouble(&err);
        if (err) {
            stack.push(Value::makeError());
            return;
        }

        double exponent = exponentValue.toDouble(&err);
        if (err) {
            stack.push(Value::makeError());
            return;
        }

        double result = pow(base, exponent);

        stack.push(Value(result, VALUE_TYPE_DOUBLE));
    }
}

static void do_OPERATION_TYPE_MATH_LOG(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (a.isDouble()) {
        stack.push(Value(log(a.getDouble()), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(logf(a.toFloat()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt64()) {
        stack.push(Value(log(a.toInt64()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt32OrLess()) {
        stack.push(Value(logf(a.int32Value), VALUE_TYPE_FLOAT));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_MATH_LOG10(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (a.isDouble()) {
        stack.push(Value(log10(a.getDouble()), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(log10f(a.toFloat()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt64()) {
        stack.push(Value(log10(a.toInt64()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt32OrLess()) {
        stack.push(Value(log10f(a.int32Value), VALUE_TYPE_FLOAT));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_MATH_ABS(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (a.isDouble()) {
        stack.push(Value(abs(a.getDouble()), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(abs(a.toFloat()), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt64()) {
        stack.push(Value((int64_t)abs(a.getInt64()), VALUE_TYPE_INT64));
        return;
    }

    if (a.isInt32()) {
        stack.push(Value((int)abs(a.getInt32()), VALUE_TYPE_INT32));
        return;
    }

    if (a.isInt16()) {
        stack.push(Value(abs(a.getInt16()), VALUE_TYPE_INT16));
        return;
    }

    if (a.isInt8()) {
        stack.push(Value(abs(a.getInt8()), VALUE_TYPE_INT8));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_MATH_FLOOR(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (a.isInt32OrLess()) {
        stack.push(a);
        return;
    }

    if (a.isDouble()) {
        stack.push(Value(floor(a.getDouble()), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(floorf(a.toFloat()), VALUE_TYPE_FLOAT));
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_MATH_CEIL(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (a.isInt32OrLess()) {
        stack.push(a);
        return;
    }

    if (a.isDouble()) {
        stack.push(Value(ceil(a.getDouble()), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(ceilf(a.toFloat()), VALUE_TYPE_FLOAT));
        return;
    }

    stack.push(Value::makeError());
}

static float roundN(float value, unsigned int numDigits) {
  float pow_10 = pow(10.0f, numDigits);
  return round(value * pow_10) / pow_10;
}

static double roundN(double value, unsigned int numDigits) {
  float pow_10 = pow(10.0f, numDigits);
  return round(value * pow_10) / pow_10;
}

static void do_OPERATION_TYPE_MATH_ROUND(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    unsigned int numDigits;
    if (numArgs > 1) {
        auto b = stack.pop().getValue();
        numDigits = b.toInt32();
    } else {
        numDigits = 0;
    }

    if (a.isInt32OrLess()) {
        stack.push(a);
        return;
    }

    if (a.isDouble()) {
        stack.push(Value(roundN(a.getDouble(), numDigits), VALUE_TYPE_DOUBLE));
        return;
    }

    if (a.isFloat()) {
        stack.push(Value(roundN(a.toFloat(), numDigits), VALUE_TYPE_FLOAT));
        return;
    }

    if (a.isInt32OrLess()) {
        stack.push(a);
        return;
    }

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_MATH_MIN(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    Value minValue;

    for (int i = 0; i < numArgs; i++) {
        auto value = stack.pop().getValue();
        if (value.isError()) {
            stack.push(value);
            return;
        }
        if (minValue.isUndefinedOrNull() || is_less(value, minValue)) {
            minValue = value;
        }
    }

    stack.push(minValue);
}

static void do_OPERATION_TYPE_MATH_MAX(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    Value maxValue;

    for (int i = 0; i < numArgs; i++) {
        auto value = stack.pop().getValue();
        if (value.isError()) {
            stack.push(value);
            return;
        }
        if (maxValue.isUndefinedOrNull() || is_great(value, maxValue)) {
            maxValue = value;
        }
    }

    stack.push(maxValue);
}

static void do_OPERATION_TYPE_STRING_LENGTH(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    const char *aStr = a.getString();
    if (!aStr) {
        stack.push(Value::makeError());
        return;
    }

    int aStrLen = strlen(aStr);

    stack.push(Value(aStrLen, VALUE_TYPE_INT32));
}

static void do_OPERATION_TYPE_STRING_SUBSTRING(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    Value strValue = stack.pop().getValue();
    if (strValue.isError()) {
        stack.push(strValue);
        return;
    }
    Value startValue = stack.pop().getValue();
    if (startValue.isError()) {
        stack.push(startValue);
        return;
    }
    Value endValue;
    if (numArgs == 3) {
        endValue = stack.pop().getValue();
        if (endValue.isError()) {
            stack.push(endValue);
            return;
        }
    }

    const char *str = strValue.getString();
    if (!str) {
        stack.push(Value::makeError());
        return;
    }

    int strLen = (int)strlen(str);

    int err = 0;

    int start = startValue.toInt32(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

    int end;
    if (endValue.getType() == VALUE_TYPE_UNDEFINED) {
        end = strLen;
    } else {
        end = endValue.toInt32(&err);
        if (err != 0) {
            stack.push(Value::makeError());
            return;
        }
    }

    if (start < 0) {
        start = 0;
    } else if (start > strLen) {
        start = strLen;
    }

    if (end < 0) {
        end = 0;
    } else if (end > strLen) {
        end = strLen;
    }

    if (start < end) {
        Value resultValue = Value::makeStringRef(str + start, end - start, 0x203b08a2);
        stack.push(resultValue);
        return;
    }

    stack.push(Value("", VALUE_TYPE_STRING));
}

static void do_OPERATION_TYPE_STRING_FIND(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    auto b = stack.pop().getValue();
    if (b.isError()) {
        stack.push(b);
        return;
    }

    Value aStr = a.toString(0xf616bf4d);
    Value bStr = b.toString(0x81229133);
    if (!aStr.getString() || !bStr.getString()) {
        stack.push(Value(-1, VALUE_TYPE_INT32));
        return;
    }

    const char *pos = strstr(aStr.getString(), bStr.getString());
    if (pos) {
        stack.push(Value((int)(pos - aStr.getString()), VALUE_TYPE_INT32));
        return;
    }

    stack.push(Value(-1, VALUE_TYPE_INT32));
}

#if !defined(EEZ_DASHBOARD_API)
typedef enum {
    type_int,
    type_signed_char,
    type_short_int,
    type_long_int,
    type_long_long_int,
    type_intmax_t,

    type_size_t,

    type_unsigned_int,
    type_unsigned_char,
    type_unsigned_short_int,
    type_unsigned_long_int,
    type_unsigned_long_long_int,
    type_uintmax_t,

    type_double,

    type_string
} FormatType;

typedef enum {
    length_none,
    length_hh,
    length_h,
    length_l,
    length_ll,
    length_j,
    length_z,
    length_t,
    length_L
} FormatLength;

static size_t do_string_format(FormatType type, const Value& b, char *result, size_t result_size, const char *format) {
    if (type == type_int) return snprintf(result, result_size, format, (int)b.getInt());
    if (type == type_signed_char) return snprintf(result, result_size, format, (signed char)b.getInt32());
    if (type == type_short_int) return snprintf(result, result_size, format, (short int)b.getInt32());
    if (type == type_long_int) return snprintf(result, result_size, format, (long int)b.getInt64());
    if (type == type_long_long_int) return snprintf(result, result_size, format, (long long int)b.getInt64());
    if (type == type_intmax_t) return snprintf(result, result_size, format, (intmax_t)b.getInt64());

    if (type == type_size_t) return snprintf(result, result_size, format, (size_t)b.getInt64());

    if (type == type_unsigned_int) return snprintf(result, result_size, format, (unsigned int)b.getUInt32());
    if (type == type_unsigned_char) return snprintf(result, result_size, format, (unsigned char)b.getUInt32());
    if (type == type_unsigned_short_int) return snprintf(result, result_size, format, (unsigned short int)b.getUInt32());
    if (type == type_unsigned_long_int) return snprintf(result, result_size, format, (unsigned long int)b.getUInt64());
    if (type == type_unsigned_long_long_int) return snprintf(result, result_size, format, (unsigned long long int)b.getUInt64());
    if (type == type_uintmax_t) return snprintf(result, result_size, format, (uintmax_t)b.getUInt64());

    if (type == type_double) {
        if (b.isDouble()) {
            return snprintf(result, result_size, format, b.getDouble());
        }
        float f = b.toFloat();
        return snprintf(result, result_size, format, f);
    }

    return snprintf(result, result_size, format, b.getString());
}
#endif

static void do_OPERATION_TYPE_STRING_FORMAT(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    auto b = stack.pop().getValue();
    if (b.isError()) {
        stack.push(b);
        return;
    }

    if (!a.isString()) {
        stack.push(Value::makeError());
        return;
    }

#if defined(EEZ_DASHBOARD_API)
    stack.push(operationStringFormat(a.getString(), &b));
#else
    const char *format = a.getString();
    size_t formatLength = strlen(format);
    if (formatLength == 0) {
        stack.push(Value::makeError());
        return;
    }

    char specifier = format[formatLength-1];

    char l1 = formatLength > 1 ? format[formatLength-2] : 0;
    char l2 = formatLength > 2 ? format[formatLength-3] : 0;
    FormatLength length = length_none;
    if (l1 == 'h' && l2 == 'h') length = length_hh;
    else if (l1 == 'h') length = length_h;
    else if (l1 == 'l') length = length_l;
    else if (l1 == 'l' && l2 == 'l') length = length_ll;
    else if (l1 == 'j') length = length_j;
    else if (l1 == 'z') length = length_z;
    else if (l1 == 't') length = length_t;
    else if (l1 == 'L') length = length_L;

    FormatType type = type_int;


    if (specifier == 'd' || specifier == 'i') {
        if (length == length_none) {
            type = type_int;
        } else if (length == length_hh) {
            type = type_signed_char;
        } else if (length == length_h) {
            type = type_short_int;
        } else if (length == length_l) {
            type = type_long_int;
        } else if (length == length_ll) {
            type = type_long_long_int;
        } else if (length == length_j) {
            type = type_intmax_t;
        } else if (length == length_z) {
            type = type_size_t;
        } else {
            stack.push(Value::makeError());
            return;
        }
    } else if (specifier == 'u' || specifier == 'o' || specifier == 'x' || specifier == 'X') {
        if (length == length_none) {
            type = type_unsigned_int;
        } else if (length == length_hh) {
            type = type_unsigned_char;
        } else if (length == length_h) {
            type = type_unsigned_short_int;
        } else if (length == length_l) {
            type = type_unsigned_long_int;
        } else if (length == length_ll) {
            type = type_unsigned_long_long_int;
        } else if (length == length_j) {
            type = type_uintmax_t;
        } else if (length == length_z) {
            type = type_size_t;
        } else {
            stack.push(Value::makeError());
            return;
        }
    } else if (specifier == 'f' || specifier == 'F' || specifier == 'e' || specifier == 'E' || specifier == 'g' || specifier == 'G' || specifier == 'a' || specifier == 'A') {
        type = type_double;
    } else if (specifier == 'c') {
        type = type_int;
    } else if (specifier == 's') {
        type = type_string;
    } else {
        stack.push(Value::makeError());
        return;
    }

    int resultStrLen = do_string_format(type, b, NULL, 0, format);
    char *resultStr = (char *)eez::alloc(resultStrLen + 1, 0x987ee4eb);
    do_string_format(type, b, resultStr, resultStrLen + 1, format);



    stack.push(Value::makeStringRef(resultStr, -1, 0x1e1227fd));

    eez::free(resultStr);
#endif
}

static void do_OPERATION_TYPE_STRING_FORMAT_PREFIX(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    auto b = stack.pop().getValue();
    if (b.isError()) {
        stack.push(b);
        return;
    }

    auto c = stack.pop().getValue();
    if (c.isError()) {
        stack.push(c);
        return;
    }

    if (!a.isString()) {
        stack.push(Value::makeError());
        return;
    }

#if defined(EEZ_DASHBOARD_API)
    stack.push(operationStringFormatPrefix(a.getString(), &b, &c));
#else
    stack.push(Value::makeError());
    return;
#endif
}

static void do_OPERATION_TYPE_STRING_PAD_START(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    auto b = stack.pop().getValue();
    if (b.isError()) {
        stack.push(b);
        return;
    }
    auto c = stack.pop().getValue();
    if (c.isError()) {
        stack.push(c);
        return;
    }

    auto str = a.toString(0xcf6aabe6);
    if (!str.getString()) {
        stack.push(Value::makeError());
        return;
    }
    int strLen = strlen(str.getString());

    int err;
    int targetLength = b.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }
    if (targetLength < strLen) {
        targetLength = strLen;
    }

    auto padStr = c.toString(0x81353bd7);
    if (!padStr.getString()) {
        stack.push(Value::makeError());
        return;
    }
    int padStrLen = strlen(padStr.getString());

    Value resultValue = Value::makeStringRef("", targetLength, 0xf43b14dd);
    if (resultValue.type == VALUE_TYPE_NULL) {
        stack.push(Value::makeError());
        return;
    }
    char *resultStr = (char *)resultValue.getString();

    auto n = targetLength - strLen;
    stringCopy(resultStr + (targetLength - strLen), strLen + 1, str.getString());

    for (int i = 0; i < n; i++) {
        resultStr[i] = padStr.getString()[i % padStrLen];
    }

    stack.push(resultValue);
}

static void do_OPERATION_TYPE_STRING_SPLIT(EvalStack &stack) {
    auto strValue = stack.pop().getValue();
    if (strValue.isError()) {
        stack.push(strValue);
        return;
    }
    auto delimValue = stack.pop().getValue();
    if (delimValue.isError()) {
        stack.push(delimValue);
        return;
    }

    auto str = strValue.getString();
    if (!str) {
        stack.push(Value::makeError());
        return;
    }

    auto delim = delimValue.getString();
    if (!delim) {
        stack.push(Value::makeError());
        return;
    }

    auto strLen = strlen(str);

    char *strCopy = (char *)eez::alloc(strLen + 1, 0xea9d0bc0);
    stringCopy(strCopy, strLen + 1, str);

    // get num parts
    size_t arraySize = 0;
    char *token = strtok(strCopy, delim);
    while (token != NULL) {
        arraySize++;
        token = strtok(NULL, delim);
    }

    eez::free(strCopy);
    strCopy = (char *)eez::alloc(strLen + 1, 0xea9d0bc1);
    stringCopy(strCopy, strLen + 1, str);

    // make array
    auto arrayValue = Value::makeArrayRef(arraySize, VALUE_TYPE_STRING, 0xe82675d4);
    auto array = arrayValue.getArray();
    int i = 0;
    token = strtok(strCopy, delim);
    while (token != NULL) {
        array->values[i++] = Value::makeStringRef(token, -1, 0x45209ec0);
        token = strtok(NULL, delim);
    }

    eez::free(strCopy);

    stack.push(arrayValue);
}

static void do_OPERATION_TYPE_STRING_FROM_CODE_POINT(EvalStack &stack) {
    Value charCodeValue = stack.pop().getValue();
    if (charCodeValue.isError()) {
        stack.push(charCodeValue);
        return;
    }

    int err = 0;
    int32_t charCode = charCodeValue.toInt32(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

    char str[16] = {0};

    utf8catcodepoint(str, charCode, sizeof(str));

    Value resultValue = Value::makeStringRef(str, strlen(str), 0x02c2e778);
    stack.push(resultValue);
    return;
}

static void do_OPERATION_TYPE_STRING_CODE_POINT_AT(EvalStack &stack) {
    auto strValue = stack.pop().getValue();
    if (strValue.isError()) {
        stack.push(strValue);
        return;
    }

    Value indexValue = stack.pop().getValue();
    if (indexValue.isError()) {
        stack.push(indexValue);
        return;
    }

    utf8_int32_t codePoint = 0;

    const char *str = strValue.getString();
    if (str) {
        int index = indexValue.toInt32();
        if (index >= 0) {
            do {
                str = utf8codepoint(str, &codePoint);
            } while (codePoint && --index >= 0);
        }
    }

    stack.push(Value((int)codePoint, VALUE_TYPE_INT32));

    return;
}

static void do_OPERATION_TYPE_ARRAY_LENGTH(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (a.isArray()) {
        auto array = a.getArray();
        stack.push(Value(array->arraySize, VALUE_TYPE_UINT32));
        return;
    }

    if (a.isBlob()) {
        auto blobRef = a.getBlob();
        stack.push(Value(blobRef->len, VALUE_TYPE_UINT32));
        return;
    }

#if defined(EEZ_DASHBOARD_API)
    if (a.isJson()) {
        int length = operationJsonArrayLength(a.getInt());
        if (length >= 0) {
            stack.push(Value(length, VALUE_TYPE_UINT32));
            return;
        }
    }
#endif

    stack.push(Value::makeError());
}

static void do_OPERATION_TYPE_ARRAY_SLICE(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }

    int from = 0;
    if (numArgs > 1) {
        auto fromValue = stack.pop().getValue();
        if (fromValue.isError()) {
            stack.push(fromValue);
            return;
        }

        int err;
        from = fromValue.toInt32(&err);
        if (err) {
            stack.push(Value::makeError());
            return;
        }

        if (from < 0) {
            from = 0;
        }
    }

    int to = -1;
    if (numArgs > 2) {
        auto toValue = stack.pop().getValue();
        if (toValue.isError()) {
            stack.push(toValue);
            return;
        }
        int err;
        to = toValue.toInt32(&err);
        if (err) {
            stack.push(Value::makeError());
            return;
        }

        if (to < 0) {
            to = 0;
        }
    }

#if defined(EEZ_DASHBOARD_API)
    if (arrayValue.isJson()) {
        stack.push(operationJsonArraySlice(arrayValue.getInt(), from, to));
        return;
    }
#endif

    if (!arrayValue.isArray()) {
        stack.push(Value::makeError());
        return;
    }
    auto array = arrayValue.getArray();

    if (to == -1) {
        to = array->arraySize;
    }

    if (from > to) {
        stack.push(Value::makeError());
        return;
    }

    auto size = to - from;

    auto resultArrayValue = Value::makeArrayRef(size, array->arrayType, 0xe2d78c65);
    auto resultArray = resultArrayValue.getArray();

    for (int elementIndex = from; elementIndex < to && elementIndex < (int)array->arraySize; elementIndex++) {
        resultArray->values[elementIndex - from] = array->values[elementIndex];
    }

    stack.push(resultArrayValue);
}

static void do_OPERATION_TYPE_ARRAY_ALLOCATE(EvalStack &stack) {
    auto sizeValue = stack.pop();
    if (sizeValue.isError()) {
        stack.push(sizeValue);
        return;
    }
    int err;
    auto size = sizeValue.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    auto resultArrayValue = Value::makeArrayRef(size, defs_v3::ARRAY_TYPE_ANY, 0xe2d78c65);

    stack.push(resultArrayValue);
}

static void do_OPERATION_TYPE_ARRAY_APPEND(EvalStack &stack) {
    auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }
    auto value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }

#if defined(EEZ_DASHBOARD_API)
    if (arrayValue.isJson()) {
        stack.push(operationJsonArrayAppend(arrayValue.getInt(), &value));
        return;
    }
#endif

    if (!arrayValue.isArray()) {
        stack.push(Value::makeError());
        return;
    }

    auto array = arrayValue.getArray();
    auto resultArrayValue = Value::makeArrayRef(array->arraySize + 1, array->arrayType, 0x664c3199);
    auto resultArray = resultArrayValue.getArray();

    for (uint32_t elementIndex = 0; elementIndex < array->arraySize; elementIndex++) {
        resultArray->values[elementIndex] = array->values[elementIndex];
    }

    resultArray->values[array->arraySize] = value;

    stack.push(resultArrayValue);
}

static void do_OPERATION_TYPE_ARRAY_INSERT(EvalStack &stack) {
    auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }
    auto positionValue = stack.pop().getValue();
    if (positionValue.isError()) {
        stack.push(positionValue);
        return;
    }
    auto value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }

    int err;
    auto position = positionValue.toInt32(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }
#if defined(EEZ_DASHBOARD_API)
    if (arrayValue.isJson()) {
        stack.push(operationJsonArrayInsert(arrayValue.getInt(), position, &value));
        return;
    }
#endif

    if (!arrayValue.isArray()) {
        stack.push(Value::makeError());
        return;
    }

    auto array = arrayValue.getArray();
    auto resultArrayValue = Value::makeArrayRef(array->arraySize + 1, array->arrayType, 0xc4fa9cd9);
    auto resultArray = resultArrayValue.getArray();

    if (position < 0) {
        position = 0;
    } else if ((uint32_t)position > array->arraySize) {
        position = array->arraySize;
    }

    for (uint32_t elementIndex = 0; (int)elementIndex < position; elementIndex++) {
        resultArray->values[elementIndex] = array->values[elementIndex];
    }

    resultArray->values[position] = value;

    for (uint32_t elementIndex = position; elementIndex < array->arraySize; elementIndex++) {
        resultArray->values[elementIndex + 1] = array->values[elementIndex];
    }

    stack.push(resultArrayValue);
}

static void do_OPERATION_TYPE_ARRAY_REMOVE(EvalStack &stack) {
    auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }
    auto positionValue = stack.pop().getValue();
    if (positionValue.isError()) {
        stack.push(positionValue);
        return;
    }

    int err;
    auto position = positionValue.toInt32(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

#if defined(EEZ_DASHBOARD_API)
    if (arrayValue.isJson()) {
        stack.push(operationJsonArrayRemove(arrayValue.getInt(), position));
        return;
    }
#endif

    if (!arrayValue.isArray()) {
        stack.push(Value::makeError());
        return;
    }

    auto array = arrayValue.getArray();

    if (position >= 0 && position < (int32_t)array->arraySize) {
        auto resultArrayValue = Value::makeArrayRef(array->arraySize - 1, array->arrayType, 0x40e9bb4b);
        auto resultArray = resultArrayValue.getArray();

        for (uint32_t elementIndex = 0; (int)elementIndex < position; elementIndex++) {
            resultArray->values[elementIndex] = array->values[elementIndex];
        }

        for (uint32_t elementIndex = position + 1; elementIndex < array->arraySize; elementIndex++) {
            resultArray->values[elementIndex - 1] = array->values[elementIndex];
        }

        stack.push(resultArrayValue);
    } else {
        // out of bounds error
        stack.push(Value::makeError());
    }
}

static void do_OPERATION_TYPE_ARRAY_CLONE(EvalStack &stack) {
    auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }

    auto resultArray = arrayValue.clone();

    stack.push(resultArray);
}

static void do_OPERATION_TYPE_LVGL_METER_TICK_INDEX(EvalStack &stack) {
    stack.push(g_eezFlowLvlgMeterTickIndex);
}

static void do_OPERATION_TYPE_CRYPTO_SHA256(EvalStack &stack) {
#if EEZ_FOR_LVGL_SHA256_OPTION
    auto value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }

    const uint8_t *data;
    uint32_t dataLen;

    if (value.isString()) {
        const char *str = value.getString();
        data = (uint8_t *)str;
        dataLen = strlen(str);
    } else if (value.isBlob()) {
        auto blobRef = value.getBlob();
        data = blobRef->blob;
        dataLen = blobRef->len;
    } else {
        stack.push(Value::makeError());
        return;
    }

    BYTE buf[SHA256_BLOCK_SIZE];
    SHA256_CTX ctx;
    sha256_init(&ctx);
	sha256_update(&ctx, data, dataLen);
	sha256_final(&ctx, buf);

    auto result = Value::makeBlobRef(buf, SHA256_BLOCK_SIZE, 0x1f0c0c0c);

    stack.push(result);
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_BLOB_ALLOCATE(EvalStack &stack) {
    auto sizeValue = stack.pop();
    if (sizeValue.isError()) {
        stack.push(sizeValue);
        return;
    }
    int err;
    auto size = sizeValue.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    auto result = Value::makeBlobRef(nullptr, size, 0xd3de43f1);

    stack.push(result);
}

static void do_OPERATION_TYPE_BLOB_TO_STRING(EvalStack &stack) {
#if defined(EEZ_DASHBOARD_API)
    auto blobValue = stack.pop().getValue();

    if (blobValue.isError()) {
        stack.push(blobValue);
        return;
    }

    if (!blobValue.isBlob()) {
        stack.push(Value::makeError());
        return;
    }

    auto blobRef = blobValue.getBlob();

    stack.push(operationBlobToString(blobRef->blob, blobRef->len));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_JSON_GET(EvalStack &stack) {
#if defined(EEZ_DASHBOARD_API)
    auto jsonValue = stack.pop().getValue();
    auto propertyValue = stack.pop();

    if (jsonValue.isError()) {
        stack.push(jsonValue);
        return;
    }

    if (jsonValue.type != VALUE_TYPE_JSON) {
        stack.push(Value::makeError());
        return;
    }

    if (propertyValue.isError()) {
        stack.push(propertyValue);
        return;
    }

    stack.push(Value::makeJsonMemberRef(jsonValue, propertyValue.toString(0xc73d02e7), 0xebcc230a));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_JSON_CLONE(EvalStack &stack) {
#if defined(EEZ_DASHBOARD_API)
    auto jsonValue = stack.pop().getValue();

    if (jsonValue.isError()) {
        stack.push(jsonValue);
        return;
    }

    if (jsonValue.type != VALUE_TYPE_JSON) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(operationJsonClone(jsonValue.getInt()));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_EVENT_GET_CODE(EvalStack &stack) {
#if defined(EEZ_FOR_LVGL)
    auto eventValue = stack.pop().getValue();
    if (eventValue.type != VALUE_TYPE_EVENT) {
        stack.push(Value::makeError());
        return;
    }
    auto event = eventValue.getLVGLEventRef();

    stack.push(Value(event->code, VALUE_TYPE_UINT32));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_EVENT_GET_CURRENT_TARGET(EvalStack &stack) {
#if defined(EEZ_FOR_LVGL)
    auto eventValue = stack.pop().getValue();
    if (eventValue.type != VALUE_TYPE_EVENT) {
        stack.push(Value::makeError());
        return;
    }
    auto event = eventValue.getLVGLEventRef();

    stack.push(Value(event->currentTarget, VALUE_TYPE_WIDGET));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_EVENT_GET_TARGET(EvalStack &stack) {
#if defined(EEZ_FOR_LVGL)
    auto eventValue = stack.pop().getValue();
    if (eventValue.type != VALUE_TYPE_EVENT) {
        stack.push(Value::makeError());
        return;
    }
    auto event = eventValue.getLVGLEventRef();

    stack.push(Value(event->target, VALUE_TYPE_WIDGET));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_EVENT_GET_USER_DATA(EvalStack &stack) {
#if defined(EEZ_FOR_LVGL)
    auto eventValue = stack.pop().getValue();
    if (eventValue.type != VALUE_TYPE_EVENT) {
        stack.push(Value::makeError());
        return;
    }
    auto event = eventValue.getLVGLEventRef();

    stack.push(Value((int)event->userData, VALUE_TYPE_INT32));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_EVENT_GET_KEY(EvalStack &stack) {
#if defined(EEZ_FOR_LVGL)
    auto eventValue = stack.pop().getValue();
    if (eventValue.type != VALUE_TYPE_EVENT) {
        stack.push(Value::makeError());
        return;
    }
    auto event = eventValue.getLVGLEventRef();

    stack.push(Value(event->key, VALUE_TYPE_UINT32));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_EVENT_GET_GESTURE_DIR(EvalStack &stack) {
#if defined(EEZ_FOR_LVGL)
    auto eventValue = stack.pop().getValue();
    if (eventValue.type != VALUE_TYPE_EVENT) {
        stack.push(Value::makeError());
        return;
    }
    auto event = eventValue.getLVGLEventRef();

    stack.push(Value((int)event->gestureDir, VALUE_TYPE_INT32));
#else
    stack.push(Value::makeError());
#endif
}

static void do_OPERATION_TYPE_EVENT_GET_ROTARY_DIFF(EvalStack &stack) {
#if defined(EEZ_FOR_LVGL)
    auto eventValue = stack.pop().getValue();
    if (eventValue.type != VALUE_TYPE_EVENT) {
        stack.push(Value::makeError());
        return;
    }
    auto event = eventValue.getLVGLEventRef();

    stack.push(Value((int)event->rotaryDiff, VALUE_TYPE_INT32));
#else
    stack.push(Value::makeError());
#endif
}

EvalOperation g_evalOperations[] = {
    do_OPERATION_TYPE_ADD,
    do_OPERATION_TYPE_SUB,
    do_OPERATION_TYPE_MUL,
    do_OPERATION_TYPE_DIV,
    do_OPERATION_TYPE_MOD,
    do_OPERATION_TYPE_LEFT_SHIFT,
    do_OPERATION_TYPE_RIGHT_SHIFT,
    do_OPERATION_TYPE_BINARY_AND,
    do_OPERATION_TYPE_BINARY_OR,
    do_OPERATION_TYPE_BINARY_XOR,
    do_OPERATION_TYPE_EQUAL,
    do_OPERATION_TYPE_NOT_EQUAL,
    do_OPERATION_TYPE_LESS,
    do_OPERATION_TYPE_GREATER,
    do_OPERATION_TYPE_LESS_OR_EQUAL,
    do_OPERATION_TYPE_GREATER_OR_EQUAL,
    do_OPERATION_TYPE_LOGICAL_AND,
    do_OPERATION_TYPE_LOGICAL_OR,
    do_OPERATION_TYPE_UNARY_PLUS,
    do_OPERATION_TYPE_UNARY_MINUS,
    do_OPERATION_TYPE_BINARY_ONE_COMPLEMENT,
    do_OPERATION_TYPE_NOT,
    do_OPERATION_TYPE_CONDITIONAL,
    do_OPERATION_TYPE_SYSTEM_GET_TICK,
    do_OPERATION_TYPE_FLOW_INDEX,
    do_OPERATION_TYPE_FLOW_IS_PAGE_ACTIVE,
    do_OPERATION_TYPE_FLOW_PAGE_TIMELINE_POSITION,
    do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE,
    do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE,
    do_OPERATION_TYPE_FLOW_LANGUAGES,
    do_OPERATION_TYPE_FLOW_TRANSLATE,
    do_OPERATION_TYPE_FLOW_PARSE_INTEGER,
    do_OPERATION_TYPE_FLOW_PARSE_FLOAT,
    do_OPERATION_TYPE_FLOW_PARSE_DOUBLE,
    do_OPERATION_TYPE_DATE_NOW,
    do_OPERATION_TYPE_DATE_TO_STRING,
    do_OPERATION_TYPE_DATE_FROM_STRING,
    do_OPERATION_TYPE_MATH_SIN,
    do_OPERATION_TYPE_MATH_COS,
    do_OPERATION_TYPE_MATH_LOG,
    do_OPERATION_TYPE_MATH_LOG10,
    do_OPERATION_TYPE_MATH_ABS,
    do_OPERATION_TYPE_MATH_FLOOR,
    do_OPERATION_TYPE_MATH_CEIL,
    do_OPERATION_TYPE_MATH_ROUND,
    do_OPERATION_TYPE_MATH_MIN,
    do_OPERATION_TYPE_MATH_MAX,
    do_OPERATION_TYPE_STRING_LENGTH,
    do_OPERATION_TYPE_STRING_SUBSTRING,
    do_OPERATION_TYPE_STRING_FIND,
    do_OPERATION_TYPE_STRING_PAD_START,
    do_OPERATION_TYPE_STRING_SPLIT,
    do_OPERATION_TYPE_ARRAY_LENGTH,
    do_OPERATION_TYPE_ARRAY_SLICE,
    do_OPERATION_TYPE_ARRAY_ALLOCATE,
    do_OPERATION_TYPE_ARRAY_APPEND,
    do_OPERATION_TYPE_ARRAY_INSERT,
    do_OPERATION_TYPE_ARRAY_REMOVE,
    do_OPERATION_TYPE_ARRAY_CLONE,
    do_OPERATION_TYPE_DATE_TO_LOCALE_STRING,
    do_OPERATION_TYPE_DATE_GET_YEAR,
    do_OPERATION_TYPE_DATE_GET_MONTH,
    do_OPERATION_TYPE_DATE_GET_DAY,
    do_OPERATION_TYPE_DATE_GET_HOURS,
    do_OPERATION_TYPE_DATE_GET_MINUTES,
    do_OPERATION_TYPE_DATE_GET_SECONDS,
    do_OPERATION_TYPE_DATE_GET_MILLISECONDS,
    do_OPERATION_TYPE_DATE_MAKE,
    do_OPERATION_TYPE_MATH_POW,
    do_OPERATION_TYPE_LVGL_METER_TICK_INDEX,
    do_OPERATION_TYPE_FLOW_GET_BITMAP_INDEX,
    do_OPERATION_TYPE_FLOW_TO_INTEGER,
    do_OPERATION_TYPE_STRING_FROM_CODE_POINT,
    do_OPERATION_TYPE_STRING_CODE_POINT_AT,
    do_OPERATION_TYPE_CRYPTO_SHA256,
    do_OPERATION_TYPE_BLOB_ALLOCATE,
    do_OPERATION_TYPE_JSON_GET,
    do_OPERATION_TYPE_JSON_CLONE,
    do_OPERATION_TYPE_FLOW_GET_BITMAP_AS_DATA_URL,
    do_OPERATION_TYPE_STRING_FORMAT,
    do_OPERATION_TYPE_STRING_FORMAT_PREFIX,
    do_OPERATION_TYPE_EVENT_GET_CODE,
    do_OPERATION_TYPE_EVENT_GET_CURRENT_TARGET,
    do_OPERATION_TYPE_EVENT_GET_TARGET,
    do_OPERATION_TYPE_EVENT_GET_USER_DATA,
    do_OPERATION_TYPE_EVENT_GET_KEY,
    do_OPERATION_TYPE_EVENT_GET_GESTURE_DIR,
    do_OPERATION_TYPE_EVENT_GET_ROTARY_DIFF,
    do_OPERATION_TYPE_BLOB_TO_STRING,
    do_OPERATION_TYPE_FLOW_THEMES,
};

} // namespace flow
} // namespace eez

