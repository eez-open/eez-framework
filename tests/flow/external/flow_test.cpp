/*
 * eez-framework tests: flows of external assets (flow::start/stop for a module,
 * actions and errors in external flows, the BB3+ setExternalFlowError and
 * stopExternalFlow, Flow functions in external flows).
 */

#include <string.h>

#include <string>

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

const int MOD_MAIN_INDEX = fwtest_ext::PAGE_ID_MOD_MAIN - 1;

// buttons of mod_main and mod_catch (150x50)
const int INC_X = 85, INC_Y = 175;
const int FAIL_X = 245, FAIL_Y = 175;

void click(int x, int y) {
    harness::touch(x, y);
    harness::frame(3);
    harness::release();
    harness::frame(3);
}

int showPage(Assets *assets, int extPageId) {
    int pageId = ext::pageId(assets, extPageId);
    ext::show(pageId);
    return pageId;
}

Value &local(ext::FlowState *flowState, int localIndex) {
    return flowState->values[flowState->flow->componentInputs.count + localIndex];
}

// mod_local is the first local variable of mod_main
const int MOD_LOCAL = 0;

int globalInt(Assets *assets, int index) {
    return ext::getGlobal(assets, index).getInt();
}

bool startsWith(const std::string &s, const char *prefix) {
    return s.compare(0, strlen(prefix), prefix) == 0;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// start and stop

TEST("external/flow: start for external assets keeps the main flow running") {
    auto mainPageFlowState = ext::topLevelFlowState(g_mainAssets, PAGE_ID_MAIN - 1);
    REQUIRE(mainPageFlowState != nullptr);
    ext::setup();
    harness::frame(2);
    CHECK(!flow::isFlowStopped());
    CHECK(ext::topLevelFlowState(g_mainAssets, PAGE_ID_MAIN - 1) == mainPageFlowState);
}

TEST("external/flow: external global variables are in the external assets memory") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    ext::setGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER, Value(41, VALUE_TYPE_INT32));
    Value result;
    REQUIRE(ext::probe(ext::topLevelFlowState(assets, MOD_MAIN_INDEX), "counter", result));
    CHECK_EQ(result.getInt(), 41);
    // the value in the assets blob
    CHECK_EQ(assets->flowDefinition->globalVariables[fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER]->getInt(), 41);
}

TEST("external/flow: stop(assets) deletes the top level flow states of the assets in the next tick") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    harness::appContext()->popPage();
    harness::frame(2);
    REQUIRE(ext::countFlowStates(assets, false) == 1);
    flow::stop(assets);
    CHECK_EQ(ext::countFlowStates(assets, false), 1);
    harness::frame(1);
    CHECK_EQ(ext::countFlowStates(assets, true), 0);
    CHECK(!flow::isFlowStopped());
    CHECK(ext::topLevelFlowState(g_mainAssets, PAGE_ID_MAIN - 1) != nullptr);
}

TEST("external/flow: stop(assets) does not touch the flow states of other external assets") {
    Assets *a = ext::setup();
    Assets *b = ext::setup();
    showPage(a, fwtest_ext::PAGE_ID_MOD_MAIN);
    showPage(b, fwtest_ext::PAGE_ID_MOD_MAIN);
    harness::appContext()->popPage();
    harness::appContext()->popPage();
    harness::frame(2);
    flow::stop(a);
    harness::frame(2);
    CHECK_EQ(ext::countFlowStates(a, true), 0);
    CHECK_EQ(ext::countFlowStates(b, true), 1);
}

TEST("external/flow: stopping the main flow deletes the external flow states too") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    harness::appContext()->popPage();
    harness::frame(2);
    flow::stop(g_mainAssets);
    harness::frame(2);
    CHECK(flow::isFlowStopped());
    CHECK_EQ(ext::countFlowStates(assets, true), 0);
    CHECK(flow::g_firstFlowState == nullptr);
}

TEST("external/flow: after a restart the page flow state is new") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    local(ext::topLevelFlowState(assets, MOD_MAIN_INDEX), MOD_LOCAL) = Value(99, VALUE_TYPE_INT32);
    harness::appContext()->popPage();
    flow::stop(assets);
    harness::frame(2);
    REQUIRE(ext::countFlowStates(assets, true) == 0);
    CHECK_EQ(flow::start(assets), 1u);
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    Value result;
    REQUIRE(ext::probe(ext::topLevelFlowState(assets, MOD_MAIN_INDEX), "local", result));
    CHECK_EQ(result.getInt(), 7);
}

TEST_BUG("external/flow: after a restart the external global variables have their initial values", "F18") {
    // the global variables of external assets live in the assets blob and
    // start() does not initialize them (initGlobalVariables returns at once)
    Assets *assets = ext::setup();
    ext::setGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER, Value(42, VALUE_TYPE_INT32));
    flow::stop(assets);
    harness::frame(2);
    flow::start(assets);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER), 0);
}

TEST("external/flow: stop(nullptr) for unloaded script assets does not stop the main flow") {
    // BB3+ stopFlowScript: unloadScriptExternalAssets() frees the assets and
    // sets g_scriptExternalAssets to nullptr, then flow::stop(g_scriptExternalAssets):
    // stop(nullptr) does nothing (F4)
    Assets *scriptAssets = ext::setup();
    ext::unload(scriptAssets);
    scriptAssets = nullptr;
    flow::stop(scriptAssets);
    harness::frame(2);
    CHECK(!flow::isFlowStopped());
}

TEST("external/flow: start with assets without a flow definition returns 0") {
    Assets *assets = ext::load();
    assets->flowDefinition = nullptr;
    CHECK_EQ(flow::start(assets), 0u);
}

TEST("external/flow: starting external assets does not cancel a stop of the main flow") {
    Assets *assets = ext::load();
    flow::stop(g_mainAssets);
    flow::start(assets);
    harness::frame(2);
    CHECK(flow::isFlowStopped());
}

TEST("external/flow: starting external assets does not restart a stopped engine") {
    Assets *assets = ext::load();
    flow::stop(g_mainAssets);
    harness::frame(2);
    REQUIRE(flow::isFlowStopped());
    flow::start(assets);
    CHECK(flow::isFlowStopped());
}

TEST_BUG("external/flow: deletePageFlowState of a main page does not delete an external page flow state", "FK1") {
    // deletePageFlowState ignores its assets argument and marks the first flow
    // state with the page index (used only by the LVGL API)
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    harness::appContext()->popPage();
    harness::frame(2);
    // PAGE_ID_NUMERIC_KEYPAD - 1 == MOD_MAIN_INDEX, the main page has no flow state
    REQUIRE(PAGE_ID_NUMERIC_KEYPAD - 1 == MOD_MAIN_INDEX);
    flow::deletePageFlowState(g_mainAssets, MOD_MAIN_INDEX);
    harness::frame(2);
    CHECK(ext::topLevelFlowState(assets, MOD_MAIN_INDEX) != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// actions and errors

TEST("external/flow: an action of an external page runs in the external flow") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    // the second click at the same place is not an accidental double tap after
    // the page change
    harness::advance(400);
    click(INC_X, INC_Y);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER), 1);
    click(INC_X, INC_Y);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER), 2);
    // the action flow states are freed, the page flow state stays
    CHECK_EQ(ext::countFlowStates(assets, true), 1);
    CHECK(harness::flowErrors().empty());
}

TEST("external/flow: an error in an external action reaches onFlowErrorHook with the external flow state") {
    Assets *assets = ext::setup();
    ext::recordErrors();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    click(FAIL_X, FAIL_Y);
    REQUIRE(ext::errors().size() == 1);
    CHECK(ext::errors()[0].assets == assets);
    CHECK(startsWith(ext::errors()[0].message, "mod failure"));
}

TEST("external/flow: an uncaught error in an external flow also calls stopScriptHook") {
    // the framework calls stopScriptHook after every uncaught error, the BB3+
    // ignores the call after an error of a module flow (src/bb3/flow/flow.cpp)
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    click(FAIL_X, FAIL_Y);
    CHECK_EQ(harness::stopScriptCount(), 1);
}

TEST("external/flow: Catch Error of an external page catches the error of an external action") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_CATCH);
    click(FAIL_X, FAIL_Y);
    harness::frame(2);
    Value caught = ext::getGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_CAUGHT);
    REQUIRE(caught.isString());
    CHECK(startsWith(caught.getString(), "mod failure"));
    CHECK(harness::flowErrors().empty());
    CHECK_EQ(harness::stopScriptCount(), 0);
}

TEST("external/flow: an error in an external page flow") {
    Assets *assets = ext::setup();
    ext::recordErrors();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_ERR_PAGE);
    REQUIRE(ext::errors().size() == 1);
    CHECK(ext::errors()[0].assets == assets);
    CHECK_EQ(ext::errors()[0].flowIndex, (int)fwtest_ext::PAGE_ID_MOD_ERR_PAGE - 1);
    CHECK(startsWith(ext::errors()[0].message, "page failure"));
}

TEST("external/flow: setExternalFlowError drops the queued tasks of the external flow states") {
    // BB3+ Module::onFlowError: the module flow executes nothing more
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    REQUIRE(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_OPENED) == 1);
    harness::appContext()->popPage();
    harness::frame(2);
    ext::setExternalFlowError(assets);
    // the page open event is queued, the task is dropped
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_OPENED), 1);
}

TEST_BUG("external/flow: after setExternalFlowError a click does not run an external action", "F17") {
    // executeFlowAction ignores the error flag of the page flow state: the
    // action gets a new flow state without the flag
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    ext::setExternalFlowError(assets);
    click(INC_X, INC_Y);
    CHECK_EQ(globalInt(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER), 0);
}

TEST_BUG("external/flow: stop(assets) deletes the flow state of a shown external page", "NEW-external-2") {
    // stop() sets deleteOnNextTick, but a frame renders (display::update)
    // before the flow tick and getPageFlowState of the shown page clears the
    // flag again: the old flow state stays (a new one has mod_local 7)
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    local(ext::topLevelFlowState(assets, MOD_MAIN_INDEX), MOD_LOCAL) = Value(99, VALUE_TYPE_INT32);
    flow::stop(assets);
    harness::frame(2);
    auto flowState = ext::topLevelFlowState(assets, MOD_MAIN_INDEX);
    if (flowState) {
        CHECK_EQ(local(flowState, MOD_LOCAL).getInt(), 7);
    }
}

TEST("external/flow: stopExternalFlow after the pages are removed deletes the flow states") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    harness::appContext()->popPage();
    ext::stopExternalFlow(assets);
    harness::frame(2);
    CHECK_EQ(ext::countFlowStates(assets, true), 0);
    CHECK(!flow::isFlowStopped());
}

////////////////////////////////////////////////////////////////////////////////
// Flow functions and components that depend on the assets

TEST("external/flow: Flow.isPageActive in an active main page") {
    ext::setup();
    harness::appContext()->pushPage(PAGE_ID_EXT_HOST);
    harness::frame(3);
    Value result;
    REQUIRE(ext::probe(ext::topLevelFlowState(g_mainAssets, PAGE_ID_EXT_HOST - 1), "active", result));
    CHECK(result.getBoolean());
}

TEST("external/flow: Flow.isPageActive in a main page that is not shown") {
    auto flowState = flow::getPageFlowState(g_mainAssets, PAGE_ID_EXT_HOST - 1, WidgetCursor());
    Value result;
    REQUIRE(ext::probe(flowState, "active", result));
    CHECK(!result.getBoolean());
}

TEST_BUG("external/flow: Flow.isPageActive in an active external page", "O14") {
    // the operation looks for -(pageIndex + 1), the BB3+ pushes
    // -(((i + 1) << 16) | pageId)
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    Value result;
    REQUIRE(ext::probe(ext::topLevelFlowState(assets, MOD_MAIN_INDEX), "active", result));
    CHECK(result.getBoolean());
}

TEST_BUG("external/flow: Flow.getBitmapIndex in an external flow finds an external bitmap", "O14") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    Value result;
    REQUIRE(ext::probe(ext::topLevelFlowState(assets, MOD_MAIN_INDEX), "bitmap", result));
    CHECK(result.getInt() != 0);
}

TEST_BUG("external/flow: End in an external page flow does not stop the script", "O14") {
    Assets *assets = ext::setup();
    showPage(assets, fwtest_ext::PAGE_ID_MOD_END);
    CHECK_EQ(harness::stopScriptCount(), 0);
}
