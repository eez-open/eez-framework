/*
 * eez-framework tests: helpers of the flow tests (tests/flow).
 *
 * The flows are pages and actions of projects/parts/flow.py. A test creates the
 * flow state of a page (pageFlowState, runPage) or shows the page, runs frames
 * (harness::frame ticks the flow) and checks what the Log components wrote
 * (harness::logMessages()), the flow errors and the flow states.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/core/value.h>
#include <eez/gui/gui.h>
#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include <eez/flow/hooks.h>
#include <eez/flow/queue.h>
#include <eez/flow/flow_defs_v3.h>

#include "document.h"
#include "harness.h"
#include "test.h"

namespace flowtest {

using eez::Value;
using eez::flow::FlowState;

// flow state of a main assets page, created if it doesn't exist yet (the page
// is not shown)
FlowState *pageFlowState(int pageId);

// creates the page flow state and runs the frames
FlowState *runPage(int pageId, int frames = 3);

// shows the page and runs the frames
void showPage(int pageId, int frames = 3);

// Log component messages
bool logged(const std::string &message);
int countLogged(const std::string &message);
// index of the first message equal to message, -1 if none
int logIndex(const std::string &message);
// index of the first message that starts with prefix, -1 if none
int logIndexPrefix(const std::string &prefix);
// all messages, for the failure output
std::string logs();

// flow errors (onFlowErrorHook)
bool errorContains(const std::string &text);
std::string errors();

// flow states
int childCount(FlowState *flowState);
int topLevelCount();
uint32_t numFlows();

// index of the n-th component of this type in the flow, -1 if none. Studio
// numbers the components in its own order, so a flow searched this way should
// have only one component of that type.
int findComponent(FlowState *flowState, uint16_t type, int nth = 0);

// global variable of flow.py (flow_g_*), through the flow_vars page
Value *globalVar(const char *name);

// widget of a page asset, by type (the first one)
const eez::gui::Widget *findWidget(int pageId, uint16_t type);

// native actions (flow_native.cpp)
extern eez::flow::AsyncAction *g_asyncAction;
extern eez::flow::AsyncAction *g_asyncOutAction;
extern int g_getPropCalls;
extern int g_syncCalls;
extern Value g_getPropValue;

} // namespace flowtest

#define CHECK_LOGGED(MESSAGE) \
    do { \
        if (!flowtest::logged(MESSAGE)) { \
            test::fail(__FILE__, __LINE__, std::string("not logged: \"") + (MESSAGE) + "\", log: " + flowtest::logs()); \
        } \
    } while (0)

#define CHECK_NOT_LOGGED(MESSAGE) \
    do { \
        if (flowtest::logged(MESSAGE)) { \
            test::fail(__FILE__, __LINE__, std::string("logged: \"") + (MESSAGE) + "\", log: " + flowtest::logs()); \
        } \
    } while (0)
