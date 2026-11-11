/*
 * eez-framework tests: the application around the framework.
 *
 * harness::init() starts the framework like the BB3+ does (memory, GUI thread,
 * main assets of projects/fwtest, flow, display) with a headless SDL display and
 * shows the "main" page. Every test starts from that state (test.h).
 *
 * The GUI loop runs on the test thread: frame() processes the GUI queue until one
 * display update is done. millis() of the framework is a fake clock that only
 * advance() and frame() move.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace eez {
namespace gui {
class AppContext;
struct WidgetCursor;
}
namespace flow {
struct FlowState;
}
}

namespace harness {

void init();

// fake clock (eez::millis())
uint32_t now();
void advance(uint32_t ms);

// GUI
eez::gui::AppContext *appContext();

// GUI frames: before each one the clock advances by FRAME_MS
static const uint32_t FRAME_MS = 16;
void frame(int count = 1);
int frameCount();

// RGBA of the last presented frame (bytes R, G, B, A in memory, 0xAABBGGRR)
uint32_t pixel(int x, int y);

// touch screen state, read by the touch pipeline in the next frames
void touch(int x, int y);
void release();

// keypad pages besides PAGE_ID_NUMERIC_KEYPAD: getActiveKeypad() returns the
// active page if its id is registered (numeric: getActiveNumericKeypad() too)
void registerKeypadPage(int pageId, bool numeric);

// overrides of the AppContext virtual functions of the device app context
// (nullptr: the AppContext default)
struct AppContextOverrides {
    int (*getLongTouchAction)(const eez::gui::WidgetCursor &widgetCursor);
    bool (*isAutoRepeatAction)(int action);
    bool (*testExecuteActionOnTouchDown)(int action);
    bool (*canExecuteActionWhenTouchedOutsideOfActivePage)(int pageId, int action);
};
extern AppContextOverrides g_appContextOverrides;

// flow errors (onFlowErrorHook), stopScriptHook calls and Log component messages
struct FlowErrorRecord {
    int flowIndex;
    int componentIndex;
    std::string message;
};
std::vector<FlowErrorRecord> &flowErrors();
int stopScriptCount();
std::vector<std::string> &logMessages();
void clearFlowRecords();

} // namespace harness
