/*
 * eez-framework tests: user widgets across assets. An external layout inside
 * a main page (the BB3+ slot views: a dynamic user widget whose data is an
 * external page id) and a main layout inside an external page.
 */

#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/flow/flow.h>
#include <eez/flow/private.h>

#include "document.h"
#include "harness.h"
#include "test.h"
#include "ext.h"

using namespace eez;
using namespace eez::gui;

namespace {

const int MOD_UW_INDEX = fwtest_ext::PAGE_ID_MOD_UW - 1;
const int MOD_HOST_INDEX = fwtest_ext::PAGE_ID_MOD_HOST - 1;
const int EXT_UW_MAIN_INDEX = PAGE_ID_EXT_UW_MAIN - 1;

ext::FlowState *extHostFlowState() {
    return flow::getPageFlowState(g_mainAssets, PAGE_ID_EXT_HOST - 1, WidgetCursor());
}

// ext_host (main page) with the external layout mod_uw in its user widget.
// The flow records are cleared at the end, unless clearFlowRecords is false.
Assets *showExternalLayoutInMainPage(bool clearFlowRecords = true) {
    Assets *assets = ext::setup();
    ext::assignProbe(extHostFlowState(), "layout", Value(ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_UW), VALUE_TYPE_INT32));
    harness::appContext()->pushPage(PAGE_ID_EXT_HOST);
    harness::frame(3);
    if (clearFlowRecords) {
        harness::clearFlowRecords();
    }
    return assets;
}

// mod_host (external page) with the main layout ext_uw_main in its user widget
Assets *showMainLayoutInExternalPage(bool clearFlowRecords = true) {
    Assets *assets = ext::setup();
    ext::setGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_MAIN_LAYOUT, Value((int)PAGE_ID_EXT_UW_MAIN, VALUE_TYPE_INT32));
    ext::show(ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_HOST));
    if (clearFlowRecords) {
        harness::clearFlowRecords();
    }
    return assets;
}

int globalInt(Assets *assets, int index) {
    return ext::getGlobal(assets, index).getInt();
}

void setHostLayout(int layoutId) {
    ext::assignProbe(extHostFlowState(), "layout", Value(layoutId, VALUE_TYPE_INT32));
    harness::frame(2);
}

// probe_result, the second local variable of mod_uw
Value &modUwLocal(ext::FlowState *flowState) {
    return flowState->values[flowState->flow->componentInputs.count + 1];
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// an external layout in a main page

TEST("external/user widgets: an external layout in a main page is drawn with the external styles") {
    showExternalLayoutInMainPage();
    // mod_uw/box 0,0 100x50: mod_fill_box
    CHECK_EQ(ext::pixelRgb(50, 25), ext::MOD_FILL);
}

TEST("external/user widgets: a main widget after an external layout") {
    showExternalLayoutInMainPage();
    // ext_host/after 300,0 100x50: ext_m_box (positive style id)
    CHECK_EQ(ext::pixelRgb(350, 25), ext::ACCENT);
}

TEST("external/user widgets: the flow state of an external layout is a child of the main page flow state") {
    Assets *assets = showExternalLayoutInMainPage();
    auto flowState = ext::findFlowState(assets, MOD_UW_INDEX);
    REQUIRE(flowState != nullptr);
    CHECK(flowState->parentFlowState == extHostFlowState());
    CHECK(ext::topLevelFlowState(assets, MOD_UW_INDEX) == nullptr);
    CHECK_EQ(ext::countFlowStates(assets, true), 1);
}

TEST("external/user widgets: the flow of an external layout in a main page runs") {
    Assets *assets = showExternalLayoutInMainPage();
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_UW_STARTED), 1);
    // the Watch fires once at the start, then on every change
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_WATCH_HITS), 1);
    ext::setGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_TRIGGER, Value(3, VALUE_TYPE_INT32));
    harness::frame(2);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_WATCH_HITS), 2);
}

TEST("external/user widgets: stop(assets) deletes the flow state of an external layout in a main page") {
    // stop(assets) marks the flow states of the assets anywhere in the tree
    // (F6: only the top level flow states)
    Assets *assets = showExternalLayoutInMainPage();
    harness::appContext()->popPage();
    harness::frame(2);
    flow::stop(assets);
    harness::frame(2);
    CHECK_EQ(ext::countFlowStates(assets, true), 0);
}

TEST("external/user widgets: after stop(assets) the Watch of an external layout does not run") {
    Assets *assets = showExternalLayoutInMainPage();
    harness::appContext()->popPage();
    harness::frame(2);
    flow::stop(assets);
    harness::frame(2);
    ext::setGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_TRIGGER, Value(2, VALUE_TYPE_INT32));
    harness::frame(2);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_WATCH_HITS), 1);
}

TEST("external/user widgets: the components after a Watch of a flow state in error do not run") {
    // the Watch propagates its value, the queued task is dropped
    Assets *assets = showExternalLayoutInMainPage();
    auto flowState = ext::findFlowState(assets, MOD_UW_INDEX);
    REQUIRE(flowState != nullptr);
    flowState->error = true;
    ext::setGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_TRIGGER, Value(2, VALUE_TYPE_INT32));
    harness::frame(2);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_WATCH_HITS), 1);
}

TEST_BUG("external/user widgets: a Watch of a flow state in error is not evaluated", "F17") {
    // visitWatchList does not check the error flag: the watched expression
    // ([0, 1, 2, 3][mod_trigger]) fails and a new flow error is reported
    Assets *assets = showExternalLayoutInMainPage();
    auto flowState = ext::findFlowState(assets, MOD_UW_INDEX);
    REQUIRE(flowState != nullptr);
    flowState->error = true;
    ext::setGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_TRIGGER, Value(9, VALUE_TYPE_INT32));
    harness::frame(2);
    ext::printFlowErrors();
    CHECK(harness::flowErrors().empty());
}

TEST("external/user widgets: a button of an external layout in a main page runs an external action") {
    Assets *assets = showExternalLayoutInMainPage();
    // mod_uw/inc 100,50 100x50
    harness::touch(150, 75);
    harness::frame(3);
    harness::release();
    harness::frame(3);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER), 1);
    CHECK(harness::flowErrors().empty());
}

TEST("external/user widgets: an external layout with a Start component in a main page reports no flow error") {
    // the dynamic user widget component (no user widget page: inputsStartIndex
    // 0, no inputs) executes in the first tick after the render created the
    // layout flow state, it passes no input values to the layout
    // (NEW-external-3: it passed the Start of the layout to getCallActionValue,
    // "Invalid input index in Input")
    showExternalLayoutInMainPage(false);
    ext::printFlowErrors();
    CHECK(harness::flowErrors().empty());
}

TEST("external/user widgets: a main layout with a Start component in an external page reports no flow error") {
    showMainLayoutInExternalPage(false);
    ext::printFlowErrors();
    CHECK(harness::flowErrors().empty());
}

TEST("external/user widgets: the layouts of two external assets in one user widget have their own flow states") {
    Assets *a = showExternalLayoutInMainPage();
    Assets *b = ext::setup();
    auto flowStateA = ext::findFlowState(a, MOD_UW_INDEX);
    REQUIRE(flowStateA != nullptr);
    setHostLayout(ext::pageId(b, fwtest_ext::PAGE_ID_MOD_UW));
    auto flowStateB = ext::findFlowState(b, MOD_UW_INDEX);
    REQUIRE(flowStateB != nullptr);
    CHECK(flowStateB != flowStateA);
    CHECK_EQ(globalInt(b, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_UW_STARTED), 1);
    // the layout of a is kept (the user widget keeps the other layouts)
    CHECK(ext::findFlowState(a, MOD_UW_INDEX) == flowStateA);
    setHostLayout(ext::pageId(a, fwtest_ext::PAGE_ID_MOD_UW));
    CHECK(ext::findFlowState(a, MOD_UW_INDEX) == flowStateA);
    CHECK_EQ(globalInt(a, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_UW_STARTED), 1);
}

TEST("external/user widgets: a user widget that showed the layouts of ten external assets keeps the most recent ones") {
    // each restart of a module allocates new assets, a new key of the user
    // widget cache: at the 10th the least recent one is freed, only that one
    // (F3)
    std::vector<Assets *> all;
    for (int i = 0; i < 10; i++) {
        all.push_back(ext::setup());
    }
    harness::appContext()->pushPage(PAGE_ID_EXT_HOST);
    harness::frame(2);
    for (auto assets : all) {
        setHostLayout(ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_UW));
    }
    // the current layout and the 8 most recent others
    int total = 0;
    for (auto assets : all) {
        total += ext::countFlowStates(assets, true);
    }
    CHECK_EQ(total, 9);
    CHECK_EQ(ext::countFlowStates(all[0], true), 0);
    CHECK(ext::findFlowState(all[9], MOD_UW_INDEX) != nullptr);
}

TEST("external/user widgets: an external layout cached during a stop starts again after the restart") {
    // BB3+: after a module flow error the slot view shows another layout, the
    // module layout flow state goes to the cache of the user widget;
    // stop(assets) deletes it, so the layout starts again after the restart
    // (F6: the cached flow state came back with its old values)
    Assets *assets = showExternalLayoutInMainPage();
    auto flowState = ext::findFlowState(assets, MOD_UW_INDEX);
    REQUIRE(flowState != nullptr);
    modUwLocal(flowState) = Value(1234, VALUE_TYPE_INT32);
    ext::stopExternalFlow(assets);
    setHostLayout(PAGE_ID_EXT_UW_MAIN);
    flow::start(assets);
    setHostLayout(ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_UW));
    flowState = ext::findFlowState(assets, MOD_UW_INDEX);
    REQUIRE(flowState != nullptr);
    CHECK(modUwLocal(flowState).getType() != VALUE_TYPE_INT32);
}

TEST_BUG("external/user widgets: Flow.isPageActive in an external layout of an active main page", "NEW-external-1") {
    // the page index is the one of the root flow state (the main page), the
    // sign comes from the assets of the evaluating flow state (external):
    // -(index + 1) is not on the stack
    Assets *assets = showExternalLayoutInMainPage();
    Value result;
    REQUIRE(ext::probe(ext::findFlowState(assets, MOD_UW_INDEX), "active", result));
    CHECK(result.getBoolean());
}

////////////////////////////////////////////////////////////////////////////////
// a main layout in an external page

TEST("external/user widgets: a main layout in an external page is drawn with the main styles") {
    showMainLayoutInExternalPage();
    // ext_uw_main/box 0,0 100x50: ext_m_box
    CHECK_EQ(ext::pixelRgb(50, 25), ext::ACCENT);
}

TEST("external/user widgets: the flow state of a main layout is a child of the external page flow state") {
    Assets *assets = showMainLayoutInExternalPage();
    auto hostFlowState = ext::topLevelFlowState(assets, MOD_HOST_INDEX);
    REQUIRE(hostFlowState != nullptr);
    auto flowState = ext::findFlowState(g_mainAssets, EXT_UW_MAIN_INDEX);
    REQUIRE(flowState != nullptr);
    CHECK(flowState->parentFlowState == hostFlowState);
    // its Start component ran in the main flow
    Value result;
    REQUIRE(ext::probe(extHostFlowState(), "uw_started", result));
    CHECK_EQ(result.getInt(), 1);
}

TEST("external/user widgets: an external widget before a main layout") {
    showMainLayoutInExternalPage();
    // mod_host/before 300,100 100x50: mod_fill_box
    CHECK_EQ(ext::pixelRgb(350, 125), ext::MOD_FILL);
}

TEST_BUG("external/user widgets: an external widget after a main layout is drawn with its external style", "W9") {
    // the user widget leaves the main assets in the cursor: getStyle(-n)
    // returns the default style
    showMainLayoutInExternalPage();
    // mod_host/after 300,0 100x50: mod_fill_box
    CHECK_EQ(ext::pixelRgb(350, 25), ext::MOD_FILL);
}

TEST_BUG("external/user widgets: a user widget with a main layout is drawn with its external style", "W9") {
    // updateState switches the cursor to the main assets before render():
    // the own style (mod_uw_frame) resolves to the default style
    showMainLayoutInExternalPage();
    // mod_host/uw 0,0 200x100, the layout covers 0,0 100x50
    CHECK_EQ(ext::pixelRgb(150, 75), ext::MOD_LINE);
}

TEST_BUG("external/user widgets: Flow.isPageActive in a main layout of a closed external page", "NEW-external-1") {
    // mod_host is the first external page: the root page index 0 with the
    // sign of the main assets is PAGE_ID_MAIN, which is on the stack
    Assets *assets = showMainLayoutInExternalPage();
    harness::appContext()->popPage();
    harness::frame(2);
    REQUIRE(ext::topLevelFlowState(assets, MOD_HOST_INDEX) != nullptr);
    Value result;
    REQUIRE(ext::probe(ext::findFlowState(g_mainAssets, EXT_UW_MAIN_INDEX), "active", result));
    CHECK(!result.getBoolean());
}
