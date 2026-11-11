/*
 * eez-framework tests: the time budget of the flow tick (flow/flow.cpp tick,
 * EEZ_FLOW_TICK_MAX_DURATION_MS, 5 ms in the tests).
 *
 * The Log components of the "flow_loop" page (Start -> Loop of 3 -> Log body,
 * Log done: 9 tasks, 4 of them Log) are replaced with a component that moves
 * the fake clock, so the tasks take time.
 */

#include "flow_helpers.h"

#include <eez/flow/components.h>

#include <stdio.h>

using namespace eez;
using namespace eez::gui;
using namespace eez::flow;
using namespace flowtest;

namespace {

int g_slowCalls;
uint32_t g_slowMs;

void slowComponent(FlowState *flowState, unsigned componentIndex) {
    g_slowCalls++;
    harness::advance(g_slowMs);
    propagateValueThroughSeqout(flowState, componentIndex);
}

// runs the loop page with Log tasks of ms each, returns the number of Log
// tasks of every tick until all 4 ran
std::vector<int> slowLogsPerTick(uint32_t ms) {
    registerComponent(defs_v3::COMPONENT_TYPE_LOG_ACTION, slowComponent);
    g_slowMs = ms;
    g_slowCalls = 0;
    pageFlowState(PAGE_ID_FLOW_LOOP);

    std::vector<int> result;
    for (int i = 0; i < 20 && g_slowCalls < 4; i++) {
        int calls = g_slowCalls;
        eez::flow::tick();
        result.push_back(g_slowCalls - calls);
    }
    return result;
}

std::string join(const std::vector<int> &v) {
    std::string result;
    for (auto n : v) {
        result += (result.empty() ? "" : ",") + std::to_string(n);
    }
    return result;
}

} // namespace

TEST("flow/tick: a task that uses up the time budget ends the tick") {
    unsigned counter = getTickMaxDurationCounter();
    uint32_t start = harness::now();
    auto perTick = slowLogsPerTick(5);
    printf("5 ms tasks: Log tasks per tick %s\n", join(perTick).c_str());
    // one 5 ms task per tick
    CHECK_EQ(join(perTick), std::string("1,1,1,1"));
    CHECK_EQ(getTickMaxDurationCounter(), counter + 4);
    CHECK_EQ(harness::now() - start, 20u);
}

TEST("flow/tick: the time budget is checked after every task") {
    unsigned counter = getTickMaxDurationCounter();
    auto perTick = slowLogsPerTick(2);
    printf("2 ms tasks: Log tasks per tick %s\n", join(perTick).c_str());
    // 2 + 2 + 2 ms: the third task ends the first tick at 6 ms
    CHECK_EQ(join(perTick), std::string("3,1"));
    CHECK_EQ(getTickMaxDurationCounter(), counter + 1);
}

TEST("flow/tick: tasks that take no time run in one tick") {
    unsigned counter = getTickMaxDurationCounter();
    auto perTick = slowLogsPerTick(0);
    CHECK_EQ(join(perTick), std::string("4"));
    CHECK_EQ(getTickMaxDurationCounter(), counter);
}
