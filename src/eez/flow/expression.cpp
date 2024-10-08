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

#include <eez/flow/private.h>
#include <eez/flow/operations.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

namespace eez {
namespace flow {

EvalStack g_stack;

static void evalExpression(FlowState *flowState, const uint8_t *instructions, int *numInstructionBytes) {
	auto flowDefinition = flowState->flowDefinition;
	auto flow = flowState->flow;

	int i = 0;
	while (true) {
		uint16_t instruction = instructions[i] + (instructions[i + 1] << 8);
		auto instructionType = instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK;
		auto instructionArg = instruction & EXPR_EVAL_INSTRUCTION_PARAM_MASK;
		if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT) {
			g_stack.push(*flowDefinition->constants[instructionArg]);
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT) {
			g_stack.push(flowState->values[instructionArg]);
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR) {
			g_stack.push(&flowState->values[flow->componentInputs.count + instructionArg]);
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR) {
			if ((uint32_t)instructionArg < flowDefinition->globalVariables.count) {
                if (g_globalVariables) {
				    g_stack.push(g_globalVariables->values + instructionArg);
                } else {
                    g_stack.push(flowDefinition->globalVariables[instructionArg]);
                }
			} else {
				// native variable
				g_stack.push(Value((int)(instructionArg - flowDefinition->globalVariables.count + 1), VALUE_TYPE_NATIVE_VARIABLE));
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_OUTPUT) {
			g_stack.push(Value((uint16_t)instructionArg, VALUE_TYPE_FLOW_OUTPUT));
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_ARRAY_ELEMENT) {
			auto elementIndexValue = g_stack.pop().getValue();
			auto arrayValue = g_stack.pop().getValue();

            if (arrayValue.getType() == VALUE_TYPE_UNDEFINED || arrayValue.getType() == VALUE_TYPE_NULL) {
                g_stack.push(Value(0, VALUE_TYPE_UNDEFINED));
            } else {
                if (arrayValue.isArray()) {
                    auto array = arrayValue.getArray();

                    int err;
                    auto elementIndex = elementIndexValue.toInt32(&err);
                    if (!err) {
                        if (elementIndex >= 0 && elementIndex < (int)array->arraySize) {
                            g_stack.push(Value::makeArrayElementRef(arrayValue, elementIndex, 0x132e0e2f));
                        } else {
                            g_stack.push(Value::makeError());
                            g_stack.setErrorMessage("Array element index out of bounds\n");
                        }
                    } else {
                        g_stack.push(Value::makeError());
                        g_stack.setErrorMessage("Integer value expected for array element index\n");
                    }
                } else if (arrayValue.isBlob()) {
                    auto blobRef = arrayValue.getBlob();

                    int err;
                    auto elementIndex = elementIndexValue.toInt32(&err);
                    if (!err) {
                        if (elementIndex >= 0 && elementIndex < (int)blobRef->len) {
                            g_stack.push(Value::makeArrayElementRef(arrayValue, elementIndex, 0x132e0e2f));
                        } else {
                            g_stack.push(Value::makeError());
                            g_stack.setErrorMessage("Blob element index out of bounds\n");
                        }
                    } else {
                        g_stack.push(Value::makeError());
                        g_stack.setErrorMessage("Integer value expected for blob element index\n");
                    }

                } else {
                    g_stack.push(Value::makeError());
                    g_stack.setErrorMessage("Array value expected\n");
                }
            }
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_OPERATION) {
			g_evalOperations[instructionArg](g_stack);
		} else {
            if (instruction == EXPR_EVAL_INSTRUCTION_TYPE_END_WITH_DST_VALUE_TYPE) {
    			i += 2;
                if (g_stack.sp == 1) {
                    auto finalResult = g_stack.pop();

                    #define VALUE_TYPE (instructions[i] + (instructions[i + 1] << 8) + (instructions[i + 2] << 16) + (instructions[i + 3] << 24))
                    if (finalResult.getType() == VALUE_TYPE_VALUE_PTR) {
                        finalResult.dstValueType = VALUE_TYPE;
                    } else if (finalResult.getType() == VALUE_TYPE_ARRAY_ELEMENT_VALUE) {
                        auto arrayElementValue = (ArrayElementValue *)finalResult.refValue;
                        arrayElementValue->dstValueType = VALUE_TYPE;
                    }

                    g_stack.push(finalResult);
                }
                i += 4;
                break;
            } else {
			    i += 2;
			    break;
            }
		}

		i += 2;
	}

	if (numInstructionBytes) {
		*numInstructionBytes = i;
	}
}

#if EEZ_OPTION_GUI
bool evalExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const FlowError &errorMessage, int *numInstructionBytes, const int32_t *iterators, DataOperationEnum operation) {
#else
bool evalExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const FlowError &errorMessage, int *numInstructionBytes, const int32_t *iterators) {
#endif
	//g_stack.sp = 0;

    size_t savedSp = g_stack.sp;
    FlowState *savedFlowState = g_stack.flowState;
	int savedComponentIndex = g_stack.componentIndex;
	const int32_t *savedIterators = g_stack.iterators;
    const char *savedErrorMessage = g_stack.errorMessage;

	g_stack.flowState = flowState;
	g_stack.componentIndex = componentIndex;
	g_stack.iterators = iterators;
    g_stack.errorMessage = nullptr;

	evalExpression(flowState, instructions, numInstructionBytes);

	g_stack.flowState = savedFlowState;
	g_stack.componentIndex = savedComponentIndex;
	g_stack.iterators = savedIterators;
    g_stack.errorMessage = savedErrorMessage;

    if (g_stack.sp == savedSp + 1) {
#if EEZ_OPTION_GUI
        if (operation == DATA_OPERATION_GET_TEXT_REFRESH_RATE) {
            result = g_stack.pop();
            if (!result.isError()) {
                if (result.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
                    auto nativeVariableId = result.getInt();
                    result = Value(getTextRefreshRate(g_widgetCursor, nativeVariableId), VALUE_TYPE_UINT32);
                } else {
                    result = 0;
                }
                return true;
            }
        } else if (operation == DATA_OPERATION_GET_TEXT_CURSOR_POSITION) {
            result = g_stack.pop();
            if (!result.isError()) {
                if (result.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
                    auto nativeVariableId = result.getInt();
                    result = Value(getTextCursorPosition(g_widgetCursor, nativeVariableId), VALUE_TYPE_INT32);
                } else {
                    result = Value();
                }
                return true;
            }
        } else {
#endif
            result = g_stack.pop().getValue();
            if (!result.isError()) {
                return true;
            }
#if EEZ_OPTION_GUI
        }
#endif
    }

    FlowError flowError = errorMessage.setDescription(g_stack.errorMessage);
    throwError(flowState, componentIndex, flowError);
	return false;
}

bool evalAssignableExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const FlowError &errorMessage, int *numInstructionBytes, const int32_t *iterators) {
    FlowState *savedFlowState = g_stack.flowState;
	int savedComponentIndex = g_stack.componentIndex;
	const int32_t *savedIterators = g_stack.iterators;
    const char *savedErrorMessage = g_stack.errorMessage;

	g_stack.flowState = flowState;
	g_stack.componentIndex = componentIndex;
	g_stack.iterators = iterators;
    g_stack.errorMessage = nullptr;

	evalExpression(flowState, instructions, numInstructionBytes);

	g_stack.flowState = savedFlowState;
	g_stack.componentIndex = savedComponentIndex;
	g_stack.iterators = savedIterators;
    g_stack.errorMessage = savedErrorMessage;

    if (g_stack.sp == 1) {
        auto finalResult = g_stack.pop();
        if (
            finalResult.getType() == VALUE_TYPE_VALUE_PTR ||
            finalResult.getType() == VALUE_TYPE_NATIVE_VARIABLE ||
            finalResult.getType() == VALUE_TYPE_FLOW_OUTPUT ||
            finalResult.getType() == VALUE_TYPE_ARRAY_ELEMENT_VALUE ||
            finalResult.getType() == VALUE_TYPE_JSON_MEMBER_VALUE
        ) {
            result = finalResult;
            return true;
        }
    }

    errorMessage.setDescription(g_stack.errorMessage);
    throwError(flowState, componentIndex, errorMessage);

	return false;
}

#if EEZ_OPTION_GUI
bool evalProperty(FlowState *flowState, int componentIndex, int propertyIndex, Value &result, const FlowError &errorMessage, int *numInstructionBytes, const int32_t *iterators, DataOperationEnum operation) {
#else
bool evalProperty(FlowState *flowState, int componentIndex, int propertyIndex, Value &result, const FlowError &errorMessage, int *numInstructionBytes, const int32_t *iterators) {
#endif
    if (componentIndex < 0 || componentIndex >= (int)flowState->flow->components.count) {
        char message[256];
        snprintf(message, sizeof(message), "invalid component index %d in flow at index %d", componentIndex, flowState->flowIndex);
        FlowError flowError = errorMessage.setDescription(message);
        throwError(flowState, componentIndex, flowError);
        return false;
    }
    auto component = flowState->flow->components[componentIndex];
    if (propertyIndex < 0 || propertyIndex >= (int)component->properties.count) {
        char message[256];
        snprintf(message, sizeof(message), "invalid property index %d in component at index %d in flow at index %d", propertyIndex, componentIndex, flowState->flowIndex);
        FlowError flowError = errorMessage.setDescription(message);
        throwError(flowState, componentIndex, flowError);
        return false;
    }
#if EEZ_OPTION_GUI
    return evalExpression(flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, result, errorMessage, numInstructionBytes, iterators, operation);
#else
    return evalExpression(flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, result, errorMessage, numInstructionBytes, iterators);
#endif
}

bool evalAssignableProperty(FlowState *flowState, int componentIndex, int propertyIndex, Value &result, const FlowError &errorMessage, int *numInstructionBytes, const int32_t *iterators) {
    if (componentIndex < 0 || componentIndex >= (int)flowState->flow->components.count) {
        char message[256];
        snprintf(message, sizeof(message), "invalid component index %d in flow at index %d", componentIndex, flowState->flowIndex);
        FlowError flowError = errorMessage.setDescription(message);
        throwError(flowState, componentIndex, flowError);
        return false;
    }
    auto component = flowState->flow->components[componentIndex];
    if (propertyIndex < 0 || propertyIndex >= (int)component->properties.count) {
        char message[256];
        snprintf(message, sizeof(message), "invalid property index %d in component at index %d in flow at index %d", propertyIndex, componentIndex, flowState->flowIndex);
        FlowError flowError = errorMessage.setDescription(message);
        throwError(flowState, componentIndex, flowError);
        return false;
    }
    return evalAssignableExpression(flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, result, errorMessage, numInstructionBytes, iterators);
}

#if EEZ_OPTION_GUI
int16_t getNativeVariableId(const WidgetCursor &widgetCursor) {
	if (widgetCursor.flowState) {
		FlowState *flowState = widgetCursor.flowState;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems[-(widgetCursor.widget->data + 1)];
		if (widgetDataItem && widgetDataItem->componentIndex != -1 && widgetDataItem->propertyValueIndex != -1) {
			auto component = flow->components[widgetDataItem->componentIndex];
			auto property = component->properties[widgetDataItem->propertyValueIndex];

            FlowState *savedFlowState = g_stack.flowState;
            int savedComponentIndex = g_stack.componentIndex;
            const int32_t *savedIterators = g_stack.iterators;
            const char *savedErrorMessage = g_stack.errorMessage;

			g_stack.flowState = flowState;
			g_stack.componentIndex = widgetDataItem->componentIndex;
			g_stack.iterators = widgetCursor.iterators;
            g_stack.errorMessage = nullptr;

			evalExpression(flowState, property->evalInstructions, nullptr);

            g_stack.flowState = savedFlowState;
            g_stack.componentIndex = savedComponentIndex;
            g_stack.iterators = savedIterators;
            g_stack.errorMessage = savedErrorMessage;

            if (g_stack.sp == 1) {
                auto finalResult = g_stack.pop();
                if (finalResult.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
                    return finalResult.getInt();
                }
            }
		}
	}

	return DATA_ID_NONE;
}
#endif

} // flow
} // eez
