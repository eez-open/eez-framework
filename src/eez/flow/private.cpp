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

#include <eez/core/debug.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

#include <eez/flow/flow.h>
#include <eez/flow/operations.h>
#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/hooks.h>
#include <eez/flow/watch_list.h>
#include <eez/flow/components/call_action.h>
#include <eez/flow/components/on_event.h>

#if defined(EEZ_DASHBOARD_API)
#include <eez/flow/dashboard_api.h>
#endif

namespace eez {
namespace flow {

GlobalVariables *g_globalVariables = nullptr;

static const unsigned NO_COMPONENT_INDEX = 0xFFFFFFFF;

static bool g_enableThrowError = true;

inline bool isInputEmpty(const Value& inputValue) {
    return inputValue.type == VALUE_TYPE_UNDEFINED && inputValue.int32Value > 0;
}

inline Value getEmptyInputValue() {
    Value emptyInputValue;
    emptyInputValue.int32Value = 1;
    return emptyInputValue;
}

void initGlobalVariables(Assets *assets) {
    if (!g_mainAssetsUncompressed) {
        return;
    }

	auto flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);

    auto numVars = flowDefinition->globalVariables.count;

    g_globalVariables = (GlobalVariables *) alloc(
        sizeof(GlobalVariables) +
        (numVars > 0 ? numVars - 1 : 0) * sizeof(Value),
        0xcc34ca8e
    );

    for (uint32_t i = 0; i < numVars; i++) {
		new (g_globalVariables->values + i) Value();
        g_globalVariables->values[i] = flowDefinition->globalVariables[i]->clone();
	}
}

static bool isComponentReadyToRun(FlowState *flowState, unsigned componentIndex) {
	auto component = flowState->flow->components[componentIndex];

	if (component->type == defs_v3::COMPONENT_TYPE_CATCH_ERROR_ACTION) {
		return false;
	}

    if (component->type == defs_v3::COMPONENT_TYPE_ON_EVENT_ACTION) {
        return false;
    }

    if (component->type == defs_v3::COMPONENT_TYPE_LABEL_IN_ACTION) {
        return false;
    }

    if (component->type > defs_v3::FIRST_LVGL_WIDGET_COMPONENT_TYPE) {
        return false;
    }

    if ((component->type < defs_v3::COMPONENT_TYPE_START_ACTION && component->type != defs_v3::COMPONENT_TYPE_USER_WIDGET_WIDGET) || component->type >= defs_v3::FIRST_DASHBOARD_WIDGET_COMPONENT_TYPE) {
        // always execute widget
        return true;
    }

    if (component->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
        if (flowState->parentComponent && flowState->parentComponentIndex != -1) {
            auto flowInputIndex = flowState->parentComponent->inputs[0];
            auto value = flowState->parentFlowState->values[flowInputIndex];
            return value.getType() != VALUE_TYPE_UNDEFINED;
        } else {
            return true;
        }
    }

	// check if required inputs are defined:
	//   - at least 1 seq input must be defined
	//   - all non optional data inputs must be defined
	int numSeqInputs = 0;
	int numDefinedSeqInputs = 0;
	for (unsigned inputIndex = 0; inputIndex < component->inputs.count; inputIndex++) {
		auto inputValueIndex = component->inputs[inputIndex];

		auto input = flowState->flow->componentInputs[inputValueIndex];

		if (input & COMPONENT_INPUT_FLAG_IS_SEQ_INPUT) {
			numSeqInputs++;
			auto &value = flowState->values[inputValueIndex];
			if (!isInputEmpty(value)) {
				numDefinedSeqInputs++;
			}
		} else {
			if (!(input & COMPONENT_INPUT_FLAG_IS_OPTIONAL)) {
				auto &value = flowState->values[inputValueIndex];
				if (isInputEmpty(value)) {
					// non optional data input is undefined
					return false;
				}
			}
		}
	}

	if (numSeqInputs && !numDefinedSeqInputs) {
		// no seq input is defined
		return false;
	}

	return true;
}

static bool pingComponent(FlowState *flowState, unsigned componentIndex, int sourceComponentIndex = -1, int sourceOutputIndex = -1, int targetInputIndex = -1) {
	if (isComponentReadyToRun(flowState, componentIndex)) {
		return addToQueue(flowState, componentIndex, sourceComponentIndex, sourceOutputIndex, targetInputIndex, false);
	}
	return false;
}


static FlowState *initFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState, int parentComponentIndex, const Value& inputValue) {
	auto flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);
	auto flow = flowDefinition->flows[flowIndex];

	auto nValues = flow->componentInputs.count + flow->localVariables.count;

	FlowState *flowState = new (
		alloc(
			sizeof(FlowState) +
			nValues * sizeof(Value) +
			flow->components.count * sizeof(ComponenentExecutionState *) +
			flow->components.count * sizeof(bool),
			0x4c3b6ef5
		)
	) FlowState;

	flowState->flowStateIndex = (int)((uint8_t *)flowState - ALLOC_BUFFER);
	flowState->assets = assets;
	flowState->flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);
	flowState->flow = flowDefinition->flows[flowIndex];
	flowState->flowIndex = flowIndex;
	flowState->error = false;
    flowState->deleteOnNextTick = false;
	flowState->refCounter = 0;
	flowState->parentFlowState = parentFlowState;

    flowState->executingComponentIndex = NO_COMPONENT_INDEX;

    flowState->timelinePosition = 0;

#if defined(EEZ_FOR_LVGL)
    flowState->lvglWidgetStartIndex = 0;
#endif

    if (parentFlowState) {
        if (parentFlowState->lastChild) {
            parentFlowState->lastChild->nextSibling = flowState;
            flowState->previousSibling = parentFlowState->lastChild;
            parentFlowState->lastChild = flowState;
        } else {
            flowState->previousSibling = nullptr;
            parentFlowState->firstChild = flowState;
            parentFlowState->lastChild = flowState;
        }

		flowState->parentComponentIndex = parentComponentIndex;
		flowState->parentComponent = parentComponentIndex == -1 ? nullptr : parentFlowState->flow->components[parentComponentIndex];
	} else {
        if (g_lastFlowState) {
            g_lastFlowState->nextSibling = flowState;
            flowState->previousSibling = g_lastFlowState;
            g_lastFlowState = flowState;
        } else {
            flowState->previousSibling = nullptr;
            g_firstFlowState = flowState;
            g_lastFlowState = flowState;
        }

		flowState->parentComponentIndex = -1;
		flowState->parentComponent = nullptr;
	}

    flowState->inputValue = inputValue;

    flowState->firstChild = nullptr;
    flowState->lastChild = nullptr;
    flowState->nextSibling = nullptr;

	flowState->values = (Value *)(flowState + 1);
	flowState->componenentExecutionStates = (ComponenentExecutionState **)(flowState->values + nValues);
    flowState->componenentAsyncStates = (bool *)(flowState->componenentExecutionStates + flow->components.count);

	for (unsigned i = 0; i < nValues; i++) {
		new (flowState->values + i) Value();
	}

    // empty input is VALUE_TYPE_UNDEFINED, but with int value greater than zero, so
    // we can differentiate it from undefined value
	Value emptyInputValue = getEmptyInputValue();
	for (unsigned i = 0; i < flow->componentInputs.count; i++) {
		flowState->values[i] = emptyInputValue;
	}

	for (unsigned i = 0; i < flow->localVariables.count; i++) {
		auto value = flow->localVariables[i];
		flowState->values[flow->componentInputs.count + i] = *value;
	}

	for (unsigned i = 0; i < flow->components.count; i++) {
		flowState->componenentExecutionStates[i] = nullptr;
		flowState->componenentAsyncStates[i] = false;
	}

	onFlowStateCreated(flowState);

	for (unsigned componentIndex = 0; componentIndex < flow->components.count; componentIndex++) {
		pingComponent(flowState, componentIndex);
	}

	return flowState;
}

FlowState *initActionFlowState(int flowIndex, FlowState *parentFlowState, int parentComponentIndex, const Value &inputValue) {
	auto flowState = initFlowState(parentFlowState->assets, flowIndex, parentFlowState, parentComponentIndex, inputValue);
	if (flowState) {
		flowState->isAction = true;
	}
	return flowState;
}

FlowState *initPageFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState, int parentComponentIndex) {
	auto flowState = initFlowState(assets, flowIndex, parentFlowState, parentComponentIndex, Value());
	if (flowState) {
		flowState->isAction = false;
	}
	return flowState;
}

void incRefCounterForFlowState(FlowState *flowState) {
    flowState->refCounter++;
    for (auto parent = flowState->parentFlowState; parent; parent = parent->parentFlowState) {
        parent->refCounter++;
    }
}

void decRefCounterForFlowState(FlowState *flowState) {
    flowState->refCounter--;
    for (auto parent = flowState->parentFlowState; parent; parent = parent->parentFlowState) {
        parent->refCounter--;
    }
}

bool canFreeFlowState(FlowState *flowState) {
    if (!flowState->isAction) {
        return false;
    }

    if (flowState->refCounter > 0) {
        return false;
    }

    return true;
}

void freeFlowState(FlowState *flowState) {
    auto parentFlowState = flowState->parentFlowState;
    if (parentFlowState) {
        if (flowState->parentComponentIndex != -1) {
            auto componentExecutionState = parentFlowState->componenentExecutionStates[flowState->parentComponentIndex];
            if (componentExecutionState) {
                deallocateComponentExecutionState(parentFlowState, flowState->parentComponentIndex);
                return;
            }
        }

        if (parentFlowState->firstChild == flowState) {
            parentFlowState->firstChild = flowState->nextSibling;
        }
        if (parentFlowState->lastChild == flowState) {
            parentFlowState->lastChild = flowState->previousSibling;
        }
    } else {
        if (g_firstFlowState == flowState) {
            g_firstFlowState = flowState->nextSibling;
        }
        if (g_lastFlowState == flowState) {
            g_lastFlowState = flowState->previousSibling;
        }
    }

    if (flowState->previousSibling) {
        flowState->previousSibling->nextSibling = flowState->nextSibling;
    }
    if (flowState->nextSibling) {
        flowState->nextSibling->previousSibling = flowState->previousSibling;
    }

	auto flow = flowState->flow;

	auto valuesCount = flow->componentInputs.count + flow->localVariables.count;

	for (unsigned int i = 0; i < valuesCount; i++) {
		(flowState->values + i)->~Value();
	}

	for (unsigned i = 0; i < flow->components.count; i++) {
        deallocateComponentExecutionState(flowState, i);
	}

    removeTasksFromQueueForFlowState(flowState);
    removeWatchesForFlowState(flowState);

    freeAllChildrenFlowStates(flowState->firstChild);

	onFlowStateDestroyed(flowState);

	flowState->~FlowState();
	free(flowState);
}

void freeAllChildrenFlowStates(FlowState *firstChildFlowState) {
    auto flowState = firstChildFlowState;
    while (flowState != nullptr) {
        auto nextFlowState = flowState->nextSibling;
        freeAllChildrenFlowStates(flowState->firstChild);
        freeFlowState(flowState);
        flowState = nextFlowState;
    }
}

void deallocateComponentExecutionState(FlowState *flowState, unsigned componentIndex) {
    auto executionState = flowState->componenentExecutionStates[componentIndex];
    if (executionState) {
        auto component = flowState->flow->components[componentIndex];
        if (TRACK_REF_COUNTER_FOR_COMPONENT_STATE(component)) {
            decRefCounterForFlowState(flowState);
        }

        flowState->componenentExecutionStates[componentIndex] = nullptr;
        onComponentExecutionStateChanged(flowState, componentIndex);
        ObjectAllocator<ComponenentExecutionState>::deallocate(executionState);
    }
}

void resetSequenceInputs(FlowState *flowState) {
    if (flowState->executingComponentIndex != NO_COMPONENT_INDEX) {
		auto component = flowState->flow->components[flowState->executingComponentIndex];
        flowState->executingComponentIndex = NO_COMPONENT_INDEX;

        if (component->type != defs_v3::COMPONENT_TYPE_OUTPUT_ACTION) {
            for (uint32_t i = 0; i < component->inputs.count; i++) {
                auto inputIndex = component->inputs[i];
                if (flowState->flow->componentInputs[inputIndex] & COMPONENT_INPUT_FLAG_IS_SEQ_INPUT) {
                    auto pValue = &flowState->values[inputIndex];
                    if (!isInputEmpty(*pValue)) {
                        *pValue = getEmptyInputValue();
                        onValueChanged(pValue);
                    }
                }
            }
        }
    }
}

void propagateValue(FlowState *flowState, unsigned componentIndex, unsigned outputIndex, const Value &value) {
    if ((int)componentIndex == -1) {
        // call action flow directly
        auto flowIndex = outputIndex;
        executeCallAction(flowState, -1, flowIndex, value);
        return;
    }

    // Reset sequence inputs before propagate value, in case component propagates value to itself
    resetSequenceInputs(flowState);

	auto component = flowState->flow->components[componentIndex];
	auto componentOutput = component->outputs[outputIndex];

    auto value2 = value.getValue();

	for (unsigned connectionIndex = 0; connectionIndex < componentOutput->connections.count; connectionIndex++) {
		auto connection = componentOutput->connections[connectionIndex];

		auto pValue = &flowState->values[connection->targetInputIndex];

		if (*pValue != value2) {
			*pValue = value2;

			//if (!(flowState->flow->componentInputs[connection->targetInputIndex] & COMPONENT_INPUT_FLAG_IS_SEQ_INPUT)) {
				onValueChanged(pValue);
			//}
		}

		pingComponent(flowState, connection->targetComponentIndex, componentIndex, outputIndex, connection->targetInputIndex);
	}
}

void propagateValue(FlowState *flowState, unsigned componentIndex, unsigned outputIndex) {
	auto &nullValue = *flowState->flowDefinition->constants[NULL_VALUE_INDEX];
	propagateValue(flowState, componentIndex, outputIndex, nullValue);
}

void propagateValueThroughSeqout(FlowState *flowState, unsigned componentIndex) {
	// find @seqout output
	// TODO optimization hint: always place @seqout at 0-th index
	auto component = flowState->flow->components[componentIndex];
	for (uint32_t i = 0; i < component->outputs.count; i++) {
		if (component->outputs[i]->isSeqOut) {
			propagateValue(flowState, componentIndex, i);
			return;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

#if EEZ_OPTION_GUI
void getValue(uint16_t dataId, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (!isFlowStopped()) {
		FlowState *flowState = widgetCursor.flowState;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems[dataId];
		if (widgetDataItem && widgetDataItem->componentIndex != -1 && widgetDataItem->propertyValueIndex != -1) {
			evalProperty(flowState, widgetDataItem->componentIndex, widgetDataItem->propertyValueIndex, value, FlowError::Plain("doGetFlowValue failed"), nullptr, widgetCursor.iterators, operation);
		}
	}
}

void setValue(uint16_t dataId, const WidgetCursor &widgetCursor, const Value& value) {
	if (!isFlowStopped()) {
		FlowState *flowState = widgetCursor.flowState;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems[dataId];
		if (widgetDataItem && widgetDataItem->componentIndex != -1 && widgetDataItem->propertyValueIndex != -1) {
			auto component = flow->components[widgetDataItem->componentIndex];
			auto property = component->properties[widgetDataItem->propertyValueIndex];
			Value dstValue;
			if (evalAssignableExpression(flowState, widgetDataItem->componentIndex, property->evalInstructions, dstValue, FlowError::Plain("doSetFlowValue failed"), nullptr, widgetCursor.iterators)) {
				assignValue(flowState, widgetDataItem->componentIndex, dstValue, value);
			}
		}
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////

void assignValue(FlowState *flowState, int componentIndex, Value &dstValue, const Value &srcValue) {
	if (dstValue.getType() == VALUE_TYPE_FLOW_OUTPUT) {
		propagateValue(flowState, componentIndex, dstValue.getUInt16(), srcValue);
	} else if (dstValue.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
#if EEZ_OPTION_GUI
		set(g_widgetCursor, dstValue.getInt(), srcValue);
#else
		setVar(dstValue.getInt(), srcValue);
#endif
	} else {
		Value *pDstValue;
        uint32_t dstValueType = VALUE_TYPE_UNDEFINED;

        if (dstValue.getType() == VALUE_TYPE_ARRAY_ELEMENT_VALUE) {
            auto arrayElementValue = (ArrayElementValue *)dstValue.refValue;
            if (arrayElementValue->arrayValue.isBlob()) {
                auto blobRef = arrayElementValue->arrayValue.getBlob();
                if (arrayElementValue->elementIndex < 0 || arrayElementValue->elementIndex >= (int)blobRef->len) {
                    throwError(flowState, componentIndex, FlowError::Plain("Can not assign, blob element index out of bounds"));
                    return;
                }

                int err;
                int32_t elementValue = srcValue.toInt32(&err);
                if (err != 0) {
                    throwError(flowState, componentIndex, FlowError::Plain("Can not non-integer to blob"));
                } else if (elementValue < 0 || elementValue > 255) {
                    char errorMessage[100];
                    snprintf(errorMessage, sizeof(errorMessage), "Can not assign %d to blob", (int)elementValue);
                    throwError(flowState, componentIndex, FlowError::Plain(errorMessage));
                } else {
                    blobRef->blob[arrayElementValue->elementIndex] = elementValue;
                    // TODO: onValueChanged
                }
                return;
            } else {
                auto array = arrayElementValue->arrayValue.getArray();
                if (arrayElementValue->elementIndex < 0 || arrayElementValue->elementIndex >= (int)array->arraySize) {
                    throwError(flowState, componentIndex, FlowError::Plain("Can not assign, array element index out of bounds"));
                    return;
                }
                pDstValue = &array->values[arrayElementValue->elementIndex];
                // TODO
                //dstValueType = arrayElementValue->dstValueType;
            }
        }
#if defined(EEZ_DASHBOARD_API)
        else if (dstValue.getType() == VALUE_TYPE_JSON_MEMBER_VALUE) {
            auto jsonMemberValue = (JsonMemberValue *)dstValue.refValue;
            int err = operationJsonSet(jsonMemberValue->jsonValue.getInt(), jsonMemberValue->propertyName.getString(), &srcValue);
            if (err) {
                throwError(flowState, componentIndex, FlowError::Plain("Can not assign to JSON member"));
            }
            return;
        }
#endif
        else {
            pDstValue = dstValue.pValueValue;
            dstValueType = dstValue.dstValueType;
        }

        while (true) {
            if (pDstValue->type == VALUE_TYPE_ARRAY_ELEMENT_VALUE) {
                assignValue(flowState, componentIndex, *pDstValue, srcValue);
                return;
            }

#if defined(EEZ_DASHBOARD_API)
            if (pDstValue->type == VALUE_TYPE_JSON_MEMBER_VALUE) {
                assignValue(flowState, componentIndex, *pDstValue, srcValue);
                return;
            }
#endif

            if (pDstValue->type == VALUE_TYPE_PROPERTY_REF) {
                auto propertyRef = pDstValue->getPropertyRef();
                Value dstValue;
                if (evalAssignableProperty(propertyRef->flowState, propertyRef->componentIndex, propertyRef->propertyIndex, dstValue, FlowError::Plain("Failed to evaluate an assignable user property in UserWidget"), nullptr, nullptr)) {
                    if (dstValue.getType() == VALUE_TYPE_FLOW_OUTPUT) {
		                propagateValue(propertyRef->flowState, propertyRef->componentIndex, dstValue.getUInt16(), srcValue);
                    } else {
	                    assignValue(flowState, componentIndex, dstValue, srcValue);
                        onValueChanged(pDstValue);
                    }
                }
                return;
            }

            if (pDstValue->type == VALUE_TYPE_VALUE_PTR) {
                onValueChanged(pDstValue);
                pDstValue = pDstValue->pValueValue;
            } else {
                break;
            }
        }

        if (assignValue(*pDstValue, srcValue, dstValueType)) {
            onValueChanged(pDstValue);
        } else {
            char errorMessage[100];
            snprintf(errorMessage, sizeof(errorMessage), "Can not assign %s to %s\n",
                g_valueTypeNames[pDstValue->type](srcValue), g_valueTypeNames[srcValue.type](*pDstValue)
            );
            throwError(flowState, componentIndex, FlowError::Plain(errorMessage));
        }
	}
}

////////////////////////////////////////////////////////////////////////////////

void clearInputValue(FlowState *flowState, int inputIndex) {
    flowState->values[inputIndex] = Value();
    onValueChanged(flowState->values + inputIndex);
}

////////////////////////////////////////////////////////////////////////////////

void startAsyncExecution(FlowState *flowState, int componentIndex) {
    if (!flowState->componenentAsyncStates[componentIndex]) {
        flowState->componenentAsyncStates[componentIndex] = true;
        onComponentAsyncStateChanged(flowState, componentIndex);

	    incRefCounterForFlowState(flowState);
    }
}

void endAsyncExecution(FlowState *flowState, int componentIndex) {
    if (!g_firstFlowState) {
        // flow engine is stopped
        return;
    }

    if (flowState->componenentAsyncStates[componentIndex]) {
        flowState->componenentAsyncStates[componentIndex] = false;
        onComponentAsyncStateChanged(flowState, componentIndex);

        decRefCounterForFlowState(flowState);
        do {
            if (!canFreeFlowState(flowState)) {
                break;
            }
            auto temp = flowState->parentFlowState;
            freeFlowState(flowState);
            flowState = temp;
        } while (flowState);
    }
}

void onEvent(FlowState *flowState, FlowEvent flowEvent, Value eventValue) {
	for (unsigned componentIndex = 0; componentIndex < flowState->flow->components.count; componentIndex++) {
		auto component = flowState->flow->components[componentIndex];
		if (component->type == defs_v3::COMPONENT_TYPE_ON_EVENT_ACTION) {
            auto onEventComponent = (OnEventComponent *)component;
            if (onEventComponent->event == flowEvent) {
                flowState->eventValue = eventValue;

                if (!isInQueue(flowState, componentIndex)) {
                    if (!addToQueue(flowState, componentIndex, -1, -1, -1, false)) {
                        return;
                    }
                }
            }
		}
	}

    if (flowEvent == FLOW_EVENT_KEYDOWN) {
        for (auto childFlowState = flowState->firstChild; childFlowState != nullptr; childFlowState = childFlowState->nextSibling) {
            onEvent(childFlowState, flowEvent, eventValue);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

static bool findCatchErrorComponent(FlowState *flowState, FlowState *&catchErrorFlowState, int &catchErrorComponentIndex) {
    if (!flowState) {
        return false;
    }

	for (unsigned componentIndex = 0; componentIndex < flowState->flow->components.count; componentIndex++) {
		auto component = flowState->flow->components[componentIndex];
		if (component->type == defs_v3::COMPONENT_TYPE_CATCH_ERROR_ACTION) {
			catchErrorFlowState = flowState;
			catchErrorComponentIndex = componentIndex;
			return true;
		}
	}

    if (flowState->parentFlowState && flowState->parentComponent && flowState->parentComponent->errorCatchOutput != -1) {
        catchErrorFlowState = flowState->parentFlowState;
        catchErrorComponentIndex = flowState->parentComponentIndex;
        return true;
    }

    return findCatchErrorComponent(flowState->parentFlowState, catchErrorFlowState, catchErrorComponentIndex);
}

void throwError(FlowState *flowState, int componentIndex, const char *errorMessage) {
    auto component = flowState->flow->components[componentIndex];

    if (!g_enableThrowError) {
        return;
    }

#if defined(EEZ_FOR_LVGL)
    LV_LOG_ERROR("EEZ-FLOW error: %s", errorMessage);
#elif defined(__EMSCRIPTEN__)
    printf("throwError: %s\n", errorMessage);
#endif

	if (component->errorCatchOutput != -1) {
		propagateValue(
			flowState,
			componentIndex,
			component->errorCatchOutput,
			Value::makeStringRef(errorMessage, strlen(errorMessage), 0xef6f8414)
		);
	} else {
		FlowState *catchErrorFlowState;
		int catchErrorComponentIndex;
		if (
            findCatchErrorComponent(
                component->type == defs_v3::COMPONENT_TYPE_ERROR_ACTION ? flowState->parentFlowState : flowState,
                catchErrorFlowState,
                catchErrorComponentIndex
            )
        ) {
            for (FlowState *fs = flowState; fs != catchErrorFlowState; fs = fs->parentFlowState) {
                if (fs->isAction) {
                    fs->error = true;
                }
            }

            auto component = catchErrorFlowState->flow->components[catchErrorComponentIndex];

            if (component->type == defs_v3::COMPONENT_TYPE_CATCH_ERROR_ACTION) {
                auto catchErrorComponentExecutionState = allocateComponentExecutionState<CatchErrorComponenentExecutionState>(catchErrorFlowState, catchErrorComponentIndex);
                catchErrorComponentExecutionState->message = Value::makeStringRef(errorMessage, strlen(errorMessage), 0x9473eef2);

                if (!addToQueue(catchErrorFlowState, catchErrorComponentIndex, -1, -1, -1, false)) {
                    onFlowError(flowState, componentIndex, errorMessage);
                    stopScriptHook();
                }
            } else {
                propagateValue(
                    catchErrorFlowState,
                    catchErrorComponentIndex,
                    component->errorCatchOutput,
                    Value::makeStringRef(errorMessage, strlen(errorMessage), 0x9473eef3)
                );
            }
		} else {
			onFlowError(flowState, componentIndex, errorMessage);
			stopScriptHook();
		}
	}
}

const char *FlowError::getMessage(char *messageStr, size_t messageStrLength, int flowIndex, int componentIndex) const {
    #define GET_MESSAGE(FMT, ...) \
        if (file) snprintf(messageStr, messageStrLength, FMT " | %d.%d | %s:%d", __VA_ARGS__, flowIndex, componentIndex, file, line); \
        else snprintf(messageStr, messageStrLength, FMT " | %d.%d", __VA_ARGS__, flowIndex, componentIndex);

    if (type == FLOW_ERROR_PLAIN) {
        if (description) {
            GET_MESSAGE("%s: %s", messagePart1, description);
        } else {
            GET_MESSAGE("%s", messagePart1);
        }
    } else if (type == FLOW_ERROR_PROPERTY) {
        if (!description) {
            GET_MESSAGE("Failed to evaluate '%s' in '%s'", messagePart2, messagePart1);
        } else {
            GET_MESSAGE("Failed to evaluate '%s' in '%s': %s", messagePart2, messagePart1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_INVALID) {
        if (!description) {
            GET_MESSAGE("Invalid '%s' value in '%s'", messagePart2, messagePart1);
        } else {
            GET_MESSAGE("Invalid '%s' value in '%s': %s", messagePart2, messagePart1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_CONVERT) {
        if (!description) {
            GET_MESSAGE("Failed to convert '%s' to '%s' in '%s'", messagePart2, messagePart3, messagePart1);
        } else {
            GET_MESSAGE("Failed to convert '%s' to '%s' in '%s': %s", messagePart2, messagePart3, messagePart1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_IN_ARRAY) {
        if (!description) {
            GET_MESSAGE("Failed to evaluate '%s' no. %d in '%s'", messagePart2, messagePartInt + 1, messagePart1);
        } else {
            GET_MESSAGE("Failed to evaluate '%s' no. %d in '%s': %s", messagePart2, messagePartInt + 1, messagePart1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_IN_ARRAY_CONVERT) {
        if (!description) {
            GET_MESSAGE("Failed to convert '%s' no. %d to '%s' in '%s'", messagePart2, messagePartInt + 1, messagePart3, messagePart1);
        } else {
            GET_MESSAGE("Failed to evaluate '%s' no. %d to '%s' in '%s': %s", messagePart2, messagePartInt + 1, messagePart3, messagePart1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_NUM) {
        if (!description) {
            GET_MESSAGE("Failed to evaluate property #%d in '%s'", messagePartInt + 1, messagePart1);
        } else {
            GET_MESSAGE("Failed to evaluate property #%d in '%s': %s", messagePartInt + 1, messagePart1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_IN_ACTION) {
        if (!description) {
            GET_MESSAGE("Failed to evaluate '%s' in '%s' action #%d", messagePart2, messagePart1, messagePartInt + 1);
        } else {
            GET_MESSAGE("Failed to evaluate '%s' in '%s' action #%d: %s", messagePart2, messagePart1, messagePartInt + 1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_ASSIGN_IN_ACTION) {
        if (!description) {
            GET_MESSAGE("Failed to assign '%s' in '%s' action #%d", messagePart2, messagePart1, messagePartInt + 1);
        } else {
            GET_MESSAGE("Failed to assign '%s' in '%s' action #%d: %s", messagePart2, messagePart1, messagePartInt + 1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_IN_ACTION_CONVERT) {
        if (!description) {
            GET_MESSAGE("Failed to convert '%s' to '%s' in '%s' action #%d", messagePart2, messagePart3, messagePart1, messagePartInt + 1);
        } else {
            GET_MESSAGE("Failed to convert '%s' to '%s' in '%s' action #%d: %s", messagePart2, messagePart3, messagePart1, messagePartInt + 1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_NOT_FOUND_IN_ACTION) {
        if (!description) {
            GET_MESSAGE("%s '%s' not found in '%s' action #%d", messagePart1, messagePart2, messagePart3, messagePartInt + 1);
        } else {
            GET_MESSAGE("%s '%s' not found in '%s' action #%d: %s", messagePart1, messagePart2, messagePart3, messagePartInt + 1, description);
        }
    } else if (type == FLOW_ERROR_PROPERTY_IS_NULL_IN_ACTION) {
        if (!description) {
            GET_MESSAGE("%s is NULL in '%s' action #%d", messagePart1, messagePart2, messagePartInt + 1);
        } else {
            GET_MESSAGE("%s is NULL in '%s' action #%d: %s", messagePart1, messagePart2, messagePartInt + 1, description);
        }
    } else if (type == FLOW_ERROR_USER_PROPERTY) {
        if (!description) {
            GET_MESSAGE("Failed to evaluate property #%d in '%s'", messagePartInt + 1, messagePart1);
        } else {
            GET_MESSAGE("Failed to evaluate property #%d in '%s': %s", messagePartInt + 1, messagePart1, description);
        }
    } else if (type == FLOW_ERROR_USER_ASSIGNABLE_PROPERTY) {
        if (!description) {
            GET_MESSAGE("Failed to evaluate assignable property #%d in '%s'", messagePartInt + 1, messagePart1);
        } else {
            GET_MESSAGE("Failed to evaluate assignable property #%d in '%s': %s", messagePartInt + 1, messagePart1, description);
        }
    } else {
        GET_MESSAGE("%s", "UNKNOWN ERROR");
    }

    return messageStr;
}


void throwError(FlowState *flowState, int componentIndex, const FlowError &error) {
    char errorMessageStr[512];
    const char *errorMessage = error.getMessage(errorMessageStr, sizeof(errorMessageStr), flowState->flowIndex, componentIndex);
    throwError(flowState, componentIndex, errorMessage);
}

void enableThrowError(bool enable) {
    g_enableThrowError = enable;
}

} // namespace flow
} // namespace eez
