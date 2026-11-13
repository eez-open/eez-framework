/*
 * eez-framework tests: the internal pages of gui/page.cpp (toast, question,
 * menu with buttons, select from enum) and AppContext::yesNoDialog.
 */

#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include <eez/flow/flow_defs_v3.h>

#include "gui_test.h"

using namespace gui_test;

static ToastMessagePage *activeToast() {
    REQUIRE(activePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE);
    return (ToastMessagePage *)app()->getActivePage();
}

// finds the widget of an internal page for which accept returns true by
// scanning the page; returns false if none
template <typename Accept>
static bool findInternalWidget(InternalPage *page, Accept accept, int &x, int &y, WidgetCursor *found = nullptr) {
    for (int yy = page->y; yy < page->y + page->height; yy += 2) {
        for (int xx = page->x; xx < page->x + page->width; xx += 2) {
            auto widgetCursor = page->findWidgetInternalPage(xx, yy, false);
            if (widgetCursor.widget && accept(widgetCursor)) {
                // a bit inside of the top left corner (the cursors of the
                // internal pages don't have the widget size)
                x = xx + 4;
                y = yy + 4;
                if (found) {
                    *found = widgetCursor;
                }
                return true;
            }
        }
    }
    return false;
}

static bool findToastButton(ToastMessagePage *toast, int &x, int &y, WidgetCursor *found = nullptr) {
    return findInternalWidget(toast, [](const WidgetCursor &wc) { return wc.widget->type == WIDGET_TYPE_BUTTON; }, x, y, found);
}

static int countButtons(InternalPage *page, int action) {
    int count = 0;
    for (int i = 0; i < 8; i++) {
        int x, y;
        if (findInternalWidget(page, [&](const WidgetCursor &wc) { return getWidgetAction(wc) == action && wc.cursor == i; }, x, y)) {
            count++;
        }
    }
    return count;
}

static void showPageA() {
    app()->showPage(PAGE_ID_GUI_PAGE_A);
    harness::frame(2);
    harness::advance(400);
}

// checksum of the presented frame inside the rect
static uint32_t checksum(int x, int y, int w, int h) {
    uint32_t sum = 0;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            sum = sum * 31 + (harness::pixel(xx, yy) & 0xFFFFFF);
        }
    }
    return sum;
}

////////////////////////////////////////////////////////////////////////////////
// toast

TEST("gui/page: infoMessage shows a toast without a button") {
    app()->infoMessage("info");
    auto toast = activeToast();
    CHECK(!toast->hasAction());
    CHECK_STR(toast->messageValue.getString(), "info");
    int x, y;
    CHECK(!findToastButton(toast, x, y));
}

TEST("gui/page: an info toast is closed after 1 s without activity") {
    app()->infoMessage("info");
    harness::frame(55);
    CHECK_EQ(activePageId(), (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    harness::frame(10);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/page: an error toast has a Close button and is not closed by time") {
    app()->errorMessage("error");
    auto toast = activeToast();
    CHECK(toast->hasAction());
    harness::frame(200);
    CHECK_EQ(activePageId(), (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
    int x, y;
    WidgetCursor button;
    REQUIRE(findToastButton(toast, x, y, &button));
    CHECK_STR(((const ButtonWidget *)button.widget)->text, "Close");
}

TEST("gui/page: the Close button closes the error toast") {
    showPageA();
    app()->errorMessage("error");
    harness::frame(2);
    harness::advance(400);
    int x, y;
    REQUIRE(findToastButton(activeToast(), x, y));
    tap(x, y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/page: an error toast with autoDismiss has no button and is closed after 1 s") {
    app()->errorMessage("error", true);
    auto toast = activeToast();
    CHECK(!toast->hasAction());
    harness::frame(65);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/page: errorMessage(Value) has a Close button") {
    app()->errorMessage(Value("error"));
    CHECK(activeToast()->hasAction());
}

static int g_toastActionCount;
static int g_toastActionPageId;
static int g_toastActionParam;

TEST("gui/page: the toast action is executed after the toast is closed") {
    showPageA();
    app()->infoMessage("saved", []() {
        g_toastActionCount++;
        g_toastActionPageId = activePageId();
    }, "Show");
    harness::frame(2);
    harness::advance(400);
    auto toast = activeToast();
    CHECK(toast->hasAction());
    int x, y;
    REQUIRE(findToastButton(toast, x, y));
    tap(x, y);
    CHECK_EQ(g_toastActionCount, 1);
    CHECK_EQ(g_toastActionPageId, (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/page: errorMessageWithAction passes the action parameter") {
    showPageA();
    app()->errorMessageWithAction(Value("module error"), [](int param) { g_toastActionParam = param; }, "Settings", 3);
    harness::frame(2);
    harness::advance(400);
    int x, y;
    WidgetCursor button;
    REQUIRE(findToastButton(activeToast(), x, y, &button));
    CHECK_STR(((const ButtonWidget *)button.widget)->text, "Settings");
    tap(x, y);
    CHECK_EQ(g_toastActionParam, 3);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/page: errorMessageWithAction without an action only closes the toast") {
    showPageA();
    app()->errorMessageWithAction("error", nullptr, "Close");
    harness::frame(2);
    harness::advance(400);
    int x, y;
    REQUIRE(findToastButton(activeToast(), x, y));
    tap(x, y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/page: the encoder click closes a toast without action") {
    app()->infoMessage("info");
    activeToast()->onEncoderClicked();
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/page: the encoder click executes the toast action") {
    app()->infoMessage("info", []() { g_toastActionCount++; }, "Do");
    activeToast()->onEncoderClicked();
    CHECK_EQ(g_toastActionCount, 1);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/hooks: the encoder rotation closes the toast") {
    app()->errorMessage("error");
    activeToast()->onEncoder(1);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/hooks: a toast with a button has the backdrop, a toast without has not") {
    app()->infoMessage("info");
    CHECK(!g_hooks.activePageHasBackdrop());
    app()->errorMessage("error");
    CHECK(g_hooks.activePageHasBackdrop());
    app()->popPage();
    CHECK(g_hooks.activePageHasBackdrop());
}

TEST("gui/page: the toast is centered on the display") {
    app()->infoMessage("centered");
    auto toast = activeToast();
    CHECK_EQ(toast->x, (800 - toast->width) / 2);
    CHECK_EQ(toast->y, (480 - toast->height) / 2);
}

TEST("gui/page: the toast text is at least 80 px wide") {
    // info_alert: border 2, padding 8
    app()->infoMessage("x");
    CHECK_EQ(activeToast()->width, 2 + 8 + 80 + 8 + 2);
}

TEST("gui/page: a toast message is split into up to three lines") {
    // RobotoCondensed: 26 px line height
    app()->infoMessage("one");
    int h1 = activeToast()->height;
    app()->infoMessage("one\ntwo");
    int h2 = activeToast()->height;
    app()->infoMessage("one\ntwo\nthree");
    int h3 = activeToast()->height;
    CHECK_EQ(h2 - h1, 26);
    CHECK_EQ(h3 - h2, 26);
    // the fourth line stays in the third line
    app()->infoMessage("one\ntwo\nthree\nfour");
    CHECK_EQ(activeToast()->height, h3);
}

TEST("gui/page: the toast is as wide as its longest line") {
    app()->infoMessage("a much longer second line of the toast");
    int w = activeToast()->width;
    app()->infoMessage("short\na much longer second line of the toast");
    CHECK_EQ(activeToast()->width, w);
}

TEST("gui/page: the toast shows a copy of the message") {
    char buffer[32];
    strcpy(buffer, "first message");
    app()->infoMessage(buffer);
    harness::frame(2);
    auto toast = activeToast();
    uint32_t before = checksum(toast->x, toast->y, toast->width, toast->height);
    strcpy(buffer, "other message");
    refreshScreen();
    harness::frame(2);
    CHECK_EQ(checksum(toast->x, toast->y, toast->width, toast->height), before);
}

// the pixels in the area that are not of the color (0xRRGGBB)
static int countOtherColor(int x, int y, int w, int h, uint32_t color) {
    int n = 0;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            if ((harness::pixel(xx, yy) & 0xFFFFFF) != color) {
                n++;
            }
        }
    }
    return n;
}

TEST("gui/page: the toast lines are inside the border and the padding of the toast style") {
    // info_alert: border 2, padding 8; the lines are drawn in its font and
    // color, not in boxes of the style (no border, background or padding per
    // line)
    app()->infoMessage("one\ntwo");
    harness::frame(2);
    auto toast = activeToast();
    uint32_t accent = harness::pixel(toast->x + 2, toast->y + 2) & 0xFFFFFF;
    // the padding above the lines and on their left
    CHECK_EQ(countOtherColor(toast->x + 2, toast->y + 2, toast->width - 4, 8, accent), 0);
    CHECK_EQ(countOtherColor(toast->x + 2, toast->y + 10, 8, toast->height - 12, accent), 0);
    // the text is there
    CHECK(countOtherColor(toast->x + 10, toast->y + 10, toast->width - 20, toast->height - 20, accent) > 0);
}

TEST("gui/page: the error toast button has the error button style") {
    app()->errorMessage("error");
    int x, y;
    WidgetCursor button;
    REQUIRE(findToastButton(activeToast(), x, y, &button));
    CHECK_EQ((int)button.widget->style, (int)EEZ_STYLE_ID_ERROR_ALERT_BUTTON);
}

TEST("gui/page: the info toast button has the info button style") {
    app()->infoMessage("info", []() {}, "Do");
    int x, y;
    WidgetCursor button;
    REQUIRE(findToastButton(activeToast(), x, y, &button));
    CHECK_EQ((int)button.widget->style, (int)EEZ_STYLE_ID_INFO_ALERT_BUTTON);
}

TEST("gui/page: a toast is not wider than the display") {
    // e.g. DISP:WIND:ERR with a 128 character message
    std::string message(128, 'W');
    app()->errorMessage(message.c_str());
    auto toast = activeToast();
    CHECK(toast->width <= 800);
    CHECK(toast->x >= 0);
}

TEST("gui/page: the toast keeps its message when the caller's buffer is freed") {
    // BB3+ compares messageValue later (isTripCleared); DISP:WIND:ERR passes a
    // buffer of the SCPI thread
    char *buffer = (char *)malloc(32);
    strcpy(buffer, "Trip on channel 1");
    app()->errorMessage(buffer);
    ::free(buffer);
    // the freed memory is used again (seen also without AddressSanitizer)
    char *other = (char *)malloc(32);
    strcpy(other, "Xxxx xx xxxxxxx x");
    harness::frame(2);
    CHECK_STR(activeToast()->messageValue.getString(), "Trip on channel 1");
    ::free(other);
}

TEST("gui/page: the toast button label in a stack buffer is the label") {
    // the label is copied into the static toast page (the widget text is an
    // AssetsPtr, a 32-bit offset from the widget, the stack is too far away)
    char label[16];
    strcpy(label, "Settings");
    app()->errorMessageWithAction("error", []() {}, label);
    strcpy(label, "Changed");
    int x, y;
    WidgetCursor button;
    REQUIRE(findToastButton(activeToast(), x, y, &button));
    const char *text = ((const ButtonWidget *)button.widget)->text;
    CHECK_STR(text, "Settings");
}

TEST("gui/page: a long toast button label is cut at a UTF-8 character boundary") {
    // 47 bytes fit in the label buffer: 45 x 'a' and then a 2-byte character
    std::string label(45, 'a');
    label += "\xc3\xb1\xc3\xb1";
    app()->errorMessageWithAction("error", []() {}, label.c_str());
    int x, y;
    WidgetCursor button;
    REQUIRE(findToastButton(activeToast(), x, y, &button));
    const char *text = ((const ButtonWidget *)button.widget)->text;
    std::string expected = std::string(45, 'a') + "\xc3\xb1";
    CHECK_STR(text, expected.c_str());
}

////////////////////////////////////////////////////////////////////////////////
// question

static int g_questionCallbackCount;
static unsigned g_questionButton;
static void *g_questionUserParam;

static void questionCallback(void *userParam, unsigned buttonIndex) {
    g_questionCallbackCount++;
    g_questionButton = buttonIndex;
    g_questionUserParam = userParam;
}

static Value makeButtons(int n, const char **labels) {
    Value buttons = Value::makeArrayRef(n, flow::defs_v3::ARRAY_TYPE_STRING, 0);
    for (int i = 0; i < n; i++) {
        buttons.getArray()->values[i] = Value(labels[i]);
    }
    return buttons;
}

static bool questionButtonPosition(int i, int &x, int &y) {
    auto page = (InternalPage *)app()->getActivePage();
    return findInternalWidget(page, [i](const WidgetCursor &wc) {
        return getWidgetAction(wc) == ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON && wc.cursor == i;
    }, x, y);
}

TEST("gui/page: questionDialog calls the callback with the tapped button and closes the page") {
    static const char *labels[] = { "Yes", "No", "Maybe" };
    showPageA();
    int userParam;
    app()->questionDialog(Value("Delete?"), makeButtons(3, labels), &userParam, questionCallback);
    harness::frame(2);
    harness::advance(400);
    REQUIRE(activePageId() == INTERNAL_PAGE_ID_QUESTION);
    int x, y;
    REQUIRE(questionButtonPosition(1, x, y));
    tap(x, y);
    CHECK_EQ(g_questionCallbackCount, 1);
    CHECK_EQ(g_questionButton, 1u);
    CHECK(g_questionUserParam == &userParam);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/page: the question is centered and shows all buttons") {
    static const char *labels[] = { "A", "B", "C", "D" };
    app()->questionDialog(Value("Choose"), makeButtons(4, labels), nullptr, questionCallback);
    auto page = (InternalPage *)app()->getActivePage();
    CHECK_EQ(page->x, (800 - page->width) / 2);
    CHECK_EQ(page->y, (480 - page->height) / 2);
    CHECK_EQ(countButtons(page, ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON), 4);
}

TEST("gui/page: a tap outside of the question doesn't close it") {
    // pinned: findWidgetInternalPage returns the container for any point, so
    // the question is never touched outside
    static const char *labels[] = { "OK" };
    showPageA();
    app()->questionDialog(Value("Question"), makeButtons(1, labels), nullptr, questionCallback);
    harness::frame(2);
    harness::advance(400);
    tap(20, 460);
    CHECK_EQ(activePageId(), (int)INTERNAL_PAGE_ID_QUESTION);
    CHECK_EQ(g_questionCallbackCount, 0);
}

// A string ref whose text is in static memory: a heap pointer is too far from
// the page objects for the 32-bit offsets of the widget texts (NEW-gui-1).
// It is never freed.
static Value makeStaticStringRef(const char *text) {
    static char staticText[64];
    Value value = Value::makeStringRef(text, -1, 0);
    auto stringRef = (StringRef *)value.refValue;
    strncpy(staticText, text, sizeof(staticText) - 1);
    stringRef->str = staticText;
    stringRef->refCounter++;
    return value;
}

TEST("gui/page: questionDialog keeps a reference to its message") {
    // the widgets of the page point into the message: the page keeps the
    // message value, so a computed message (string ref) stays alive
    static const char *labels[] = { "OK" };
    Value message = makeStaticStringRef("Delete file?");
    uint32_t refs = message.refValue->refCounter;
    app()->questionDialog(message, makeButtons(1, labels), nullptr, questionCallback);
    CHECK_EQ(message.refValue->refCounter, refs + 1);
}

TEST("gui/page: questionDialog keeps a reference to its buttons") {
    static const char *labels[] = { "OK" };
    Value buttons = makeButtons(1, labels);
    app()->questionDialog(Value("Question"), buttons, nullptr, questionCallback);
    CHECK_EQ(buttons.refValue->refCounter, 2u);
}

TEST("gui/page: a question with more than four buttons shows at most four") {
    static const char *labels[] = { "1", "2", "3", "4", "5", "6" };
    app()->questionDialog(Value("Question"), makeButtons(6, labels), nullptr, questionCallback);
    harness::frame(2);
    CHECK(countButtons((InternalPage *)app()->getActivePage(), ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON) <= 4);
}

TEST("gui/page: a question without buttons is lower than one with a button") {
    static const char *labels[] = { "OK" };
    app()->questionDialog(Value("Question"), makeButtons(1, labels), nullptr, questionCallback);
    int withButton = ((InternalPage *)app()->getActivePage())->height;
    app()->popPage();
    app()->questionDialog(Value("Question"), makeButtons(0, labels), nullptr, questionCallback);
    int withoutButtons = ((InternalPage *)app()->getActivePage())->height;
    CHECK(withoutButtons < withButton);
}

TEST("gui/page: the question button text from a string ref is the button string") {
    // e.g. a flow ShowMessageBox with buttons built at runtime; the text is
    // copied into the static question page (the widget text is an AssetsPtr, a
    // 32-bit offset from the widget)
    Value buttons = Value::makeArrayRef(1, flow::defs_v3::ARRAY_TYPE_STRING, 0);
    buttons.getArray()->values[0] = Value::makeStringRef("Retry", -1, 0);
    app()->questionDialog(Value::makeStringRef("Question?", -1, 0), buttons, nullptr, questionCallback);
    auto page = (InternalPage *)app()->getActivePage();
    int x, y;
    WidgetCursor button;
    REQUIRE(findInternalWidget(page, [](const WidgetCursor &wc) {
        return getWidgetAction(wc) == ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON;
    }, x, y, &button));
    const char *text = ((const TextWidget *)button.widget)->text;
    CHECK_STR(text, "Retry");
    harness::frame(2);
}

TEST("gui/page: the menu texts are copies") {
    static char item[16];
    static const char *items[] = { item, nullptr };
    strcpy(item, "Copy");
    showMenu(app(), "File", MENU_TYPE_BUTTON, items, [](int) {});
    strcpy(item, "Xxxx");
    auto page = (InternalPage *)app()->getActivePage();
    int x, y;
    WidgetCursor button;
    REQUIRE(findInternalWidget(page, [](const WidgetCursor &wc) {
        return getWidgetAction(wc) == ACTION_ID_INTERNAL_MENU_WITH_BUTTONS;
    }, x, y, &button));
    CHECK_STR(((const TextWidget *)button.widget)->text, "Copy");
    harness::frame(2);
}

TEST("gui/page: a menu without a message") {
    static const char *items[] = { "A", nullptr };
    showMenu(app(), nullptr, MENU_TYPE_BUTTON, items, [](int) {});
    harness::frame(2);
    CHECK_EQ(activePageId(), (int)INTERNAL_PAGE_ID_MENU_WITH_BUTTONS);
}

TEST("gui/page: a new question replaces the question shown before, which is dismissed") {
    static const char *labels[] = { "OK" };
    static int dismissedCount;
    static void *dismissedParam;
    int first, second;
    app()->questionDialog(Value("First"), makeButtons(1, labels), &first, questionCallback, [](void *userParam) {
        dismissedCount++;
        dismissedParam = userParam;
    });
    app()->questionDialog(Value("Second"), makeButtons(1, labels), &second, questionCallback);
    CHECK_EQ(dismissedCount, 1);
    CHECK(dismissedParam == &first);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
}

TEST("gui/page: a question answered with a button is not dismissed") {
    static const char *labels[] = { "Yes", "No" };
    static int dismissedCount;
    showPageA();
    app()->questionDialog(Value("Q"), makeButtons(2, labels), nullptr, questionCallback, [](void *) { dismissedCount++; });
    harness::frame(2);
    harness::advance(400);
    int x, y;
    REQUIRE(questionButtonPosition(0, x, y));
    tap(x, y);
    CHECK_EQ(g_questionCallbackCount, 1);
    CHECK_EQ(dismissedCount, 0);
}

TEST("gui/page: a question closed by code is dismissed") {
    static const char *labels[] = { "Yes" };
    static int dismissedCount;
    app()->questionDialog(Value("Q"), makeButtons(1, labels), nullptr, questionCallback, [](void *) { dismissedCount++; });
    app()->popPage();
    CHECK_EQ(dismissedCount, 1);
    CHECK_EQ(g_questionCallbackCount, 0);
}

TEST("gui/page: a toast closed by touching outside is dismissed with the action parameter") {
    static int dismissedParam;
    static int actionCount;
    showPageA();
    app()->errorMessageWithAction(Value("error"), [](int) { actionCount++; }, "Close", 7, [](int param) { dismissedParam = param; });
    harness::frame(2);
    harness::advance(400);
    tap(A_BTN1_X, A_BTN1_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(dismissedParam, 7);
    CHECK_EQ(actionCount, 0);
}

TEST("gui/page: a toast closed by its button is not dismissed") {
    static int dismissedCount;
    static int actionParam;
    showPageA();
    app()->infoMessageWithAction(Value("info"), [](int param) { actionParam = param; }, "Close", 5, [](int) { dismissedCount++; });
    harness::frame(2);
    harness::advance(400);
    int x, y;
    REQUIRE(findToastButton(activeToast(), x, y));
    tap(x, y);
    CHECK_EQ(actionParam, 5);
    CHECK_EQ(dismissedCount, 0);
}

TEST("gui/page: a toast replaced by a newer toast is dismissed") {
    static int dismissedCount;
    app()->errorMessageWithAction(Value("error"), [](int) {}, "Close", 1, [](int) { dismissedCount++; });
    app()->errorMessage("second");
    CHECK_EQ(dismissedCount, 1);
    app()->popPage();
    CHECK_EQ(dismissedCount, 1);
}

TEST("gui/page: a toast not shown on a full page stack is dismissed") {
    static int dismissedCount;
    for (int i = 0; i < 9; i++) {
        app()->pushPage(i % 2 ? PAGE_ID_GUI_PAGE_A : PAGE_ID_GUI_PAGE_B);
    }
    REQUIRE(app()->getNumPagesOnStack() == 10);
    app()->errorMessageWithAction(Value("error"), [](int) {}, "Close", 1, [](int) { dismissedCount++; });
    CHECK_EQ(dismissedCount, 1);
}

////////////////////////////////////////////////////////////////////////////////
// menu with buttons

static int g_menuCallbackCount;
static int g_menuItem;

TEST("gui/page: showMenu calls the callback with the tapped item, the callback closes the menu") {
    static const char *items[] = { "Copy", "Move", "Delete", nullptr };
    showPageA();
    showMenu(app(), "File", MENU_TYPE_BUTTON, items, [](int item) {
        g_menuCallbackCount++;
        g_menuItem = item;
    });
    harness::frame(2);
    harness::advance(400);
    REQUIRE(activePageId() == INTERNAL_PAGE_ID_MENU_WITH_BUTTONS);
    auto page = (InternalPage *)app()->getActivePage();
    CHECK_EQ(countButtons(page, ACTION_ID_INTERNAL_MENU_WITH_BUTTONS), 3);
    int x, y;
    REQUIRE(findInternalWidget(page, [](const WidgetCursor &wc) {
        return getWidgetAction(wc) == ACTION_ID_INTERNAL_MENU_WITH_BUTTONS && wc.cursor == 2;
    }, x, y));
    tap(x, y);
    CHECK_EQ(g_menuCallbackCount, 1);
    CHECK_EQ(g_menuItem, 2);
    // the menu doesn't close itself
    CHECK_EQ(activePageId(), (int)INTERNAL_PAGE_ID_MENU_WITH_BUTTONS);
}

TEST("gui/page: a menu with more than four items shows at most four") {
    static const char *items[] = { "1", "2", "3", "4", "5", "6", nullptr };
    showMenu(app(), "Menu", MENU_TYPE_BUTTON, items, [](int) {});
    harness::frame(2);
    CHECK(countButtons((InternalPage *)app()->getActivePage(), ACTION_ID_INTERNAL_MENU_WITH_BUTTONS) <= 4);
}

////////////////////////////////////////////////////////////////////////////////
// select from enum

static int g_onSetCount;
static uint16_t g_onSetValue;

static void onSet(uint16_t value) {
    g_onSetCount++;
    g_onSetValue = value;
    SelectFromEnumPage::popSelectFromEnumPage();
}

static const EnumItem g_items[] = {
    { 10, "Ten", nullptr },
    { 20, "Twenty", nullptr },
    { 30, "Thirty", nullptr },
    { 0, nullptr, nullptr }
};

static bool selectItemPosition(int i, int &x, int &y) {
    auto page = (InternalPage *)app()->getActivePage();
    return findInternalWidget(page, [i](const WidgetCursor &wc) {
        return getWidgetAction(wc) == ACTION_ID_INTERNAL_SELECT_ENUM_ITEM && wc.widget->data == i;
    }, x, y);
}

TEST("gui/page: a tap on a select item calls onSet with the item value") {
    showPageA();
    SelectFromEnumPage::pushSelectFromEnumPage(app(), g_items, 10, nullptr, onSet);
    harness::frame(2);
    harness::advance(400);
    REQUIRE(SelectFromEnumPage::getActiveSelectEnumDefinition() == g_items);
    int x, y;
    REQUIRE(selectItemPosition(2, x, y));
    tap(x, y);
    CHECK_EQ(g_onSetCount, 1);
    CHECK_EQ((int)g_onSetValue, 30);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK(SelectFromEnumPage::getActiveSelectEnumDefinition() == nullptr);
}

TEST("gui/page: a disabled select item can't be selected, a tap on it closes the page") {
    // pinned: the disabled item is not found, so the tap is a tap outside
    showPageA();
    SelectFromEnumPage::pushSelectFromEnumPage(app(), g_items, 10, [](uint16_t value) { return value == 20; }, onSet);
    harness::frame(2);
    harness::advance(400);
    int x, y;
    CHECK(!selectItemPosition(1, x, y));
    REQUIRE(selectItemPosition(0, x, y));
    // the disabled item is just below the first one (items are 34 px high)
    tap(x, y + 34);
    CHECK_EQ(g_onSetCount, 0);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/page: without onSet a tap on a select item closes the page") {
    showPageA();
    SelectFromEnumPage::pushSelectFromEnumPage(app(), g_items, 10, nullptr, nullptr);
    harness::frame(2);
    harness::advance(400);
    int x, y;
    REQUIRE(selectItemPosition(1, x, y));
    tap(x, y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/page: a tap outside of the select page closes it") {
    showPageA();
    SelectFromEnumPage::pushSelectFromEnumPage(app(), g_items, 10, nullptr, onSet);
    harness::frame(2);
    harness::advance(400);
    tap(20, 460);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(g_onSetCount, 0);
}

TEST("gui/page: the select page is placed below the widget found at touch down") {
    showPageA();
    g_onAction = [](int actionId) {
        if (actionId == ACTION_ID_GUI_CLICK_A) {
            SelectFromEnumPage::pushSelectFromEnumPage(app(), g_items, 10, nullptr, onSet);
        }
    };
    tap(A_BTN1_X, A_BTN1_Y);
    REQUIRE(activePageId() == INTERNAL_PAGE_ID_SELECT_FROM_ENUM);
    auto page = (InternalPage *)app()->getActivePage();
    // button "A1": 20, 20, 200x60; right edges aligned
    CHECK_EQ(page->y, 80);
    CHECK_EQ(page->x + page->width, 220);
}

TEST("gui/page: the select page without a touch is centered") {
    SelectFromEnumPage::pushSelectFromEnumPage(app(), g_items, 10, nullptr, onSet);
    auto page = (InternalPage *)app()->getActivePage();
    CHECK_EQ(page->x, (800 - page->width) / 2);
    CHECK_EQ(page->y, (480 - page->height) / 2);
}

TEST("gui/page: many select items are shown in two columns") {
    static EnumItem items[21];
    static char labels[20][8];
    for (int i = 0; i < 20; i++) {
        snprintf(labels[i], sizeof(labels[i]), "Item %d", i);
        items[i] = { (uint16_t)i, labels[i], nullptr };
    }
    items[20] = { 0, nullptr, nullptr };
    SelectFromEnumPage::pushSelectFromEnumPage(app(), items, 0, nullptr, onSet);
    auto page = (InternalPage *)app()->getActivePage();
    CHECK(page->height <= 480);
    int x0, y0, x19, y19;
    REQUIRE(selectItemPosition(0, x0, y0));
    REQUIRE(selectItemPosition(19, x19, y19));
    CHECK(x19 > x0);
}

static void enumFunction(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    static const char *labels[] = { "Low", "High" };
    int i = widgetCursor.cursor;
    if (i < 0 || i >= 2) {
        return;
    }
    if (operation == DATA_OPERATION_GET_VALUE) {
        value = Value((uint16_t)(300 + i), VALUE_TYPE_UINT16);
    } else if (operation == DATA_OPERATION_GET_LABEL) {
        value = labels[i];
    }
}

TEST("gui/page: a select page with a function defined enum shows its items") {
    SelectFromEnumPage::pushSelectFromEnumPage(app(), enumFunction, 0, nullptr, onSet);
    int x, y;
    CHECK(selectItemPosition(0, x, y));
    CHECK(selectItemPosition(1, x, y));
    CHECK(!selectItemPosition(2, x, y));
}

TEST_BUG("gui/page: values of a function defined enum above 255 are kept", "G17") {
    showPageA();
    SelectFromEnumPage::pushSelectFromEnumPage(app(), enumFunction, 0, nullptr, onSet);
    harness::frame(2);
    harness::advance(400);
    int x, y;
    REQUIRE(selectItemPosition(1, x, y));
    tap(x, y);
    CHECK_EQ(g_onSetCount, 1);
    CHECK_EQ((int)g_onSetValue, 301);
}

////////////////////////////////////////////////////////////////////////////////
// yes/no dialog

static int g_yesCount;
static int g_noCount;
static int g_cancelCount;

static void yesNo() {
    showPageA();
    app()->yesNoDialog(PAGE_ID_GUI_YES_NO, "Are you sure?",
        []() { g_yesCount++; }, []() { g_noCount++; }, []() { g_cancelCount++; });
    harness::frame(2);
    harness::advance(400);
}

TEST("gui/app_context: yesNoDialog shows the page with the message") {
    yesNo();
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_YES_NO);
    CHECK_STR(g_alertMessage.getString(), "Are you sure?");
}

TEST("gui/app_context: Yes of the yes/no dialog closes it and calls the yes callback") {
    yesNo();
    tap(YES_X, YES_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(g_yesCount, 1);
    CHECK_EQ(g_noCount + g_cancelCount, 0);
}

TEST("gui/app_context: No of the yes/no dialog closes it and calls the no callback") {
    yesNo();
    tap(NO_X, NO_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(g_noCount, 1);
    CHECK_EQ(g_yesCount + g_cancelCount, 0);
}

TEST("gui/app_context: Cancel of the yes/no dialog closes it and calls the cancel callback") {
    yesNo();
    tap(CANCEL_X, CANCEL_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(g_cancelCount, 1);
}

TEST("gui/app_context: yesNoDialog with a Value message") {
    showPageA();
    app()->yesNoDialog(PAGE_ID_GUI_YES_NO, Value::makeStringRef("Overwrite?", -1, 0), nullptr, nullptr, nullptr);
    CHECK_STR(g_alertMessage.getString(), "Overwrite?");
    harness::frame(2);
    harness::advance(400);
    tap(YES_X, YES_Y);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/page: the select page is drawn with the border of its container style") {
    // select_enum_item_popup_container: border 1 in the text color (white),
    // padding 4, the background of the default style (black)
    SelectFromEnumPage::pushSelectFromEnumPage(app(), g_items, 10, nullptr, onSet);
    harness::frame(2);
    harness::advance(400);
    harness::frame(2);
    auto page = (InternalPage *)app()->getActivePage();
    int yMiddle = page->y + page->height / 2;
    CHECK_EQ(harness::pixel(page->x, yMiddle) & 0xFFFFFF, 0xFFFFFFu);
    CHECK_EQ(harness::pixel(page->x + page->width - 1, yMiddle) & 0xFFFFFF, 0xFFFFFFu);
    CHECK_EQ(harness::pixel(page->x + 1, yMiddle) & 0xFFFFFF, 0u);
    SelectFromEnumPage::popSelectFromEnumPage();
}

TEST("gui/page: the rounded corners of a menu show the page below, not an earlier popup") {
    // menu_with_buttons_container has the radius 12: its corners are drawn
    // from the page below (the buffer background of the internal page)
    static const char *items[] = { "Yes", "No", nullptr };
    showPageA();
    showMenu(app(), "Question", MENU_TYPE_BUTTON, items, [](int) { app()->popPage(); });
    harness::frame(3);
    auto menu = (InternalPage *)app()->getActivePage();
    int x = menu->x;
    int y = menu->y;
    int w = menu->width;
    int h = menu->height;
    std::vector<uint32_t> corners;
    for (int dy = 0; dy < 4; dy++) {
        for (int dx = 0; dx < 4; dx++) {
            corners.push_back(harness::pixel(x + dx, y + dy));
            corners.push_back(harness::pixel(x + w - 1 - dx, y + h - 1 - dy));
        }
    }
    app()->popPage();
    harness::frame(2);

    // a wide error toast over the same place, closed, then the menu again
    app()->errorMessage("A long message of an error toast that is wider than the menu");
    harness::frame(3);
    app()->popPage();
    harness::frame(2);
    showMenu(app(), "Question", MENU_TYPE_BUTTON, items, [](int) { app()->popPage(); });
    harness::frame(3);
    menu = (InternalPage *)app()->getActivePage();
    REQUIRE(menu->x == x && menu->y == y && menu->width == w && menu->height == h);
    int i = 0;
    int different = 0;
    for (int dy = 0; dy < 4; dy++) {
        for (int dx = 0; dx < 4; dx++) {
            different += harness::pixel(x + dx, y + dy) != corners[i++];
            different += harness::pixel(x + w - 1 - dx, y + h - 1 - dy) != corners[i++];
        }
    }
    CHECK_EQ(different, 0);
    app()->popPage();
}
