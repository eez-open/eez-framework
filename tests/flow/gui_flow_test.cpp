/*
 * eez-framework tests: the GUI side of the flow: OnEvent (page_open,
 * page_close, keydown), ShowPage, the dialog components ShowKeypad,
 * ShowKeyboard and ShowMessageBox (hooks, callbacks, async state, what happens
 * when the flow goes away), executeFlowAction (widget event handlers) and
 * user widgets with their own flows (flow/components/user_widget.cpp).
 */

#include "flow_helpers.h"

#include <eez/gui/page.h>

using namespace eez;
using namespace eez::gui;
using namespace eez::flow;
using namespace flowtest;

namespace eez {
namespace flow {
// flow/components/show_message_box.cpp
void questionCallback(void *userParam, unsigned buttonIndex);
}
}

namespace {

// ShowKeypad hook: records the call, shows the numeric keypad page like an
// application does; the test calls onOk or onCancel
struct KeypadCall {
    int calls;
    std::string label;
    double initialValue;
    double min;
    double max;
    Unit unit;
    float precision;
    void (*onOk)(float);
    void (*onCancel)();
} g_keypad;

void keypadHook(Value label, Value initialValue, Value min, Value max, Unit unit, float precision, void (*onOk)(float), void (*onCancel)()) {
    g_keypad.calls++;
    g_keypad.label = label.getString() ? label.getString() : "";
    g_keypad.initialValue = initialValue.toDouble();
    g_keypad.min = min.toDouble();
    g_keypad.max = max.toDouble();
    g_keypad.unit = unit;
    g_keypad.precision = precision;
    g_keypad.onOk = onOk;
    g_keypad.onCancel = onCancel;
    harness::appContext()->pushPage(PAGE_ID_NUMERIC_KEYPAD);
}

struct KeyboardCall {
    int calls;
    std::string label;
    std::string initialText;
    int minChars;
    int maxChars;
    bool isPassword;
    void (*onOk)(char *);
    void (*onCancel)();
} g_keyboard;

void keyboardHook(Value label, Value initialText, Value minChars, Value maxChars, bool isPassword, void (*onOk)(char *), void (*onCancel)()) {
    g_keyboard.calls++;
    g_keyboard.label = label.getString() ? label.getString() : "";
    g_keyboard.initialText = initialText.getString() ? initialText.getString() : "";
    g_keyboard.minChars = minChars.getInt();
    g_keyboard.maxChars = maxChars.getInt();
    g_keyboard.isPassword = isPassword;
    g_keyboard.onOk = onOk;
    g_keyboard.onCancel = onCancel;
    harness::appContext()->pushPage(PAGE_ID_NUMERIC_KEYPAD);
}

void installDialogHooks() {
    flow::showKeypadHook = keypadHook;
    flow::showKeyboardHook = keyboardHook;
}

// the button of the active toast page (Close)
void clickToastButton() {
    REQUIRE(harness::appContext()->getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE);
    ((ToastMessagePage *)harness::appContext()->getActivePage())->onEncoderClicked();
}

WidgetCursor widgetCursorOf(int pageId, uint16_t widgetType) {
    WidgetCursor widgetCursor;
    widgetCursor.assets = g_mainAssets;
    widgetCursor.appContext = harness::appContext();
    widgetCursor.flowState = pageFlowState(pageId);
    widgetCursor.widget = findWidget(pageId, widgetType);
    return widgetCursor;
}

// a touch and its release, with enough frames for the touch pipeline
void tap(int x, int y) {
    harness::touch(x, y);
    harness::frame(4);
    harness::release();
    harness::frame(4);
}

// the position of a button of the active question page
bool findQuestionButton(int buttonIndex, int &x, int &y) {
    auto page = (InternalPage *)harness::appContext()->getActivePage();
    for (int yy = page->y; yy < page->y + page->height; yy += 2) {
        for (int xx = page->x; xx < page->x + page->width; xx += 2) {
            auto widgetCursor = page->findWidgetInternalPage(xx, yy, false);
            if (widgetCursor.widget && getWidgetAction(widgetCursor) == ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON && widgetCursor.cursor == buttonIndex) {
                x = xx + 4;
                y = yy + 4;
                return true;
            }
        }
    }
    return false;
}

void setLayout(int pageId) {
    auto layout = globalVar("flow_g_layout");
    REQUIRE(layout != nullptr);
    *layout = Value(pageId, VALUE_TYPE_INT32);
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// OnEvent

TEST("flow/on_event: page_open fires when the page is shown") {
    showPage(PAGE_ID_FLOW_EVENTS);
    CHECK_LOGGED("evt open");
    CHECK_NOT_LOGGED("evt close");
}

TEST("flow/on_event: page_close fires when another page replaces it") {
    showPage(PAGE_ID_FLOW_EVENTS);
    showPage(PAGE_ID_MAIN);
    CHECK_LOGGED("evt close");
}

TEST("flow/on_event: page_open fires each time the page is shown again") {
    showPage(PAGE_ID_FLOW_EVENTS);
    showPage(PAGE_ID_MAIN);
    showPage(PAGE_ID_FLOW_EVENTS);
    CHECK_EQ(countLogged("evt open"), 2);
}

TEST("flow/on_event: a page pushed over it and popped again fires neither event") {
    showPage(PAGE_ID_FLOW_EVENTS);
    harness::appContext()->pushPage(PAGE_ID_NUMERIC_KEYPAD);
    harness::frame(2);
    harness::appContext()->popPage();
    harness::frame(2);
    CHECK_EQ(countLogged("evt open"), 1);
    CHECK_NOT_LOGGED("evt close");
}

TEST("flow/on_event: the event output gets the event value") {
    auto flowState = pageFlowState(PAGE_ID_FLOW_KEYS);
    onEvent(flowState, FLOW_EVENT_KEYDOWN, Value("a"));
    harness::frame(1);
    CHECK_LOGGED("key a");
}

TEST_BUG("flow/on_event: two events before the next tick are both delivered", "F17") {
    // the event value is kept in one field of the flow state and the second
    // event is not queued because the first one is still in the queue
    auto flowState = pageFlowState(PAGE_ID_FLOW_KEYS);
    onEvent(flowState, FLOW_EVENT_KEYDOWN, Value("a"));
    onEvent(flowState, FLOW_EVENT_KEYDOWN, Value("b"));
    harness::frame(1);
    CHECK_LOGGED("key a");
    CHECK_LOGGED("key b");
}

////////////////////////////////////////////////////////////////////////////////
// ShowPage

TEST("flow/show_page: replaces the active page and continues") {
    runPage(PAGE_ID_FLOW_SHOWPAGE);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_FLOW_TARGET);
    CHECK_LOGGED("showpage done");
    CHECK_LOGGED("target open");
}

////////////////////////////////////////////////////////////////////////////////
// ShowKeypad

TEST("flow/show_keypad: calls the keypad hook with the evaluated properties") {
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYPAD);
    REQUIRE(g_keypad.calls == 1);
    CHECK(g_keypad.label == "Volts");
    CHECK_NEAR(g_keypad.initialValue, 1.5, 1e-6);
    CHECK_NEAR(g_keypad.min, 0, 1e-9);
    CHECK_NEAR(g_keypad.max, 10, 1e-9);
    CHECK_EQ((int)g_keypad.unit, (int)UNIT_VOLT);
    CHECK_NEAR(g_keypad.precision, 0.01, 1e-6);
}

TEST("flow/show_keypad: OK sends the value rounded to the precision to the result output") {
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYPAD);
    REQUIRE(g_keypad.onOk != nullptr);
    g_keypad.onOk(2.346f);
    harness::frame(2);
    CHECK(logIndexPrefix("kp 2.35") != -1);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_MAIN);
}

TEST("flow/show_keypad: Cancel fires the canceled output") {
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYPAD);
    REQUIRE(g_keypad.onCancel != nullptr);
    g_keypad.onCancel();
    harness::frame(2);
    CHECK_LOGGED("kp canceled");
    CHECK_EQ(logIndexPrefix("kp 2"), -1);
}

TEST("flow/show_keypad: the action stays alive while the keypad is open") {
    installDialogHooks();
    auto flowState = runPage(PAGE_ID_FLOW_KEYPAD_ACTION);
    REQUIRE(g_keypad.calls == 1);
    CHECK_EQ(childCount(flowState), 1);
    harness::frame(10);
    CHECK_EQ(childCount(flowState), 1);
    g_keypad.onOk(0.7f);
    harness::frame(2);
    CHECK(logIndexPrefix("akp 0.7") != -1);
    CHECK_LOGGED("akp after");
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/show_keypad: closing the keypad after the flow stopped does not use the freed flow state") {
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYPAD);
    REQUIRE(g_keypad.onCancel != nullptr);
    flow::stop();
    harness::frame(1);
    REQUIRE(isFlowStopped());
    g_keypad.onCancel();
    harness::frame(1);
    CHECK(true);
}

TEST("flow/show_keypad: with two open keypads OK goes to the most recently opened one, then to the first") {
    // the components waiting for a keypad are a stack (the keypad pages are
    // stacked too): an answer goes to the most recently opened keypad
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYPAD);
    REQUIRE(g_keypad.calls == 1);
    runPage(PAGE_ID_FLOW_KEYPAD_ACTION);
    REQUIRE(g_keypad.calls == 2);
    g_keypad.onOk(3.0f);
    harness::frame(2);
    CHECK(logIndexPrefix("akp 3") != -1);
    CHECK_EQ(logIndexPrefix("kp 3"), -1);
    g_keypad.onOk(4.0f);
    harness::frame(2);
    CHECK(logIndexPrefix("kp 4") != -1);
}

TEST("flow/show_keypad: a Precision that fails to evaluate does not keep the action alive") {
    // the Precision is evaluated before the component starts waiting (O4)
    installDialogHooks();
    auto flowState = runPage(PAGE_ID_FLOW_KEYPAD_BAD);
    CHECK_EQ(g_keypad.calls, 0);
    CHECK(errorContains("Precision"));
    CHECK_EQ(childCount(flowState), 0);
}

////////////////////////////////////////////////////////////////////////////////
// ShowKeyboard

TEST("flow/show_keyboard: calls the keyboard hook, the label gets \": \"") {
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYBOARD);
    REQUIRE(g_keyboard.calls == 1);
    CHECK(g_keyboard.label == "Name: ");
    CHECK(g_keyboard.initialText == "abc");
    CHECK_EQ(g_keyboard.minChars, 1);
    CHECK_EQ(g_keyboard.maxChars, 8);
    CHECK(!g_keyboard.isPassword);
}

TEST("flow/show_keyboard: OK sends the text to the result output") {
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYBOARD);
    REQUIRE(g_keyboard.onOk != nullptr);
    char text[] = "xyz";
    g_keyboard.onOk(text);
    harness::frame(2);
    CHECK_LOGGED("kb xyz");
}

TEST("flow/show_keyboard: Cancel fires the canceled output") {
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYBOARD);
    REQUIRE(g_keyboard.onCancel != nullptr);
    g_keyboard.onCancel();
    harness::frame(2);
    CHECK_LOGGED("kb canceled");
}

TEST("flow/show_keyboard: OK after the flow stopped does not use the freed flow state") {
    installDialogHooks();
    runPage(PAGE_ID_FLOW_KEYBOARD);
    REQUIRE(g_keyboard.onOk != nullptr);
    flow::stop();
    harness::frame(1);
    char text[] = "late";
    g_keyboard.onOk(text);
    harness::frame(1);
    CHECK(true);
}

////////////////////////////////////////////////////////////////////////////////
// ShowMessageBox

TEST("flow/show_message_box: an info message is a toast, Close continues the flow") {
    runPage(PAGE_ID_FLOW_MSGBOX_INFO);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    CHECK_NOT_LOGGED("info closed");
    clickToastButton();
    harness::frame(2);
    CHECK_LOGGED("info closed");
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_MAIN);
}

TEST("flow/show_message_box: an error message is a toast, Close continues the flow") {
    runPage(PAGE_ID_FLOW_MSGBOX_ERROR);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    clickToastButton();
    harness::frame(2);
    CHECK_LOGGED("error closed");
}

TEST("flow/show_message_box: a question fires the output of the chosen button") {
    auto flowState = runPage(PAGE_ID_FLOW_MSGBOX_QUESTION);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)INTERNAL_PAGE_ID_QUESTION);
    int index = findComponent(flowState, defs_v3::COMPONENT_TYPE_SHOW_MESSAGE_BOX_ACTION);
    REQUIRE(index != -1);
    REQUIRE(flowState->componenentExecutionStates[index] != nullptr);
    int x, y;
    REQUIRE(findQuestionButton(1, x, y));
    tap(x, y);
    CHECK_LOGGED("q no");
    CHECK_NOT_LOGGED("q yes");
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_MAIN);
    CHECK(flowState->componenentExecutionStates[index] == nullptr);
}

TEST("flow/show_message_box: a question closed without a button fires no output, the component ends") {
    auto flowState = runPage(PAGE_ID_FLOW_MSGBOX_QUESTION);
    REQUIRE(harness::appContext()->getActivePageId() == (int)INTERNAL_PAGE_ID_QUESTION);
    int index = findComponent(flowState, defs_v3::COMPONENT_TYPE_SHOW_MESSAGE_BOX_ACTION);
    REQUIRE(index != -1);
    // closed by code
    harness::appContext()->popPage();
    harness::frame(2);
    CHECK(flowState->componenentExecutionStates[index] == nullptr);
    CHECK_NOT_LOGGED("q no");
    CHECK_NOT_LOGGED("q yes");
    // a late answer is ignored
    questionCallback(nullptr, 1);
    harness::frame(2);
    CHECK_NOT_LOGGED("q no");
}

TEST("flow/show_message_box: a newer toast replaces the toast of a message box, the flow continues") {
    auto flowState = runPage(PAGE_ID_FLOW_MSGBOX_ACTION);
    REQUIRE(harness::appContext()->getActivePageId() == (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    harness::appContext()->errorMessage("module error");
    harness::frame(2);
    CHECK_LOGGED("mb action after");
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/show_message_box: a page pushed over the toast of a message box, the flow continues") {
    auto flowState = runPage(PAGE_ID_FLOW_MSGBOX_ACTION);
    REQUIRE(harness::appContext()->getActivePageId() == (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    harness::appContext()->pushPage(PAGE_ID_NUMERIC_KEYPAD);
    harness::frame(2);
    CHECK_LOGGED("mb action after");
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/show_message_box: the info message toast keeps a computed message") {
    runPage(PAGE_ID_FLOW_MSGBOX_INFO);
    REQUIRE(harness::appContext()->getActivePageId() == (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    auto toast = (ToastMessagePage *)harness::appContext()->getActivePage();
    harness::frame(2);
    CHECK(toast->messageValue.getString() != nullptr);
}

TEST("flow/show_message_box: the action stays alive while the box is open, Close frees it") {
    auto flowState = runPage(PAGE_ID_FLOW_MSGBOX_ACTION);
    CHECK_EQ(childCount(flowState), 1);
    clickToastButton();
    harness::frame(2);
    CHECK_LOGGED("mb action after");
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/show_message_box: Buttons that are not an array is a flow error") {
    runPage(PAGE_ID_FLOW_MSGBOX_BAD);
    CHECK(errorContains("Buttons in ShowMessageBox is not an array"));
    CHECK(harness::appContext()->getActivePageId() != (int)INTERNAL_PAGE_ID_QUESTION);
}

TEST("flow/show_message_box: an error in Buttons does not keep the action alive") {
    // the execution state is allocated (and counted) before Buttons is checked
    auto flowState = runPage(PAGE_ID_FLOW_MSGBOX_BAD);
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/show_message_box: touching outside closes the toast of a message box") {
    runPage(PAGE_ID_FLOW_MSGBOX_ACTION);
    REQUIRE(harness::appContext()->getActivePageId() == (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    tap(5, 5);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_MAIN);
}

TEST("flow/show_message_box: a toast closed by touching outside does not leave the action waiting") {
    // the toast is popped without the callback: the action flow state is never
    // freed and the flow never continues
    auto flowState = runPage(PAGE_ID_FLOW_MSGBOX_ACTION);
    REQUIRE(harness::appContext()->getActivePageId() == (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    tap(5, 5);
    REQUIRE(harness::appContext()->getActivePageId() == (int)PAGE_ID_MAIN);
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/show_message_box: Close after the flow stopped does not use the freed state") {
    runPage(PAGE_ID_FLOW_MSGBOX_INFO);
    flow::stop();
    harness::frame(1);
    REQUIRE(isFlowStopped());
    clickToastButton();
    harness::frame(1);
    CHECK(true);
}

////////////////////////////////////////////////////////////////////////////////
// executeFlowAction: widget event handlers

TEST("flow/execute_flow_action: a click on a button with a flow handler fires its output") {
    showPage(PAGE_ID_FLOW_BUTTON);
    tap(200, 130);
    CHECK_LOGGED("clicked");
}

TEST("flow/execute_flow_action: the handler runs the flow at once (three ticks)") {
    auto widgetCursor = widgetCursorOf(PAGE_ID_FLOW_BUTTON, WIDGET_TYPE_BUTTON);
    REQUIRE(widgetCursor.widget != nullptr);
    REQUIRE(widgetCursor.widget->action < 0);
    executeFlowAction(widgetCursor, widgetCursor.widget->action, nullptr);
    CHECK_LOGGED("clicked");
}

TEST("flow/execute_flow_action: a handler bound to an action runs it with the click event") {
    showPage(PAGE_ID_FLOW_BUTTON_ACTION);
    auto flowState = pageFlowState(PAGE_ID_FLOW_BUTTON_ACTION);
    tap(200, 130);
    CHECK_LOGGED("click action");
    CHECK(logIndexPrefix("click index ") != -1);
    CHECK_EQ(childCount(flowState), 0);
}

TEST("flow/execute_flow_action: a drop-down change fires the ON_CHANGE output") {
    auto widgetCursor = widgetCursorOf(PAGE_ID_FLOW_DROPDOWN, WIDGET_TYPE_DROP_DOWN_LIST);
    REQUIRE(widgetCursor.widget != nullptr);
    int selectedIndex = 1;
    executeFlowAction(widgetCursor, widgetCursor.widget->action, &selectedIndex);
    CHECK_LOGGED("dropdown changed");
}

TEST_BUG("flow/execute_flow_action: a drop-down action without a param does not crash", "F12") {
    // BB3+'s executeExternalActionHook passes nullptr; the param is read as int
    auto widgetCursor = widgetCursorOf(PAGE_ID_FLOW_DROPDOWN, WIDGET_TYPE_DROP_DOWN_LIST);
    REQUIRE(widgetCursor.widget != nullptr);
    executeFlowAction(widgetCursor, widgetCursor.widget->action, nullptr);
    CHECK_LOGGED("dropdown changed");
}

TEST("flow/execute_flow_action: does nothing while the flow is stopped") {
    auto widgetCursor = widgetCursorOf(PAGE_ID_FLOW_BUTTON, WIDGET_TYPE_BUTTON);
    REQUIRE(widgetCursor.widget != nullptr);
    flow::stop();
    harness::frame(1);
    executeFlowAction(widgetCursor, widgetCursor.widget->action, nullptr);
    CHECK_NOT_LOGGED("clicked");
}

////////////////////////////////////////////////////////////////////////////////
// user widgets

TEST("flow/user_widget: every instance has its own flow state with its own properties") {
    showPage(PAGE_ID_FLOW_UW_HOST2);
    CHECK_LOGGED("uw one");
    CHECK_LOGGED("uw two");
    auto flowState = pageFlowState(PAGE_ID_FLOW_UW_HOST2);
    CHECK_EQ(childCount(flowState), 2);
}

TEST("flow/user_widget: the flow state of an instance is a child of the page flow state") {
    showPage(PAGE_ID_FLOW_UW_HOST2);
    auto flowState = pageFlowState(PAGE_ID_FLOW_UW_HOST2);
    REQUIRE(flowState->firstChild != nullptr);
    auto child = flowState->firstChild;
    CHECK(!child->isAction);
    CHECK_EQ((int)child->flowIndex, (int)PAGE_ID_FLOW_UW_A - 1);
    CHECK_EQ(getPageIndex(child), (int)PAGE_ID_FLOW_UW_A - 1);
    CHECK_EQ(getPageIndexIncludeParents(child), (int)PAGE_ID_FLOW_UW_HOST2 - 1);
}

TEST("flow/user_widget: the flow of an instance runs once, not on every frame") {
    showPage(PAGE_ID_FLOW_UW_HOST2, 10);
    CHECK_EQ(countLogged("uw one"), 1);
}

TEST("flow/user_widget: an assignable user property writes to the variable of the host") {
    showPage(PAGE_ID_FLOW_UW_HOST_ASSIGN);
    auto result = globalVar("flow_g_result");
    REQUIRE(result != nullptr);
    CHECK_EQ(result->getInt(), 77);
}

TEST("flow/user_widget: an Input component gets the value wired to the user widget") {
    showPage(PAGE_ID_FLOW_UW_HOST_IN);
    CHECK_LOGGED("uw in 0");
}

TEST("flow/user_widget: an Input component fires again when the wired value changes") {
    showPage(PAGE_ID_FLOW_UW_HOST_IN);
    auto watch = globalVar("flow_g_watch");
    REQUIRE(watch != nullptr);
    *watch = Value(5, VALUE_TYPE_INT32);
    harness::frame(3);
    CHECK_LOGGED("uw in 5");
    CHECK_EQ(countLogged("uw in 0"), 1);
}

TEST("flow/user_widget: the layout from data gets a flow state, a cached one when it comes back") {
    setLayout(PAGE_ID_FLOW_UW_L0);
    showPage(PAGE_ID_FLOW_UW_DYN);
    CHECK_LOGGED("layout 0");
    setLayout(PAGE_ID_FLOW_UW_L1);
    harness::frame(3);
    CHECK_LOGGED("layout 1");
    setLayout(PAGE_ID_FLOW_UW_L0);
    harness::frame(3);
    CHECK_EQ(countLogged("layout 0"), 1);
    CHECK_EQ(childCount(pageFlowState(PAGE_ID_FLOW_UW_DYN)), 2);
}

TEST("flow/user_widget: the flow of a cached layout keeps running") {
    // O17 notes the cost; this pins it: the Watch of the hidden layout 0 fires
    setLayout(PAGE_ID_FLOW_UW_L0);
    showPage(PAGE_ID_FLOW_UW_DYN);
    setLayout(PAGE_ID_FLOW_UW_L1);
    harness::frame(3);
    auto watch = globalVar("flow_g_watch");
    REQUIRE(watch != nullptr);
    *watch = Value(5, VALUE_TYPE_INT32);
    harness::frame(3);
    CHECK_LOGGED("w1 5");
    CHECK_LOGGED("w0 5");
}

TEST("flow/user_widget: nine layouts are kept without freeing any") {
    showPage(PAGE_ID_FLOW_UW_DYN, 1);
    for (int k = 0; k < 9; k++) {
        setLayout(PAGE_ID_FLOW_UW_L0 + k);
        harness::frame(2);
    }
    setLayout(PAGE_ID_FLOW_UW_L0);
    harness::frame(2);
    CHECK_EQ(countLogged("layout 0"), 1);
    CHECK_EQ(childCount(pageFlowState(PAGE_ID_FLOW_UW_DYN)), 9);
}

TEST("flow/user_widget: a tenth layout evicts the least recently used one") {
    // only the evicted layout is freed, not the execution state of the user
    // widget that holds the cache (F3)
    showPage(PAGE_ID_FLOW_UW_DYN, 1);
    for (int k = 0; k < 10; k++) {
        setLayout(PAGE_ID_FLOW_UW_L0 + k);
        harness::frame(2);
    }
    CHECK_LOGGED("layout 9");
    CHECK_EQ(childCount(pageFlowState(PAGE_ID_FLOW_UW_DYN)), 9);
}

TEST("flow/user_widget: stop() with two cached layouts frees each flow state once") {
    // freeing the first layout frees the whole execution state (both layouts):
    // freeAllChildrenFlowStates must not continue with the freed second one (F3)
    setLayout(PAGE_ID_FLOW_UW_L0);
    showPage(PAGE_ID_FLOW_UW_DYN);
    setLayout(PAGE_ID_FLOW_UW_L1);
    harness::frame(3);
    REQUIRE(childCount(pageFlowState(PAGE_ID_FLOW_UW_DYN)) == 2);
    flow::stop();
    harness::frame(1);
    CHECK(isFlowStopped());
}
