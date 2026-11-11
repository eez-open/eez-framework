/*
 * eez-framework tests: actions: CallAction (user properties, Input/Output
 * components, nesting), End, native actions (getUserProperty,
 * setUserProperty, beginAsyncExecution and the other functions of flow.h).
 */

#include "flow_helpers.h"

using namespace eez;
using namespace eez::gui;
using namespace eez::flow;
using namespace flowtest;

////////////////////////////////////////////////////////////////////////////////
// flow actions

TEST("flow/call_action: the action reads a user property and assigns an assignable one") {
    runPage(PAGE_ID_FLOW_CALL_PROPS);
    CHECK_LOGGED("props in 21");
    CHECK_LOGGED("props res 42");
}

TEST("flow/call_action: @seqout of the CallAction fires when the action reaches End") {
    runPage(PAGE_ID_FLOW_CALL_PROPS);
    int body = logIndex("props in 21"), after = logIndex("props res 42");
    CHECK(body != -1 && after != -1);
    CHECK(body < after);
}

TEST("flow/call_action: a user property that fails to evaluate is a flow error") {
    runPage(PAGE_ID_FLOW_CALL_PROPS_BAD);
    CHECK(errorContains("property #1 in 'CallAction'"));
}

TEST_BUG("flow/call_action: the error of a plain user property does not call it assignable", "NEW-flow-4") {
    // executeCallAction passes FlowError::UserAssignableProperty for the plain
    // properties and FlowError::UserProperty for the assignable ones
    runPage(PAGE_ID_FLOW_CALL_PROPS_BAD);
    CHECK(errorContains("Failed to evaluate property #1 in 'CallAction'"));
    CHECK(!errorContains("assignable property"));
}

TEST("flow/call_action: the action does not run when a user property fails") {
    // the properties are evaluated before the action flow state starts, a
    // failed property frees it (NEW-flow-5: the action ran with the property
    // undefined and its End fired @seqout of the CallAction)
    runPage(PAGE_ID_FLOW_CALL_PROPS_BAD);
    CHECK_EQ(logIndexPrefix("props in"), -1);
    CHECK_NOT_LOGGED("props bad after");
}

TEST("flow/call_action: Input and Output components pass values in and out") {
    runPage(PAGE_ID_FLOW_CALL_IO);
    CHECK_LOGGED("io 40");
}

TEST("flow/call_action: nested actions run in order") {
    runPage(PAGE_ID_FLOW_CALL_NESTED);
    int a = logIndex("outer start"), b = logIndex("inner"), c = logIndex("outer after inner"), d = logIndex("nested done");
    CHECK(a != -1 && b != -1 && c != -1 && d != -1);
    CHECK(a < b && b < c && c < d);
}

TEST("flow/call_action: the action flow states are freed when the actions are done") {
    auto flowState = runPage(PAGE_ID_FLOW_CALL_NESTED);
    CHECK_LOGGED("nested done");
    CHECK_EQ(childCount(flowState), 0);
    CHECK_EQ((int)flowState->refCounter, 0);
}

TEST("flow/call_action: the action flow state is a child of the caller while it runs") {
    auto flowState = pageFlowState(PAGE_ID_FLOW_DELAY_ACTION);
    harness::frame(1);
    REQUIRE(childCount(flowState) == 1);
    auto child = flowState->firstChild;
    CHECK(child->isAction);
    CHECK(child->parentFlowState == flowState);
    CHECK(child->refCounter > 0);
    // the counts of the child are counted in the parent too
    CHECK(flowState->refCounter >= child->refCounter);
}

TEST("flow/call_action: an action without End does not fire @seqout of the CallAction") {
    // the action flow state is freed when it has nothing more to do, without
    // propagating to the caller
    auto flowState = runPage(PAGE_ID_FLOW_CALL_NOEND);
    CHECK_LOGGED("noend body");
    CHECK_NOT_LOGGED("noend after");
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/end: End in a page flow calls stopScriptHook") {
    runPage(PAGE_ID_FLOW_END_PAGE);
    CHECK_EQ(harness::stopScriptCount(), 1);
    CHECK(harness::flowErrors().empty());
}

////////////////////////////////////////////////////////////////////////////////
// native actions

TEST("flow/native_action: reads a user property and assigns an assignable one") {
    runPage(PAGE_ID_FLOW_NATIVE_CALL);
    CHECK_LOGGED("native r 15");
}

TEST("flow/native_action: an async action continues the flow only when it ends") {
    auto flowState = runPage(PAGE_ID_FLOW_NATIVE_ASYNC_CALL);
    REQUIRE(g_asyncAction != nullptr);
    CHECK_NOT_LOGGED("async after");
    CHECK(flowState->refCounter > 0);
    endAsyncExecution(g_asyncAction);
    harness::frame(2);
    CHECK_LOGGED("async after");
    CHECK_EQ((int)flowState->refCounter, 0);
}

TEST("flow/native_action: ending an async action after the flow stopped does not use the freed flow state") {
    // the AsyncAction finds its flow state by id: after the flow stopped it
    // does nothing (F2)
    runPage(PAGE_ID_FLOW_NATIVE_ASYNC_CALL);
    REQUIRE(g_asyncAction != nullptr);
    flow::stop();
    harness::frame(1);
    REQUIRE(isFlowStopped());
    endAsyncExecution(g_asyncAction);
    CHECK(true);
}

TEST("flow/native_action: setUserPropertyAsync assigns to a flow output of the CallAction") {
    runPage(PAGE_ID_FLOW_NATIVE_ASYNC_OUT_CALL);
    REQUIRE(g_asyncOutAction != nullptr);
    setUserPropertyAsync(g_asyncOutAction, 0, Value(7, VALUE_TYPE_INT32));
    harness::frame(2);
    CHECK_LOGGED("async out 7");
    endAsyncExecution(g_asyncOutAction);
}

TEST("flow/native_action: getUserPropertyAsync evaluates the property of the CallAction") {
    runPage(PAGE_ID_FLOW_NATIVE_ASYNC_OUT_CALL);
    REQUIRE(g_asyncOutAction != nullptr);
    // pinned as it is: the property is the custom output "out", an assignable
    // expression; evaluated as a plain value it is a flow output reference
    auto value = getUserPropertyAsync(g_asyncOutAction, 0);
    CHECK_EQ((int)value.getType(), (int)VALUE_TYPE_FLOW_OUTPUT);
    endAsyncExecution(g_asyncOutAction);
}

TEST("flow/native_action: setUserPropertyAsync uses the CallAction of the async action") {
    // another native action ran in between: the value goes to the CallAction
    // of the async action (F14: it went to the other one)
    runPage(PAGE_ID_FLOW_NATIVE_ASYNC_OUT_CALL);
    REQUIRE(g_asyncOutAction != nullptr);
    runPage(PAGE_ID_FLOW_NATIVE_SYNC_CALL);
    setUserPropertyAsync(g_asyncOutAction, 0, Value(7, VALUE_TYPE_INT32));
    harness::frame(2);
    CHECK_LOGGED("async out 7");
    CHECK_NOT_LOGGED("sync out 7");
}

TEST("flow/native_action: a native action called without a CallAction runs") {
    // the path of a widget event handler bound to an action:
    // propagateValue(flowState, -1, flowIndex)
    auto flowState = runPage(PAGE_ID_FLOW_SEQ);
    propagateValue(flowState, (unsigned)-1, numFlows() + ACTION_ID_FLOW_NATIVE_SYNC, Value());
    CHECK_EQ(g_syncCalls, 1);
    CHECK(harness::flowErrors().empty());
}

TEST("flow/native_action: getUserProperty without a CallAction is a flow error") {
    // evalProperty reports the invalid index -1 as an error of the flow (F10:
    // throwError read components[-1], a garbage component, called flow 1 as
    // an action and reported no error)
    auto flowState = runPage(PAGE_ID_FLOW_SEQ);
    propagateValue(flowState, (unsigned)-1, numFlows() + ACTION_ID_FLOW_NATIVE_GETPROP, Value());
    CHECK_EQ(g_getPropCalls, 1);
    CHECK_EQ((int)harness::flowErrors().size(), 1);
    CHECK(errorContains("invalid component index -1"));
}
