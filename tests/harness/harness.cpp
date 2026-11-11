/*
 * eez-framework tests: the application around the framework (harness.h).
 */

#include "harness.h"

#include <stdio.h>
#include <stdlib.h>

#include <eez/conf-internal.h>
#include <eez/core/alloc.h>
#include <eez/core/memory.h>
#include <eez/gui/gui.h>
#include <eez/gui/thread.h>
#include <eez/gui/display.h>
#include <eez/flow/flow.h>
#include <eez/flow/hooks.h>
#include <eez/flow/private.h>

#include "document.h"

namespace harness {

////////////////////////////////////////////////////////////////////////////////
// flow records

static std::vector<FlowErrorRecord> g_flowErrors;
static int g_stopScriptCount;
static std::vector<std::string> g_logMessages;

std::vector<FlowErrorRecord> &flowErrors() {
    return g_flowErrors;
}

int stopScriptCount() {
    return g_stopScriptCount;
}

std::vector<std::string> &logMessages() {
    return g_logMessages;
}

void clearFlowRecords() {
    g_flowErrors.clear();
    g_stopScriptCount = 0;
    g_logMessages.clear();
}

static void onFlowError(eez::flow::FlowState *flowState, int componentIndex, const char *errorMessage) {
    g_flowErrors.push_back({ flowState ? (int)flowState->flowIndex : -1, componentIndex, errorMessage ? errorMessage : "" });
}

static void stopScript() {
    g_stopScriptCount++;
}

static void logInfo(eez::flow::FlowState *, unsigned, const char *message) {
    g_logMessages.push_back(message ? message : "");
}

////////////////////////////////////////////////////////////////////////////////
// GUI

static int g_frameCount;
static void (*g_stateManagment)();

static void stateManagment() {
    g_frameCount++;
    g_stateManagment();
}

eez::gui::AppContext *appContext() {
    return eez::gui::getAppContextFromId(APP_CONTEXT_ID_DEVICE);
}

int frameCount() {
    return g_frameCount;
}

void frame(int count) {
    for (int i = 0; i < count; i++) {
        advance(FRAME_MS);
        int start = g_frameCount;
        // the display update of a frame queues the next VSYNC message, so there
        // is always one in the queue
        for (int n = 0; g_frameCount == start; n++) {
            if (n == 10000) {
                fprintf(stderr, "harness::frame: no display update after %d GUI messages\n", n);
                abort();
            }
            eez::gui::processGuiQueue(0);
        }
    }
}

uint32_t pixel(int x, int y) {
    return eez::gui::display::getBufferPointer()[y * DISPLAY_WIDTH + x];
}

////////////////////////////////////////////////////////////////////////////////

void init() {
    // headless SDL: the dummy video driver and the software renderer, which
    // SDL_CreateRenderer accepts with the ACCELERATED flag only through this hint
    setenv("SDL_VIDEODRIVER", "dummy", 0);
    setenv("SDL_RENDER_DRIVER", "software", 0);

    eez::initMemory();

    eez::flow::onFlowErrorHook = onFlowError;
    eez::flow::stopScriptHook = stopScript;
    eez::flow::logInfoHook = logInfo;

    g_stateManagment = eez::gui::g_hooks.stateManagment;
    eez::gui::g_hooks.stateManagment = stateManagment;

    eez::gui::startThread();
    eez::gui::guiInit();
    eez::gui::display::turnOn();

    appContext()->showPage(eez::gui::PAGE_ID_MAIN);
    frame(3);

    clearFlowRecords();
}

} // namespace harness
