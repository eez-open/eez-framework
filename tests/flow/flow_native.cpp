/*
 * eez-framework tests: the native actions of projects/parts/flow.py.
 */

#include "flow_helpers.h"

using namespace eez;
using namespace eez::flow;

namespace flowtest {

AsyncAction *g_asyncAction;
AsyncAction *g_asyncOutAction;
int g_getPropCalls;
int g_syncCalls;
Value g_getPropValue;

} // namespace flowtest

namespace eez {
namespace gui {

// user properties: a (integer), out (assignable integer); out = a * 3
void action_flow_native_props() {
    auto a = flow::getUserProperty(0);
    flow::setUserProperty(1, Value(a.getInt() * 3, VALUE_TYPE_INT32));
}

// starts an async operation, the test ends it
void action_flow_native_async() {
    flowtest::g_asyncAction = flow::beginAsyncExecution();
}

// user properties: res (assignable, a flow output of the CallAction); the test
// sets it and ends the async operation
void action_flow_native_async_out() {
    flowtest::g_asyncOutAction = flow::beginAsyncExecution();
}

// counts its calls (and makes its CallAction the "executing action")
void action_flow_native_sync() {
    flowtest::g_syncCalls++;
}

// reads user property 0 of the calling CallAction
void action_flow_native_getprop() {
    flowtest::g_getPropCalls++;
    flowtest::g_getPropValue = flow::getUserProperty(0);
}

} // namespace gui
} // namespace eez
