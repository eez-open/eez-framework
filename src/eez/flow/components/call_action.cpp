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

#include <eez/core/action.h>

#include <eez/flow/components.h>
#include <eez/flow/components/call_action.h>
#include <eez/flow/debugger.h>
#include <eez/flow/queue.h>

namespace eez {
namespace flow {

void executeCallAction(FlowState *flowState, unsigned componentIndex, int flowIndex, const Value& inputValue) {
    // if componentIndex == -1 then execute flow at flowIndex without CallAction component

	if (flowIndex >= (int)flowState->flowDefinition->flows.count) {
        // native action
		executeActionFunction(flowIndex - flowState->flowDefinition->flows.count);
		propagateValueThroughSeqout(flowState, componentIndex);
		return;
	}

	FlowState *actionFlowState = initActionFlowState(flowIndex, flowState, componentIndex, inputValue);

	if (canFreeFlowState(actionFlowState)) {
        freeFlowState(actionFlowState);
        if ((int)componentIndex != -1) {
		    propagateValueThroughSeqout(flowState, componentIndex);
        }
	}
}

void executeCallActionComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = (CallActionActionComponent *)flowState->flow->components[componentIndex];

	auto flowIndex = component->flowIndex;
	if (flowIndex < 0) {
		throwError(flowState, componentIndex, "Invalid action flow index in CallAction\n");
		return;
	}

    executeCallAction(flowState, componentIndex, flowIndex, Value());
}

} // namespace flow
} // namespace eez
