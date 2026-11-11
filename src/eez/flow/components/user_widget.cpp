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
#include <eez/core/os.h>

#include <eez/flow/components.h>
#include <eez/flow/flow.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>
#include <eez/flow/expression.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/components/call_action.h>
#include <eez/flow/components/input.h>

namespace eez {
namespace flow {

typedef CallActionActionComponent UserWidgetWidgetComponent;

// The same user widget component can display different layouts (pages), e.g.
// list items with layouts from the data (BB3+ slot views of the modules).
// Besides the flow state of the last displayed layout, the flow states of
// MAX_OTHER_FLOW_STATES other layouts are kept, so the flow state of a layout
// is not freed and created again on each render (a widget found on touch down
// keeps a valid flow state until the action is executed on touch up).
struct UserWidgetWidgetExecutionState : public ComponenentExecutionState {
    static const int MAX_OTHER_FLOW_STATES = 8;

    FlowState *flowState;
    FlowState *otherFlowStates[MAX_OTHER_FLOW_STATES];
    int numOtherFlowStates;

    ~UserWidgetWidgetExecutionState() {
        freeOwnedFlowState(flowState);
        for (int i = 0; i < numOtherFlowStates; i++) {
            freeOwnedFlowState(otherFlowStates[i]);
        }
    }
};

static FlowState *initUserWidgetFlowState(Assets *assets, FlowState *flowState, uint16_t userWidgetWidgetComponentIndex, int16_t pageIndex) {
    auto userWidgetFlowState = initPageFlowState(assets, pageIndex, flowState, userWidgetWidgetComponentIndex);

    // init user properties
    auto component = flowState->flow->components[userWidgetWidgetComponentIndex];
    auto offset = defs_v3::USER_WIDGET_WIDGET_USER_PROPERTIES_START;
    for (uint32_t i = offset; i < component->properties.count; i++) {
        Value value = Value::makePropertyRef(flowState, userWidgetWidgetComponentIndex, i, 0x5166d8a4);
        auto propValuePtr = userWidgetFlowState->values + userWidgetFlowState->flow->componentInputs.count + (i - offset);
        *propValuePtr = value;
        onValueChanged(propValuePtr);
    }

    return userWidgetFlowState;
}

static UserWidgetWidgetExecutionState *createUserWidgetFlowState(Assets *assets, FlowState *flowState, uint16_t userWidgetWidgetComponentIndex, int16_t pageIndex) {
    if (pageIndex == -1) {
        return 0;
    }
    auto userWidgetFlowState = initUserWidgetFlowState(assets, flowState, userWidgetWidgetComponentIndex, pageIndex);

    auto userWidgetWidgetExecutionState = allocateComponentExecutionState<UserWidgetWidgetExecutionState>(flowState, userWidgetWidgetComponentIndex);
    userWidgetWidgetExecutionState->flowState = userWidgetFlowState;
    userWidgetWidgetExecutionState->numOtherFlowStates = 0;
    return userWidgetWidgetExecutionState;
}

FlowState *getUserWidgetFlowState(Assets *assets, FlowState *flowState, uint16_t userWidgetWidgetComponentIndex, int16_t pageIndex) {
    auto userWidgetWidgetExecutionState = (UserWidgetWidgetExecutionState *)flowState->componenentExecutionStates[userWidgetWidgetComponentIndex];
    if (!userWidgetWidgetExecutionState) {
        userWidgetWidgetExecutionState = createUserWidgetFlowState(assets, flowState, userWidgetWidgetComponentIndex, pageIndex);
        return userWidgetWidgetExecutionState ? userWidgetWidgetExecutionState->flowState : nullptr;
    }

    auto currentFlowState = userWidgetWidgetExecutionState->flowState;
    if (currentFlowState->assets == assets && currentFlowState->flowIndex == pageIndex) {
        return currentFlowState;
    }

    if (pageIndex == -1) {
        return nullptr;
    }

    // flow state of another layout: already created or a new one
    FlowState *userWidgetFlowState = nullptr;
    auto &numOtherFlowStates = userWidgetWidgetExecutionState->numOtherFlowStates;
    auto otherFlowStates = userWidgetWidgetExecutionState->otherFlowStates;
    for (int i = 0; i < numOtherFlowStates; i++) {
        if (otherFlowStates[i]->assets == assets && otherFlowStates[i]->flowIndex == pageIndex) {
            userWidgetFlowState = otherFlowStates[i];
            for (int j = i + 1; j < numOtherFlowStates; j++) {
                otherFlowStates[j - 1] = otherFlowStates[j];
            }
            numOtherFlowStates--;
            break;
        }
    }

    if (!userWidgetFlowState) {
        userWidgetFlowState = initUserWidgetFlowState(assets, flowState, userWidgetWidgetComponentIndex, pageIndex);
    }

    // the current one becomes the most recent of the others, the least
    // recently used is freed if there is no room
    if (numOtherFlowStates == UserWidgetWidgetExecutionState::MAX_OTHER_FLOW_STATES) {
        // only this flow state: freeFlowState would free the whole execution state
        freeOwnedFlowState(otherFlowStates[0]);
        for (int j = 1; j < numOtherFlowStates; j++) {
            otherFlowStates[j - 1] = otherFlowStates[j];
        }
        numOtherFlowStates--;
    }
    otherFlowStates[numOtherFlowStates++] = currentFlowState;

    userWidgetWidgetExecutionState->flowState = userWidgetFlowState;

    return userWidgetFlowState;
}

void freeUserWidgetFlowState(FlowState *flowState) {
    auto parentFlowState = flowState->parentFlowState;
    auto userWidgetWidgetComponentIndex = flowState->parentComponentIndex;
    auto userWidgetWidgetExecutionState = (UserWidgetWidgetExecutionState *)parentFlowState->componenentExecutionStates[userWidgetWidgetComponentIndex];

    if (userWidgetWidgetExecutionState) {
        auto &numOtherFlowStates = userWidgetWidgetExecutionState->numOtherFlowStates;
        auto otherFlowStates = userWidgetWidgetExecutionState->otherFlowStates;

        if (userWidgetWidgetExecutionState->flowState == flowState) {
            if (numOtherFlowStates == 0) {
                // the last one: the execution state frees it
                deallocateComponentExecutionState(parentFlowState, userWidgetWidgetComponentIndex);
                return;
            }
            // the most recently used other layout becomes the current one, the
            // next render switches to the layout it shows
            userWidgetWidgetExecutionState->flowState = otherFlowStates[--numOtherFlowStates];
        } else {
            for (int i = 0; i < numOtherFlowStates; i++) {
                if (otherFlowStates[i] == flowState) {
                    for (int j = i + 1; j < numOtherFlowStates; j++) {
                        otherFlowStates[j - 1] = otherFlowStates[j];
                    }
                    numOtherFlowStates--;
                    break;
                }
            }
        }
    }

    freeOwnedFlowState(flowState);
}

void executeUserWidgetWidgetComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (UserWidgetWidgetComponent *)flowState->flow->components[componentIndex];

    auto userWidgetWidgetExecutionState = (UserWidgetWidgetExecutionState *)flowState->componenentExecutionStates[componentIndex];
    if (!userWidgetWidgetExecutionState) {
        createUserWidgetFlowState(flowState->assets, flowState, componentIndex, component->flowIndex);
    } else if (component->inputs.count > 0) {
        // the values of the inputs of the user widget go to the Input and
        // Start components of the layout. A user widget without inputs (e.g.
        // a layout from the data) has nothing to pass: the Start component
        // ran when the flow state of the layout was created.
        auto userWidgetFlowState = userWidgetWidgetExecutionState->flowState;
        for (
            unsigned userWidgetComponentIndex = 0;
            userWidgetComponentIndex < userWidgetFlowState->flow->components.count;
            userWidgetComponentIndex++
        ) {
            auto userWidgetComponent = userWidgetFlowState->flow->components[userWidgetComponentIndex];
            if (userWidgetComponent->type == defs_v3::COMPONENT_TYPE_INPUT_ACTION) {
                auto inputActionComponentExecutionState = (InputActionComponentExecutionState *)userWidgetFlowState->componenentExecutionStates[userWidgetComponentIndex];
                if (inputActionComponentExecutionState) {
                    Value value;
                    if (getCallActionValue(userWidgetFlowState, userWidgetComponentIndex, value)) {
                        if (inputActionComponentExecutionState->value != value) {
                            addToQueue(userWidgetWidgetExecutionState->flowState, userWidgetComponentIndex, -1, -1, -1, false);
                            inputActionComponentExecutionState->value = value;
                        }
                    } else {
                        return;
                    }
                }
            } else if (userWidgetComponent->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
                Value value;
                if (getCallActionValue(userWidgetFlowState, userWidgetComponentIndex, value)) {
                    if (value.getType() != VALUE_TYPE_UNDEFINED) {
                        addToQueue(userWidgetWidgetExecutionState->flowState, userWidgetComponentIndex, -1, -1, -1, false);
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

#endif // EEZ_OPTION_GUI
