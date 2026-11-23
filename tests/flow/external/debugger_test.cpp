/*
 * eez-framework tests: the flow debugger with external assets. BB3+ runs the
 * flows of its GUI and of the modules next to the flows of an app, the
 * debugger of EEZ Studio debugs only the app (flow::setDebuggedAssets).
 */

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/flow/flow.h>
#include <eez/flow/hooks.h>
#include <eez/flow/private.h>
#include <eez/flow/debugger.h>

#include "document.h"
#include "harness.h"
#include "test.h"
#include "ext.h"

using namespace eez;
using namespace eez::gui;

namespace {

const int MOD_MAIN_INDEX = fwtest_ext::PAGE_ID_MOD_MAIN - 1;

// the INC button of mod_main
const int INC_X = 85, INC_Y = 175;

// message types (debugger.cpp MessagesToDebugger)
const int STATE_CHANGED = 0;
const int ADD_TO_QUEUE = 1;
const int REMOVE_FROM_QUEUE = 2;
const int GLOBAL_VARIABLE_INIT = 3;
const int FLOW_STATE_CREATED = 7;
const int PAGE_CHANGED = 12;

const char *STATE_PAUSED = "1";
const char *STATE_STOPPED = "3";

std::string g_output;

void writeDebuggerBuffer(const char *buffer, uint32_t length) {
    g_output.append(buffer, length);
}

void noop() {
}

// the messages the debugger received, each split at the tabs
std::vector<std::vector<std::string>> messages() {
    std::vector<std::vector<std::string>> result;
    size_t start = 0;
    while (start < g_output.size()) {
        size_t end = g_output.find('\n', start);
        if (end == std::string::npos) {
            end = g_output.size();
        }
        std::vector<std::string> fields;
        size_t fieldStart = start;
        for (size_t i = start; i <= end; i++) {
            if (i == end || g_output[i] == '\t') {
                fields.push_back(g_output.substr(fieldStart, i - fieldStart));
                fieldStart = i + 1;
            }
        }
        result.push_back(fields);
        start = end + 1;
    }
    return result;
}

std::vector<std::vector<std::string>> messagesOfType(int type) {
    std::vector<std::vector<std::string>> result;
    for (auto &message : messages()) {
        if (atoi(message[0].c_str()) == type) {
            result.push_back(message);
        }
    }
    return result;
}

// a connected debugger that receives every message, resumed
void connectDebugger() {
    flow::startToDebuggerMessageHook = noop;
    flow::writeDebuggerBufferHook = writeDebuggerBuffer;
    flow::finishToDebuggerMessageHook = noop;
    flow::onDebuggerClientConnected();
}

void resume() {
    char input[] = "0\n";
    flow::processDebuggerInput(input, (uint32_t)strlen(input));
}

void click(int x, int y) {
    harness::touch(x, y);
    harness::frame(3);
    harness::release();
    harness::frame(3);
}

// the app: loaded, debugged, started, its page shown
Assets *startApp() {
    Assets *assets = ext::load();
    flow::setDebuggedAssets(assets);
    REQUIRE(flow::start(assets) == 1u);
    ext::show(ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_MAIN));
    return assets;
}

} // namespace

TEST("external/debugger: with debugged assets the debugger sees only their flows") {
    flow::setDebuggedAssets(nullptr);
    connectDebugger();
    resume();
    harness::frame(2);
    // the main flows run, the debugger sees nothing of them
    CHECK_EQ(messagesOfType(FLOW_STATE_CREATED).size(), 0u);
    CHECK_EQ(messagesOfType(ADD_TO_QUEUE).size(), 0u);

    Assets *assets = startApp();
    harness::advance(400);
    click(INC_X, INC_Y);
    CHECK_EQ(ext::getGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER).getInt(), 1);

    // the global variables of the app
    CHECK_EQ(messagesOfType(GLOBAL_VARIABLE_INIT).size(), (size_t)assets->flowDefinition->globalVariables.count);

    // the flow states of the app only (flow index of the app, not the main page)
    auto mainFlowState = ext::topLevelFlowState(g_mainAssets, PAGE_ID_MAIN - 1);
    REQUIRE(mainFlowState != nullptr);
    std::string mainIndex = std::to_string(mainFlowState->flowStateIndex);
    auto created = messagesOfType(FLOW_STATE_CREATED);
    CHECK(created.size() >= 2); // the page and the action of INC
    for (auto &message : created) {
        CHECK(atoi(message[2].c_str()) < (int)assets->flowDefinition->flows.count);
        CHECK(message[1] != mainIndex);
    }

    // every task the debugger was told about is removed once
    auto added = messagesOfType(ADD_TO_QUEUE);
    CHECK(added.size() > 0);
    for (auto &message : added) {
        CHECK(message[1] != mainIndex);
    }
    CHECK_EQ(messagesOfType(REMOVE_FROM_QUEUE).size(), added.size());

    // the page with its id in the app
    auto pages = messagesOfType(PAGE_CHANGED);
    REQUIRE(pages.size() >= 1);
    CHECK_EQ(pages.back()[1], std::to_string(fwtest_ext::PAGE_ID_MOD_MAIN));
}

TEST("external/debugger: the paused debugger pauses only the debugged flows") {
    flow::setDebuggedAssets(nullptr);
    connectDebugger();
    Assets *assets = startApp();
    CHECK_EQ(messagesOfType(STATE_CHANGED).back()[1], STATE_PAUSED);

    auto mainFlowState = ext::topLevelFlowState(g_mainAssets, PAGE_ID_MAIN - 1);
    auto appFlowState = ext::topLevelFlowState(assets, MOD_MAIN_INDEX);
    REQUIRE(mainFlowState != nullptr);
    REQUIRE(appFlowState != nullptr);
    unsigned componentIndex = 0;
    CHECK(flow::canExecuteStep(mainFlowState, componentIndex));
    CHECK(!flow::canExecuteStep(appFlowState, componentIndex));

    // the action of INC waits for the debugger
    harness::advance(400);
    click(INC_X, INC_Y);
    CHECK_EQ(ext::getGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER).getInt(), 0);
    resume();
    harness::frame(3);
    CHECK_EQ(ext::getGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER).getInt(), 1);
}

TEST("external/debugger: no debugged assets: the debugger is told the flows stopped") {
    flow::setDebuggedAssets(nullptr);
    connectDebugger();
    resume();
    Assets *assets = startApp();
    harness::frame(2);

    flow::stop(assets);
    flow::setDebuggedAssets(nullptr);
    CHECK_EQ(messagesOfType(STATE_CHANGED).back()[1], STATE_STOPPED);

    // nothing more about the deleted flow states of the app
    size_t count = messages().size();
    harness::frame(3);
    CHECK_EQ(messages().size(), count);
}

TEST("external/debugger: a breakpoint is set in a flow of the debugged assets") {
    flow::setDebuggedAssets(nullptr);
    connectDebugger();
    resume();
    Assets *assets = startApp();

    auto appComponent = assets->flowDefinition->flows[MOD_MAIN_INDEX]->components[0];
    REQUIRE(MOD_MAIN_INDEX < (int)g_mainAssets->flowDefinition->flows.count);
    auto mainComponent = g_mainAssets->flowDefinition->flows[MOD_MAIN_INDEX]->components[0];
    REQUIRE(appComponent->breakpoint == 0);
    REQUIRE(mainComponent->breakpoint == 0);

    std::string input = "3\t" + std::to_string(MOD_MAIN_INDEX) + "\t0\n";
    std::vector<char> buffer(input.begin(), input.end());
    flow::processDebuggerInput(buffer.data(), (uint32_t)buffer.size());

    CHECK_EQ(appComponent->breakpoint, 1);
    CHECK_EQ(mainComponent->breakpoint, 0);
}

TEST("external/debugger: without the filter external assets don't send their global variables") {
    connectDebugger();
    resume();
    Assets *assets = ext::setup();
    harness::frame(2);
    CHECK(assets != nullptr);
    CHECK_EQ(messagesOfType(GLOBAL_VARIABLE_INIT).size(), 0u);
}
