/*
 * eez-framework tests: gui/app_context.cpp, the page stack.
 */

#include "gui_test.h"

using namespace gui_test;

TEST("gui/app_context: showPage makes the page active and the only page on the stack") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    app()->pushPage(PAGE_ID_GUI_PAGE_B);
    REQUIRE(app()->getNumPagesOnStack() == 3);

    app()->showPage(PAGE_ID_GUI_DIALOG);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_DIALOG);
    CHECK_EQ(app()->getNumPagesOnStack(), 1);
    CHECK(!app()->isPageOnStack(PAGE_ID_MAIN));
}

TEST("gui/app_context: showPage of the active page does nothing") {
    TestPage page;
    app()->pushPage(PAGE_ID_GUI_PAGE_A, &page);
    harness::advance(100);
    auto showTime = app()->getShowPageTime();

    app()->showPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
    CHECK_EQ(page.freeCount, 0);
    CHECK_EQ(app()->getShowPageTime(), showTime);
}

TEST("gui/app_context: pushPage puts the page on the stack, popPage returns to the previous page") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
    CHECK_EQ(app()->getPreviousPageId(), (int)PAGE_ID_MAIN);

    app()->pushPage(PAGE_ID_GUI_PAGE_B);
    CHECK_EQ(app()->getNumPagesOnStack(), 3);
    CHECK_EQ(app()->getPreviousPageId(), (int)PAGE_ID_GUI_PAGE_A);

    app()->popPage();
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
    app()->popPage();
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
    CHECK_EQ(app()->getNumPagesOnStack(), 1);
}

TEST("gui/app_context: popPage of the only page on the stack does nothing") {
    app()->popPage();
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
    CHECK_EQ(app()->getNumPagesOnStack(), 1);
    harness::frame(2);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/app_context: the same page can be pushed twice") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getNumPagesOnStack(), 3);
    app()->popPage();
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_A);
}

TEST("gui/app_context: getPage and isPageOnStack find pages below the top") {
    TestPage pageA;
    TestPage pageB;
    app()->pushPage(PAGE_ID_GUI_PAGE_A, &pageA);
    app()->pushPage(PAGE_ID_GUI_PAGE_B, &pageB);

    CHECK(app()->getPage(PAGE_ID_GUI_PAGE_A) == &pageA);
    CHECK(app()->getPage(PAGE_ID_GUI_PAGE_B) == &pageB);
    CHECK(app()->getActivePage() == &pageB);
    CHECK(app()->isPageOnStack(PAGE_ID_MAIN));
    CHECK(app()->isPageOnStack(PAGE_ID_GUI_PAGE_A));
    CHECK(!app()->isPageOnStack(PAGE_ID_GUI_DIALOG));
    CHECK(app()->getPage(PAGE_ID_GUI_DIALOG) == nullptr);
}

TEST("gui/app_context: pageWillAppear is called on push and when the page is active again after popPage") {
    TestPage pageA;
    TestPage pageB;
    app()->pushPage(PAGE_ID_GUI_PAGE_A, &pageA);
    CHECK_EQ(pageA.willAppearCount, 1);
    app()->pushPage(PAGE_ID_GUI_PAGE_B, &pageB);
    CHECK_EQ(pageB.willAppearCount, 1);
    app()->popPage();
    CHECK_EQ(pageB.freeCount, 1);
    CHECK_EQ(pageA.willAppearCount, 2);
    CHECK_EQ(pageA.freeCount, 0);
    // pageAlloc is not called by the framework (BB3+ calls it from its getPageFromId hook)
    CHECK_EQ(pageA.allocCount, 0);
}

TEST("gui/app_context: the page object comes from the getPageFromId hook when none is given") {
    static TestPage hookPage;
    g_hooks.getPageFromId = [](int pageId) -> Page * {
        return pageId == PAGE_ID_GUI_PAGE_B ? &hookPage : nullptr;
    };
    app()->pushPage(PAGE_ID_GUI_PAGE_B);
    CHECK(app()->getActivePage() == &hookPage);
    CHECK_EQ(hookPage.willAppearCount, 1);
}

TEST("gui/app_context: replacePage replaces the active page and frees it") {
    TestPage pageA;
    TestPage pageB;
    app()->pushPage(PAGE_ID_GUI_PAGE_A, &pageA);
    app()->replacePage(PAGE_ID_GUI_PAGE_B, &pageB);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_B);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
    CHECK_EQ(pageA.freeCount, 1);
    CHECK(!app()->isPageOnStack(PAGE_ID_GUI_PAGE_A));
    app()->popPage();
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/app_context: showPage frees all pages of the stack") {
    TestPage pageA;
    TestPage pageB;
    app()->pushPage(PAGE_ID_GUI_PAGE_A, &pageA);
    app()->pushPage(PAGE_ID_GUI_PAGE_B, &pageB);
    app()->showPage(PAGE_ID_GUI_DIALOG);
    CHECK_EQ(pageA.freeCount, 1);
    CHECK_EQ(pageB.freeCount, 1);
}

TEST("gui/app_context: removePageFromStack removes a page below the top") {
    TestPage pageA;
    TestPage pageB;
    app()->pushPage(PAGE_ID_GUI_PAGE_A, &pageA);
    app()->pushPage(PAGE_ID_GUI_PAGE_B, &pageB);

    app()->removePageFromStack(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_B);
    CHECK(app()->getActivePage() == &pageB);
    CHECK(!app()->isPageOnStack(PAGE_ID_GUI_PAGE_A));
    CHECK_EQ(pageA.freeCount, 1);
    CHECK_EQ(pageB.freeCount, 0);

    app()->popPage();
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/app_context: removePageFromStack of the top page pops it") {
    TestPage pageA;
    app()->pushPage(PAGE_ID_GUI_PAGE_A, &pageA);
    app()->removePageFromStack(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
    CHECK_EQ(pageA.freeCount, 1);
}

TEST("gui/app_context: removePageFromStack removes only the topmost of two equal pages") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    app()->pushPage(PAGE_ID_GUI_PAGE_B);
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    app()->removePageFromStack(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getNumPagesOnStack(), 3);
    CHECK_EQ(activePageId(), (int)PAGE_ID_GUI_PAGE_B);
    CHECK(app()->isPageOnStack(PAGE_ID_GUI_PAGE_A));
}

TEST("gui/app_context: removePageFromStack does not remove the page at the bottom of the stack") {
    // pinned: index 0 is never removed and nothing is reported (G16 suggests
    // reporting the failure)
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    app()->removePageFromStack(PAGE_ID_MAIN);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
    CHECK(app()->isPageOnStack(PAGE_ID_MAIN));
}

TEST("gui/app_context: a page pushed over a toast replaces the toast") {
    app()->infoMessage("info");
    REQUIRE(activePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE);
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
    CHECK(!app()->isPageOnStack(INTERNAL_PAGE_ID_TOAST_MESSAGE));
    app()->popPage();
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/app_context: a toast replaces the toast shown before") {
    app()->infoMessage("first");
    app()->errorMessage("second");
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
    CHECK_EQ(activePageId(), (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
}

TEST("gui/app_context: a page pushed over the async operation page replaces it") {
    // the async operation page is transient: the page with the result replaces it
    app()->pushPage(PAGE_ID_ASYNC_OPERATION_IN_PROGRESS);
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getNumPagesOnStack(), 2);
    CHECK(!app()->isPageOnStack(PAGE_ID_ASYNC_OPERATION_IN_PROGRESS));
}

TEST_BUG("gui/app_context: a toast pushed over the async operation page doesn't remove it", "G12") {
    app()->pushPage(PAGE_ID_ASYNC_OPERATION_IN_PROGRESS);
    app()->errorMessage("error event");
    CHECK(app()->isPageOnStack(PAGE_ID_ASYNC_OPERATION_IN_PROGRESS));
}

TEST("gui/app_context: ten pages fit on the stack") {
    for (int i = 0; i < 9; i++) {
        app()->pushPage(i % 2 ? PAGE_ID_GUI_PAGE_A : PAGE_ID_GUI_PAGE_B);
    }
    CHECK_EQ(app()->getNumPagesOnStack(), 10);
    harness::frame(2);
    for (int i = 0; i < 9; i++) {
        app()->popPage();
    }
    CHECK_EQ(activePageId(), (int)PAGE_ID_MAIN);
}

TEST("gui/app_context: a push on a full page stack doesn't abort") {
    // e.g. DISP:WIND:DLOG sent 10 times: every call pushes the DLOG view page
    for (int i = 0; i < 11; i++) {
        app()->pushPage(PAGE_ID_GUI_PAGE_A);
    }
    CHECK(app()->getNumPagesOnStack() <= 10);
    harness::frame(2);
}

TEST("gui/app_context: a page change notes the time of the change") {
    harness::advance(1234);
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    CHECK_EQ(app()->getShowPageTime(), harness::now());
}

TEST("gui/app_context: getPageRect of a page is its rect in the app context") {
    int x, y, w, h;
    getPageRect(app(), PAGE_ID_GUI_POPUP, nullptr, x, y, w, h);
    CHECK_EQ(x, 200);
    CHECK_EQ(y, 120);
    CHECK_EQ(w, 400);
    CHECK_EQ(h, 240);
}

TEST("gui/app_context: getPageRect of a toast is the toast rect") {
    app()->infoMessage("info");
    auto toast = (InternalPage *)app()->getActivePage();
    int x, y, w, h;
    getPageRect(app(), INTERNAL_PAGE_ID_TOAST_MESSAGE, toast, x, y, w, h);
    CHECK_EQ(x, toast->x);
    CHECK_EQ(y, toast->y);
    CHECK_EQ(w, toast->width);
    CHECK_EQ(h, toast->height);
}

TEST_BUG("gui/app_context: getPageRect of a toast is the toast rect when the app context is not at 0, 0", "G16") {
    // e.g. the device app context inside the simulator front panel; the frame
    // rendering sets the rect from the AppView, so no frame runs here
    app()->rect.x = 100;
    app()->rect.y = 50;
    app()->infoMessage("info");
    auto toast = (InternalPage *)app()->getActivePage();
    int x, y, w, h;
    getPageRect(app(), INTERNAL_PAGE_ID_TOAST_MESSAGE, toast, x, y, w, h);
    CHECK_EQ(x, toast->x);
    CHECK_EQ(y, toast->y);
}

TEST("gui/app_context: a full screen page covers the page below") {
    app()->pushPage(PAGE_ID_GUI_PAGE_A);
    CHECK(isPageFullyCovered(app(), 0));
    CHECK(!isPageFullyCovered(app(), 1));
}

TEST("gui/app_context: a popup doesn't cover the page below") {
    app()->pushPage(PAGE_ID_GUI_POPUP);
    CHECK(!isPageFullyCovered(app(), 0));
}

TEST("gui/app_context: a transparent page doesn't cover the page below") {
    app()->pushPage(PAGE_ID_GUI_TRANSPARENT);
    CHECK(!isPageFullyCovered(app(), 0));
    // the page below the transparent page is drawn (dimmed by the backdrop):
    // its box is visible
    harness::frame(2);
    uint32_t inside = harness::pixel(60, 85) & 0xFFFFFF;
    uint32_t outside = harness::pixel(400, 300) & 0xFFFFFF;
    CHECK(inside != outside);
    CHECK((inside >> 16) > (inside & 0xFF));
}

TEST("gui/app_context: a page under a full screen page is not drawn") {
    app()->pushPage(PAGE_ID_GUI_PAGE_B);
    harness::frame(2);
    // main page box (10, 60, 100x50) is not visible, page B has the background there
    CHECK_EQ(harness::pixel(60, 85) & 0xFFFFFF, 0u);
}

TEST_BUG("gui/app_context: a page whose assets are gone is not rendered", "G16") {
    // BB3+: external (module) page whose assets were unloaded, the hook
    // resolves it to no assets
    g_hooks.resolveExternalPage = [](int pageId, Assets **assets) -> int {
        *assets = nullptr;
        return 0;
    };
    app()->pushPage(-1);
    harness::frame(2);
    CHECK_EQ(activePageId(), -1);
}
