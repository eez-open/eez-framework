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
#include <eez/core/alloc.h>

#include <eez/flow/private.h>
#include <eez/flow/operations.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

namespace eez {
namespace flow {

EvalStack g_stack;

// Removes what an evaluation left on the stack above sp (e.g. the operands of an
// operation that stopped at an error), so the next evaluation starts clean.
static void restoreStack(size_t sp) {
    while (g_stack.sp > sp) {
        g_stack.stack[--g_stack.sp] = Value();
    }
}

// baseSp: the stack pointer at the start of the evaluation
static void evalExpression(FlowState *flowState, const uint8_t *instructions, int *numInstructionBytes, size_t baseSp) {
	auto flowDefinition = static_cast<FlowDefinition*>(flowState->assets->flowDefinition);
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
                if (g_globalVariables && !flowState->assets->external) {
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
                            g_stack.setErrorMessage("Array element index out of bounds");
                        }
                    } else {
                        g_stack.push(Value::makeError());
                        g_stack.setErrorMessage("Integer value expected for array element index");
                    }
                } else if (arrayValue.isDynamicArray()) {
                    auto dynamicArray = arrayValue.getDynamicArray();

                    int err;
                    auto elementIndex = elementIndexValue.toInt32(&err);
                    if (!err) {
                        if (elementIndex >= 0 && elementIndex < (int)dynamicArray->arraySize) {
                            g_stack.push(Value::makeArrayElementRef(arrayValue, elementIndex, 0x132e0e2f));
                        } else {
                            g_stack.push(Value::makeError());
                            g_stack.setErrorMessage("Array element index out of bounds");
                        }
                    } else {
                        g_stack.push(Value::makeError());
                        g_stack.setErrorMessage("Integer value expected for array element index");
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
                            g_stack.setErrorMessage("Blob element index out of bounds");
                        }
                    } else {
                        g_stack.push(Value::makeError());
                        g_stack.setErrorMessage("Integer value expected for blob element index");
                    }

                } else {
                    g_stack.push(Value::makeError());
                    g_stack.setErrorMessage("Array value expected");
                }
            }
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_OPERATION) {
			g_evalOperations[instructionArg](g_stack);
		} else if ((instruction & EXPR_EVAL_INSTRUCTION_TYPE_JUMP_MASK) == EXPR_EVAL_INSTRUCTION_TYPE_JUMP && instruction <= EXPR_EVAL_INSTRUCTION_JUMP_OR) {
            // the operand is a forward offset in 16-bit words, counted from the
            // word after the operand
            int next = i + 4;
            int target = next + 2 * (instructions[i + 2] + (instructions[i + 3] << 8));

            if (instruction == EXPR_EVAL_INSTRUCTION_JUMP) {
                i = target;
            } else if (instruction == EXPR_EVAL_INSTRUCTION_JUMP_IF_FALSE) {
                // t ? c : a, the target is the JUMP over the alternate. The test is
                // popped and checked like the CONDITIONAL operation does it, an
                // error is the result.
                auto conditionValue = g_stack.sp > baseSp ? g_stack.pop() : Value::makeError();
                if (conditionValue.isError()) {
                    g_stack.push(conditionValue);
                    i = target;
                } else {
                    int err;
                    auto condition = conditionValue.toBool(&err);
                    if (err != 0) {
                        g_stack.push(Value::makeError());
                        i = target;
                    } else if (condition) {
                        i = next;
                    } else {
                        // the alternate, after the JUMP instruction and its operand
                        i = target + 4;
                    }
                }
            } else {
                // a && b, a || b: when the left operand decides the result, it is
                // replaced with the result and the right operand and the
                // LOGICAL_AND / LOGICAL_OR operation are skipped
                if (g_stack.sp <= baseSp) {
                    g_stack.push(Value::makeError());
                    i = target;
                } else {
                    Value &top = g_stack.stack[g_stack.sp - 1];
                    top = top.getValue();
                    bool isOr = instruction == EXPR_EVAL_INSTRUCTION_JUMP_OR;
                    if (top.isError()) {
                        i = target;
                    } else if (top.toBool() == isOr) {
                        top = Value(isOr, VALUE_TYPE_BOOLEAN);
                        i = target;
                    } else {
                        i = next;
                    }
                }
            }
            continue;
		} else {
            if (instruction == EXPR_EVAL_INSTRUCTION_TYPE_END_WITH_DST_VALUE_TYPE) {
    			i += 2;
                if (g_stack.sp == baseSp + 1) {
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

	evalExpression(flowState, instructions, numInstructionBytes, savedSp);

    // why the evaluation failed, nullptr if not known
    const char *evalErrorMessage = g_stack.errorMessage;

	g_stack.flowState = savedFlowState;
	g_stack.componentIndex = savedComponentIndex;
	g_stack.iterators = savedIterators;
    g_stack.errorMessage = savedErrorMessage;

    if (g_stack.sp == savedSp + 1) {
        result = g_stack.pop().getValue();
        if (!result.isError()) {
            return true;
        }
    }

    restoreStack(savedSp);

    FlowError flowError = errorMessage.setDescription(evalErrorMessage);
    throwError(flowState, componentIndex, flowError);
	return false;
}

bool evalAssignableExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const FlowError &errorMessage, int *numInstructionBytes, const int32_t *iterators) {
    size_t savedSp = g_stack.sp;
    FlowState *savedFlowState = g_stack.flowState;
	int savedComponentIndex = g_stack.componentIndex;
	const int32_t *savedIterators = g_stack.iterators;
    const char *savedErrorMessage = g_stack.errorMessage;

	g_stack.flowState = flowState;
	g_stack.componentIndex = componentIndex;
	g_stack.iterators = iterators;
    g_stack.errorMessage = nullptr;

	evalExpression(flowState, instructions, numInstructionBytes, savedSp);

    // why the evaluation failed, nullptr if not known
    const char *evalErrorMessage = g_stack.errorMessage;

	g_stack.flowState = savedFlowState;
	g_stack.componentIndex = savedComponentIndex;
	g_stack.iterators = savedIterators;
    g_stack.errorMessage = savedErrorMessage;

    if (g_stack.sp == savedSp + 1) {
        auto finalResult = g_stack.pop();
        if (
            finalResult.getType() == VALUE_TYPE_VALUE_PTR ||
            finalResult.getType() == VALUE_TYPE_NATIVE_VARIABLE ||
            finalResult.getType() == VALUE_TYPE_MODULE_FIELD ||
            finalResult.getType() == VALUE_TYPE_FLOW_OUTPUT ||
            finalResult.getType() == VALUE_TYPE_ARRAY_ELEMENT_VALUE ||
            finalResult.getType() == VALUE_TYPE_JSON_MEMBER_VALUE
        ) {
            result = finalResult;
            return true;
        }
    }

    restoreStack(savedSp);

    FlowError flowError = errorMessage.setDescription(evalErrorMessage);
    throwError(flowState, componentIndex, flowError);

	return false;
}

static uint16_t readListWord(const uint8_t *p) { return p[0] | (p[1] << 8); }

// Prevalidate the entire item before calling the ordinary evaluator. In
// particular, no operand or branch can read the next item or leave the span.
static bool validateListItem(FlowState *flowState, const uint8_t *instructions, size_t size) {
    if (size < 2 || size % 2) return false;
    const size_t words = size / 2;
    const size_t mapSize = (words + 7) / 8;
    uint8_t localBoundaries[16];
    auto boundaries = localBoundaries;
    struct FreeMap {
        uint8_t *allocated = nullptr;
        ~FreeMap() { if (allocated) free(allocated); }
    } freeMap;
    if (mapSize > sizeof(localBoundaries)) {
        freeMap.allocated = (uint8_t *)alloc(mapSize, 0x481c5331);
        if (!freeMap.allocated) return false;
        boundaries = freeMap.allocated;
    }
    memset(boundaries, 0, mapSize);
    auto isBoundary = [&](size_t offset) { return offset < size && (boundaries[(offset / 2) / 8] & (1 << ((offset / 2) % 8))); };
    for (size_t offset = 0; offset < size;) {
        boundaries[(offset / 2) / 8] |= 1 << ((offset / 2) % 8);
        const auto instruction = readListWord(instructions + offset);
        const auto type = instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK;
        const auto argument = instruction & EXPR_EVAL_INSTRUCTION_PARAM_MASK;
        if (type == EXPR_EVAL_INSTRUCTION_TYPE_OPERATION && argument >= g_numEvalOperations) return false;
        if (type == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT && argument >= flowState->assets->flowDefinition->constants.count) return false;
        if (type == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT && argument >= flowState->flow->componentInputs.count) return false;
        if (type == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR && argument >= flowState->flow->localVariables.count) return false;
        if (type == EXPR_EVAL_INSTRUCTION_TYPE_END) {
            if (instruction == EXPR_EVAL_INSTRUCTION_TYPE_END) {
                if (offset + 2 != size) return false;
                offset += 2;
            } else if (instruction == EXPR_EVAL_INSTRUCTION_TYPE_END_WITH_DST_VALUE_TYPE) {
                if (size - offset != 6) return false;
                offset += 6;
            } else if (instruction >= EXPR_EVAL_INSTRUCTION_JUMP && instruction <= EXPR_EVAL_INSTRUCTION_JUMP_OR) {
                if (size - offset < 4) return false;
                offset += 4;
            } else return false;
        } else {
            if (offset + 2 == size) return false; // Missing END.
            offset += 2;
        }
    }
    // Check destinations only after all instruction boundaries are known.
    for (size_t offset = 0; offset < size;) {
        const auto instruction = readListWord(instructions + offset);
        if (instruction >= EXPR_EVAL_INSTRUCTION_JUMP && instruction <= EXPR_EVAL_INSTRUCTION_JUMP_OR) {
            const size_t target = offset + 4 + 2U * readListWord(instructions + offset + 2);
            if (!isBoundary(target)) return false;
            if (instruction == EXPR_EVAL_INSTRUCTION_JUMP_IF_FALSE &&
                (readListWord(instructions + target) != EXPR_EVAL_INSTRUCTION_JUMP || !isBoundary(target + 4))) return false;
            offset += 4;
        } else if (instruction == EXPR_EVAL_INSTRUCTION_TYPE_END_WITH_DST_VALUE_TYPE) offset += 6;
        else offset += 2;
    }
    return true;
}

bool evalAssignableListExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, size_t instructionsSize, Value &result, const FlowError &errorMessage, int *numInstructionBytes, const int32_t *iterators) {
    result = Value();
    if (numInstructionBytes) *numInstructionBytes = 0;
    auto invalid = [&]() {
        throwError(flowState, componentIndex, errorMessage.setDescription("Invalid assignable list byte span"));
        return false;
    };
    if (!instructions || instructionsSize < 8 || readListWord(instructions) != EXPR_EVAL_ASSIGNABLE_LIST) return invalid();
    const size_t size = readListWord(instructions + 4) | ((uint32_t)readListWord(instructions + 6) << 16);
    if (size < 8 || size > instructionsSize || size > INT32_MAX || size % 2) return invalid();
    const unsigned count = readListWord(instructions + 2);
    size_t offset = 8;
    for (unsigned i = 0; i < count; ++i) {
        if (size - offset < 2) return invalid();
        const size_t bytes = 2U * readListWord(instructions + offset);
        offset += 2;
        if (bytes > size - offset || !validateListItem(flowState, instructions + offset, bytes)) return invalid();
        offset += bytes;
    }
    if (offset != size) return invalid();
    auto array = Value::makeArrayRef(count, defs_v3::ARRAY_TYPE_ANY, 0x733d341a);
    if (!array.isArray()) {
        throwError(flowState, componentIndex, errorMessage.setDescription("Assignable list allocation failed"));
        return false;
    }
    offset = 8;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned words = readListWord(instructions + offset);
        offset += 2;
        int consumed = 0;
        if (!evalAssignableExpression(flowState, componentIndex,
                instructions + offset, array.getArray()->values[i], errorMessage, &consumed, iterators)) {
            return false;
        }
        if ((unsigned)consumed != words * 2) {
            throwError(flowState, componentIndex, errorMessage.setDescription("Invalid assignable list item size"));
            return false;
        }
        offset += consumed;
    }
    if (numInstructionBytes) *numInstructionBytes = offset;
    result = array;
    return true;
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
    const auto *instructions = component->properties[propertyIndex]->evalInstructions;
    const auto available = getAssetsRemainingSize(flowState->assets, instructions);
    // Alternate loaders may not supply a size for ordinary Flow assets.
    // A reference-list container always needs a known enclosing span.
    if (available < 2 && (flowState->assets == g_mainAssets || getAssetsDataSize(flowState->assets) != 0)) {
        throwError(flowState, componentIndex, errorMessage.setDescription("Property is outside loaded assets"));
        return false;
    }
    if (readListWord(instructions) == EXPR_EVAL_ASSIGNABLE_LIST) {
        return evalAssignableListExpression(flowState, componentIndex, instructions, available, result, errorMessage, numInstructionBytes, iterators);
    }
#if EEZ_OPTION_GUI
    return evalExpression(flowState, componentIndex, instructions, result, errorMessage, numInstructionBytes, iterators, operation);
#else
    return evalExpression(flowState, componentIndex, instructions, result, errorMessage, numInstructionBytes, iterators);
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

            size_t savedSp = g_stack.sp;
            FlowState *savedFlowState = g_stack.flowState;
            int savedComponentIndex = g_stack.componentIndex;
            const int32_t *savedIterators = g_stack.iterators;
            const char *savedErrorMessage = g_stack.errorMessage;

			g_stack.flowState = flowState;
			g_stack.componentIndex = widgetDataItem->componentIndex;
			g_stack.iterators = widgetCursor.iterators;
            g_stack.errorMessage = nullptr;

			evalExpression(flowState, property->evalInstructions, nullptr, savedSp);

            g_stack.flowState = savedFlowState;
            g_stack.componentIndex = savedComponentIndex;
            g_stack.iterators = savedIterators;
            g_stack.errorMessage = savedErrorMessage;

            int16_t nativeVariableId = EEZ_DATA_ID_NONE;
            if (g_stack.sp == savedSp + 1) {
                auto finalResult = g_stack.pop();
                if (finalResult.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
                    nativeVariableId = finalResult.getInt();
                }
            }
            restoreStack(savedSp);
            return nativeVariableId;
		}
	}

	return EEZ_DATA_ID_NONE;
}
#endif

} // flow
} // eez
