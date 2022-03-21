
/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(__EMSCRIPTEN__)

#include <stdlib.h>

#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include <eez/flow/expression.h>

namespace eez {
namespace flow {

const char *DashboardComponentContext::getStringParam(int offset) {
    auto component = flowState->flow->components[componentIndex];
    auto ptr = (const uint32_t *)((const uint8_t *)component + sizeof(Component) + offset);
    return (const char *)(MEMORY_BEGIN + 4 + *ptr);
}

struct ExpressionList {
    uint32_t count;
    Value values[1];
};

void *DashboardComponentContext::getExpressionListParam(int offset) {
    auto component = flowState->flow->components[componentIndex];

    struct List {
        uint32_t count;
        uint32_t items;
    };
    auto list = (const List *)((const uint8_t *)component + sizeof(Component) + offset);

    auto expressionList = (ExpressionList *)::malloc((list->count + 1) * sizeof(Value));

    expressionList->count = list->count;

    auto items = (const uint32_t *)(MEMORY_BEGIN + 4 + list->items);

    for (uint32_t i = 0; i < list->count; i++) {
        // call Value constructor
        new (expressionList->values + i) Value();

        auto valueExpression = (const uint8_t *)(MEMORY_BEGIN + 4 + items[i]);
        if (!evalExpression(flowState, componentIndex, valueExpression, expressionList->values[i])) {
            eez::flow::throwError(flowState, componentIndex, "Failed to evaluate expression");
            return nullptr;
        }
    }

    return expressionList;
}

void DashboardComponentContext::freeExpressionListParam(void *ptr) {
    auto expressionList = (ExpressionList*)ptr;

    for (uint32_t i = 0; i < expressionList->count; i++) {
        // call Value desctructor
        (expressionList->values + i)->~Value();
    }

    ::free(ptr);
}

void DashboardComponentContext::propagateIntValue(unsigned outputIndex, int value) {
    Value intValue = value;
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, intValue);
}

void DashboardComponentContext::propagateDoubleValue(unsigned outputIndex, double value) {
    Value doubleValue(value, VALUE_TYPE_DOUBLE);
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, doubleValue);
}

void DashboardComponentContext::propagateBooleanValue(unsigned outputIndex, bool value) {
    Value doubleValue(value, VALUE_TYPE_BOOLEAN);
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, doubleValue);
}

void DashboardComponentContext::propagateStringValue(unsigned outputIndex, const char *value) {
    Value stringValue = Value::makeStringRef(value, strlen(value), 0x4d05952a);
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, stringValue);
}

void DashboardComponentContext::propagateValueThroughSeqout() {
	eez::flow::propagateValueThroughSeqout(flowState, componentIndex);
}

void DashboardComponentContext::throwError(const char *errorMessage) {
	eez::flow::throwError(flowState, componentIndex, errorMessage);
}

} // flow
} // eez

using namespace eez::flow;

EM_PORT_API(const char *) DashboardContext_getStringParam(DashboardComponentContext *context, int offset) {
    return context->getStringParam(offset);
}

EM_PORT_API(void *) DashboardContext_getExpressionListParam(DashboardComponentContext *context, int offset) {
    return context->getExpressionListParam(offset);
}

EM_PORT_API(void) DashboardContext_freeExpressionListParam(DashboardComponentContext *context, void *ptr) {
    return context->freeExpressionListParam(ptr);
}

EM_PORT_API(void) DashboardContext_propagateIntValue(DashboardComponentContext *context, unsigned outputIndex, int value) {
    context->propagateIntValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateDoubleValue(DashboardComponentContext *context, unsigned outputIndex, double value) {
    context->propagateDoubleValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateBooleanValue(DashboardComponentContext *context, unsigned outputIndex, bool value) {
    context->propagateBooleanValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateStringValue(DashboardComponentContext *context, unsigned outputIndex, const char *value) {
    context->propagateStringValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateValueThroughSeqout(DashboardComponentContext *context) {
    context->propagateValueThroughSeqout();
}

EM_PORT_API(void) DashboardContext_throwError(DashboardComponentContext *context, const char *errorMessage) {
    context->throwError(errorMessage);
}

#endif // __EMSCRIPTEN__
