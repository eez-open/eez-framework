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

#include <stdio.h>

#include <eez/core/debug.h>

#include <eez/gui/gui.h>

#include <eez/flow/flow.h>
#include <eez/flow/operations.h>
#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/hooks.h>

using namespace eez::gui;

namespace eez {
namespace flow {

uint32_t g_lastFlowStateIndex = 0;

bool isComponentReadyToRun(FlowState *flowState, unsigned componentIndex) {
	auto component = flowState->flow->components[componentIndex];

	if (component->type == defs_v3::COMPONENT_TYPE_CATCH_ERROR_ACTION) {
		return false;
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
			if (value.type != VALUE_TYPE_UNDEFINED) {
				numDefinedSeqInputs++;
			}
		} else {
			if (!(input & COMPONENT_INPUT_FLAG_IS_OPTIONAL)) {
				auto &value = flowState->values[inputValueIndex];
				if (value.type == VALUE_TYPE_UNDEFINED) {
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
		if (!addToQueue(flowState, componentIndex, sourceComponentIndex, sourceOutputIndex, targetInputIndex)) {
			throwError(flowState, componentIndex, "Execution queue is full\n");
			return false;
		}
		return true;
	}
	return false;
}


static FlowState *initFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState, int parentComponentIndex) {
	auto flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);
	auto flow = flowDefinition->flows[flowIndex];

	auto nValues = flow->componentInputs.count + flow->localVariables.count;

	FlowState *flowState = (FlowState *)alloc(
		sizeof(FlowState) + nValues * sizeof(Value) + flow->components.count * sizeof(ComponenentExecutionState *),
		0x4c3b6ef5
	);

	flowState->flowStateIndex = ++g_lastFlowStateIndex;
	flowState->assets = assets;
	flowState->flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);
	flowState->flow = flowDefinition->flows[flowIndex];
	flowState->flowIndex = flowIndex;
	flowState->error = false;
	flowState->numActiveComponents = 0;
	flowState->parentFlowState = parentFlowState;
	if (parentFlowState) {
		flowState->parentComponentIndex = parentComponentIndex;
		flowState->parentComponent = parentFlowState->flow->components[parentComponentIndex];
	} else {
		flowState->parentComponentIndex = -1;
		flowState->parentComponent = nullptr;
	}
	flowState->values = (Value *)(flowState + 1);
	flowState->componenentExecutionStates = (ComponenentExecutionState **)(flowState->values + nValues);

	for (unsigned i = 0; i < nValues; i++) {
		new (flowState->values + i) Value();
	}

	auto &undefinedValue = *flowDefinition->constants[UNDEFINED_VALUE_INDEX];
	for (unsigned i = 0; i < flow->componentInputs.count; i++) {
		flowState->values[i] = undefinedValue;
	}

	for (unsigned i = 0; i < flow->localVariables.count; i++) {
		auto value = flow->localVariables[i];
		flowState->values[flow->componentInputs.count + i] = *value;
	}

	for (unsigned i = 0; i < flow->components.count; i++) {
		flowState->componenentExecutionStates[i] = nullptr;
	}

	onFlowStateCreated(flowState);

	for (unsigned componentIndex = 0; componentIndex < flow->components.count; componentIndex++) {
		pingComponent(flowState, componentIndex);
	}

	return flowState;
}

FlowState *initActionFlowState(int flowIndex, FlowState *parentFlowState, int parentComponentIndex) {
	auto flowState = initFlowState(parentFlowState->assets, flowIndex, parentFlowState, parentComponentIndex);
	if (flowState) {
		flowState->isAction = true;
	}
	return flowState;
}

FlowState *initPageFlowState(Assets *assets, int flowIndex, FlowState *parentFlowState, int parentComponentIndex) {
	auto flowState = initFlowState(assets, flowIndex, parentFlowState, parentComponentIndex);
	if (flowState) {
		flowState->isAction = false;
	}
	return flowState;
}

void freeFlowState(FlowState *flowState) {
	auto flow = flowState->flow;

	auto valuesCount = flow->componentInputs.count + flow->localVariables.count;

	for (unsigned int i = 0; i < valuesCount; i++) {
		(flowState->values + i)->~Value();
	}

	for (unsigned i = 0; i < flow->components.count; i++) {
		auto componentExecutionState = flowState->componenentExecutionStates[i];
		if (componentExecutionState) {
			ObjectAllocator<ComponenentExecutionState>::deallocate(componentExecutionState);
			flowState->componenentExecutionStates[i] = nullptr;
		}
	}

	onFlowStateDestroyed(flowState);

	free(flowState);
}

void propagateValue(FlowState *flowState, unsigned componentIndex, unsigned outputIndex, const gui::Value &value) {
	auto component = flowState->flow->components[componentIndex];
	auto componentOutput = component->outputs[outputIndex];

	for (unsigned connectionIndex = 0; connectionIndex < componentOutput->connections.count; connectionIndex++) {
		auto connection = componentOutput->connections[connectionIndex];
		
		auto pValue = &flowState->values[connection->targetInputIndex];

		if (*pValue != value) {
			*pValue = value;

			if (!(flowState->flow->componentInputs[connection->targetInputIndex] & COMPONENT_INPUT_FLAG_IS_SEQ_INPUT)) {
				onValueChanged(pValue);
			}
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

void getValue(uint16_t dataId, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (isFlowRunningHook()) {
		FlowState *flowState = widgetCursor.flowState;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems[dataId];
		if (widgetDataItem && widgetDataItem->componentIndex != -1 && widgetDataItem->propertyValueIndex != -1) {
			auto component = flow->components[widgetDataItem->componentIndex];
			auto propertyValue = component->propertyValues[widgetDataItem->propertyValueIndex];

			if (!evalExpression(flowState, widgetDataItem->componentIndex, propertyValue->evalInstructions, value, nullptr, widgetCursor.iterators, operation)) {
				throwError(flowState, widgetDataItem->componentIndex, "doGetFlowValue failed\n");
			}
		}
	}
}

void setValue(uint16_t dataId, const WidgetCursor &widgetCursor, const Value& value) {
	if (isFlowRunningHook()) {
		FlowState *flowState = widgetCursor.flowState;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems[dataId];
		if (widgetDataItem && widgetDataItem->componentIndex != -1 && widgetDataItem->propertyValueIndex != -1) {
			auto component = flow->components[widgetDataItem->componentIndex];
			auto propertyValue = component->propertyValues[widgetDataItem->propertyValueIndex];

			Value dstValue;
			if (evalAssignableExpression(flowState, widgetDataItem->componentIndex, propertyValue->evalInstructions, dstValue, nullptr, widgetCursor.iterators)) {
				assignValue(flowState, widgetDataItem->componentIndex, dstValue, value);
			} else {
				throwError(flowState, widgetDataItem->componentIndex, "doSetFlowValue failed\n");
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void assignValue(FlowState *flowState, int componentIndex, Value &dstValue, const Value &srcValue) {
	if (dstValue.getType() == VALUE_TYPE_FLOW_OUTPUT) {
		propagateValue(flowState, componentIndex, dstValue.getUInt16(), srcValue);
	} else if (dstValue.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
		set(g_widgetCursor, dstValue.getInt(), srcValue);
	} else {
		Value *pDstValue = dstValue.pValueValue;
		if (assignValue(*pDstValue, srcValue)) {
			onValueChanged(pDstValue);
		} else {
			char errorMessage[100];
			snprintf(errorMessage, sizeof(errorMessage), "Can not assign %s to %s\n",
				g_valueTypeNames[pDstValue->type](*pDstValue), g_valueTypeNames[srcValue.type](srcValue)
			);
			throwError(flowState, componentIndex, errorMessage);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void startAsyncExecution(FlowState *flowState, int componentIndex) {
	flowState->numActiveComponents++;
}

void endAsyncExecution(FlowState *flowState, int componentIndex) {
	if (--flowState->numActiveComponents == 0 && flowState->isAction) {
		auto componentExecutionState = flowState->parentFlowState->componenentExecutionStates[flowState->parentComponentIndex];
		if (componentExecutionState) {
			flowState->parentFlowState->componenentExecutionStates[flowState->parentComponentIndex] = nullptr;
			ObjectAllocator<ComponenentExecutionState>::deallocate(componentExecutionState);
		} else {
			throwError(flowState, componentIndex, "Unexpected: no CallAction component state\n");
			return;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool findCatchErrorComponent(FlowState *flowState, FlowState *&catchErrorFlowState, int &catchErrorComponentIndex) {
	for (unsigned componentIndex = 0; componentIndex < flowState->flow->components.count; componentIndex++) {
		auto component = flowState->flow->components[componentIndex];
		if (component->type == defs_v3::COMPONENT_TYPE_CATCH_ERROR_ACTION) {
			catchErrorFlowState = flowState;
			catchErrorComponentIndex = componentIndex;
			return true;
		}
	}

	if (flowState->parentFlowState) {
		return findCatchErrorComponent(flowState->parentFlowState, catchErrorFlowState, catchErrorComponentIndex);
	}

	return false;
}

void throwError(FlowState *flowState, int componentIndex, const char *errorMessage) {
    auto component = flowState->flow->components[componentIndex];

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
		if (findCatchErrorComponent(flowState, catchErrorFlowState, catchErrorComponentIndex)) {
			removeQueueTasksForFlowState(flowState);

			auto catchErrorComponentExecutionState = ObjectAllocator<CatchErrorComponenentExecutionState>::allocate(0xe744a4ec);
			catchErrorComponentExecutionState->message = Value::makeStringRef(errorMessage, strlen(errorMessage), 0x9473eef2);
			catchErrorFlowState->componenentExecutionStates[catchErrorComponentIndex] = catchErrorComponentExecutionState;

			if (!addToQueue(catchErrorFlowState, catchErrorComponentIndex, -1, -1, -1)) {
				catchErrorFlowState->error = true;
				onFlowError(catchErrorFlowState, catchErrorComponentIndex, "Execution queue is full\n");
				stopScriptHook();
			}
		} else {
			flowState->error = true;
			onFlowError(flowState, componentIndex, errorMessage);
			stopScriptHook();
		}
	}
}

} // namespace flow
} // namespace eez