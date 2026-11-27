/*
 * eez-framework tests: gui/app_context.cpp, the values kept for widgets
 * (AppContext::getWidgetMemory) for a page on the stack or for the app context.
 */

#include "gui_test.h"

using namespace gui_test;

// widgets and assets only compared, never used
static char g_widgetBytes[3][16];
static char g_otherAssetsBytes[16];

static const Assets *otherAssets() {
    return (const Assets *)g_otherAssetsBytes;
}

static WidgetCursor widgetCursor(int widgetIndex, const Assets *assets = nullptr) {
    WidgetCursor widgetCursor;
    widgetCursor.widget = (const Widget *)g_widgetBytes[widgetIndex];
    widgetCursor.assets = (Assets *)(assets ? assets : g_mainAssets);
    return widgetCursor;
}

static const int KIND_A = WIDGET_MEMORY_FIRST_APP_KIND;
static const int KIND_B = WIDGET_MEMORY_FIRST_APP_KIND + 1;

static void set(int kind, int pageStackIndex, const WidgetCursor &wc, int value, int16_t dataId = 1) {
    WidgetMemoryValue v;
    v.intValue = value;
    app()->setWidgetMemory(kind, pageStackIndex, wc, dataId, v);
}

// the kept value, -1 if none
static int get(int kind, int pageStackIndex, const WidgetCursor &wc, int16_t dataId = 1) {
    WidgetMemoryValue v;
    return app()->getWidgetMemory(kind, pageStackIndex, wc, dataId, v) ? v.intValue : -1;
}

TEST("gui/widget_memory: a value of the app context stays when the pages change") {
    set(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 5);
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    app()->popPage();
    app()->showPage(PAGE_ID_GUI_PAGE_B);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0)), 5);
}

TEST("gui/widget_memory: a float value") {
    WidgetMemoryValue v;
    v.floatValue = 0.1f;
    app()->setWidgetMemory(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 1, v);
    WidgetMemoryValue result;
    REQUIRE(app()->getWidgetMemory(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 1, result));
    CHECK_EQ(result.floatValue, 0.1f);
}

TEST("gui/widget_memory: the kind, the scope, the widget, the assets, the cursor, the iterators and the data id are separate values") {
    set(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 1);
    CHECK_EQ(get(KIND_B, WIDGET_MEMORY_APP, widgetCursor(0)), -1);
    CHECK_EQ(get(KIND_A, 0, widgetCursor(0)), -1);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(1)), -1);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0, otherAssets())), -1);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 2), -1);

    auto withCursor = widgetCursor(0);
    withCursor.cursor = 3;
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, withCursor), -1);

    auto withIterator = widgetCursor(0);
    withIterator.iterators[0] = 2;
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, withIterator), -1);

    set(KIND_A, WIDGET_MEMORY_APP, withIterator, 2);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0)), 1);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, withIterator), 2);
}

TEST("gui/widget_memory: a value set again replaces the old one, a removed value is gone") {
    set(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 1);
    set(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 2);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0)), 2);
    app()->removeWidgetMemory(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 1);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0)), -1);
}

TEST("gui/widget_memory: a value of a page stays while another page is on top of it, it goes with the page") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    set(KIND_A, 1, widgetCursor(0), 7);
    app()->pushPage(PAGE_ID_GUI_PAGE_B);
    CHECK_EQ(get(KIND_A, 1, widgetCursor(0)), 7);
    app()->popPage();
    CHECK_EQ(get(KIND_A, 1, widgetCursor(0)), 7);
    app()->popPage();
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(get(KIND_A, 1, widgetCursor(0)), -1);
}

TEST("gui/widget_memory: a value of a page goes when another page replaces it or showPage shows a page") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    set(KIND_A, 0, widgetCursor(0), 1);
    set(KIND_A, 1, widgetCursor(1), 2);
    TestPage pageB;
    app()->replacePage(PAGE_ID_GUI_PAGE_B, &pageB);
    CHECK_EQ(get(KIND_A, 1, widgetCursor(1)), -1);
    CHECK_EQ(get(KIND_A, 0, widgetCursor(0)), 1);

    app()->showPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(get(KIND_A, 0, widgetCursor(0)), -1);
}

TEST("gui/widget_memory: no value for a page above the top of the stack") {
    set(KIND_A, 1, widgetCursor(0), 1);
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(get(KIND_A, 1, widgetCursor(0)), -1);
}

TEST("gui/widget_memory: removePageFromStack drops the values of the page, the values of the pages above it go down with them") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    app()->pushPage(PAGE_ID_GUI_PAGE_B);
    app()->pushPage(PAGE_ID_GUI_DIALOG);
    set(KIND_A, 1, widgetCursor(0), 1);
    set(KIND_A, 2, widgetCursor(1), 2);
    set(KIND_A, 3, widgetCursor(2), 3);

    app()->removePageFromStack(PAGE_ID_GUI_PAGE_B);
    REQUIRE(app()->getNumPagesOnStack() == 3);
    CHECK_EQ(get(KIND_A, 1, widgetCursor(0)), 1);
    CHECK_EQ(get(KIND_A, 1, widgetCursor(1)), -1);
    CHECK_EQ(get(KIND_A, 2, widgetCursor(1)), -1);
    CHECK_EQ(get(KIND_A, 2, widgetCursor(2)), 3);
}

TEST("gui/widget_memory: forgetWidgetMemory drops the values of the widgets of the assets") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    set(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0, otherAssets()), 1);
    set(KIND_A, 1, widgetCursor(1, otherAssets()), 2);
    set(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 3);

    app()->forgetWidgetMemory(otherAssets());
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0, otherAssets())), -1);
    CHECK_EQ(get(KIND_A, 1, widgetCursor(1, otherAssets())), -1);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0)), 3);
}

TEST("gui/widget_memory: with no room the least recently set value is dropped") {
    // the data id makes the values different
    for (int i = 0; i < CONF_GUI_WIDGET_MEMORY_SIZE; i++) {
        set(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 100 + i, (int16_t)i);
    }
    // the first one is now the most recently set
    set(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 100, 0);

    set(KIND_B, WIDGET_MEMORY_APP, widgetCursor(0), 1);
    CHECK_EQ(get(KIND_B, WIDGET_MEMORY_APP, widgetCursor(0)), 1);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 0), 100);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 1), -1);
    CHECK_EQ(get(KIND_A, WIDGET_MEMORY_APP, widgetCursor(0), 2), 102);
}
