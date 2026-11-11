/*
 * eez-framework tests: the native actions and variables of the gui part
 * (projects/parts/gui.py) and the helpers of gui_test.h.
 */

#include "gui_test.h"

#include <string.h>

namespace eez {
namespace gui {

// framework actions and data (gui/action_impl.cpp, gui/keypad.cpp), declared by
// the application document.h when the project uses them directly
void action_yes();
void action_no();
void action_cancel();
void action_keypad_key();
void action_keypad_space();
void action_keypad_back();
void action_keypad_clear();
void action_keypad_ok();
void action_keypad_cancel();
void action_keypad_sign();
void action_keypad_unit();
void action_toggle_keypad_mode();
void action_keypad_option1();
void data_keypad_ok_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
void data_keypad_option1_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);

} // namespace gui
} // namespace eez

namespace gui_test {

Actions g_actions;
std::function<void(int actionId)> g_onAction;

bool g_enabled = true;
bool g_switch;
std::function<void(DataOperationEnum operation, Value &value)> g_nameData;
Overlay g_overlay;
bool g_overlayEnabled;

// the keypad pages of the gui part
static struct RegisterKeypadPages {
    RegisterKeypadPages() {
        harness::registerKeypadPage(PAGE_ID_GUI_KEYPAD_NUM, true);
        harness::registerKeypadPage(PAGE_ID_GUI_KEYPAD_TEXT, false);
    }
} g_registerKeypadPages;

void press(int x, int y) {
    harness::touch(x, y);
    harness::frame(4);
}

void moveTo(int x, int y) {
    harness::touch(x, y);
    harness::frame(3);
}

void releaseTouch() {
    harness::release();
    harness::frame(4);
}

void tap(int x, int y) {
    press(x, y);
    releaseTouch();
    harness::advance(400);
}

struct KeyPosition {
    const char *key;
    int x;
    int y;
};

// centers of the keys, see projects/parts/gui.py
static const KeyPosition g_numericKeys[] = {
    { "7", 49, 119 }, { "8", 149, 119 }, { "9", 249, 119 },
    { "4", 49, 199 }, { "5", 149, 199 }, { "6", 249, 199 },
    { "1", 49, 279 }, { "2", 149, 279 }, { "3", 249, 279 },
    { "0", 49, 359 }, { ".", 149, 359 },
    { "back", 369, 119 }, { "sign", 369, 199 }, { "unit", 369, 279 }, { "clear", 369, 359 },
    { "ok", 489, 119 }, { "cancel", 489, 199 }, { "option1", 489, 279 },
};

static const KeyPosition g_textKeys[] = {
    { "a", 49, 119 }, { "b", 149, 119 }, { "c", 249, 119 }, { "n~", 349, 119 },
    { "space", 49, 199 }, { "back", 149, 199 }, { "clear", 249, 199 }, { "ok", 349, 199 },
    { "cancel", 449, 199 }, { "mode", 549, 199 },
};

template <size_t N>
static void findKey(const KeyPosition (&keys)[N], const char *key, int &x, int &y) {
    for (auto &k : keys) {
        if (strcmp(k.key, key) == 0) {
            x = k.x;
            y = k.y;
            return;
        }
    }
    test::fail(__FILE__, __LINE__, std::string("unknown key ") + key);
    throw test::Abort();
}

void numericKeyPosition(const char *key, int &x, int &y) {
    findKey(g_numericKeys, key, x, y);
}

void textKeyPosition(const char *key, int &x, int &y) {
    findKey(g_textKeys, key, x, y);
}

void tapNumericKey(const char *key) {
    int x, y;
    numericKeyPosition(key, x, y);
    tap(x, y);
}

void tapTextKey(const char *key) {
    int x, y;
    textKeyPosition(key, x, y);
    tap(x, y);
}

void typeNumeric(const char *digits) {
    for (const char *p = digits; *p; p++) {
        char key[2] = { *p, 0 };
        tapNumericKey(key);
    }
}

std::string keypadText() {
    auto keypad = getActiveKeypad();
    if (!keypad) {
        return "<no keypad>";
    }
    Value value = keypad->getKeypadTextValue();
    return value.getString() ? value.getString() : "";
}

// protected members of AppContext through member pointers of a derived class
struct AppContextAccess : public AppContext {
    using AppContext::isPageFullyCovered;
    using AppContext::getPageRect;
};

bool isPageFullyCovered(AppContext *appContext, int stackIndex) {
    bool (AppContext::*f)(int) = &AppContextAccess::isPageFullyCovered;
    return (appContext->*f)(stackIndex);
}

void getPageRect(AppContext *appContext, int pageId, const Page *page, int &x, int &y, int &w, int &h) {
    void (AppContext::*f)(int, const Page *, int &, int &, int &, int &) = &AppContextAccess::getPageRect;
    (appContext->*f)(pageId, page, x, y, w, h);
}

} // namespace gui_test

////////////////////////////////////////////////////////////////////////////////
// native actions and variables of the gui part

namespace eez {
namespace gui {

using namespace gui_test;

static void onAction(int actionId) {
    if (g_onAction) {
        g_onAction(actionId);
    }
}

void action_gui_click_a() {
    g_actions.clickA++;
    onAction(ACTION_ID_GUI_CLICK_A);
}

void action_gui_click_b() {
    g_actions.clickB++;
    onAction(ACTION_ID_GUI_CLICK_B);
}

void action_gui_click_c() {
    g_actions.clickC++;
    onAction(ACTION_ID_GUI_CLICK_C);
}

void action_gui_click_d() {
    g_actions.clickD++;
    onAction(ACTION_ID_GUI_CLICK_D);
}

void action_gui_click_p() {
    g_actions.clickP++;
    onAction(ACTION_ID_GUI_CLICK_P);
}

void action_gui_list_item() {
    g_actions.listItem++;
    g_actions.listItemIndex = getFoundWidgetAtDown().iterators[0];
}

void action_gui_grid_item() {
    g_actions.gridItem++;
    g_actions.gridItemIndex = getFoundWidgetAtDown().iterators[0];
}

void action_gui_yes() { action_yes(); }
void action_gui_no() { action_no(); }
void action_gui_cancel() { action_cancel(); }
void action_gui_keypad_key() { action_keypad_key(); }
void action_gui_keypad_space() { action_keypad_space(); }
void action_gui_keypad_back() { action_keypad_back(); }
void action_gui_keypad_clear() { action_keypad_clear(); }
void action_gui_keypad_ok() { action_keypad_ok(); }
void action_gui_keypad_cancel() { action_keypad_cancel(); }
void action_gui_keypad_sign() { action_keypad_sign(); }
void action_gui_keypad_unit() { action_keypad_unit(); }
void action_gui_toggle_keypad_mode() { action_toggle_keypad_mode(); }
void action_gui_keypad_option1() { action_keypad_option1(); }

void data_gui_enabled(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_enabled;
    }
}

void data_gui_switch(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_switch;
    } else if (operation == DATA_OPERATION_SET) {
        g_switch = value.getInt() != 0;
    }
}

void data_gui_name(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (g_nameData) {
        g_nameData(operation, value);
    }
}

void data_gui_overlay(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    // the flow reads native variables with DATA_OPERATION_GET only (also for
    // DATA_OPERATION_GET_OVERLAY_DATA of the container)
    if (g_overlayEnabled) {
        value = Value(&g_overlay, VALUE_TYPE_POINTER);
    }
}

void data_gui_keypad_ok_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    data_keypad_ok_enabled(operation, widgetCursor, value);
}

void data_gui_keypad_option1_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    data_keypad_option1_text(operation, widgetCursor, value);
}

} // namespace gui
} // namespace eez
