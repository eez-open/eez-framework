/*
 * eez-framework tests: helpers of the flow tests (flow_helpers.h).
 */

#include "flow_helpers.h"

#include <string.h>

#include <eez/flow/expression.h>
#include <eez/flow/components/set_variable.h>

using namespace eez;
using namespace eez::flow;

namespace flowtest {

FlowState *pageFlowState(int pageId) {
    return getPageFlowState(g_mainAssets, pageId - 1, gui::WidgetCursor());
}

FlowState *runPage(int pageId, int frames) {
    auto flowState = pageFlowState(pageId);
    harness::frame(frames);
    return flowState;
}

void showPage(int pageId, int frames) {
    harness::appContext()->showPage(pageId);
    harness::frame(frames);
}

bool logged(const std::string &message) {
    return logIndex(message) != -1;
}

int countLogged(const std::string &message) {
    int n = 0;
    for (auto &m : harness::logMessages()) {
        if (m == message) {
            n++;
        }
    }
    return n;
}

int logIndex(const std::string &message) {
    auto &messages = harness::logMessages();
    for (size_t i = 0; i < messages.size(); i++) {
        if (messages[i] == message) {
            return (int)i;
        }
    }
    return -1;
}

int logIndexPrefix(const std::string &prefix) {
    auto &messages = harness::logMessages();
    for (size_t i = 0; i < messages.size(); i++) {
        if (messages[i].rfind(prefix, 0) == 0) {
            return (int)i;
        }
    }
    return -1;
}

std::string logs() {
    std::string result = "[";
    for (auto &m : harness::logMessages()) {
        if (result.size() > 1) {
            result += ", ";
        }
        result += "\"" + m + "\"";
    }
    return result + "]";
}

bool errorContains(const std::string &text) {
    for (auto &error : harness::flowErrors()) {
        if (error.message.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string errors() {
    std::string result = "[";
    for (auto &error : harness::flowErrors()) {
        if (result.size() > 1) {
            result += ", ";
        }
        result += "\"" + error.message + "\"";
    }
    return result + "]";
}

int childCount(FlowState *flowState) {
    int n = 0;
    for (auto child = flowState->firstChild; child; child = child->nextSibling) {
        n++;
    }
    return n;
}

int topLevelCount() {
    int n = 0;
    for (auto flowState = g_firstFlowState; flowState; flowState = flowState->nextSibling) {
        n++;
    }
    return n;
}

uint32_t numFlows() {
    return g_mainAssets->flowDefinition->flows.count;
}

int findComponent(FlowState *flowState, uint16_t type, int nth) {
    for (uint32_t i = 0; i < flowState->flow->components.count; i++) {
        if (flowState->flow->components[i]->type == type) {
            if (nth-- == 0) {
                return (int)i;
            }
        }
    }
    return -1;
}

Value *globalVar(const char *name) {
    auto flowState = pageFlowState(gui::PAGE_ID_FLOW_VARS);
    if (!flowState) {
        return nullptr;
    }
    for (uint32_t i = 0; i < flowState->flow->components.count; i++) {
        auto component = flowState->flow->components[i];
        if (component->type != defs_v3::COMPONENT_TYPE_SET_VARIABLE_ACTION) {
            continue;
        }
        auto setVariable = (SetVariableActionComponent *)component;
        if (setVariable->entries.count != 2) {
            continue;
        }
        Value label;
        if (!evalExpression(flowState, (int)i, setVariable->entries[0]->value, label, FlowError::Plain("globalVar label"))) {
            continue;
        }
        if (!label.isString() || strcmp(label.getString(), name) != 0) {
            continue;
        }
        Value dst;
        if (!evalAssignableExpression(flowState, (int)i, setVariable->entries[1]->variable, dst, FlowError::Plain("globalVar variable"))) {
            return nullptr;
        }
        if (dst.getType() != VALUE_TYPE_VALUE_PTR) {
            return nullptr;
        }
        return dst.pValueValue;
    }
    return nullptr;
}

const gui::Widget *findWidget(int pageId, uint16_t type) {
    auto page = g_mainAssets->pages[pageId - 1];
    for (uint32_t i = 0; i < page->widgets.count; i++) {
        auto widget = page->widgets[i];
        if (widget->type == type) {
            return widget;
        }
    }
    return nullptr;
}

} // namespace flowtest
