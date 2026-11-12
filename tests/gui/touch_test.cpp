/*
 * eez-framework tests: the touch pipeline (gui/touch.cpp, gui/event.cpp,
 * findWidget) through the touch screen of the harness.
 */

#include "gui_test.h"

using namespace gui_test;

static void showPageA() {
    app()->showPage(PAGE_ID_GUI_PAGE_A);
    harness::frame(2);
    harness::advance(400);
}

// color of the presented pixel without alpha (0xBBGGRR)
static uint32_t rgb(int x, int y) {
    return harness::pixel(x, y) & 0xFFFFFF;
}

static const uint32_t ACCENT = 0xC06020; // #2060c0
static const uint32_t ERROR = 0x0000C0;  // #c00000

////////////////////////////////////////////////////////////////////////////////
// taps

TEST("gui/event: a tap executes the button action once, at release") {
    showPageA();
    press(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(g_actions.clickA, 0);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 1);
    CHECK_EQ(g_actions.clickB, 0);
}

TEST("gui/event: the pressed button is the active widget and is drawn with the active style") {
    showPageA();
    CHECK_EQ(rgb(A_BTN1_X - 60, A_BTN1_Y), ACCENT);
    press(A_BTN1_X, A_BTN1_Y);
    CHECK(g_activeWidget.widget != nullptr);
    CHECK(g_activeWidget == getFoundWidgetAtDown());
    CHECK_EQ(rgb(A_BTN1_X - 60, A_BTN1_Y), ERROR);
    releaseTouch();
    CHECK(g_activeWidget.widget == nullptr);
    CHECK_EQ(rgb(A_BTN1_X - 60, A_BTN1_Y), ACCENT);
}

TEST("gui/event: the widget nearest to the touch is found") {
    showPageA();
    auto a1 = findWidget(A_BTN1_X, A_BTN1_Y, false);
    auto a2 = findWidget(A_BTN2_X, A_BTN2_Y, false);
    REQUIRE(a1.widget != nullptr);
    REQUIRE(a2.widget != nullptr);
    CHECK_EQ(getWidgetAction(a1), (int)ACTION_ID_GUI_CLICK_A);
    CHECK_EQ(getWidgetAction(a2), (int)ACTION_ID_GUI_CLICK_B);
    // just below the button (60 px high, not extended to the minimal touch
    // size): no button
    auto below = findWidget(A_BTN1_X, A_BTN1_Y + 31, false);
    CHECK(!below.widget || getWidgetAction(below) == EEZ_ACTION_ID_NONE);
}

TEST("gui/event: a touch outside of all widgets executes no action") {
    showPageA();
    tap(A_EMPTY_X, A_EMPTY_Y);
    CHECK_EQ(g_actions.clickA + g_actions.clickB + g_actions.clickC, 0);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/event: two taps on two buttons execute both actions") {
    showPageA();
    tap(A_BTN1_X, A_BTN1_Y);
    tap(A_BTN2_X, A_BTN2_Y);
    tap(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(g_actions.clickA, 2);
    CHECK_EQ(g_actions.clickB, 1);
}

TEST("gui/event: release outside of the pressed button still executes its action") {
    // pinned: the widget found at touch down gets the touch up wherever it is
    showPageA();
    press(A_BTN1_X, A_BTN1_Y);
    moveTo(A_EMPTY_X, A_EMPTY_Y);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 1);
}

TEST("gui/event: a disabled button executes no action") {
    g_enabled = false;
    showPageA();
    tap(A_DISABLED_X, A_DISABLED_Y);
    CHECK_EQ(g_actions.clickC, 0);
    g_enabled = true;
    harness::frame(2);
    tap(A_DISABLED_X, A_DISABLED_Y);
    CHECK_EQ(g_actions.clickC, 1);
}

TEST("gui/event: a switch is toggled at release") {
    showPageA();
    press(A_SWITCH_X, A_SWITCH_Y);
    CHECK(!g_switch);
    releaseTouch();
    CHECK(g_switch);
    tap(A_SWITCH_X, A_SWITCH_Y);
    CHECK(!g_switch);
}

TEST("gui/event: no touch is handled while the event handling is disabled") {
    showPageA();
    g_hooks.isEventHandlingDisabled = []() { return true; };
    tap(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(g_actions.clickA, 0);
}

////////////////////////////////////////////////////////////////////////////////
// long touch, auto repeat, action at touch down

TEST("gui/event: a press longer than the long touch time (1 s) doesn't click") {
    // pinned: the long touch takes the touch over even when the app context has
    // no long touch action
    showPageA();
    press(A_BTN1_X, A_BTN1_Y);
    harness::frame(70);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 0);
}

TEST("gui/event: a press shorter than the long touch time clicks") {
    showPageA();
    press(A_BTN1_X, A_BTN1_Y);
    harness::frame(50);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 1);
}

TEST("gui/event: the long touch action of the app context is executed after 1 s") {
    harness::g_appContextOverrides.getLongTouchAction = [](const WidgetCursor &widgetCursor) {
        return getWidgetAction(widgetCursor) == ACTION_ID_GUI_CLICK_A ? (int)ACTION_ID_GUI_CLICK_B : (int)EEZ_ACTION_ID_NONE;
    };
    bool longTouchSeen = false;
    g_onAction = [&](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_B) {
            longTouchSeen = g_isLongTouch;
        }
    };
    showPageA();
    press(A_BTN1_X, A_BTN1_Y);
    harness::frame(55);
    CHECK_EQ(g_actions.clickB, 0);
    harness::frame(15);
    CHECK_EQ(g_actions.clickB, 1);
    CHECK(longTouchSeen);
    CHECK(!g_isLongTouch);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 0);
    CHECK_EQ(g_actions.clickB, 1);
}

TEST("gui/event: the extra long touch action (15 s) is executed") {
    g_hooks.getExtraLongTouchAction = []() { return (int)ACTION_ID_GUI_CLICK_C; };
    showPageA();
    press(A_EMPTY_X, A_EMPTY_Y);
    harness::frame(15000 / harness::FRAME_MS - 20);
    CHECK_EQ(g_actions.clickC, 0);
    harness::frame(30);
    CHECK_EQ(g_actions.clickC, 1);
    harness::frame(30);
    CHECK_EQ(g_actions.clickC, 1);
    releaseTouch();
}

TEST("gui/event: an auto repeat action is repeated while the button is pressed") {
    harness::g_appContextOverrides.isAutoRepeatAction = [](int action) { return action == ACTION_ID_GUI_CLICK_A; };
    showPageA();
    press(A_BTN1_X, A_BTN1_Y);
    // the first repeat after 300 ms
    harness::frame(15);
    CHECK_EQ(g_actions.clickA, 0);
    harness::frame(10);
    int afterFirstDelay = g_actions.clickA;
    CHECK(afterFirstDelay >= 1);
    // then about every 50 ms (the touch is polled every 32 ms while the finger is still)
    harness::frame(25);
    CHECK(g_actions.clickA >= afterFirstDelay + 4);
    int beforeRelease = g_actions.clickA;
    releaseTouch();
    // release doesn't add a click
    CHECK_EQ(g_actions.clickA, beforeRelease);
}

TEST("gui/event: an action that is executed at touch down is not executed again at release") {
    harness::g_appContextOverrides.testExecuteActionOnTouchDown = [](int action) { return action == ACTION_ID_GUI_CLICK_A; };
    showPageA();
    press(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(g_actions.clickA, 1);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 1);
}

////////////////////////////////////////////////////////////////////////////////
// accidental double tap

// The second tap is within the 50 px radius of the release of the first one.
static const int CORNER_X = 25, CORNER_Y = 25; // on page_a's "A1" button

TEST("gui/event: a second tap at the same place right after a page change is ignored") {
    showPageA();
    g_onAction = [](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_A) {
            app()->pushPage(PAGE_ID_GUI_PAGE_A);
        }
    };
    press(CORNER_X, CORNER_Y);
    releaseTouch();
    REQUIRE(g_actions.clickA == 1);
    REQUIRE(app()->getNumPagesOnStack() == 2);

    // double tap: the new page is not seen yet
    press(CORNER_X + 10, CORNER_Y);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 1);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
}

TEST("gui/event: a second tap in the middle of the screen right after a page change is ignored") {
    // the release is at the position of the touch, so the second touch is near
    // it
    showPageA();
    g_onAction = [](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_A) {
            app()->pushPage(PAGE_ID_GUI_PAGE_A);
        }
    };
    press(A_BTN1_X, A_BTN1_Y);
    releaseTouch();
    press(A_BTN1_X + 10, A_BTN1_Y);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 1);
}

TEST("gui/touch: the touch up event is at the position where the finger was lifted") {
    static Event upEvent;
    g_hooks.getWidgetTouchFunction = [](const WidgetCursor &widgetCursor) -> OnTouchFunctionType {
        if (widgetCursor.widget->type == WIDGET_TYPE_TEXT) {
            return [](const WidgetCursor &, Event &event) {
                if (event.type == EVENT_TYPE_TOUCH_UP) {
                    upEvent = event;
                }
            };
        }
        return nullptr;
    };
    app()->showPage(PAGE_ID_GUI_PAGE_B);
    harness::frame(2);
    press(400, 40);
    moveTo(420, 45);
    releaseTouch();
    CHECK_EQ(upEvent.type, EVENT_TYPE_TOUCH_UP);
    CHECK_EQ(upEvent.x, 420);
    CHECK_EQ(upEvent.y, 45);
}

TEST("gui/event: a second tap far from the first one after a page change is not ignored") {
    showPageA();
    g_onAction = [](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_A) {
            app()->pushPage(PAGE_ID_GUI_PAGE_A);
        }
    };
    press(A_BTN1_X, A_BTN1_Y);
    releaseTouch();
    press(A_BTN2_X, A_BTN2_Y);
    releaseTouch();
    CHECK_EQ(g_actions.clickB, 1);
}

TEST("gui/event: a second tap 300 ms after the page change is not ignored") {
    showPageA();
    g_onAction = [](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_A && app()->getNumPagesOnStack() == 1) {
            app()->pushPage(PAGE_ID_GUI_PAGE_A);
        }
    };
    press(A_BTN1_X, A_BTN1_Y);
    releaseTouch();
    harness::frame(20);
    press(A_BTN1_X, A_BTN1_Y);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 2);
}

TEST("gui/event: a second tap at the same place without a page change is not ignored") {
    showPageA();
    press(A_BTN1_X, A_BTN1_Y);
    releaseTouch();
    press(A_BTN1_X, A_BTN1_Y);
    releaseTouch();
    CHECK_EQ(g_actions.clickA, 2);
}

////////////////////////////////////////////////////////////////////////////////
// pages closed by a touch outside of them

TEST("gui/app_context: a tap outside of a popup closes it") {
    showPageA();
    app()->pushPage(PAGE_ID_GUI_POPUP);
    harness::frame(2);
    harness::advance(400);
    tap(A_EMPTY_X + 150, A_EMPTY_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/app_context: a tap outside of a popup doesn't click the widget below it") {
    showPageA();
    app()->pushPage(PAGE_ID_GUI_POPUP);
    harness::frame(2);
    harness::advance(400);
    tap(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(g_actions.clickA, 0);
}

TEST("gui/app_context: a tap inside of a popup, not on a widget, doesn't close it") {
    // onPageTouch checks the popup rect for every event type (G11), the release
    // is at the position of the touch, inside of the popup
    showPageA();
    app()->pushPage(PAGE_ID_GUI_POPUP);
    harness::frame(2);
    harness::advance(400);
    tap(500, 300);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_POPUP);
}

TEST("gui/app_context: a tap on the popup button executes its action") {
    showPageA();
    app()->pushPage(PAGE_ID_GUI_POPUP);
    harness::frame(2);
    harness::advance(400);
    tap(POPUP_BTN_X, POPUP_BTN_Y);
    CHECK_EQ(g_actions.clickP, 1);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_POPUP);
}

TEST("gui/app_context: a tap outside of a page without the close flag doesn't close it") {
    showPageA();
    app()->pushPage(PAGE_ID_GUI_DIALOG);
    harness::frame(2);
    harness::advance(400);
    tap(A_EMPTY_X + 150, A_EMPTY_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_DIALOG);
}

TEST_BUG("gui/app_context: a tap outside of two stacked popups closes only the top one", "G11") {
    showPageA();
    app()->pushPage(PAGE_ID_GUI_POPUP);
    app()->pushPage(PAGE_ID_GUI_POPUP2);
    harness::frame(2);
    harness::advance(400);
    tap(A_EMPTY_X + 150, A_EMPTY_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_POPUP);
}

TEST_BUG("gui/app_context: the action below a closed popup is executed when the app context allows it", "G11") {
    // canExecuteActionWhenTouchedOutsideOfActivePage: the touch that closes
    // the popup also goes to the widget under it
    harness::g_appContextOverrides.canExecuteActionWhenTouchedOutsideOfActivePage = [](int pageId, int action) {
        return action == ACTION_ID_GUI_CLICK_A;
    };
    showPageA();
    app()->pushPage(PAGE_ID_GUI_POPUP);
    harness::frame(2);
    harness::advance(400);
    tap(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(g_actions.clickA, 1);
}

////////////////////////////////////////////////////////////////////////////////
// toasts and touches

TEST("gui/event: a toast without action lets the touch through to the page below") {
    showPageA();
    app()->infoMessage("info");
    harness::frame(2);
    harness::advance(400);
    tap(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(g_actions.clickA, 1);
}

TEST("gui/event: a toast with a button is closed by a tap outside of it, the page below is not clicked") {
    showPageA();
    app()->errorMessage("error");
    harness::frame(2);
    harness::advance(400);
    tap(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(g_actions.clickA, 0);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

////////////////////////////////////////////////////////////////////////////////
// page changes during a touch

TEST("gui/event: the widget found at touch down stays found after the touch") {
    // the select popup opened later is placed next to it (findPagePosition)
    showPageA();
    tap(A_BTN1_X, A_BTN1_Y);
    CHECK(getFoundWidgetAtDown().widget != nullptr);
    CHECK_EQ(getWidgetAction(getFoundWidgetAtDown()), (int)ACTION_ID_GUI_CLICK_A);
}

TEST("gui/event: the widget found at touch down is cleared when its page is replaced during the touch") {
    showPageA();
    press(A_SWITCH_X, A_SWITCH_Y);
    REQUIRE(getFoundWidgetAtDown().widget != nullptr);
    // not by the touch: e.g. BB3+ state management replaces the page
    app()->showPage(PAGE_ID_GUI_PAGE_B);
    harness::frame(2);
    CHECK(getFoundWidgetAtDown().widget == nullptr);
    releaseTouch();
}

TEST("gui/event: the switch of a replaced page doesn't get the rest of the touch") {
    // passes today by luck: with the stale cursor (G1) the release goes to the
    // memory of a destroyed state, whose onTouch does nothing
    showPageA();
    press(A_SWITCH_X, A_SWITCH_Y);
    app()->showPage(PAGE_ID_GUI_PAGE_B);
    harness::frame(2);
    releaseTouch();
    CHECK(!g_switch);
}

TEST("gui/event: a popup opened by code after a page change is centered") {
    static const EnumItem items[] = { { 1, "One", nullptr }, { 2, "Two", nullptr }, { 0, nullptr, nullptr } };
    showPageA();
    tap(A_BTN1_X, A_BTN1_Y);
    app()->showPage(PAGE_ID_GUI_PAGE_B);
    harness::frame(2);
    // e.g. from SCPI or the keyboard, no touch: centered
    SelectFromEnumPage::pushSelectFromEnumPage(app(), items, 1, nullptr, [](uint16_t) {});
    auto page = (SelectFromEnumPage *)app()->getActivePage();
    CHECK_EQ(page->x, (800 - page->width) / 2);
    CHECK_EQ(page->y, (480 - page->height) / 2);
}

TEST("gui/event: release on the button of a toast closed during the touch doesn't crash") {
    showPageA();
    app()->errorMessage("module error");
    harness::frame(2);
    harness::advance(400);
    auto toast = (InternalPage *)app()->getActivePage();
    // the "Close" button is at the bottom of the toast
    int x = toast->x + toast->width / 2;
    int y = toast->y + toast->height - 15;
    press(x, y);
    REQUIRE(getFoundWidgetAtDown().widget != nullptr);
    // the toast is closed by code during the touch
    app()->popPage();
    harness::frame(2);
    releaseTouch();
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/event: release on the button of a page removed during the touch doesn't act on the page below") {
    // scenario B of G4 (progress page "Abort"); a G1 fix that clears the
    // cursor at the page change also covers it
    static int cancelCount;
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    app()->yesNoDialog(PAGE_ID_GUI_YES_NO, "Abort?", nullptr, nullptr, []() { cancelCount++; });
    harness::frame(2);
    harness::advance(400);
    press(CANCEL_X, CANCEL_Y);
    // the operation is done, the page is removed by code
    app()->popPage();
    harness::frame(2);
    releaseTouch();
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(cancelCount, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Page::discard and SetPage::onSetValue use the touch cursor

struct TestSetPage : public SetPage {
    float value = -1;
    void setValue(float value_) override { value = value_; }
    static void callOnSetValue(float value) { onSetValue(value); }
};

TEST("gui/page: SetPage::onSetValue after a touch pops the page and sets the value of the page below") {
    static TestSetPage setPage;
    showPageA();
    app()->pushPage(PAGE_ID_GUI_DIALOG, &setPage);
    app()->pushPage(PAGE_ID_GUI_POPUP);
    harness::frame(2);
    harness::advance(400);
    g_onAction = [](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_P) {
            TestSetPage::callOnSetValue(2.5f);
        }
    };
    tap(POPUP_BTN_X, POPUP_BTN_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_DIALOG);
    CHECK_EQ(setPage.value, 2.5f);
}

TEST("gui/page: SetPage::onSetValue without a touch pops the page and sets the value") {
    // e.g. the keypad OK by the encoder after the touch was ignored as a double tap
    static TestSetPage setPage;
    app()->pushPage(PAGE_ID_GUI_DIALOG, &setPage);
    app()->pushPage(PAGE_ID_GUI_POPUP);
    clearFoundWidgetAtDown();
    TestSetPage::callOnSetValue(2.5f);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_DIALOG);
    CHECK_EQ(setPage.value, 2.5f);
}

TEST("gui/page: Page::discard without a touch pops the page") {
    TestPage page;
    app()->pushPage(PAGE_ID_GUI_DIALOG, &page);
    clearFoundWidgetAtDown();
    page.discard();
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/page: Page::discard after an ignored double tap pops the page") {
    // the ignored touch sets the found widget to none
    showPageA();
    TestPage page;
    g_onAction = [&page](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_A) {
            app()->pushPage(PAGE_ID_GUI_PAGE_A, &page);
        }
    };
    press(25, 25);
    releaseTouch();
    press(25, 25);
    releaseTouch();
    REQUIRE(app()->getNumPagesOnStack() == 2);
    REQUIRE(getFoundWidgetAtDown().widget == nullptr);
    page.discard();
    CHECK_EQ(app()->getNumPagesOnStack(), 1);
}

////////////////////////////////////////////////////////////////////////////////
// hooks

TEST("gui/hooks: the keypad text widget gets the keypad text touch function") {
    app()->showPage(PAGE_ID_GUI_KEYPAD_TEXT);
    harness::frame(2);
    auto widgetCursor = findWidget(400, 30, false);
    REQUIRE(widgetCursor.widget != nullptr);
    CHECK_EQ((int)widgetCursor.widget->type, (int)WIDGET_TYPE_DISPLAY_DATA);
    CHECK(getWidgetTouchFunction(widgetCursor) == onKeypadTextTouch);
}

TEST("gui/hooks: the widget touch function hook gives a touch function to a widget without action") {
    static int touches;
    g_hooks.getWidgetTouchFunction = [](const WidgetCursor &widgetCursor) -> OnTouchFunctionType {
        if (widgetCursor.widget->type == WIDGET_TYPE_TEXT) {
            return [](const WidgetCursor &, Event &event) {
                if (event.type == EVENT_TYPE_TOUCH_UP) {
                    touches++;
                }
            };
        }
        return nullptr;
    };
    app()->showPage(PAGE_ID_GUI_PAGE_B);
    harness::frame(2);
    harness::advance(400);
    tap(400, 40);
    CHECK_EQ(touches, 1);
}
