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

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/operations.h>

using namespace eez::gui;

namespace eez {
namespace flow {

struct CounterComponenentExecutionState : public ComponenentExecutionState {
    int counter;
};

void executeCounterComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = flowState->flow->components[componentIndex];

    auto counterComponenentExecutionState = (CounterComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];

    if (!counterComponenentExecutionState) {
        Value counterValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::COUNTER_ACTION_COMPONENT_PROPERTY_COUNT_VALUE, counterValue)) {
            throwError(flowState, componentIndex, "Failed to evaluate countValue in Counter\n");
            return;
        }

        counterComponenentExecutionState = ObjectAllocator<CounterComponenentExecutionState>::allocate(0xd33c9288);
        counterComponenentExecutionState->counter = counterValue.getInt();
        flowState->componenentExecutionStates[componentIndex] = counterComponenentExecutionState;
    }

    if (counterComponenentExecutionState->counter > 0) {
        counterComponenentExecutionState->counter--;
        propagateValueThroughSeqout(flowState, componentIndex);
    } else {
        // done
        flowState->componenentExecutionStates[componentIndex] = nullptr;
        ObjectAllocator<CounterComponenentExecutionState>::deallocate(counterComponenentExecutionState);
        propagateValue(flowState, componentIndex, 1);
    }
}

} // namespace flow
} // namespace eez
