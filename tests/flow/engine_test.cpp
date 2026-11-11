/*
 * eez-framework tests: the flow engine: the queue (flow/queue.cpp), flow state
 * lifetime and ref counters (flow/private.cpp), start/stop/tick (flow/flow.cpp),
 * the component table (flow/components.cpp), the float list and data
 * operations (flow.cpp dataOperation) and the evaluation stack (expression.h).
 */

#include "flow_helpers.h"

#include <eez/flow/components.h>
#include <eez/flow/components/call_action.h>
#include <eez/flow/expression.h>

#include <string.h>

using namespace eez;
using namespace eez::gui;
using namespace eez::flow;
using namespace flowtest;

namespace eez {
namespace flow {
extern EvalStack g_stack;
}
}

namespace {

// first OPERATION instruction of an expression
uint8_t *findOperationInstruction(uint8_t *instructions) {
    for (int i = 0; i < 200; i += 2) {
        uint16_t instruction = instructions[i] + (instructions[i + 1] << 8);
        auto type = instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK;
        if (type == EXPR_EVAL_INSTRUCTION_TYPE_OPERATION) {
            return instructions + i;
        }
        if (type == EXPR_EVAL_INSTRUCTION_TYPE_END) {
            return nullptr;
        }
    }
    return nullptr;
}

int g_customCalls;

void customComponent(FlowState *flowState, unsigned componentIndex) {
    g_customCalls++;
    propagateValueThroughSeqout(flowState, componentIndex);
}

// the Noop of the "flow_patch" page, before its flow state is created
Component *patchNoop() {
    auto flow = g_mainAssets->flowDefinition->flows[PAGE_ID_FLOW_PATCH - 1];
    for (uint32_t i = 0; i < flow->components.count; i++) {
        if (flow->components[i]->type == defs_v3::COMPONENT_TYPE_NOOP_ACTION) {
            return flow->components[i];
        }
    }
    return nullptr;
}

Component *patchEvalExpr() {
    auto flow = g_mainAssets->flowDefinition->flows[PAGE_ID_FLOW_PATCH - 1];
    for (uint32_t i = 0; i < flow->components.count; i++) {
        if (flow->components[i]->type == defs_v3::COMPONENT_TYPE_EVAL_EXPR_ACTION) {
            return flow->components[i];
        }
    }
    return nullptr;
}

// number of entries of g_evalOperations (flow/operations.cpp) today
const uint16_t NUM_EVAL_OPERATIONS = 97;

} // namespace

////////////////////////////////////////////////////////////////////////////////
// queue, evaluation stack

TEST("flow/queue: the queue and the evaluation stack are empty after the flows finish") {
    runPage(PAGE_ID_FLOW_CALL_NESTED);
    CHECK_LOGGED("nested done");
    CHECK_EQ((int)getQueueSize(), 0);
    CHECK_EQ((int)g_stack.sp, 0);
}

TEST("flow/queue: a full queue is a flow error, the flow goes on and the queue drains") {
    auto flowState = runPage(PAGE_ID_FLOW_FLOOD);
    CHECK(errorContains("Execution queue is full"));
    CHECK(harness::stopScriptCount() > 0);
    CHECK_EQ((int)getQueueSize(), 0);
    CHECK_EQ((int)getMaxQueueSize(), 1000);
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/queue: a full queue with a CatchError reports the error") {
    // addToQueue -> throwError -> CatchError -> addToQueue (full) -> throwError:
    // the messages a CatchError keeps are limited, which ends this (F5)
    runPage(PAGE_ID_FLOW_FLOOD_CATCH);
    CHECK(errorContains("Execution queue is full") || logIndexPrefix("flood caught") != -1);
}

TEST("flow/queue: a full queue while an action flow state is created") {
    // the second ready component of the new action doesn't fit into the queue;
    // throwError finds the @error output of the CallAction and reads isAction of
    // the new flow state, which initFlowState sets before it queues anything
    // (F19: it was not set yet, UBSan: load of an invalid bool)
    runPage(PAGE_ID_FLOW_FLOOD2);
    CHECK(errorContains("Execution queue is full"));
    CHECK_EQ((int)getQueueSize(), 0);
}

TEST("flow/queue: tasks of the non-continuous kind added during the tick run in the same tick") {
    // the loop with 3 iterations and the done output finish in one frame
    pageFlowState(PAGE_ID_FLOW_LOOP);
    harness::frame(1);
    CHECK_LOGGED("loop done");
}

////////////////////////////////////////////////////////////////////////////////
// flow state lifetime, start, stop

TEST("flow/lifetime: a page flow state stays when its flow is done, with ref counter 0") {
    auto flowState = runPage(PAGE_ID_FLOW_SEQ);
    CHECK_EQ((int)flowState->refCounter, 0);
    CHECK(!flowState->isAction);
    bool found = false;
    for (auto fs = g_firstFlowState; fs; fs = fs->nextSibling) {
        if (fs == flowState) {
            found = true;
        }
    }
    CHECK(found);
}

TEST("flow/lifetime: stop() frees all flow states at the next tick") {
    runPage(PAGE_ID_FLOW_SEQ);
    flow::stop();
    CHECK(!isFlowStopped());
    harness::frame(1);
    CHECK(isFlowStopped());
    CHECK(g_firstFlowState == nullptr);
    CHECK_EQ((int)getQueueSize(), 0);
}

TEST("flow/lifetime: no page flow state while the flow is stopped") {
    flow::stop();
    harness::frame(1);
    CHECK(pageFlowState(PAGE_ID_FLOW_SEQ) == nullptr);
}

TEST("flow/lifetime: stop() drops the pending tasks") {
    pageFlowState(PAGE_ID_FLOW_DELAY);
    harness::frame(1);
    CHECK_LOGGED("delay start");
    flow::stop();
    harness::frame(20);
    CHECK_NOT_LOGGED("delay done");
}

TEST("flow/lifetime: start() after stop() runs the page flows again") {
    runPage(PAGE_ID_FLOW_SEQ);
    flow::stop();
    harness::frame(1);
    CHECK_EQ(flow::start(g_mainAssets), 1u);
    CHECK(!isFlowStopped());
    runPage(PAGE_ID_FLOW_SEQ);
    CHECK_EQ(countLogged("seq 1"), 2);
}

TEST("flow/lifetime: start() of assets without a flow definition returns 0") {
    alignas(8) static uint8_t buffer[sizeof(Assets)];
    memset(buffer, 0, sizeof(buffer));
    auto assets = (Assets *)buffer;
    assets->external = true;
    CHECK_EQ(flow::start(assets), 0u);
}

TEST("flow/lifetime: start() of external assets does not cancel a stop of the engine") {
    // a module starting while the main flow is stopping (the main assets stand
    // in for external assets here: start() only looks at the external flag)
    flow::stop();
    g_mainAssets->external = true;
    flow::start(g_mainAssets);
    g_mainAssets->external = false;
    harness::frame(1);
    CHECK(isFlowStopped());
}

////////////////////////////////////////////////////////////////////////////////
// component table

TEST("flow/components: a component type without an execute function is a flow error") {
    auto noop = patchNoop();
    REQUIRE(noop != nullptr);
    noop->type = defs_v3::COMPONENT_TYPE_SCPI_ACTION;
    runPage(PAGE_ID_FLOW_PATCH);
    CHECK(errorContains("Unknown component"));
    CHECK_EQ(logIndexPrefix("patch"), -1);
}

TEST("flow/components: registerComponent sets the execute function of a component type") {
    auto noop = patchNoop();
    REQUIRE(noop != nullptr);
    noop->type = defs_v3::COMPONENT_TYPE_SCPI_ACTION;
    registerComponent(defs_v3::COMPONENT_TYPE_SCPI_ACTION, customComponent);
    runPage(PAGE_ID_FLOW_PATCH);
    CHECK_EQ(g_customCalls, 1);
    CHECK_LOGGED("patch 3");
}

TEST("flow/components: a component type past the table is an unknown component") {
    // g_executeComponentFunctions has entries up to type 1045
    auto noop = patchNoop();
    REQUIRE(noop != nullptr);
    noop->type = defs_v3::COMPONENT_TYPE_SET_COLOR_THEME_ACTION + 1;
    runPage(PAGE_ID_FLOW_PATCH);
    CHECK(errorContains("Unknown component"));
}

TEST("flow/components: registerComponent ignores a type past the table") {
    registerComponent((ComponentTypes)(defs_v3::COMPONENT_TYPE_SET_COLOR_THEME_ACTION + 1), customComponent);
    CHECK(true);
}

TEST_BUG("flow/expression: an operation index past the table is an evaluation error", "F13") {
    auto evalExpr = patchEvalExpr();
    REQUIRE(evalExpr != nullptr);
    auto instruction = findOperationInstruction(evalExpr->properties[0]->evalInstructions);
    REQUIRE(instruction != nullptr);
    uint16_t patched = EXPR_EVAL_INSTRUCTION_TYPE_OPERATION | NUM_EVAL_OPERATIONS;
    instruction[0] = patched & 0xFF;
    instruction[1] = patched >> 8;
    runPage(PAGE_ID_FLOW_PATCH);
    CHECK(errorContains("Failed to evaluate"));
    CHECK_EQ(logIndexPrefix("patch"), -1);
}

namespace {

// a component like BB3+'s SCPI component: keeps its execution state without
// allocateComponentExecutionState (no incRefCounterForFlowState) and waits as
// a continuous task
struct WaitingState : public ComponenentExecutionState {
    int executions;
};

void waitingComponent(FlowState *flowState, unsigned componentIndex) {
    auto state = (WaitingState *)flowState->componenentExecutionStates[componentIndex];
    if (!state) {
        state = ObjectAllocator<WaitingState>::allocate(0x5a5a5a5a);
        flowState->componenentExecutionStates[componentIndex] = state;
    }
    state->executions++;
    addToQueue(flowState, componentIndex, -1, -1, -1, true);
}

} // namespace

TEST_BUG("flow/components: a custom component state the engine did not count is not uncounted", "F7") {
    // the error of the action is caught by the page: the tick frees the state of
    // the waiting component and decrements the ref counters, which were never
    // incremented for it (TRACK_REF_COUNTER_FOR_COMPONENT_STATE counts every
    // type not on its exclusion list); the counter of the action wraps and it
    // is never freed
    auto pageFlow = g_mainAssets->flowDefinition->flows[PAGE_ID_FLOW_CUSTOM_ERR - 1];
    int actionFlowIndex = -1;
    for (uint32_t i = 0; i < pageFlow->components.count; i++) {
        if (pageFlow->components[i]->type == defs_v3::COMPONENT_TYPE_CALL_ACTION_ACTION) {
            actionFlowIndex = ((CallActionActionComponent *)pageFlow->components[i])->flowIndex;
        }
    }
    REQUIRE(actionFlowIndex != -1);
    auto actionFlow = g_mainAssets->flowDefinition->flows[actionFlowIndex];
    Component *noop = nullptr;
    for (uint32_t i = 0; i < actionFlow->components.count; i++) {
        if (actionFlow->components[i]->type == defs_v3::COMPONENT_TYPE_NOOP_ACTION) {
            noop = actionFlow->components[i];
        }
    }
    REQUIRE(noop != nullptr);
    noop->type = defs_v3::COMPONENT_TYPE_SCPI_ACTION;
    registerComponent(defs_v3::COMPONENT_TYPE_SCPI_ACTION, waitingComponent);

    auto flowState = runPage(PAGE_ID_FLOW_CUSTOM_ERR);
    CHECK(logIndexPrefix("custom caught custom boom") != -1);
    CHECK_EQ(childCount(flowState), 0);
    CHECK_EQ((int)flowState->refCounter, 0);
}

////////////////////////////////////////////////////////////////////////////////
// global variables (flow.h)

namespace {

int globalIndex(const char *name) {
    auto value = globalVar(name);
    auto &globals = g_mainAssets->flowDefinition->globalVariables;
    for (uint32_t i = 0; i < globals.count; i++) {
        if (globals[i] == value) {
            return (int)i;
        }
    }
    return -1;
}

} // namespace

TEST("flow/globals: getGlobalVariable and setGlobalVariable use the main assets globals") {
    int index = globalIndex("flow_g_int");
    REQUIRE(index != -1);
    CHECK_EQ(getGlobalVariable(index).getInt(), 0);
    setGlobalVariable(index, Value(42, VALUE_TYPE_INT32));
    CHECK_EQ(getGlobalVariable(index).getInt(), 42);
    CHECK_EQ(globalVar("flow_g_int")->getInt(), 42);
}

TEST("flow/globals: an index past the globals reads undefined and is not written") {
    uint32_t count = g_mainAssets->flowDefinition->globalVariables.count;
    CHECK(getGlobalVariable(count).isUndefinedOrNull());
    setGlobalVariable(count, Value(1, VALUE_TYPE_INT32));
    CHECK(getGlobalVariable(count).isUndefinedOrNull());
}

////////////////////////////////////////////////////////////////////////////////
// data operations for widgets (flow.cpp dataOperation)

namespace {

WidgetCursor floatListCursor(FlowState *flowState) {
    WidgetCursor widgetCursor;
    widgetCursor.assets = g_mainAssets;
    widgetCursor.appContext = harness::appContext();
    widgetCursor.flowState = flowState;
    widgetCursor.widget = findWidget(PAGE_ID_FLOW_FLOATLIST, WIDGET_TYPE_DISPLAY_DATA);
    return widgetCursor;
}

} // namespace

TEST("flow/data: the float list of an array of numbers") {
    auto flowState = pageFlowState(PAGE_ID_FLOW_FLOATLIST);
    auto widgetCursor = floatListCursor(flowState);
    REQUIRE(widgetCursor.widget != nullptr);
    harness::frame(1);
    auto list = globalVar("flow_g_list");
    REQUIRE(list != nullptr);
    auto array = Value::makeArrayRef(3, defs_v3::ARRAY_TYPE_FLOAT, 0x12345678);
    array.getArray()->values[0] = Value(1.5f, VALUE_TYPE_FLOAT);
    array.getArray()->values[1] = Value(-2, VALUE_TYPE_INT32);
    array.getArray()->values[2] = Value(4.25, VALUE_TYPE_DOUBLE);
    *list = array;

    Value length;
    dataOperation(widgetCursor.widget->data, DATA_OPERATION_GET_FLOAT_LIST_LENGTH, widgetCursor, length);
    CHECK_EQ(length.getInt(), 3);
    Value floats;
    dataOperation(widgetCursor.widget->data, DATA_OPERATION_GET_FLOAT_LIST, widgetCursor, floats);
    auto values = (float *)floats.getVoidPointer();
    REQUIRE(values != nullptr);
    CHECK_NEAR(values[0], 1.5, 1e-6);
    CHECK_NEAR(values[1], -2.0, 1e-6);
    CHECK_NEAR(values[2], 4.25, 1e-6);
}

TEST_BUG("flow/data: the float list length is not more than the float list holds", "F16") {
    // the list holds 256 floats, the length is the array size (300)
    auto flowState = runPage(PAGE_ID_FLOW_FLOATLIST);
    auto widgetCursor = floatListCursor(flowState);
    REQUIRE(widgetCursor.widget != nullptr);
    Value length;
    dataOperation(widgetCursor.widget->data, DATA_OPERATION_GET_FLOAT_LIST_LENGTH, widgetCursor, length);
    CHECK(length.getInt() > 0);
    CHECK(length.getInt() <= 256);
}

TEST("flow/expression: a popped value is not kept by the evaluation stack") {
    // pop() clears the slot, the array made by the last expression is not
    // referenced by the stack (O17: it stayed until the slot was reused)
    // one frame: later evaluations (of the GUI) reuse slot 0
    pageFlowState(PAGE_ID_FLOW_REFS);
    harness::frame(1);
    REQUIRE((int)g_stack.sp == 0);
    for (size_t i = g_stack.sp; i < STACK_SIZE; i++) {
        if (g_stack.stack[i].options & VALUE_OPTIONS_REF) {
            test::fail(__FILE__, __LINE__, test::format("slot %d above the stack pointer holds a reference (type %d)",
                (int)i, (int)g_stack.stack[i].getType()));
        }
    }
}

namespace {

WidgetCursor dropDownCursor(FlowState *flowState) {
    WidgetCursor widgetCursor;
    widgetCursor.assets = g_mainAssets;
    widgetCursor.appContext = harness::appContext();
    widgetCursor.flowState = flowState;
    widgetCursor.widget = findWidget(PAGE_ID_FLOW_DROPDOWN, WIDGET_TYPE_DROP_DOWN_LIST);
    return widgetCursor;
}

} // namespace

TEST("flow/data: a widget assigns its variable through dataOperation SET") {
    auto widgetCursor = dropDownCursor(pageFlowState(PAGE_ID_FLOW_DROPDOWN));
    REQUIRE(widgetCursor.widget != nullptr);
    Value value(1, VALUE_TYPE_INT32);
    dataOperation(widgetCursor.widget->data, DATA_OPERATION_SET, widgetCursor, value);
    CHECK_EQ(globalVar("flow_g_int")->getInt(), 1);
    Value read;
    dataOperation(widgetCursor.widget->data, DATA_OPERATION_GET, widgetCursor, read);
    CHECK_EQ(read.getInt(), 1);
}

TEST_BUG("flow/data: a widget of a flow state in error does not assign its variable", "F17") {
    // BB3+ marks a flow that must not execute anything more with the error
    // flag; dataOperation SET -> setValue -> assignValue ignores it
    auto flowState = pageFlowState(PAGE_ID_FLOW_DROPDOWN);
    auto widgetCursor = dropDownCursor(flowState);
    REQUIRE(widgetCursor.widget != nullptr);
    flowState->error = true;
    Value value(1, VALUE_TYPE_INT32);
    dataOperation(widgetCursor.widget->data, DATA_OPERATION_SET, widgetCursor, value);
    CHECK_EQ(globalVar("flow_g_int")->getInt(), 0);
}
