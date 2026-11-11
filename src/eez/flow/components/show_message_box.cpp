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

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>

#include <eez/gui/gui.h>

namespace eez {
namespace flow {

const uint8_t MESSAGE_BOX_TYPE_INFO = 1;
const uint8_t MESSAGE_BOX_TYPE_ERROR = 2;
const uint8_t MESSAGE_BOX_TYPE_QUESTION = 3;

struct ShowMessagePageActionComponent : public Component {
	uint8_t type;
};

// keeps the flow state alive while the message box is open
struct ShowMessagePageComponentExecutionState : public ComponenentExecutionState {
};

// The components waiting for the message box they opened to close, by the id of
// the message box (the action parameter of the toast, the user parameter of the
// question): weak references, the flow state can be freed while the message box
// is open (its flow stopped), then the answer is ignored. A new toast replaces
// the toast shown before and a new question the question shown before (they are
// dismissed), so only a few components are waiting at the same time.
struct WaitingComponents {
    static const int N = 4;

    ComponentRef refs[N];
    int ids[N] = {}; // 0: the slot is free
    int lastId = 0;

    // returns the id for the message box; when there is no free slot, the
    // component that waits the longest is forgotten
    int add(const ComponentRef &ref) {
        int slot = 0;
        for (int i = 0; i < N; i++) {
            if (ids[i] == 0) {
                slot = i;
                break;
            }
            if (ids[i] < ids[slot]) {
                slot = i;
            }
        }

        lastId = lastId < INT_MAX ? lastId + 1 : 1;

        refs[slot] = ref;
        ids[slot] = lastId;
        return lastId;
    }

    // the component waiting for the message box, an empty reference if none
    ComponentRef take(int id) {
        if (id != 0) {
            for (int i = 0; i < N; i++) {
                if (ids[i] == id) {
                    ids[i] = 0;
                    return refs[i];
                }
            }
        }
        return ComponentRef();
    }
};

static WaitingComponents g_waitingComponents;

static void *questionUserParam(int id) {
    return (void *)(intptr_t)id;
}

static int questionId(void *userParam) {
    return (int)(intptr_t)userParam;
}

// The component has ended: the action flow state is freed if nothing else keeps
// it (e.g. no component is connected to the output that was used).
static void freeFlowStateIfDone(FlowState *flowState) {
    while (flowState && canFreeFlowState(flowState)) {
        auto parentFlowState = flowState->parentFlowState;
        freeFlowState(flowState);
        flowState = parentFlowState;
    }
}

// Close of the info or error toast, also when the toast is closed without it
// (dismissed): the flow continues through @seqout.
static void messageClosed(int id) {
    auto ref = g_waitingComponents.take(id);
    auto flowState = ref.getFlowState();
    if (!flowState) {
        return;
    }

    deallocateComponentExecutionState(flowState, ref.componentIndex);

    propagateValueThroughSeqout(flowState, ref.componentIndex);

    freeFlowStateIfDone(flowState);
}

// a button of the question is pressed: the output of the button
void questionCallback(void *userParam, unsigned buttonIndex) {
    auto ref = g_waitingComponents.take(questionId(userParam));
    auto flowState = ref.getFlowState();
    if (!flowState) {
        return;
    }

    auto componentIndex = ref.componentIndex;

    deallocateComponentExecutionState(flowState, componentIndex);

    auto component = flowState->flow->components[componentIndex];

    if (buttonIndex < component->outputs.count - 1) {
        propagateValue(flowState, componentIndex, 1 + buttonIndex);
    }

    freeFlowStateIfDone(flowState);
}

// the question is closed without a button: no output, the component ends
static void questionDismissed(void *userParam) {
    auto ref = g_waitingComponents.take(questionId(userParam));
    auto flowState = ref.getFlowState();
    if (!flowState) {
        return;
    }

    deallocateComponentExecutionState(flowState, ref.componentIndex);

    freeFlowStateIfDone(flowState);
}

void executeShowMessageBoxComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = (ShowMessagePageActionComponent *)flowState->flow->components[componentIndex];

    Value messageValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_MESSAGE_BOX_ACTION_COMPONENT_PROPERTY_MESSAGE, messageValue, FlowError::Property("ShowMessageBox", "Message"))) {
        return;
    }

    Value buttonsValue;
    if (component->type == MESSAGE_BOX_TYPE_QUESTION) {
        if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_MESSAGE_BOX_ACTION_COMPONENT_PROPERTY_BUTTONS, buttonsValue, FlowError::Property("ShowMessageBox", "Buttons"))) {
            return;
        }

        if (!buttonsValue.isArray()) {
            throwError(flowState, componentIndex, FlowError::Plain("Buttons in ShowMessageBox is not an array"));
            return;
        }

        auto buttonsArray = buttonsValue.getArray();

        if (buttonsArray->arraySize > gui::MAX_MENU_ITEMS) {
            char errorMessage[256];
            snprintf(errorMessage, sizeof(errorMessage), "Too many buttons in ShowMessageBox: %d, the max. is %d", (int)buttonsArray->arraySize, (int)gui::MAX_MENU_ITEMS);
            throwError(flowState, componentIndex, FlowError::Plain(errorMessage));
            return;
        }

        for (uint32_t i = 0; i < buttonsArray->arraySize; i++) {
            if (!buttonsArray->values[i].isString()) {
                char errorMessage[256];
                snprintf(errorMessage, sizeof(errorMessage), "Element at index %d is not a string in Buttons array in ShowMessageBox", (int)i);
                throwError(flowState, componentIndex, FlowError::Plain(errorMessage));
                return;
            }
        }
    }

    // the execution state is allocated before the message box is shown: a toast or
    // a question shown before is dismissed when this one replaces it, which must
    // not free this flow state
    allocateComponentExecutionState<ShowMessagePageComponentExecutionState>(flowState, componentIndex);

    int id = g_waitingComponents.add(ComponentRef(flowState, componentIndex));

    auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);

	if (component->type == MESSAGE_BOX_TYPE_INFO) {
		appContext->infoMessageWithAction(messageValue, messageClosed, "Close", id, messageClosed);
	} else if (component->type == MESSAGE_BOX_TYPE_ERROR) {
		appContext->errorMessageWithAction(messageValue, messageClosed, "Close", id, messageClosed);
	} else if (component->type == MESSAGE_BOX_TYPE_QUESTION) {
        appContext->questionDialog(messageValue, buttonsValue, questionUserParam(id), questionCallback, questionDismissed);
    }
}

} // namespace flow
} // namespace eez

#endif // EEZ_OPTION_GUI
