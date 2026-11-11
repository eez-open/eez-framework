/*
 * eez-framework tests: flow action components (flow/components): Start,
 * SetVariable, Switch, Compare, IsTrue, Loop, Counter, Delay, Watch, Log,
 * LabelIn/LabelOut, Noop, TestAndSet, SortArray, Constant, EvalExpr, Animate
 * and the LineChart widget component.
 *
 * The flows are the pages of projects/parts/flow.py.
 */

#include "flow_helpers.h"

#include <eez/flow/components/sort_array.h>
#include <eez/flow/components/line_chart_widget.h>
#include <eez/flow/watch_list.h>

using namespace eez;
using namespace eez::gui;
using namespace eez::flow;
using namespace flowtest;

////////////////////////////////////////////////////////////////////////////////
// Start, sequence

TEST("flow/start: a page flow runs from its Start component when the flow state is created") {
    runPage(PAGE_ID_FLOW_SEQ);
    CHECK_LOGGED("seq 1");
}

TEST("flow/start: components run in the order of the sequence lines") {
    runPage(PAGE_ID_FLOW_SEQ);
    int i1 = logIndex("seq 1"), i2 = logIndex("seq 2"), i3 = logIndex("seq 3");
    CHECK(i1 != -1 && i2 != -1 && i3 != -1);
    CHECK(i1 < i2 && i2 < i3);
}

TEST("flow/start: a component without a connected sequence input runs when the flow state is created") {
    runPage(PAGE_ID_FLOW_SEQ);
    CHECK_LOGGED("seq free");
    CHECK_EQ(countLogged("seq free"), 1);
}

TEST("flow/start: a chain of components runs in one frame") {
    pageFlowState(PAGE_ID_FLOW_SEQ);
    harness::frame(1);
    CHECK_LOGGED("seq 3");
}

TEST("flow/start: the flow state of a page is created once and the flow runs once") {
    auto a = runPage(PAGE_ID_FLOW_SEQ);
    auto b = runPage(PAGE_ID_FLOW_SEQ);
    CHECK(a == b);
    CHECK_EQ(countLogged("seq 1"), 1);
}

TEST("flow/start: an output wired to two components queues them in line order") {
    runPage(PAGE_ID_FLOW_FANOUT);
    int a = logIndex("fan a"), b = logIndex("fan b");
    CHECK(a != -1 && b != -1);
    CHECK(a < b);
}

////////////////////////////////////////////////////////////////////////////////
// SetVariable

TEST("flow/set_variable: assigns a global, a local, an array element and a struct field in entry order") {
    runPage(PAGE_ID_FLOW_SETVAR);
    CHECK_LOGGED("set 42 43 99 7");
}

TEST("flow/set_variable: the global variable keeps the assigned value") {
    runPage(PAGE_ID_FLOW_SETVAR);
    auto v = globalVar("flow_g_int");
    REQUIRE(v != nullptr);
    CHECK_EQ(v->getInt(), 42);
}

TEST("flow/set_variable: an array index out of bounds is a flow error and the flow stops there") {
    runPage(PAGE_ID_FLOW_SETVAR_OOB);
    CHECK_EQ((int)harness::flowErrors().size(), 1);
    CHECK(errorContains("SetVariable"));
    CHECK_NOT_LOGGED("oob after");
}

TEST("flow/set_variable: the error of an assignable expression says why it failed") {
    // the description ("Array element index out of bounds") is in the message
    // (F8: it was restored away before it was used)
    runPage(PAGE_ID_FLOW_SETVAR_OOB);
    CHECK(errorContains("out of bounds"));
}

////////////////////////////////////////////////////////////////////////////////
// EvalExpr, Constant

TEST("flow/eval_expr: the result output gets the value of the expression") {
    runPage(PAGE_ID_FLOW_CONST);
    CHECK_LOGGED("eval 42");
}

TEST("flow/constant: the value output gets the constant") {
    runPage(PAGE_ID_FLOW_CONST);
    CHECK_LOGGED("const ok");
}

TEST("flow/eval_expr: a failing expression is a flow error and the result output is not used") {
    runPage(PAGE_ID_FLOW_EVAL_OOB);
    CHECK_EQ((int)harness::flowErrors().size(), 1);
    CHECK(errorContains("Failed to evaluate 'Expression'"));
    CHECK_EQ(logIndexPrefix("eval oob"), -1);
}

TEST("flow/eval_expr: the error of a failing expression says why it failed") {
    runPage(PAGE_ID_FLOW_EVAL_OOB);
    CHECK(errorContains("Array element index out of bounds"));
}

////////////////////////////////////////////////////////////////////////////////
// Switch, Compare, IsTrue

TEST("flow/switch: the first true case fires its output with its value") {
    runPage(PAGE_ID_FLOW_SWITCH);
    CHECK_LOGGED("switch mid");
    CHECK_NOT_LOGGED("switch big");
    CHECK_NOT_LOGGED("switch small");
}

TEST("flow/switch: the sequence output fires after the case output") {
    runPage(PAGE_ID_FLOW_SWITCH);
    int value = logIndex("switch mid"), done = logIndex("switch done");
    CHECK(value != -1 && done != -1);
    CHECK(value < done);
}

TEST("flow/switch: without a true case only the sequence output fires") {
    runPage(PAGE_ID_FLOW_SWITCH_NONE);
    CHECK_LOGGED("switch none done");
    CHECK_EQ(logIndexPrefix("none "), -1);
}

TEST("flow/compare: less, between and equal take the True or False output") {
    runPage(PAGE_ID_FLOW_COMPARE);
    CHECK_LOGGED("lt true");
    CHECK_NOT_LOGGED("lt false");
    CHECK_LOGGED("between true");
    CHECK_NOT_LOGGED("between false");
    CHECK_LOGGED("eq false");
    CHECK_NOT_LOGGED("eq true");
}

TEST("flow/is_true: takes the True or False output, then the sequence output") {
    runPage(PAGE_ID_FLOW_ISTRUE);
    CHECK_LOGGED("t1 false");
    CHECK_NOT_LOGGED("t1 true");
    CHECK_LOGGED("t2 true");
    CHECK_NOT_LOGGED("t2 false");
    CHECK_LOGGED("istrue done");
}

////////////////////////////////////////////////////////////////////////////////
// Loop, Counter

TEST("flow/loop: counts from 1 to 3 inclusive, then the done output") {
    runPage(PAGE_ID_FLOW_LOOP);
    int i1 = logIndex("loop 1"), i2 = logIndex("loop 2"), i3 = logIndex("loop 3"), done = logIndex("loop done");
    CHECK(i1 != -1 && i2 != -1 && i3 != -1 && done != -1);
    CHECK(i1 < i2 && i2 < i3 && i3 < done);
    CHECK_NOT_LOGGED("loop 4");
    CHECK_NOT_LOGGED("loop 0");
}

TEST("flow/loop: a negative step counts down") {
    runPage(PAGE_ID_FLOW_LOOP_DOWN);
    int i3 = logIndex("down 3"), i2 = logIndex("down 2"), i1 = logIndex("down 1"), done = logIndex("down done");
    CHECK(i3 != -1 && i2 != -1 && i1 != -1 && done != -1);
    CHECK(i3 < i2 && i2 < i1 && i1 < done);
    CHECK_NOT_LOGGED("down 0");
}

TEST("flow/counter: fires the sequence output count times, then done") {
    runPage(PAGE_ID_FLOW_COUNTER);
    CHECK_EQ(countLogged("count"), 3);
    CHECK_LOGGED("count done");
}

////////////////////////////////////////////////////////////////////////////////
// Delay

TEST("flow/delay: continues after the delay on the fake clock") {
    pageFlowState(PAGE_ID_FLOW_DELAY);
    harness::frame(1);
    CHECK_LOGGED("delay start");
    // 16 ms per frame: 80 ms after the start
    harness::frame(5);
    CHECK_NOT_LOGGED("delay done");
    harness::frame(2);
    CHECK_LOGGED("delay done");
}

TEST("flow/delay: a delay that is not a number is a flow error") {
    runPage(PAGE_ID_FLOW_DELAY_NAN);
    CHECK(errorContains("Invalid 'Milliseconds' value in 'Delay'"));
    CHECK_NOT_LOGGED("nan delay done");
}

TEST("flow/delay: a pending delay keeps its action flow state alive") {
    auto flowState = pageFlowState(PAGE_ID_FLOW_DELAY_ACTION);
    harness::frame(1);
    CHECK_EQ(childCount(flowState), 1);
    harness::frame(5);
    CHECK_LOGGED("act delay done");
    CHECK_LOGGED("delay action after");
    CHECK_EQ(childCount(flowState), 0);
}

TEST_BUG("flow/delay: a delay across the 32-bit millisecond wrap waits", "O15") {
    // millis() + 100 wraps to a small number, so millis() >= waitUntil holds
    // at once
    harness::advance(0xFFFFFFFFu - harness::now() - 40);
    pageFlowState(PAGE_ID_FLOW_DELAY);
    harness::frame(3);
    CHECK_LOGGED("delay start");
    CHECK_NOT_LOGGED("delay done");
}

////////////////////////////////////////////////////////////////////////////////
// Watch

TEST("flow/watch: fires with the current value when it starts") {
    runPage(PAGE_ID_FLOW_WATCH);
    CHECK_LOGGED("watch 0");
}

TEST("flow/watch: fires again when the value changes, once per change") {
    runPage(PAGE_ID_FLOW_WATCH);
    auto v = globalVar("flow_g_watch");
    REQUIRE(v != nullptr);
    *v = Value(5, VALUE_TYPE_INT32);
    harness::frame(3);
    CHECK_EQ(countLogged("watch 5"), 1);
    *v = Value(5, VALUE_TYPE_INT32);
    harness::frame(3);
    CHECK_EQ(countLogged("watch 5"), 1);
    *v = Value(6, VALUE_TYPE_INT32);
    harness::frame(1);
    CHECK_LOGGED("watch 6");
}

TEST_BUG("flow/watch: a flow state in error does not run its Watch components", "F17") {
    // BB3+ sets the error flag on a flow that must not execute anything more;
    // visitWatchList ignores it. The Watch output is wired to the Log input "v":
    // the Log task is dropped by the tick, but the Watch already propagated.
    auto flowState = runPage(PAGE_ID_FLOW_WATCH);
    int logIndexInFlow = -1;
    for (uint32_t i = 0; i < flowState->flow->components.count; i++) {
        if (flowState->flow->components[i]->type == defs_v3::COMPONENT_TYPE_LOG_ACTION) {
            logIndexInFlow = (int)i;
        }
    }
    REQUIRE(logIndexInFlow != -1);
    auto inputIndex = flowState->flow->components[logIndexInFlow]->inputs[0];

    flowState->error = true;
    auto v = globalVar("flow_g_watch");
    REQUIRE(v != nullptr);
    *v = Value(9, VALUE_TYPE_INT32);
    harness::frame(2);
    CHECK(flowState->values[inputIndex].getInt() != 9);
}

TEST("flow/watch: a Watch in an action lives only while the action has other work") {
    // visitWatchList frees an action flow state that is kept only by its
    // watches, so a later change is not seen
    auto flowState = runPage(PAGE_ID_FLOW_WATCH_ACTION);
    CHECK_LOGGED("aw 0");
    CHECK_EQ(childCount(flowState), 0);
    CHECK_EQ((int)getWatchListSize(), 0);
    auto v = globalVar("flow_g_watch");
    REQUIRE(v != nullptr);
    *v = Value(5, VALUE_TYPE_INT32);
    harness::frame(2);
    CHECK_NOT_LOGGED("aw 5");
}

////////////////////////////////////////////////////////////////////////////////
// Log

TEST("flow/log: a number is converted to text") {
    runPage(PAGE_ID_FLOW_LOG);
    CHECK_LOGGED("42");
}

TEST("flow/log: an empty message is not logged, the flow continues") {
    runPage(PAGE_ID_FLOW_LOG);
    CHECK_EQ((int)harness::logMessages().size(), 2);
    CHECK_LOGGED("log end");
}

////////////////////////////////////////////////////////////////////////////////
// LabelOut/LabelIn, Noop, TestAndSet

TEST("flow/label: LabelOut continues at the LabelIn with the same label") {
    runPage(PAGE_ID_FLOW_LABEL);
    CHECK_LOGGED("label in");
}

TEST("flow/noop: passes the sequence on") {
    runPage(PAGE_ID_FLOW_NOOP);
    CHECK_LOGGED("noop");
}

TEST("flow/test_and_set: a false variable is set to true and the flow continues") {
    runPage(PAGE_ID_FLOW_TAS);
    CHECK_LOGGED("tas got");
    auto lock = globalVar("flow_g_lock");
    REQUIRE(lock != nullptr);
    CHECK(lock->getBoolean());
}

TEST("flow/test_and_set: a true variable makes the flow wait until it is false") {
    auto lock = globalVar("flow_g_lock");
    REQUIRE(lock != nullptr);
    *lock = Value(true, VALUE_TYPE_BOOLEAN);
    runPage(PAGE_ID_FLOW_TAS, 5);
    CHECK_NOT_LOGGED("tas got");
    *lock = Value(false, VALUE_TYPE_BOOLEAN);
    harness::frame(2);
    CHECK_LOGGED("tas got");
    CHECK(lock->getBoolean());
}

////////////////////////////////////////////////////////////////////////////////
// SortArray

TEST("flow/sort_array: integers ascending and descending") {
    runPage(PAGE_ID_FLOW_SORT);
    CHECK_LOGGED("asc 1,2,3");
    CHECK_LOGGED("desc 3,2,1");
}

TEST("flow/sort_array: strings ignoring case and case sensitive") {
    runPage(PAGE_ID_FLOW_SORT);
    CHECK_LOGGED("icase a,b,C");
    CHECK_LOGGED("case C,a,b");
}

TEST("flow/sort_array: structures by a field") {
    runPage(PAGE_ID_FLOW_SORT_STRUCT);
    CHECK_LOGGED("by value yzx");
}

TEST("flow/sort_array: a value that is not an array is a flow error") {
    runPage(PAGE_ID_FLOW_SORT_BAD);
    CHECK(errorContains("SortArray: not an array"));
    CHECK_NOT_LOGGED("sort bad result");
}

TEST_BUG("flow/sort_array: integers above 2^24 are compared exactly", "O15") {
    // the comparator converts to float: 16777217 and 16777216 compare equal
    runPage(PAGE_ID_FLOW_SORT);
    CHECK_LOGGED("big 16777216,16777217");
}

TEST_BUG("flow/sort_array: an invalid struct field index stops the component", "O15") {
    // no return after the error: the unsorted array still goes to the output
    auto flowState = pageFlowState(PAGE_ID_FLOW_SORT_STRUCT);
    int index = findComponent(flowState, defs_v3::COMPONENT_TYPE_SORT_ARRAY_ACTION);
    REQUIRE(index != -1);
    auto component = (SortArrayActionComponent *)flowState->flow->components[index];
    component->structFieldIndex = -1;
    harness::frame(3);
    CHECK(errorContains("invalid struct field index"));
    CHECK_EQ(logIndexPrefix("by value"), -1);
}

TEST_BUG("flow/sort_array: the sequence output fires after the result", "NEW-flow-2") {
    // executeSortArrayComponent only propagates the result output
    runPage(PAGE_ID_FLOW_SORT_SEQ);
    CHECK_LOGGED("sort result 1,2");
    CHECK_LOGGED("sort seqout");
}

TEST_BUG("flow/sort_array: with catchError the sorted array still goes to the result output", "NEW-flow-3") {
    // the result is sent to outputs.count - 1, which is @error when catchError
    // adds that output
    runPage(PAGE_ID_FLOW_SORT_CATCH);
    CHECK_LOGGED("sort catch result 1,2");
    CHECK_NOT_LOGGED("sort catch error");
}

////////////////////////////////////////////////////////////////////////////////
// Animate

TEST("flow/animate: moves the page timeline from 0 to 1 at speed 1 in one second") {
    auto flowState = pageFlowState(PAGE_ID_FLOW_ANIMATE);
    harness::frame(30);
    CHECK_NOT_LOGGED("anim done");
    CHECK(flowState->timelinePosition > 0.3f && flowState->timelinePosition < 0.7f);
    harness::frame(40);
    CHECK_LOGGED("anim done");
    CHECK_NEAR(flowState->timelinePosition, 1.0, 1e-6);
}

TEST_BUG("flow/animate: a negative speed ends the animation or is an error", "O15") {
    // the position moves away from "to" and the component never ends
    runPage(PAGE_ID_FLOW_ANIMATE_NEG, 200);
    CHECK(logged("anim neg done") || !harness::flowErrors().empty());
}

////////////////////////////////////////////////////////////////////////////////
// LineChart widget component

namespace {

LineChartWidgetComponenent *lineChartComponent(FlowState *flowState, int &index) {
    index = findComponent(flowState, defs_v3::COMPONENT_TYPE_LINE_CHART_EMBEDDED_WIDGET);
    return index == -1 ? nullptr : (LineChartWidgetComponenent *)flowState->flow->components[index];
}

} // namespace

TEST("flow/line_chart: a value on the input adds a point") {
    auto flowState = pageFlowState(PAGE_ID_FLOW_LINECHART);
    int index;
    auto component = lineChartComponent(flowState, index);
    REQUIRE(component != nullptr);
    CHECK_EQ((int)component->maxPoints, 40);
    harness::frame(2);
    auto state = (LineChartWidgetComponenentExecutionState *)flowState->componenentExecutionStates[index];
    REQUIRE(state != nullptr);
    CHECK_EQ((int)state->numPoints, 1);
    CHECK_NEAR(state->getY(0, 0), 5.0, 1e-6);
    CHECK(harness::flowErrors().empty());
}

TEST_BUG("flow/line_chart: maxPoints 0 is rejected", "O15") {
    // numPoints < maxPoints is false and the ring index is taken % 0
    auto flowState = pageFlowState(PAGE_ID_FLOW_LINECHART);
    int index;
    auto component = lineChartComponent(flowState, index);
    REQUIRE(component != nullptr);
    component->maxPoints = 0;
    harness::frame(2);
    CHECK(!harness::flowErrors().empty());
}
