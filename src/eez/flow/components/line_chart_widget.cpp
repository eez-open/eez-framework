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

#include <eez/core/alloc.h>

#include <eez/flow/components.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>
#include <eez/flow/components/line_chart_widget.h>

namespace eez {
namespace flow {

LineChartWidgetComponenentExecutionState::LineChartWidgetComponenentExecutionState()
    : data(nullptr)
{
}

LineChartWidgetComponenentExecutionState::~LineChartWidgetComponenentExecutionState() {
    if (data != nullptr) {
        auto xValues = (Value *)data;
        for (uint32_t i = 0; i < maxPoints; i++) {
            (xValues + i)->~Value();
        }
        eez::free(data);
    }

    for (uint32_t i = 0; i < numLines; i++) {
		(lineLabels + i)->~Value();
	}
    eez::free(lineLabels);
}

void LineChartWidgetComponenentExecutionState::init(uint32_t numLines_, uint32_t maxPoints_) {
    numLines = numLines_;
    maxPoints = maxPoints_;

    data = eez::alloc(maxPoints * sizeof(Value) + maxPoints * numLines * sizeof(float), 0xe4945fea);

    auto xValues = (Value *)data;
    for (uint32_t i = 0; i < maxPoints; i++) {
		new (xValues + i) Value();
	}

    numPoints = 0;
    startPointIndex = 0;

    lineLabels = (Value *)eez::alloc(numLines * sizeof(Value), 0xe8afd215);
    for (uint32_t i = 0; i < numLines; i++) {
		new (lineLabels + i) Value();
	}

    updated = true;
}

Value LineChartWidgetComponenentExecutionState::getX(int pointIndex) {
    auto xValues = (Value *)data;
    return xValues[pointIndex];
}

void LineChartWidgetComponenentExecutionState::setX(int pointIndex, Value& value) {
    auto xValues = (Value *)data;
    xValues[pointIndex] = value;
}

float LineChartWidgetComponenentExecutionState::getY(int pointIndex, int lineIndex) {
    auto yValues = (float *)((Value *)data + maxPoints);
    return *(yValues + pointIndex * numLines + lineIndex);
}

void LineChartWidgetComponenentExecutionState::setY(int pointIndex, int lineIndex, float value) {
    auto yValues = (float *)((Value *)data + maxPoints);
    *(yValues + pointIndex * numLines + lineIndex) = value;
}

bool LineChartWidgetComponenentExecutionState::onInputValue(FlowState *flowState, unsigned componentIndex) {
    auto component = (LineChartWidgetComponenent *)flowState->flow->components[componentIndex];

    uint32_t pointIndex;

    if (numPoints < component->maxPoints) {
        pointIndex = numPoints++;
    } else {
        startPointIndex = (startPointIndex + 1) % component->maxPoints;
        pointIndex = (startPointIndex + component->maxPoints - 1) % component->maxPoints;
    }

    Value value;
    if (!evalExpression(flowState, componentIndex, component->xValue, value, FlowError::Plain("Failed to evaluate x value in LineChartWidget"))) {
        return false;
    }

    int err;
    value.toDouble(&err);
    if (err) {
        throwError(flowState, componentIndex, FlowError::Plain("X value not an number or date"));
        return false;
    }

    setX(pointIndex, value);

    for (uint32_t lineIndex = 0; lineIndex < numLines; lineIndex++) {
        Value value;
        if (!evalExpression(flowState, componentIndex, component->lines[lineIndex]->value, value, FlowError::PropertyInArray("LineChart Widget", "Line value", lineIndex))) {
            return false;
        }

        int err;
        auto y = value.toFloat(&err);
        if (err) {
            throwError(flowState, componentIndex, FlowError::PropertyInArrayConvert("LineChart Widget", "Line value", "float", lineIndex));
            return false;
        }

        setY(pointIndex, lineIndex, y);
    }

    return true;
}

void executeLineChartWidgetComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (LineChartWidgetComponenent *)flowState->flow->components[componentIndex];

    auto executionState = (LineChartWidgetComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];
    if (!executionState) {
        executionState = allocateComponentExecutionState<LineChartWidgetComponenentExecutionState>(flowState, componentIndex);
        executionState->init(component->lines.count, component->maxPoints);

        for (uint32_t lineIndex = 0; lineIndex < component->lines.count; lineIndex++) {
            if (!evalExpression(flowState, componentIndex, component->lines[lineIndex]->label, executionState->lineLabels[lineIndex], FlowError::PropertyInArray("LineChart Widget", "Line label", lineIndex))) {
                return;
            }
        }
    }

    // reset input is at position 0
    int resetInputIndex = 0;

    // value input must be at position 1
    int valueInputIndex = 1;

    if (flowState->values[component->inputs[resetInputIndex]].type != VALUE_TYPE_UNDEFINED) {
        // reset
        executionState->numPoints = 0;
        executionState->startPointIndex = 0;
        executionState->updated = true;

        clearInputValue(flowState, component->inputs[resetInputIndex]);
    }

    // value input must be at position 0
    auto valueInputIndexInFlow = component->inputs[valueInputIndex];
    auto inputValue = flowState->values[valueInputIndexInFlow];
    if (inputValue.type != VALUE_TYPE_UNDEFINED) {
        // data
        if (inputValue.isArray() && inputValue.getArray()->arrayType == defs_v3::ARRAY_TYPE_ANY) {
            auto array = inputValue.getArray();
            bool updated = false;
            executionState->startPointIndex = 0;
            executionState->numPoints = 0;
            for (uint32_t elementIndex = 0; elementIndex < array->arraySize; elementIndex++) {
                flowState->values[valueInputIndexInFlow] = array->values[elementIndex];
                if (executionState->onInputValue(flowState, componentIndex)) {
                    updated = true;
                } else {
                    break;
                }
            }
            if (updated) {
                executionState->updated = true;
            }
        } else {
            if (executionState->onInputValue(flowState, componentIndex)) {
                executionState->updated = true;
            }
        }

        clearInputValue(flowState, valueInputIndexInFlow);
    }
}

} // namespace flow
} // namespace eez

#endif // EEZ_OPTION_GUI
