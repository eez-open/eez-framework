/*
 * eez-framework tests: per-frame and per-touch work of the GUI runtime (the
 * performance findings FP1, G18 and G19 of the eez-framework review).
 *
 * The tests print what they measure ("perf: ..."), run.sh --verbose shows it.
 */

#include <stdio.h>
#include <string.h>

#include <chrono>
#include <thread>

#include <eez/flow/flow.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/gui/thread.h>

#include "gui_test.h"
#include "../widgets/wtest.h"

#if defined(__SANITIZE_ADDRESS__)
// sanitizer/allocator_interface.h (not installed with every compiler)
extern "C" int __sanitizer_install_malloc_and_free_hooks(
    void (*mallocHook)(const volatile void *, size_t),
    void (*freeHook)(const volatile void *));
#define PERF_COUNT_ALLOCATIONS 1
#else
#define PERF_COUNT_ALLOCATIONS 0
#endif

using namespace gui_test;

namespace eez {
namespace gui {
// gui/keypad.cpp
void data_keypad_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
}
}

static void showPageA() {
    app()->showPage(PAGE_ID_GUI_PAGE_A);
    harness::frame(2);
    harness::advance(400);
}

// color of the presented pixel without alpha (0xBBGGRR)
static uint32_t pixelRgb(int x, int y) {
    return harness::pixel(x, y) & 0xFFFFFF;
}

static const uint32_t ERROR = 0x0000C0; // #c00000, the active style of page_a's buttons

////////////////////////////////////////////////////////////////////////////////
// allocations (AddressSanitizer malloc hooks; the hooks can't be removed, every
// test runs in its own process)

#if PERF_COUNT_ALLOCATIONS
static int g_allocations;
static bool g_allocationHooksInstalled;

static void mallocHook(const volatile void *, size_t) {
    g_allocations++;
}

static void freeHook(const volatile void *) {
}

static void installAllocationHooks() {
    if (!g_allocationHooksInstalled) {
        __sanitizer_install_malloc_and_free_hooks(mallocHook, freeHook);
        g_allocationHooksInstalled = true;
    }
}

static int allocationsInFrames(int frames) {
    installAllocationHooks();
    int start = g_allocations;
    harness::frame(frames);
    return g_allocations - start;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// G19: popups are drawn only when they change

static int g_questionCallbackCount;

static void questionCallback(void *, unsigned) {
    g_questionCallbackCount++;
}

static Value makeButtons(int n, const char **labels) {
    Value buttons = Value::makeArrayRef(n, flow::defs_v3::ARRAY_TYPE_STRING, 0);
    for (int i = 0; i < n; i++) {
        buttons.getArray()->values[i] = Value(labels[i]);
    }
    return buttons;
}

// the center of the button of an internal page with the action and cursor
static bool internalButtonPosition(int action, int cursor, int &x, int &y) {
    auto page = (InternalPage *)app()->getActivePage();
    for (int yy = page->y; yy < page->y + page->height; yy += 2) {
        for (int xx = page->x; xx < page->x + page->width; xx += 2) {
            auto widgetCursor = page->findWidgetInternalPage(xx, yy, false);
            if (widgetCursor.widget && getWidgetAction(widgetCursor) == action && widgetCursor.cursor == cursor) {
                x = xx + 4;
                y = yy + 4;
                return true;
            }
        }
    }
    return false;
}

// Frames with drawing while the popup is open and nothing changes and while
// its first button is pressed; then the button is released.
static void checkPopupDrawing(const char *name, int action) {
    harness::frame(3);
    harness::advance(400);

    int idle = wt::framesWithDrawing(10);

    int x, y;
    REQUIRE(internalButtonPosition(action, 0, x, y));

    harness::touch(x, y);
    int pressed = wt::framesWithDrawing(6);

    printf("perf: %s: frames with drawing, 10 idle frames: %d, 6 frames pressed: %d\n", name, idle, pressed);

    CHECK_EQ(idle, 0);
    // drawn when the button becomes active, then not again
    CHECK_EQ(pressed, 1);

    harness::release();
    harness::frame(4);
}

TEST("perf/gui: an open question page is drawn only when it changes") {
    static const char *labels[] = { "Yes", "No" };
    showPageA();
    app()->questionDialog(Value("Delete?"), makeButtons(2, labels), nullptr, questionCallback);
    checkPopupDrawing("question", ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON);
    CHECK_EQ(g_questionCallbackCount, 1);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("perf/gui: an open menu with buttons is drawn only when it changes") {
    static const char *items[] = { "Copy", "Move", nullptr };
    static int menuCallbackCount;
    showPageA();
    showMenu(app(), "File", MENU_TYPE_BUTTON, items, [](int) {
        menuCallbackCount++;
    });
    checkPopupDrawing("menu", ACTION_ID_INTERNAL_MENU_WITH_BUTTONS);
    CHECK_EQ(menuCallbackCount, 1);
    // the menu stays open: released button drawn inactive, the same as a full redraw
    REQUIRE(activePageId() == INTERNAL_PAGE_ID_MENU_WITH_BUTTONS);
    std::string where;
    CHECK_EQ(wt::diffWithFullRedraw(&where), 0);
    CHECK_EQ(wt::framesWithDrawing(5), 0);
}

TEST("perf/gui: a question replaced by a new question shows the new one") {
    static const char *labels1[] = { "Yes", "No" };
    static const char *labels2[] = { "OK" };
    showPageA();
    app()->questionDialog(Value("First question"), makeButtons(2, labels1), nullptr, questionCallback);
    harness::frame(3);
    app()->questionDialog(Value("Second"), makeButtons(1, labels2), nullptr, questionCallback);
    harness::frame(3);
    std::string where;
    CHECK_EQ(wt::diffWithFullRedraw(&where), 0);
    int x, y;
    CHECK(internalButtonPosition(ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON, 0, x, y));
    CHECK(!internalButtonPosition(ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON, 1, x, y));
}

////////////////////////////////////////////////////////////////////////////////
// G19: the keypad text value is made only when the text changes

static Keypad g_textKeypad;

static void startTextKeypad(const char *text, bool password = false) {
    g_textKeypad.start(app(), nullptr, text, 0, 20, password, [](char *) { app()->popPage(); }, nullptr, nullptr);
    app()->pushPage(PAGE_ID_GUI_KEYPAD_TEXT, &g_textKeypad);
    harness::frame(2);
    harness::advance(400);
    REQUIRE(getActiveKeypad() == &g_textKeypad);
}

static Value keypadTextData() {
    Value value;
    data_keypad_text(DATA_OPERATION_GET, WidgetCursor(), value);
    return value;
}

TEST("perf/gui: the keypad text value is the same value until the text changes") {
    startTextKeypad("abc");
    Value first = keypadTextData();
    Value second = keypadTextData();
    CHECK_STR(first.getString(), "abc");
    // the same string object, no new one
    CHECK(first.getType() == VALUE_TYPE_STRING_REF && first.refValue == second.refValue);
    g_textKeypad.key('d');
    Value third = keypadTextData();
    CHECK_STR(third.getString(), "abcd");
    CHECK(third.refValue != first.refValue);
    // the old value is still valid for the one who keeps it
    CHECK_STR(first.getString(), "abc");
}

TEST("perf/gui: the password keypad text hides the last character after 500 ms") {
    startTextKeypad("", true);
    g_textKeypad.key('a');
    g_textKeypad.key('b');
    Value first = keypadTextData();
    Value second = keypadTextData();
    CHECK_STR(first.getString(), "*b");
    CHECK_STR(second.getString(), "*b");
    harness::advance(501);
    Value third = keypadTextData();
    CHECK_STR(third.getString(), "**");
}

TEST("perf/gui: the keypad text value is released with the keypad page") {
    startTextKeypad("abc");
    Value value = keypadTextData();
    REQUIRE(value.getType() == VALUE_TYPE_STRING_REF);
    uint32_t refsWithKeypad = value.refValue->refCounter;
    app()->popPage();
    harness::frame(2);
    // only this copy is left
    CHECK_EQ(value.refValue->refCounter, 1u);
    printf("perf: keypad text references while the keypad is shown: %u\n", (unsigned)refsWithKeypad);
}

#if PERF_COUNT_ALLOCATIONS
TEST("perf/gui: allocations per frame with an open keypad") {
    // the same page without a keypad: allocations of the frame loop itself
    showPageA();
    harness::frame(10);
    int withoutKeypad = allocationsInFrames(60);

    startTextKeypad("abc");
    harness::frame(10);
    int withKeypad = allocationsInFrames(60);

    installAllocationHooks();
    int start = g_allocations;
    for (int i = 0; i < 100; i++) {
        keypadTextData();
    }
    int perGet = (g_allocations - start) / 100;

    printf("perf: allocations in 60 frames: page_a %d, open text keypad %d; per keypad text GET %d\n", withoutKeypad, withKeypad, perGet);

    CHECK_EQ(perGet, 0);
    CHECK(withKeypad <= withoutKeypad + 2);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// G19: isWidgetActionEnabled once per touch candidate

struct CountingAppContext : public AppContext {
    int isWidgetActionEnabledCount = 0;
    int getMainPageId() override {
        return PAGE_ID_MAIN;
    }
    bool isWidgetActionEnabled(const WidgetCursor &widgetCursor) override {
        isWidgetActionEnabledCount++;
        return AppContext::isWidgetActionEnabled(widgetCursor);
    }
};

static int isWidgetActionEnabledCalls(int x, int y, OnTouchFunctionType *touchFunction = nullptr) {
    static CountingAppContext countingAppContext;
    auto widgetCursor = findWidget(x, y, false);
    REQUIRE(widgetCursor.widget != nullptr);
    widgetCursor.appContext = &countingAppContext;
    countingAppContext.isWidgetActionEnabledCount = 0;
    auto f = getWidgetTouchFunction(widgetCursor);
    if (touchFunction) {
        *touchFunction = f;
    }
    return countingAppContext.isWidgetActionEnabledCount;
}

TEST("perf/gui: getWidgetTouchFunction calls isWidgetActionEnabled once") {
    showPageA();
    OnTouchFunctionType enabledButton, disabledButton, textWidget;

    int enabledCalls = isWidgetActionEnabledCalls(A_BTN1_X, A_BTN1_Y, &enabledButton);

    g_enabled = false;
    int disabledCalls = isWidgetActionEnabledCalls(A_DISABLED_X, A_DISABLED_Y, &disabledButton);
    g_enabled = true;

    app()->showPage(PAGE_ID_GUI_PAGE_B);
    harness::frame(2);
    // the text of page_b: no action
    int textCalls = isWidgetActionEnabledCalls(400, 40, &textWidget);

    printf("perf: isWidgetActionEnabled calls per getWidgetTouchFunction: enabled button %d, disabled button %d, text %d\n",
        enabledCalls, disabledCalls, textCalls);

    CHECK_EQ(enabledCalls, 1);
    CHECK_EQ(disabledCalls, 1);
    CHECK(textCalls <= 1);
    CHECK(enabledButton != nullptr);
    CHECK(disabledButton == nullptr);
    CHECK(textWidget == nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// G19: executeAction doesn't sleep

TEST("perf/gui: executeAction doesn't wait") {
    showPageA();
    auto widgetCursor = findWidget(A_BTN1_X, A_BTN1_Y, false);
    REQUIRE(widgetCursor.widget != nullptr);
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; i++) {
        executeAction(widgetCursor, ACTION_ID_GUI_CLICK_A);
    }
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    printf("perf: 100 executeAction calls: %.1f ms\n", ms);
    CHECK_EQ(g_actions.clickA, 100);
    CHECK(ms < 50);
}

////////////////////////////////////////////////////////////////////////////////
// G18: touch to action latency in frames

// frames until cond() is true (at most max)
template <typename Cond>
static int framesUntil(Cond cond, int max = 10) {
    int n = 0;
    while (!cond() && n < max) {
        harness::frame();
        n++;
    }
    return n;
}

TEST("perf/gui: a touch is processed in the frame in which it is read") {
    showPageA();

    harness::touch(A_BTN1_X, A_BTN1_Y);
    int framesToPress = framesUntil([] { return g_activeWidget.widget != nullptr; });
    int framesToPressedDrawn = framesToPress + framesUntil([] { return pixelRgb(A_BTN1_X - 60, A_BTN1_Y) == ERROR; });

    harness::release();
    int framesToAction = framesUntil([] { return g_actions.clickA == 1; });

    printf("perf: frames from touch down to the pressed widget: %d, to the pressed widget presented: %d; from touch up to the action: %d\n",
        framesToPress, framesToPressedDrawn, framesToAction);

    CHECK_EQ(framesToPress, 1);
    CHECK_EQ(framesToPressedDrawn, 2);
    CHECK_EQ(framesToAction, 1);
}

TEST("perf/gui: a page changed by a tap is presented in the next frame") {
    showPageA();
    g_onAction = [](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_A) {
            app()->showPage(PAGE_ID_GUI_PAGE_B);
        }
    };
    harness::touch(A_BTN1_X, A_BTN1_Y);
    harness::frame(4);
    harness::release();
    int framesToPageChange = framesUntil([] { return activePageId() == PAGE_ID_GUI_PAGE_B; });
    // A1 is gone: the pixel left of its text is the page background
    int framesToPresented = framesToPageChange + framesUntil([] { return pixelRgb(A_BTN1_X - 60, A_BTN1_Y) != 0xC06020 && pixelRgb(A_BTN1_X - 60, A_BTN1_Y) != ERROR; });
    printf("perf: frames from touch up to the page change: %d, to the new page presented: %d\n", framesToPageChange, framesToPresented);
    CHECK_EQ(framesToPageChange, 1);
    CHECK_EQ(framesToPresented, 2);
}

TEST("perf/gui: the pressed list item is drawn before its action is executed") {
    // a tap on a list item: the touch down is postponed (the list can take over
    // the touch), sent at touch up, the touch up one frame later, so the
    // pressed item is presented once
    app()->showPage(PAGE_ID_GUI_LISTS);
    harness::frame(2);
    harness::advance(400);
    harness::touch(200, 90);
    harness::frame();
    harness::release();
    harness::frame();
    // touch down sent, touch up postponed
    CHECK(g_activeWidget.widget != nullptr);
    CHECK_EQ(g_actions.listItem, 0);
    harness::frame();
    // the frame with the pressed item is drawn, then the action is executed
    CHECK(wt::drawn());
    CHECK_EQ(g_actions.listItem, 1);
    CHECK(g_activeWidget.widget == nullptr);
}

TEST("perf/gui: touch events sent from another thread go through the GUI queue") {
    showPageA();
    bool processedOnTheOtherThread = true;
    std::thread thread([&processedOnTheOtherThread]() {
        Event event;
        event.type = EVENT_TYPE_TOUCH_DOWN;
        event.x = A_BTN1_X;
        event.y = A_BTN1_Y;
        sendTouchEventToGuiThread(event);
        event.type = EVENT_TYPE_TOUCH_UP;
        sendTouchEventToGuiThread(event);
        processedOnTheOtherThread = g_actions.clickA != 0;
    });
    thread.join();
    CHECK(!processedOnTheOtherThread);
    CHECK_EQ(g_actions.clickA, 0);
    harness::frame(2);
    CHECK_EQ(g_actions.clickA, 1);
}

TEST("perf/gui: touch events are processed in their order") {
    showPageA();
    // two taps on two buttons, each with one frame pressed and one released
    harness::touch(A_BTN1_X, A_BTN1_Y);
    harness::frame();
    harness::release();
    harness::frame();
    harness::advance(400);
    harness::touch(A_BTN2_X, A_BTN2_Y);
    harness::frame();
    harness::release();
    harness::frame();
    harness::frame(4);
    CHECK_EQ(g_actions.clickA, 1);
    CHECK_EQ(g_actions.clickB, 1);
}

////////////////////////////////////////////////////////////////////////////////
// FP1: the state management hook runs before the flow tick

static void (*g_savedStateManagment)();
static bool g_stopRequested;

TEST("perf/gui: the state management hook runs before the flow tick of the frame") {
    // a stop requested by the state management (e.g. BB3+ stops the flows of a
    // module after an error) is done by the flow tick of the same frame
    REQUIRE(!flow::isFlowStopped());
    g_savedStateManagment = g_hooks.stateManagment;
    g_hooks.stateManagment = []() {
        g_savedStateManagment();
        if (!g_stopRequested) {
            g_stopRequested = true;
            flow::stop();
        }
    };
    harness::frame();
    REQUIRE(g_stopRequested);
    CHECK(flow::isFlowStopped());
}
