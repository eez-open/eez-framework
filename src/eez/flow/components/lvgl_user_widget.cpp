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

#include <eez/conf-internal.h>

#include <eez/core/alloc.h>
#include <eez/core/os.h>

#include <eez/flow/components.h>
#include <eez/flow/flow.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/queue.h>
#include <eez/flow/components/call_action.h>
#include <eez/flow/components/input.h>

#if defined(EEZ_FOR_LVGL)

namespace eez {
namespace flow {

struct LVGLUserWidgetExecutionState : public ComponenentExecutionState {
    FlowState *flowState;

    ~LVGLUserWidgetExecutionState() {
        freeFlowState(flowState);
    }
};

static LVGLUserWidgetExecutionState *createLayoutViewFlowState(FlowState *flowState, uint16_t layoutViewWidgetComponentIndex, int16_t pageId) {
    auto layoutViewFlowState = initPageFlowState(flowState->assets, pageId, flowState, layoutViewWidgetComponentIndex);
    auto layoutViewWidgetExecutionState = allocateComponentExecutionState<LVGLUserWidgetExecutionState>(flowState, layoutViewWidgetComponentIndex);
    layoutViewWidgetExecutionState->flowState = layoutViewFlowState;
    return layoutViewWidgetExecutionState;
}

FlowState *getLayoutViewFlowState(FlowState *flowState, uint16_t layoutViewWidgetComponentIndex, int16_t pageId) {
    auto layoutViewWidgetExecutionState = (LVGLUserWidgetExecutionState *)flowState->componenentExecutionStates[layoutViewWidgetComponentIndex];
    if (!layoutViewWidgetExecutionState) {
        layoutViewWidgetExecutionState = createLayoutViewFlowState(flowState, layoutViewWidgetComponentIndex, pageId);
    }

    return layoutViewWidgetExecutionState->flowState;
}

void executeLVGLUserWidgetComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (CallActionActionComponent *)flowState->flow->components[componentIndex];

    auto layoutViewWidgetExecutionState = (LVGLUserWidgetExecutionState *)flowState->componenentExecutionStates[componentIndex];
    if (!layoutViewWidgetExecutionState) {
        createLayoutViewFlowState(flowState, componentIndex, component->flowIndex);
    } else {
        auto layoutViewFlowState = layoutViewWidgetExecutionState->flowState;
        for (
            unsigned layoutViewComponentIndex = 0;
            layoutViewComponentIndex < layoutViewFlowState->flow->components.count;
            layoutViewComponentIndex++
        ) {
            auto layoutViewComponent = layoutViewFlowState->flow->components[layoutViewComponentIndex];
            if (layoutViewComponent->type == defs_v3::COMPONENT_TYPE_INPUT_ACTION) {
                auto inputActionComponentExecutionState = (InputActionComponentExecutionState *)layoutViewFlowState->componenentExecutionStates[layoutViewComponentIndex];
                if (inputActionComponentExecutionState) {
                    Value value;
                    if (getCallActionValue(layoutViewFlowState, layoutViewComponentIndex, value)) {
                        if (inputActionComponentExecutionState->value != value) {
                            addToQueue(layoutViewWidgetExecutionState->flowState, layoutViewComponentIndex, -1, -1, -1, false);
                            inputActionComponentExecutionState->value = value;
                        }
                    } else {
                        return;
                    }
                }
            } else if (layoutViewComponent->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
                Value value;
                if (getCallActionValue(layoutViewFlowState, layoutViewComponentIndex, value)) {
                    if (value.getType() != VALUE_TYPE_UNDEFINED) {
                        addToQueue(layoutViewWidgetExecutionState->flowState, layoutViewComponentIndex, -1, -1, -1, false);
                    }
                } else {
                    return;
                }
            }
        }
    }
}

} // namespace flow
} // namespace eez


#else

namespace eez {
namespace flow {

void executeLVGLUserWidgetComponent(FlowState *flowState, unsigned componentIndex) {
    throwError(flowState, componentIndex, "Not implemented");
}

} // namespace flow
} // namespace eez

#endif
