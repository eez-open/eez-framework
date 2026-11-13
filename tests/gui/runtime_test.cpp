/*
 * eez-framework tests: gui/gui.cpp (actions, override style rules, blink),
 * gui/data.cpp, gui/thread.cpp (the GUI queue and other threads),
 * gui/update.cpp, gui/overlay.cpp and gui/animation.cpp.
 */

#include <stdlib.h>
#include <string.h>
#include <string>
#include <atomic>
#include <thread>
#include <vector>

#include <eez/gui/thread.h>

#include "gui_test.h"

using namespace gui_test;

namespace eez {
namespace gui {
// gui/gui.cpp
bool isInternalAction(int actionId);
// gui/animation.cpp (not in animation.h)
void animateOpenCloseCallback(float t, VideoBuffer bufferOld, VideoBuffer bufferNew, VideoBuffer bufferDst);
void animateRectsStep(float t, VideoBuffer bufferOld, VideoBuffer bufferNew, VideoBuffer bufferDst);
}
}

static uint32_t rgb(int x, int y) {
    return harness::pixel(x, y) & 0xFFFFFF;
}

////////////////////////////////////////////////////////////////////////////////
// gui.cpp

TEST("gui/gui: executeAction of a native action calls it, of no action does nothing") {
    executeAction(WidgetCursor(), ACTION_ID_GUI_CLICK_B);
    CHECK_EQ(g_actions.clickB, 1);
    executeAction(WidgetCursor(), EEZ_ACTION_ID_NONE);
    CHECK_EQ(g_actions.clickB, 1);
}

TEST("gui/gui: an internal action is recognized by its id") {
    CHECK(isInternalAction(ACTION_ID_INTERNAL_DIALOG_CLOSE));
    CHECK(!isInternalAction(ACTION_ID_GUI_CLICK_A));
}

TEST("gui/gui: the internal dialog close action pops the page") {
    app()->pushPage(PAGE_ID_GUI_DIALOG);
    executeAction(WidgetCursor(), ACTION_ID_INTERNAL_DIALOG_CLOSE);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/gui: the blink opacity fades out and in in every 800 ms") {
    // g_blinkOpacity: 255 -> 0 in the first 400 ms, 0 -> 255 in the next 400 ms,
    // updated by guiTick
    auto expected = []() {
        uint32_t t = harness::now() % 800;
        return (int)(t < 400 ? 255 - t * 255 / 400 : (t - 400) * 255 / 400);
    };
    harness::advance(800 - harness::now() % 800 + 100);
    harness::frame(1);
    CHECK_EQ((int)g_blinkOpacity, expected());
    CHECK(g_blinkOpacity < 255 && g_blinkOpacity > 100);
    harness::frame(17);
    CHECK_EQ((int)g_blinkOpacity, expected());
    CHECK(g_blinkOpacity < 100);
    harness::frame(20);
    CHECK_EQ((int)g_blinkOpacity, expected());
    CHECK(g_blinkOpacity > 100);
}

TEST("gui/gui: an override style rule replaces the style") {
    setOverrideStyleRule(STYLE_ID_GUI_BUTTON, STYLE_ID_GUI_POPUP);
    CHECK_EQ(overrideStyle(WidgetCursor(), STYLE_ID_GUI_BUTTON), (int)STYLE_ID_GUI_POPUP);
    CHECK_EQ(overrideStyle(WidgetCursor(), STYLE_ID_GUI_KEY), (int)STYLE_ID_GUI_KEY);
}

TEST("gui/gui: an override style rule is changed by a new rule for the same style") {
    setOverrideStyleRule(STYLE_ID_GUI_BUTTON, STYLE_ID_GUI_POPUP);
    setOverrideStyleRule(STYLE_ID_GUI_BUTTON, STYLE_ID_GUI_KEY);
    CHECK_EQ(overrideStyle(WidgetCursor(), STYLE_ID_GUI_BUTTON), (int)STYLE_ID_GUI_KEY);
}

TEST_BUG("gui/gui: two override style rules for two styles both work", "G10") {
    setOverrideStyleRule(STYLE_ID_GUI_BUTTON, STYLE_ID_GUI_POPUP);
    setOverrideStyleRule(STYLE_ID_GUI_KEY, STYLE_ID_GUI_ITEM);
    CHECK_EQ(overrideStyle(WidgetCursor(), STYLE_ID_GUI_BUTTON), (int)STYLE_ID_GUI_POPUP);
    CHECK_EQ(overrideStyle(WidgetCursor(), STYLE_ID_GUI_KEY), (int)STYLE_ID_GUI_ITEM);
}

TEST("gui/gui: the overrideStyle hook gets the style after the rules") {
    static int hookStyle;
    g_hooks.overrideStyle = [](const WidgetCursor &, int styleId) {
        hookStyle = styleId;
        return (int)STYLE_ID_GUI_ITEM;
    };
    setOverrideStyleRule(STYLE_ID_GUI_BUTTON, STYLE_ID_GUI_POPUP);
    CHECK_EQ(overrideStyle(WidgetCursor(), STYLE_ID_GUI_BUTTON), (int)STYLE_ID_GUI_ITEM);
    CHECK_EQ(hookStyle, (int)STYLE_ID_GUI_POPUP);
}

TEST("gui/gui: an override style rule changes the drawn widgets") {
    // key "a" of the text keypad: a text widget, style gui_key (accent), at 0, 80, 98x78
    app()->showPage(PAGE_ID_GUI_KEYPAD_TEXT);
    harness::frame(2);
    CHECK_EQ(rgb(8, 90), 0xC06020u); // accent
    setOverrideStyleRule(STYLE_ID_GUI_KEY, STYLE_ID_GUI_POPUP);
    refreshScreen();
    harness::frame(2);
    CHECK_EQ(rgb(8, 90), 0x202020u); // backdrop
}

////////////////////////////////////////////////////////////////////////////////
// data.cpp

TEST("gui/data: get and set of a native variable") {
    set(WidgetCursor(), DATA_ID_GUI_SWITCH, Value(1, VALUE_TYPE_INT32));
    CHECK(g_switch);
    CHECK_EQ(get(WidgetCursor(), DATA_ID_GUI_SWITCH).getInt(), 1);
    g_switch = false;
    CHECK_EQ(get(WidgetCursor(), DATA_ID_GUI_SWITCH).getInt(), 0);
}

TEST("gui/data: count, min and max of a data item") {
    g_nameData = [](DataOperationEnum operation, Value &value) {
        if (operation == DATA_OPERATION_COUNT) {
            value = 7;
        } else if (operation == DATA_OPERATION_GET_MIN) {
            value = Value(1.0f, UNIT_VOLT);
        } else if (operation == DATA_OPERATION_GET_MAX) {
            value = Value(40.0f, UNIT_VOLT);
        }
    };
    CHECK_EQ(count(WidgetCursor(), DATA_ID_GUI_NAME), 7);
    CHECK_EQ(getMin(WidgetCursor(), DATA_ID_GUI_NAME).getFloat(), 1.0f);
    CHECK_EQ(getMax(WidgetCursor(), DATA_ID_GUI_NAME).getFloat(), 40.0f);
    // no answer: 0 items
    g_nameData = nullptr;
    CHECK_EQ(count(WidgetCursor(), DATA_ID_GUI_NAME), 0);
}

TEST("gui/data: getName of a static string") {
    g_nameData = [](DataOperationEnum operation, Value &value) {
        if (operation == DATA_OPERATION_GET_NAME) {
            value = "Voltage";
        }
    };
    CHECK_STR(getName(WidgetCursor(), DATA_ID_GUI_NAME), "Voltage");
}

TEST_BUG("gui/data: getName of a string ref returns a valid string", "G17@san") {
    g_nameData = [](DataOperationEnum operation, Value &value) {
        if (operation == DATA_OPERATION_GET_NAME) {
            value = Value::makeStringRef("Voltage", -1, 0);
        }
    };
    const char *name = getName(WidgetCursor(), DATA_ID_GUI_NAME);
    // the freed memory is used again
    char *other = (char *)malloc(8);
    strcpy(other, "Xxxxxxx");
    CHECK_STR(name, "Voltage");
    ::free(other);
}

TEST("gui/data: isValidValue returns nullptr for a valid value") {
    g_nameData = nullptr;
    CHECK(isValidValue(WidgetCursor(), DATA_ID_GUI_NAME, Value(5)) == nullptr);
}

TEST_BUG("gui/data: isValidValue with a string ref error message returns a valid string", "G17@san") {
    g_nameData = [](DataOperationEnum operation, Value &value) {
        if (operation == DATA_OPERATION_IS_VALID_VALUE) {
            value = Value::makeStringRef("Out of range", -1, 0);
        }
    };
    const char *message = isValidValue(WidgetCursor(), DATA_ID_GUI_NAME, Value(5));
    char *other = (char *)malloc(13);
    strcpy(other, "Xxx xx xxxxx");
    CHECK_STR(message, "Out of range");
    ::free(other);
}

TEST("gui/data: getWidgetLabel prefers the widget label of an enum item") {
    static EnumItem items[] = {
        { 1, "One (menu)", "One" },
        { 2, "Two", nullptr },
        { 0, nullptr, nullptr }
    };
    CHECK_STR(getWidgetLabel(items, 1), "One");
    CHECK_STR(getWidgetLabel(items, 2), "Two");
    CHECK(getWidgetLabel(items, 3) == nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// thread.cpp

TEST("gui/thread: the test thread is the GUI thread, another thread is not") {
    CHECK(isGuiThread());
    bool other = true;
    std::thread thread([&other]() { other = isGuiThread(); });
    thread.join();
    CHECK(!other);
}

// The queue always has the VSYNC message of the next frame (harness.cpp), so a
// message put now is processed in the second frame.
static void processQueuedMessages() {
    harness::frame(2);
}

TEST("gui/thread: pushPage from another thread is done in the GUI thread") {
    std::thread thread([]() { app()->pushPage(PAGE_ID_GUI_PAGE_A); });
    thread.join();
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
    harness::frame(1);
    // still not: the VSYNC message of this frame was before it in the queue
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
    harness::frame(1);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
}

TEST("gui/thread: showPage from another thread is done in the GUI thread") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    std::thread thread([]() { app()->showPage(PAGE_ID_GUI_PAGE_B); });
    thread.join();
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    processQueuedMessages();
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_B);
    CHECK_EQ(app()->getNumPagesOnStack(), 1);
}

TEST("gui/thread: page changes from another thread are done in their order") {
    std::thread thread([]() {
        app()->pushPage(PAGE_ID_GUI_PAGE_A);
        app()->pushPage(PAGE_ID_GUI_PAGE_B);
        app()->showPage(PAGE_ID_GUI_DIALOG);
        app()->pushPage(PAGE_ID_GUI_POPUP);
    });
    thread.join();
    processQueuedMessages();
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_POPUP);
    CHECK_EQ(app()->getPreviousPageId(), (int)PAGE_ID_GUI_DIALOG);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
}

TEST("gui/thread: an info toast from another thread is shown by the GUI thread") {
    std::thread thread([]() { app()->infoMessage("from another thread"); });
    thread.join();
    processQueuedMessages();
    CHECK_EQ(activePageId(), (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
}

TEST("gui/thread: a toast from another thread doesn't change the toast shown by the GUI thread") {
    // a toast asked for on another thread is made on the GUI thread (G2): the
    // toast shown now is not touched until then
    app()->infoMessage("first");
    harness::frame(1);
    std::thread thread([]() { app()->errorMessage("second"); });
    thread.join();
    auto toast = (ToastMessagePage *)app()->getActivePage();
    CHECK_STR(toast->messageValue.getString(), "first");
    CHECK(!toast->hasAction());
}

TEST("gui/thread: yesNoDialog from another thread doesn't change the message shown by the GUI thread") {
    app()->yesNoDialog(PAGE_ID_GUI_YES_NO, "first", nullptr, nullptr, nullptr);
    harness::frame(1);
    std::thread thread([]() { app()->yesNoDialog(PAGE_ID_GUI_YES_NO, "second", nullptr, nullptr, nullptr); });
    thread.join();
    CHECK_STR(g_alertMessage.getString(), "first");
}

#if defined(__SANITIZE_THREAD__)
// Only in the ThreadSanitizer build (run.sh --tsan tsan): the other thread asks
// for toasts while the GUI thread draws them; they are made on the GUI thread
// (G2), so there is no data race on the toast page and the font of the display.
TEST("gui/thread: tsan: toasts from another thread while the GUI thread draws") {
    std::atomic<bool> done(false);
    app()->infoMessage("first");
    std::thread thread([&done]() {
        for (int i = 0; i < 20; i++) {
            app()->errorMessage("module error");
            app()->infoMessage("screenshot saved");
        }
        done = true;
    });
    while (!done) {
        harness::frame(1);
    }
    thread.join();
    harness::frame(5);
}
#endif

TEST("gui/thread: an application message goes to the onGuiQueueMessage hook") {
    static int receivedType;
    static int receivedParam;
    g_hooks.onGuiQueueMessage = [](uint8_t type, int16_t param) {
        receivedType = type;
        receivedParam = param;
    };
    std::thread thread([]() { sendMessageToGuiThread(GUI_QUEUE_MESSAGE_KEY_DOWN + 10, 1234, 0); });
    thread.join();
    processQueuedMessages();
    CHECK_EQ(receivedType, GUI_QUEUE_MESSAGE_KEY_DOWN + 10);
    CHECK_EQ(receivedParam, 1234);
}

TEST("gui/thread: the refresh screen message redraws the page") {
    harness::frame(2);
    std::thread thread([]() { sendMessageToGuiThread(GUI_QUEUE_MESSAGE_REFRESH_SCREEN, 0, 0); });
    thread.join();
    harness::frame(1);
    CHECK(!g_widgetStateStructureChanged);
    harness::frame(1);
    CHECK(g_widgetStateStructureChanged);
}

////////////////////////////////////////////////////////////////////////////////
// update.cpp

TEST("gui/update: the widget states change on the frame after a page change, not after it") {
    harness::frame(2);
    CHECK(!g_widgetStateStructureChanged);
    app()->showPage(PAGE_ID_GUI_PAGE_A);
    harness::frame(1);
    CHECK(g_widgetStateStructureChanged);
    harness::frame(1);
    CHECK(!g_widgetStateStructureChanged);
}

TEST("gui/update: refreshScreen draws the page again") {
    app()->showPage(PAGE_ID_GUI_PAGE_A);
    harness::frame(2);
    refreshScreen();
    harness::frame(1);
    CHECK(g_widgetStateStructureChanged);
    CHECK_EQ(rgb(A_BTN1_X - 60, A_BTN1_Y), 0xC06020u);
}

TEST("gui/update: the new page is drawn on the frame after showPage") {
    app()->showPage(PAGE_ID_GUI_PAGE_A);
    harness::frame(1);
    CHECK_EQ(rgb(A_BTN1_X - 60, A_BTN1_Y), 0xC06020u);
    app()->showPage(PAGE_ID_GUI_PAGE_B);
    harness::frame(1);
    CHECK_EQ(rgb(A_BTN1_X - 60, A_BTN1_Y), 0u);
    CHECK_EQ(rgb(B_BTN1_X, B_BTN1_Y - 25), 0xC06020u);
}

////////////////////////////////////////////////////////////////////////////////
// overlay.cpp

static void showOverlay() {
    g_overlay = Overlay();
    g_overlay.x = 500;
    g_overlay.y = 300;
    g_overlay.width = 200;
    g_overlay.height = 100;
    g_overlay.state = 1;
    g_overlay.widgetOverrides = nullptr;
    g_overlayEnabled = true;
    app()->showPage(PAGE_ID_GUI_OVERLAY_PAGE);
    harness::frame(2);
    harness::advance(400);
}

TEST("gui/overlay: the overlay is drawn at its position") {
    showOverlay();
    CHECK_EQ(rgb(510, 310), 0x202020u);
    CHECK_EQ(rgb(490, 310), 0u);
    auto widgetCursor = findWidget(600, 350, false);
    REQUIRE(widgetCursor.widget != nullptr);
    CHECK_EQ(getWidgetAction(widgetCursor), (int)ACTION_ID_DRAG_OVERLAY);
}

TEST("gui/overlay: dragging the overlay moves it") {
    showOverlay();
    press(600, 350);
    moveTo(550, 320);
    moveTo(450, 250);
    releaseTouch();
    CHECK(g_overlay.moved);
    CHECK_EQ(g_overlay.xOffsetMaximized, -150);
    CHECK_EQ(g_overlay.yOffsetMaximized, -100);
    harness::frame(2);
    CHECK_EQ(rgb(360, 210), 0x202020u);
    CHECK_EQ(rgb(510, 310), 0u);
    auto widgetCursor = findWidget(450, 250, false);
    REQUIRE(widgetCursor.widget != nullptr);
    CHECK_EQ(getWidgetAction(widgetCursor), (int)ACTION_ID_DRAG_OVERLAY);
}

TEST("gui/overlay: the overlay stays on the display") {
    showOverlay();
    press(600, 350);
    moveTo(300, 200);
    moveTo(10, 10);
    releaseTouch();
    harness::frame(2);
    // container at 500, 300: with the offset the overlay is not left of or above the display
    CHECK(500 + g_overlay.xOffsetMaximized >= 0);
    CHECK(300 + g_overlay.yOffsetMaximized >= 0);
}

TEST("gui/overlay: a hidden overlay is not drawn and not found") {
    showOverlay();
    g_overlay.state = 0;
    harness::frame(2);
    CHECK_EQ(rgb(510, 310), 0u);
    auto widgetCursor = findWidget(600, 350, false);
    CHECK(!widgetCursor.widget || getWidgetAction(widgetCursor) != ACTION_ID_DRAG_OVERLAY);
}

TEST("gui/overlay: a minimized overlay has its own offset") {
    showOverlay();
    g_overlay.visibility = OVERLAY_MINIMIZED;
    press(600, 350);
    moveTo(550, 320);
    moveTo(500, 300);
    releaseTouch();
    CHECK_EQ(g_overlay.xOffsetMinimized, -100);
    CHECK_EQ(g_overlay.xOffsetMaximized, 0);
}

////////////////////////////////////////////////////////////////////////////////
// animation.cpp (the callbacks are called directly with buffers of the test)

static const uint32_t OLD_COLOR = 0xFF111111;
static const uint32_t NEW_COLOR = 0xFF222222;

struct AnimationBuffers {
    // two rows more: animateOpenCloseCallback copies the right and bottom
    // edges inclusive (NEW-gui-4), which is not what these tests check
    std::vector<uint32_t> oldBuffer = std::vector<uint32_t>(DISPLAY_WIDTH * (DISPLAY_HEIGHT + 2), OLD_COLOR);
    std::vector<uint32_t> newBuffer = std::vector<uint32_t>(DISPLAY_WIDTH * (DISPLAY_HEIGHT + 2), NEW_COLOR);
    std::vector<uint32_t> dstBuffer = std::vector<uint32_t>(DISPLAY_WIDTH * (DISPLAY_HEIGHT + 2), 0);

    uint32_t at(int x, int y) {
        return dstBuffer[y * DISPLAY_WIDTH + x];
    }

    // bounding box of the pixels of the color in the visible part of dst
    bool box(uint32_t color, int &x1, int &y1, int &x2, int &y2) {
        x1 = y1 = 100000;
        x2 = y2 = -1;
        for (int y = 0; y < (int)DISPLAY_HEIGHT; y++) {
            for (int x = 0; x < (int)DISPLAY_WIDTH; x++) {
                if (at(x, y) == color) {
                    if (x < x1) x1 = x;
                    if (y < y1) y1 = y;
                    if (x > x2) x2 = x;
                    if (y > y2) y2 = y;
                }
            }
        }
        return x2 >= 0;
    }
};

static void finishAnimation() {
    // animate() started the display animation, no frame runs it
    g_animationState.enabled = false;
}

TEST("gui/animation: the open animation grows the new page from the source rect") {
    AnimationBuffers b;
    animateOpen(Rect{ 350, 200, 100, 80 }, Rect{ 0, 0, 800, 480 });
    animateOpenCloseCallback(0, b.oldBuffer.data(), b.newBuffer.data(), b.dstBuffer.data());
    int x1, y1, x2, y2;
    REQUIRE(b.box(NEW_COLOR, x1, y1, x2, y2));
    CHECK_EQ(x1, 350);
    CHECK_EQ(y1, 200);
    CHECK(x2 >= 449 && x2 <= 450);
    CHECK(y2 >= 279 && y2 <= 280);

    animateOpenCloseCallback(1, b.oldBuffer.data(), b.newBuffer.data(), b.dstBuffer.data());
    REQUIRE(b.box(NEW_COLOR, x1, y1, x2, y2));
    CHECK_EQ(x1, 0);
    CHECK_EQ(y1, 0);
    CHECK_EQ(x2, 799);
    CHECK_EQ(y2, 479);
    finishAnimation();
}

TEST_BUG("gui/animation: the open animation to the full display stays inside the buffers", "NEW-gui-4@san") {
    // the right and bottom edges (x + w, y + h) are passed to bitBlt, which
    // takes them as inclusive
    std::vector<uint32_t> oldBuffer(DISPLAY_WIDTH * DISPLAY_HEIGHT, OLD_COLOR);
    std::vector<uint32_t> newBuffer(DISPLAY_WIDTH * DISPLAY_HEIGHT, NEW_COLOR);
    std::vector<uint32_t> dstBuffer(DISPLAY_WIDTH * DISPLAY_HEIGHT, 0);
    animateOpen(Rect{ 350, 200, 100, 80 }, Rect{ 0, 0, 800, 480 });
    animateOpenCloseCallback(1, oldBuffer.data(), newBuffer.data(), dstBuffer.data());
    finishAnimation();
}

TEST_BUG("gui/animation: the close animation starts with the closed page on the whole display", "G15") {
    // closing to a rect at the bottom: srcY1 uses dx instead of dy (animation.cpp:77)
    AnimationBuffers b;
    animateClose(Rect{ 0, 0, 800, 480 }, Rect{ 300, 400, 200, 80 });
    // in the close direction the closed page is the old buffer
    animateOpenCloseCallback(0, b.oldBuffer.data(), b.newBuffer.data(), b.dstBuffer.data());
    int x1, y1, x2, y2;
    REQUIRE(b.box(OLD_COLOR, x1, y1, x2, y2));
    CHECK_EQ(x1, 0);
    CHECK_EQ(y1, 0);
    finishAnimation();
}

TEST_BUG("gui/animation: the close animation to a centered rect stays centered", "G15") {
    // dy misses the parentheses around dst.y + dst.h (animation.cpp:72-74);
    // dx == dy here, so the dx/dy mixup of line 77 doesn't matter
    AnimationBuffers b;
    animateClose(Rect{ 0, 0, 800, 480 }, Rect{ 200, 200, 400, 80 });
    animateOpenCloseCallback(0.2f, b.oldBuffer.data(), b.newBuffer.data(), b.dstBuffer.data());
    int x1, y1, x2, y2;
    REQUIRE(b.box(OLD_COLOR, x1, y1, x2, y2));
    CHECK_NEAR(x1, 799 - x2, 1);
    CHECK_NEAR(y1, 479 - y2, 1);
    finishAnimation();
}

TEST("gui/animation: the close animation ends at the destination rect") {
    AnimationBuffers b;
    animateClose(Rect{ 0, 0, 800, 480 }, Rect{ 200, 200, 400, 80 });
    animateOpenCloseCallback(1, b.oldBuffer.data(), b.newBuffer.data(), b.dstBuffer.data());
    int x1, y1, x2, y2;
    REQUIRE(b.box(OLD_COLOR, x1, y1, x2, y2));
    CHECK_EQ(x1, 200);
    CHECK_EQ(y1, 200);
    CHECK(x2 >= 599 && x2 <= 600);
    CHECK(y2 >= 279 && y2 <= 280);
    finishAnimation();
}

TEST("gui/animation: a moved rect of the old buffer ends at its destination") {
    AnimationBuffers b;
    for (int y = 0; y < (int)DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < (int)DISPLAY_WIDTH; x++) {
            b.oldBuffer[y * DISPLAY_WIDTH + x] = 0xFF000000 | (y << 12) | x;
        }
    }
    g_animRects[0].buffer = BUFFER_OLD;
    g_animRects[0].srcRect = Rect{ 100, 100, 50, 40 };
    g_animRects[0].dstRect = Rect{ 400, 300, 50, 40 };
    g_animRects[0].opacity = OPACITY_SOLID;
    g_animRects[0].position = POSITION_TOP_LEFT;
    animateRects(app(), BUFFER_NEW, 1);
    animateRectsStep(1, b.oldBuffer.data(), b.newBuffer.data(), b.dstBuffer.data());
    CHECK_EQ(b.at(400, 300), 0xFF000000 | (100 << 12) | 100);
    CHECK_EQ(b.at(449, 339), 0xFF000000 | (139 << 12) | 149);
    CHECK_EQ(b.at(399, 300), NEW_COLOR);
    finishAnimation();
}

TEST_BUG("gui/animation: a solid color rect is clipped at the bottom of the clip rect", "G15") {
    // h -= (y + h) - (clip.y + clip.y) instead of clip.y + clip.h (animation.cpp:221)
    AnimationBuffers b;
    g_animRects[0].buffer = BUFFER_SOLID_COLOR;
    g_animRects[0].srcRect = Rect{ 0, 250, 100, 100 };
    g_animRects[0].dstRect = Rect{ 0, 250, 100, 100 };
    g_animRects[0].color = COLOR_ID_ERROR;
    g_animRects[0].opacity = OPACITY_SOLID;
    g_animRects[0].position = POSITION_TOP_LEFT;
    Rect clip{ 0, 0, 800, 300 };
    animateRects(app(), BUFFER_NEW, 1, 1.0f, &clip);
    animateRectsStep(1, b.oldBuffer.data(), b.newBuffer.data(), b.dstBuffer.data());
    CHECK(b.at(50, 275) != NEW_COLOR);
    CHECK_EQ(b.at(50, 310), NEW_COLOR);
    finishAnimation();
}

TEST_BUG("gui/animation: a rect from below the clip rect is clipped in its height", "G15") {
    // sy -= ... instead of sh -= ... (animation.cpp:324-326)
    AnimationBuffers b;
    for (int y = 0; y < (int)DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < (int)DISPLAY_WIDTH; x++) {
            b.oldBuffer[y * DISPLAY_WIDTH + x] = 0xFF000000 | (y << 12) | x;
        }
    }
    g_animRects[0].buffer = BUFFER_OLD;
    g_animRects[0].srcRect = Rect{ 0, 250, 100, 100 };
    g_animRects[0].dstRect = Rect{ 0, 0, 100, 100 };
    g_animRects[0].opacity = OPACITY_SOLID;
    g_animRects[0].position = POSITION_TOP_LEFT;
    Rect clip{ 0, 0, 800, 300 };
    animateRects(app(), BUFFER_NEW, 1, 1.0f, &clip);
    animateRectsStep(1, b.oldBuffer.data(), b.newBuffer.data(), b.dstBuffer.data());
    // rows 250..299 of the old buffer (inside the clip rect) are copied to rows 0..49
    CHECK_EQ(b.at(10, 10), 0xFF000000 | (260 << 12) | 10);
    CHECK_EQ(b.at(10, 75), NEW_COLOR);
    finishAnimation();
}
