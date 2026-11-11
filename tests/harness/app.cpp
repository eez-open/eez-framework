/*
 * eez-framework tests: what the application provides to the framework.
 *
 * The ids of the framework (EEZ_STYLE_ID_* etc.) map to the items of
 * projects/fwtest, like src/bb3/gui/gui-defs.cpp does for the BB3+.
 */

#include <eez/conf-internal.h>

#include <atomic>

#include <SDL.h>

#include <eez/core/os.h>
#include <eez/core/sound.h>
#include <eez/core/keyboard.h>
#include <eez/gui/gui.h>
#include <eez/gui/keypad.h>

#include "document.h"
#include "harness.h"

namespace eez {

std::atomic_bool g_shutdown;

void shutdown() {
    g_shutdown = true;
}

namespace sound {
void playBeep(bool) {}
void playClick() {}
} // namespace sound

namespace keyboard {
void onKeyboardEvent(SDL_KeyboardEvent *) {}
bool isDisplayDirty() { return false; }
void updateDisplay() {}
void onPageChanged() {}
} // namespace keyboard

namespace gui {

const uint8_t *eezAssets = assets;
const uint32_t eezAssetsSize = sizeof(assets);

const int EEZ_DATA_ID_KEYPAD_EDIT_UNIT = DATA_ID_KEYPAD_EDIT_UNIT;
const int EEZ_DATA_ID_KEYPAD_TEXT = DATA_ID_KEYPAD_TEXT;
const int EEZ_DATA_ID_ALERT_MESSAGE = DATA_ID_ALERT_MESSAGE;

const int EEZ_ACTION_ID_DRAG_OVERLAY = ACTION_ID_DRAG_OVERLAY;
const int EEZ_ACTION_ID_EDIT = ACTION_ID_EDIT;
const int EEZ_ACTION_ID_SCROLL = ACTION_ID_SCROLL;

const int EEZ_PAGE_ID_ASYNC_OPERATION_IN_PROGRESS = PAGE_ID_ASYNC_OPERATION_IN_PROGRESS;
const int EEZ_PAGE_ID_NUMERIC_KEYPAD = PAGE_ID_NUMERIC_KEYPAD;

const int EEZ_STYLE_ID_DEFAULT = STYLE_ID_DEFAULT;
const int EEZ_STYLE_ID_INFO_ALERT = STYLE_ID_INFO_ALERT;
// not the error button style (BB3+ maps it so), so that the style of the info
// toast button is distinguishable from the error toast button (G14)
const int EEZ_STYLE_ID_INFO_ALERT_BUTTON = STYLE_ID_INFO_ALERT;
const int EEZ_STYLE_ID_ERROR_ALERT = STYLE_ID_ERROR_ALERT;
const int EEZ_STYLE_ID_ERROR_ALERT_BUTTON = STYLE_ID_ERROR_ALERT_BUTTON;
const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER_S = STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER_S;
const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER = STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER;
const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM_S = STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM_S;
const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM = STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM;
const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM_S = STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM_S;
const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM = STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM;
const int EEZ_STYLE_ID_MENU_WITH_BUTTONS_CONTAINER = STYLE_ID_MENU_WITH_BUTTONS_CONTAINER;
const int EEZ_STYLE_ID_MENU_WITH_BUTTONS_MESSAGE = STYLE_ID_MENU_WITH_BUTTONS_MESSAGE;
const int EEZ_STYLE_ID_MENU_WITH_BUTTONS_BUTTON = STYLE_ID_MENU_WITH_BUTTONS_BUTTON;

const int EEZ_THEME_ID_LEGACY = THEME_ID_DEFAULT;
const int EEZ_THEME_ID_DEFAULT = THEME_ID_DEFAULT;
const int EEZ_COLOR_ID_BOOKMARK = COLOR_ID_BOOKMARK;
const int EEZ_COLOR_ID_BACKDROP = COLOR_ID_BACKDROP;
const int EEZ_FONT_ID_SHADOW = FONT_ID_NONE;

// native enums (ENUM values), none in the tests yet
static const EnumItem g_noEnumItems[] = { { 0, nullptr, nullptr } };
const EnumItem *g_enumDefinitions[] = { g_noEnumItems };

class TestAppContext : public AppContext {
public:
    TestAppContext() {
        app_context_id = APP_CONTEXT_ID_DEVICE;
        rect.x = 0;
        rect.y = 0;
        rect.w = DISPLAY_WIDTH;
        rect.h = DISPLAY_HEIGHT;
    }

    int getMainPageId() override {
        return PAGE_ID_MAIN;
    }

    int getLongTouchActionHook(const WidgetCursor &widgetCursor) override {
        auto f = harness::g_appContextOverrides.getLongTouchAction;
        return f ? f(widgetCursor) : AppContext::getLongTouchActionHook(widgetCursor);
    }

    bool isAutoRepeatAction(int action) override {
        auto f = harness::g_appContextOverrides.isAutoRepeatAction;
        return f ? f(action) : AppContext::isAutoRepeatAction(action);
    }

    bool testExecuteActionOnTouchDown(int action) override {
        auto f = harness::g_appContextOverrides.testExecuteActionOnTouchDown;
        return f ? f(action) : AppContext::testExecuteActionOnTouchDown(action);
    }

protected:
    bool canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action) override {
        auto f = harness::g_appContextOverrides.canExecuteActionWhenTouchedOutsideOfActivePage;
        return f ? f(pageId, action) : AppContext::canExecuteActionWhenTouchedOutsideOfActivePage(pageId, action);
    }
};

static TestAppContext g_appContext;

AppContext *getAppContextFromId(int16_t id) {
    return id == APP_CONTEXT_ID_DEVICE ? &g_appContext : nullptr;
}

// registered keypad pages (harness::registerKeypadPage)
static const int MAX_KEYPAD_PAGES = 8;
static int g_keypadPageIds[MAX_KEYPAD_PAGES];
static bool g_keypadPageNumeric[MAX_KEYPAD_PAGES];
static int g_numKeypadPages;

static bool isKeypadPage(int pageId, bool numeric) {
    if (pageId == PAGE_ID_NUMERIC_KEYPAD) {
        return true;
    }
    for (int i = 0; i < g_numKeypadPages; i++) {
        if (g_keypadPageIds[i] == pageId && (!numeric || g_keypadPageNumeric[i])) {
            return true;
        }
    }
    return false;
}

Keypad *getActiveKeypad() {
    if (isKeypadPage(g_appContext.getActivePageId(), false)) {
        return (Keypad *)g_appContext.getActivePage();
    }
    return nullptr;
}

NumericKeypad *getActiveNumericKeypad() {
    if (isKeypadPage(g_appContext.getActivePageId(), true)) {
        return (NumericKeypad *)g_appContext.getActivePage();
    }
    return nullptr;
}

void executeNumericKeypadOptionHook(int) {}

void action_edit() {}

} // namespace gui
} // namespace eez

////////////////////////////////////////////////////////////////////////////////
// touch screen (CM5 platform, platform/simulator/touch.cpp)

int g_last_touch_x;
int g_last_touch_y;
int g_last_touch_pressed;

static int g_touchX;
static int g_touchY;
static bool g_touchPressed;

extern "C" void eez_custom_touch_measure_callback(int *x, int *y, int *pressed) {
    *x = g_touchX;
    *y = g_touchY;
    *pressed = g_touchPressed ? 1 : 0;
    g_last_touch_x = g_touchX;
    g_last_touch_y = g_touchY;
    g_last_touch_pressed = *pressed;
}

namespace harness {

void touch(int x, int y) {
    g_touchX = x;
    g_touchY = y;
    g_touchPressed = true;
}

void release() {
    g_touchPressed = false;
}

void registerKeypadPage(int pageId, bool numeric) {
    using namespace eez::gui;
    if (g_numKeypadPages < MAX_KEYPAD_PAGES) {
        g_keypadPageIds[g_numKeypadPages] = pageId;
        g_keypadPageNumeric[g_numKeypadPages] = numeric;
        g_numKeypadPages++;
    }
}

AppContextOverrides g_appContextOverrides;

} // namespace harness
