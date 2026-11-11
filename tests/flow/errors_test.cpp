/*
 * eez-framework tests: flow errors: the Error and CatchError components, the
 * @error output (catchError), throwError (flow/private.cpp) and the error
 * messages of the expression evaluation (flow/expression.cpp).
 */

#include "flow_helpers.h"

using namespace eez;
using namespace eez::gui;
using namespace eez::flow;
using namespace flowtest;

TEST("flow/error: a failing expression in an action goes to the @error output of the CallAction") {
    auto flowState = runPage(PAGE_ID_FLOW_ERR_CALLACTION_EVAL);
    CHECK(logIndexPrefix("eval caught Failed to evaluate 'Expression'") != -1);
    CHECK(harness::flowErrors().empty());
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/error: an Error in an action goes to the @error output of the CallAction") {
    // the search for a catch starts at the flow state of the Error component:
    // its CatchError components are skipped, the @error output of the
    // CallAction is not (NEW-flow-1: the search started at the parent flow
    // state and the error went to onFlowError)
    runPage(PAGE_ID_FLOW_ERR_CALLACTION_CATCH);
    CHECK(logIndexPrefix("caught boom") != -1);
    CHECK(harness::flowErrors().empty());
}

TEST("flow/error: an Error in an action is caught by a CatchError of the calling page") {
    runPage(PAGE_ID_FLOW_ERR_CATCH_PAGE);
    CHECK(logIndexPrefix("catch boom") != -1);
    CHECK(harness::flowErrors().empty());
    CHECK_EQ(harness::stopScriptCount(), 0);
}

TEST("flow/error: an Error in a page is not caught by a CatchError of the same page") {
    // the Error component throws to the caller of its flow (the search starts
    // at the parent flow state); a page has no caller, so it is a flow error
    runPage(PAGE_ID_FLOW_ERR_PAGE);
    CHECK(errorContains("page boom"));
    CHECK_EQ(logIndexPrefix("page catch"), -1);
    CHECK_EQ(harness::stopScriptCount(), 1);
}

TEST("flow/error: the message ends with the flow and component index") {
    runPage(PAGE_ID_FLOW_ERR_PAGE);
    REQUIRE(!harness::flowErrors().empty());
    auto &error = harness::flowErrors()[0];
    CHECK_EQ(error.flowIndex, (int)PAGE_ID_FLOW_ERR_PAGE - 1);
    auto suffix = test::format(" | %d.%d", error.flowIndex, error.componentIndex);
    CHECK(error.message.size() >= suffix.size() &&
        error.message.compare(error.message.size() - suffix.size(), suffix.size(), suffix) == 0);
}

TEST("flow/error: an uncaught error in an action calls onFlowError and stopScriptHook") {
    runPage(PAGE_ID_FLOW_ERR_UNCAUGHT_ACTION);
    CHECK(errorContains("boom"));
    CHECK_EQ(harness::stopScriptCount(), 1);
    CHECK_NOT_LOGGED("uncaught after");
}

TEST("flow/error: the action flow state of an uncaught error is freed") {
    auto flowState = runPage(PAGE_ID_FLOW_ERR_UNCAUGHT_ACTION);
    CHECK_EQ(childCount(flowState), 0);
    CHECK_EQ((int)flowState->refCounter, 0);
}

TEST("flow/catch_error: catches a failing expression of a component in the same page") {
    runPage(PAGE_ID_FLOW_CATCH_EVAL);
    CHECK(logIndexPrefix("catch eval Failed to evaluate 'Expression' in 'Evalute'") != -1);
    CHECK(harness::flowErrors().empty());
    CHECK_EQ(logIndexPrefix("catch eval result"), -1);
}

TEST("flow/catch_error: the @error output of a component gets its error message") {
    runPage(PAGE_ID_FLOW_EVAL_ERROR_OUTPUT);
    CHECK(logIndexPrefix("error output Failed to evaluate 'Expression'") != -1);
    CHECK(harness::flowErrors().empty());
    CHECK_EQ(logIndexPrefix("error output result"), -1);
}

TEST("flow/catch_error: an error in a nested action is caught by the CatchError of the calling action") {
    auto flowState = runPage(PAGE_ID_FLOW_ERR_NESTED);
    CHECK(logIndexPrefix("outer caught boom") != -1);
    CHECK(harness::flowErrors().empty());
    // the outer action has no End: it is freed when it has nothing more to do
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/catch_error: an action with a caught error drops its queued tasks") {
    // Start is wired to Error and then to Log: the Log task is already in the
    // queue when the error marks the action flow state
    auto flowState = runPage(PAGE_ID_FLOW_ERR_DROP);
    CHECK(logIndexPrefix("drop caught stop here") != -1);
    CHECK_NOT_LOGGED("act continues");
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/catch_error: two errors before the CatchError runs are both handled") {
    // the CatchError keeps a message per error and runs once per error (F1)
    runPage(PAGE_ID_FLOW_CATCH_TWICE);
    CHECK(logIndexPrefix("twice ") != -1);
}

TEST_BUG("flow/error: a message that is not a string is converted to text", "O15") {
    // FlowError::Plain(value.getString()) with a number gives "(null)"
    runPage(PAGE_ID_FLOW_ERR_NONSTRING);
    REQUIRE(!harness::flowErrors().empty());
    CHECK(harness::flowErrors()[0].message.rfind("42", 0) == 0);
}

TEST("flow/error: a failed function argument leaves the evaluation stack usable") {
    // Math.min with an error argument pops all its arguments (O1), so the
    // later SetVariable components still work
    runPage(PAGE_ID_FLOW_DRIFT);
    CHECK_LOGGED("drift 5");
}

TEST("flow/error: a failed function argument does not fire the result output") {
    runPage(PAGE_ID_FLOW_DRIFT);
    CHECK_EQ(logIndexPrefix("drift result"), -1);
}
