/*
 * eez-framework tests: helpers of the GUI runtime tests (tests/gui).
 *
 * The pages, actions and variables are in projects/parts/gui.py. The native
 * actions and variables of that part are implemented in gui_app.cpp.
 */

#pragma once

#include <functional>
#include <string>

#include <eez/conf-internal.h>
#include <eez/gui/gui.h>
#include <eez/gui/keypad.h>

#include "document.h"
#include "harness.h"
#include "test.h"

namespace gui_test {

using namespace eez;
using namespace eez::gui;

////////////////////////////////////////////////////////////////////////////////
// what the native actions of the gui part did

struct Actions {
    int clickA;
    int clickB;
    int clickC;
    int clickD;
    int clickP;
    int listItem;       // number of gui_list_item calls
    int listItemIndex;  // $index of the last clicked list item
    int gridItem;
    int gridItemIndex;
};
extern Actions g_actions;

// called by the gui_click_* actions with their action id (ACTION_ID_GUI_CLICK_*),
// e.g. to change the page from the action
extern std::function<void(int actionId)> g_onAction;

// native variables
extern bool g_enabled;        // gui_enabled (enabled of page_a's "Off" button)
extern bool g_switch;         // gui_switch (page_a's switch)
extern std::function<void(DataOperationEnum operation, Value &value)> g_nameData; // gui_name
extern Overlay g_overlay;     // gui_overlay
extern bool g_overlayEnabled; // gui_overlay returns the overlay (else undefined)

////////////////////////////////////////////////////////////////////////////////
// widget positions (centers) of the gui part pages

// gui_page_a
static const int A_BTN1_X = 120, A_BTN1_Y = 50;       // "A1" -> gui_click_a (20, 20, 200x60)
static const int A_BTN2_X = 400, A_BTN2_Y = 50;       // "A2" -> gui_click_b (300, 20, 200x60)
static const int A_DISABLED_X = 120, A_DISABLED_Y = 150; // "Off" -> gui_click_c, enabled: gui_enabled
static const int A_SWITCH_X = 350, A_SWITCH_Y = 145;  // switch (300, 120, 100x50)
static const int A_EMPTY_X = 600, A_EMPTY_Y = 400;    // no widget

// gui_page_b
static const int B_BTN1_X = 600, B_BTN1_Y = 330;      // "B1" -> gui_click_d (500, 300, 200x60)

// gui_popup (200, 120, 400x240), close if touched outside
static const int POPUP_BTN_X = 300, POPUP_BTN_Y = 170; // "P1" -> gui_click_p
// gui_popup2 (300, 200, 200x80), close if touched outside
// gui_dialog (250, 150, 300x180)

// gui_yes_no (150, 100, 500x280)
static const int YES_X = 240, YES_Y = 310;
static const int NO_X = 400, NO_Y = 310;
static const int CANCEL_X = 560, CANCEL_Y = 310;

// gui_keypad_num and gui_keypad_text: keypad text (0, 0, 800x60)
// gui_lists: list (0, 0, 400x300, items 400x60), grid (420, 0, 360x300, items 90x60)

////////////////////////////////////////////////////////////////////////////////
// GUI helpers

inline AppContext *app() {
    return harness::appContext();
}

inline int activePageId() {
    return app()->getActivePageId();
}

// touch down and the frames until the GUI has processed it
void press(int x, int y);
// moves the finger (pressed) and processes it
void moveTo(int x, int y);
// touch up and the frames until the GUI has processed it
void releaseTouch();
// press, release and 400 ms more, so that the next tap is never taken as an
// accidental double tap
void tap(int x, int y);

// the tap position of a numeric keypad key ("0".."9", ".", "back", "sign",
// "unit", "clear", "ok", "cancel", "option1") or a text keypad key ("a", "b",
// "c", "n~", "space", "back", "clear", "ok", "cancel", "mode")
void numericKeyPosition(const char *key, int &x, int &y);
void textKeyPosition(const char *key, int &x, int &y);
// taps the digit and '.' keys of the numeric keypad
void typeNumeric(const char *digits);
void tapNumericKey(const char *key);
void tapTextKey(const char *key);

// text the active keypad shows (getKeypadTextValue)
std::string keypadText();

// the page objects the tests push
struct TestPage : public Page {
    int allocCount = 0;
    int freeCount = 0;
    int willAppearCount = 0;
    void pageAlloc() override { allocCount++; }
    void pageFree() override { freeCount++; }
    void pageWillAppear() override { willAppearCount++; }
};

// protected AppContext members, called without a cast to a derived class
bool isPageFullyCovered(AppContext *appContext, int stackIndex);
void getPageRect(AppContext *appContext, int pageId, const Page *page, int &x, int &y, int &w, int &h);

} // namespace gui_test
