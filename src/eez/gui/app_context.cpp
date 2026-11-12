/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <eez/core/sound.h>
#include <eez/core/os.h>
#include <eez/core/util.h>

#if OPTION_KEYBOARD
#include <eez/core/keyboard.h>
#endif

#if OPTION_MOUSE
#include <eez/core/mouse.h>
#endif

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>
#include <eez/gui/touch_calibration.h>
#include <eez/gui/widgets/button.h>

#include <eez/flow/debugger.h>
#include <eez/flow/private.h>

#include <eez/core/hmi.h>

#define CONF_GUI_TOAST_DURATION_MS 1000L

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

AppContext::AppContext() {
    m_updatePageIndex = -1;
}

void AppContext::stateManagment() {
    // remove alert message after period of time
    if (getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        ToastMessagePage *page = (ToastMessagePage *)getActivePage();
        if (!page->hasAction() && eez::hmi::getInactivityPeriodMs() >= CONF_GUI_TOAST_DURATION_MS) {
            popPage();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::isActivePageInternal() {
    return isPageInternal(getActivePageId());
}

bool AppContext::isTouchScrollEnabled(const WidgetCursor &widgetCursor) {
    return true;
}

bool AppContext::isWidgetActionEnabled(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    auto action = getWidgetAction(widgetCursor);
    if (action) {
        if (widget->type == WIDGET_TYPE_BUTTON) {
            auto buttonWidget = (const ButtonWidget *)widget;
			auto enabled = get(widgetCursor, buttonWidget->enabled);
            if (!(enabled.getType() == VALUE_TYPE_UNDEFINED || enabled.getInt() ? 1 : 0)) {
                return false;
            }
        }

        return true;
    }

    return false;
}

bool AppContext::isAutoRepeatAction(int action) {
    return false;
}

bool AppContext::isFocusWidget(const WidgetCursor &widgetCursor) {
    return false;
}

////////////////////////////////////////////////////////////////////////////////

void AppContext::onBeforePushPage(int pageId) {
}

void AppContext::onBeforeShowPage(int pageId) {
}

void AppContext::onPageChanged(int previousPageId, int activePageId, bool activePageIsFromStack, bool previousPageIsStillOnStack) {
    display::turnOn();
    hmi::noteActivity();

#if OPTION_MOUSE
    mouse::onPageChanged();
#endif

#if OPTION_KEYBOARD
    keyboard::onPageChanged();
#endif

    flow::onPageChanged(previousPageId, activePageId, activePageIsFromStack, previousPageIsStillOnStack);
}

void AppContext::doShowPage(int pageId, Page *page, int previousPageId, bool activePageIsFromStack, bool previousPageIsStillOnStack) {
#if CONF_OPTION_FPGA
    pageId = PAGE_ID_WELCOME_800X480;
    page = nullptr;
#endif

    page = page ? page : g_hooks.getPageFromId(pageId);

    m_pageNavigationStack[m_pageNavigationStackPointer].page = page;
    m_pageNavigationStack[m_pageNavigationStackPointer].pageId = pageId;
    m_pageNavigationStack[m_pageNavigationStackPointer].displayBufferIndex = -1;
    m_pageNavigationStack[m_pageNavigationStackPointer].timelinePosition = 0;

    if (page) {
        page->m_pageAppContext = this;
        page->pageWillAppear();
    }

    m_showPageTime = millis();

    onPageChanged(previousPageId, pageId, activePageIsFromStack, previousPageIsStillOnStack);

    refreshScreen();
}

void AppContext::setPage(int pageId) {
    int previousPageId = getActivePageId();

    // delete stack
    for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].page) {
            m_pageNavigationStack[i].page->pageFree();
        }
    }
    m_pageNavigationStackPointer = 0;

    //
    doShowPage(pageId, nullptr, previousPageId, false, false);
}

void AppContext::replacePage(int pageId, Page *page) {
    int previousPageId = getActivePageId();

    // the page that stays on the stack is not freed
    Page *activePage = getActivePage();
    if (activePage && activePage != page) {
    	activePage->pageFree();
    }

    doShowPage(pageId, page, previousPageId, false, false);
}

void AppContext::pushPage(int pageId, Page *page) {
    if (pushPageInGuiThread(this, pageId, page)) {
        return;
    }

    // the page goes above the active page, except above these pages, which it replaces
    bool advance = getActivePageId() != EEZ_PAGE_ID_NONE && getActivePageId() != EEZ_PAGE_ID_ASYNC_OPERATION_IN_PROGRESS && getActivePageId() != INTERNAL_PAGE_ID_TOAST_MESSAGE;

    if (advance && m_pageNavigationStackPointer + 1 >= CONF_GUI_PAGE_NAVIGATION_STACK_SIZE) {
        // the page stack is full: the page is not shown
        printf("pushPage: the page stack is full (%d pages), page %d is not shown\n", CONF_GUI_PAGE_NAVIGATION_STACK_SIZE, pageId);
        if (page && !isPageObjectOnStack(page)) {
            // e.g. a toast or a question that waits to be closed is dismissed
            page->pageFree();
        }
        return;
    }

    onBeforePushPage(pageId);

    int previousPageId = getActivePageId();

    // advance stack pointer
    if (advance) {
        m_pageNavigationStackPointer++;
    } else {
        // the replaced page is freed, unless it is the pushed page itself (a toast
        // replaced by a newer toast)
        Page *replacedPage = getActivePage();
        if (replacedPage && replacedPage != page && !isPageObjectOnStack(replacedPage, m_pageNavigationStackPointer)) {
            replacedPage->pageFree();
        }
    }

    doShowPage(pageId, page, previousPageId, false, true);
}

void AppContext::popPage() {
    if (m_pageNavigationStackPointer > 0) {
        int previousPageId = getActivePageId();

        if (m_pageNavigationStack[m_pageNavigationStackPointer].page) {
            m_pageNavigationStack[m_pageNavigationStackPointer].page->pageFree();
            m_pageNavigationStack[m_pageNavigationStackPointer].page = nullptr;
        }
        --m_pageNavigationStackPointer;

        doShowPage(m_pageNavigationStack[m_pageNavigationStackPointer].pageId, m_pageNavigationStack[m_pageNavigationStackPointer].page, previousPageId, true, false);
    }
}

void AppContext::removePageFromStack(int pageId) {
    for (int i = m_pageNavigationStackPointer; i > 0; i--) {
        if (m_pageNavigationStack[i].pageId == pageId) {
            if (i == m_pageNavigationStackPointer) {
                popPage();
            } else {
                if (m_pageNavigationStack[i].page) {
                    m_pageNavigationStack[i].page->pageFree();
                }

                for (int j = i + 1; j <= m_pageNavigationStackPointer; j++) {
                    memcpy(m_pageNavigationStack + j - 1, m_pageNavigationStack + j, sizeof(PageOnStack));
                }

                m_pageNavigationStack[m_pageNavigationStackPointer].page = nullptr;

                --m_pageNavigationStackPointer;
            }
            refreshScreen();
            break;
        }
    }
}

Page *AppContext::getPage(int pageId) {
    for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].pageId == pageId) {
            return m_pageNavigationStack[i].page;
        }
    }
    return nullptr;
}

bool AppContext::isPageOnStack(int pageId) {
    for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].pageId == pageId) {
            return true;
        }
    }
    return false;
}

bool AppContext::isPageObjectOnStack(const Page *page, int exceptIndex) {
    for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
        if (i != exceptIndex && m_pageNavigationStack[i].page == page) {
            return true;
        }
    }
    return false;
}

void AppContext::showPage(int pageId) {
    if (showPageInGuiThread(this, pageId)) {
        return;
    }

    onBeforeShowPage(pageId);

    if (pageId != getActivePageId()) {
        setPage(pageId);
    }
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::testExecuteActionOnTouchDown(int action) {
    return false;
}

bool AppContext::isBlinking(const WidgetCursor &widgetCursor, int16_t id) {
    return false;
}

bool AppContext::canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action) {
    return false;
}

void AppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
    int activePageId = getActivePageId();
#if OPTION_TOUCH_CALIBRATION
    if (activePageId == PAGE_ID_TOUCH_CALIBRATION) {
        onTouchCalibrationPageTouch(foundWidget, touchEvent);
        return;
    }
#endif

    if (activePageId != EEZ_PAGE_ID_NONE && !isPageInternal(activePageId)) {
        auto page = getPageAsset(activePageId);
        if ((page->flags & CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG) != 0) {
            int xPage;
            int yPage;
            int wPage;
            int hPage;
            getPageRect(activePageId, getActivePage(), xPage, yPage, wPage, hPage);

            // widgets are checked only at touch down: the rest of the touch that closed
            // the page above must not close this page also
            if (
                !pointInsideRect(touchEvent.x, touchEvent.y, xPage, yPage, wPage, hPage) ||
                (touchEvent.type == EVENT_TYPE_TOUCH_DOWN && isOutsideActivePageWidgets(this, touchEvent.x, touchEvent.y))
            ) {
                int activePageId = getActivePageId();

                // clicked outside page, close page
                popPage();

                auto widgetCursor = findWidget(touchEvent.x, touchEvent.y);

                if (widgetCursor.widget) {
                   auto action = getWidgetAction(widgetCursor);
                   if (action != EEZ_ACTION_ID_NONE && canExecuteActionWhenTouchedOutsideOfActivePage(activePageId, action)) {
                       processTouchEvent(touchEvent);
                   }
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

// page with transparent background shows what is below it
static bool isPageTransparent(int pageId) {
    if (isPageInternal(pageId)) {
        return false;
    }

    Assets *assets = g_mainAssets;
    const PageAsset *page = nullptr;
    if (pageId > 0) {
        page = g_mainAssets->pages[pageId - 1];
    } else if (pageId < 0) {
        assets = g_widgetCursor.assets;
        int externalPageId = g_hooks.resolveExternalPage(pageId, &assets);
        if (assets) {
            page = assets->pages[externalPageId - 1];
        }
    }
    if (!page) {
        return false;
    }

    // style of the external page is from its assets
    auto savedAssets = g_widgetCursor.assets;
    g_widgetCursor.assets = assets;
    auto style = getStyle(page->style);
    g_widgetCursor.assets = savedAssets;

    return style->backgroundColor == TRANSPARENT_COLOR_INDEX || style->opacity != 255;
}

void AppContext::updatePage(int i, WidgetCursor &widgetCursor) {
	if (g_findCallback == nullptr) {
		m_pageNavigationStack[i].displayBufferIndex = display::beginBufferRendering();
	}

    m_updatePageIndex = i;

    int x;
    int y;
    int width;
    int height;
    bool withShadow;
    Rect *backdrop;

    if (isPageInternal(m_pageNavigationStack[i].pageId)) {
        auto internalPage = ((InternalPage *)m_pageNavigationStack[i].page);

		x = internalPage->x;
		y = internalPage->y;
		width = internalPage->width;
		height = internalPage->height;
		withShadow = hasPageBackdrops();
		backdrop = hasPageBackdrops() && g_hooks.activePageHasBackdrop() ? &rect : nullptr;

        widgetCursor.w = width;
        widgetCursor.h = height;

		if (g_findCallback == nullptr) {
			internalPage->updateInternalPage();
		}

		enumNoneWidget();
    } else {
        auto page = getPageAsset(m_pageNavigationStack[i].pageId, widgetCursor);

        auto savedHasPreviousState = widgetCursor.hasPreviousState;
        if (widgetCursor.flowState && widgetCursor.flowState->timelinePosition != m_pageNavigationStack[i].timelinePosition) {
            widgetCursor.hasPreviousState = false;
            m_pageNavigationStack[i].timelinePosition = widgetCursor.flowState->timelinePosition;
        }

        auto savedWidget = widgetCursor.widget;
        widgetCursor.widget = page;

        if ((page->flags & PAGE_SCALE_TO_FIT) && flow::g_debuggerMode == flow::DEBUGGER_MODE_RUN) {
		    x = rect.x;
		    y = rect.y;
		    width = rect.w;
		    height = rect.h;
        } else {
            x = widgetCursor.x + page->x;
            y = widgetCursor.y + page->y;
            width = page->width;
            height = page->height;
        }

        bool transparent = isPageTransparent(m_pageNavigationStack[i].pageId);

        if (!hasPageBackdrops()) {
            withShadow = false;
            backdrop = nullptr;
        } else if (transparent) {
            // never with shadow, but always with backdrop (also when the page is full size)
            withShadow = false;
            backdrop = &rect;
        } else {
            withShadow = page->x > 0;
            backdrop = withShadow && g_hooks.activePageHasBackdrop() ? &rect : nullptr;
        }

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;

        widgetCursor.x = x;
        widgetCursor.y = y;

        widgetCursor.w = width;
        widgetCursor.h = height;

        auto savedRefreshed = widgetCursor.refreshed;

        BackgroundStyle savedBackgroundStyleStack[BACKGROUND_STYLE_STACK_SIZE];
        auto savedBackgroundStyleStackPointer = widgetCursor.backgroundStyleStackPointer;

        if (transparent && g_findCallback == nullptr) {
            // what is below the page is its background (see drawBorderAndBackground),
            // so the backgrounds outside of the page are not used
            memcpy(savedBackgroundStyleStack, widgetCursor.backgroundStyleStack, sizeof(savedBackgroundStyleStack));
            widgetCursor.backgroundStyleStackPointer = 0;

            bool backgroundChanged = display::setBufferBackgroundBelow(x, y, width, height, backdrop);
            if (backgroundChanged || widgetCursor.refreshed) {
                display::drawBufferBackground(x, y, x + width - 1, y + height - 1);
                widgetCursor.refreshed = true;
            }
        }

        enumWidget();

        if (transparent && g_findCallback == nullptr) {
            widgetCursor.refreshed = savedRefreshed;
            memcpy(widgetCursor.backgroundStyleStack, savedBackgroundStyleStack, sizeof(savedBackgroundStyleStack));
            widgetCursor.backgroundStyleStackPointer = savedBackgroundStyleStackPointer;
        }

        widgetCursor.x = savedX;
        widgetCursor.y = savedY;

        widgetCursor.widget = savedWidget;
        widgetCursor.hasPreviousState = savedHasPreviousState;
    }

	if (g_findCallback == nullptr) {
        pageRenderCustom(i, widgetCursor);
		display::endBufferRendering(m_pageNavigationStack[i].displayBufferIndex, x, y, width, height, withShadow, 255, 0, 0, backdrop);
	}

    m_updatePageIndex = -1;
}

void AppContext::pageRenderCustom(int i, WidgetCursor &widgetCursor) {
}

bool isRect1FullyCoveredByRect2(int xRect1, int yRect1, int wRect1, int hRect1, int xRect2, int yRect2, int wRect2, int hRect2) {
    return xRect2 <= xRect1 && yRect2 <= yRect1 && xRect2 + wRect2 >= xRect1 + wRect1 && yRect2 + hRect2 >= yRect1 + hRect1;
}

void AppContext::getPageRect(int pageId, const Page *page, int &x, int &y, int &w, int &h) {
    if (isPageInternal(pageId)) {
        x = rect.x + ((InternalPage *)page)->x;
        y = rect.y + ((InternalPage *)page)->y;
        w = ((InternalPage *)page)->width;
        h = ((InternalPage *)page)->height;
    } else {
        auto page = getPageAsset(pageId);
        if ((page->flags & PAGE_SCALE_TO_FIT) && flow::g_debuggerMode == flow::DEBUGGER_MODE_RUN) {
		    x = rect.x;
		    y = rect.y;
		    w = rect.w;
		    h = rect.h;
        } else {
            x = rect.x + page->x;
            y = rect.y + page->y;
            w = page->width;
            h = page->height;
        }
    }
}

bool AppContext::isPageFullyCovered(int pageNavigationStackIndex) {
    int xPage, yPage, wPage, hPage;
    getPageRect(m_pageNavigationStack[pageNavigationStackIndex].pageId, m_pageNavigationStack[pageNavigationStackIndex].page, xPage, yPage, wPage, hPage);

    for (int i = pageNavigationStackIndex + 1; i <= m_pageNavigationStackPointer; i++) {
        if (isPageTransparent(m_pageNavigationStack[i].pageId)) {
            // page below is seen through
            continue;
        }

        int xPageAbove, yPageAbove, wPageAbove, hPageAbove;
        getPageRect(m_pageNavigationStack[i].pageId, m_pageNavigationStack[i].page, xPageAbove, yPageAbove, wPageAbove, hPageAbove);

        if (isRect1FullyCoveredByRect2(xPage, yPage, wPage, hPage, xPageAbove, yPageAbove, wPageAbove, hPageAbove)) {
            return true;
        }
    }

    return false;
}

int AppContext::getLongTouchActionHook(const WidgetCursor &widgetCursor) {
    return EEZ_ACTION_ID_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// Toasts and dialogs asked for on another thread are made on the GUI thread
// (the toast and dialog pages, the font used to measure the texts and the page
// stack belong to the GUI thread).

enum DeferredMessageKind {
    DEFERRED_INFO,
    DEFERRED_INFO_WITH_ACTION,
    DEFERRED_INFO_WITH_ACTION_PARAM,
    DEFERRED_ERROR,
    DEFERRED_ERROR_WITH_ACTION,
    DEFERRED_ERROR_WITH_ACTION_PARAM,
    DEFERRED_YES_NO,
    DEFERRED_QUESTION
};

struct DeferredMessage {
    DeferredMessageKind kind;
    AppContext *appContext;
    // copies only this message references (copyForGuiThread)
    Value message;
    Value buttons;
    bool autoDismiss = false;
    void (*action)() = nullptr;
    void (*actionWithParam)(int param) = nullptr;
    void (*dismissed)(int param) = nullptr;
    const char *actionLabel = nullptr;
    int actionParam = 0;
    int yesNoPageId = 0;
    void (*yesCallback)() = nullptr;
    void (*noCallback)() = nullptr;
    void (*cancelCallback)() = nullptr;
    void *userParam = nullptr;
    void (*questionCallback)(void *userParam, unsigned buttonIndex) = nullptr;
    void (*questionDismissed)(void *userParam) = nullptr;
};

// A copy of the value for the GUI thread: the reference counts of Values are not
// thread safe, so strings and arrays are copied into new ones that only the
// message references; other values (no reference) are copied as they are.
static Value copyForGuiThread(const Value &value) {
    Value v = value.getValue();
    if (!(v.getOptions() & VALUE_OPTIONS_REF)) {
        return v;
    }

    if (v.isArray()) {
        auto array = v.getArray();
        Value copy = Value::makeArrayRef(array->arraySize, array->arrayType, 0x3b5d81e2);
        auto copyArray = copy.getArray();
        if (!copyArray) {
            return Value();
        }
        for (uint32_t i = 0; i < array->arraySize; i++) {
            copyArray->values[i] = copyForGuiThread(array->values[i]);
        }
        return copy;
    }

    Value text = v.toString(0x6a0e2b41);
    const char *str = text.getString();
    return Value::makeStringRef(str ? str : "", -1, 0x47c90d16);
}

static bool deferMessage(DeferredMessage *message) {
    return callInGuiThread(AppContext::showDeferredMessage, message);
}

void AppContext::showDeferredMessage(void *param) {
    auto message = (DeferredMessage *)param;
    auto appContext = message->appContext;

    switch (message->kind) {
    case DEFERRED_INFO:
        appContext->pushToastMessage(ToastMessagePage::create(appContext, INFO_TOAST, message->message));
        break;
    case DEFERRED_INFO_WITH_ACTION:
        appContext->pushToastMessage(ToastMessagePage::create(appContext, INFO_TOAST, message->message, message->action, message->actionLabel));
        break;
    case DEFERRED_INFO_WITH_ACTION_PARAM:
        appContext->pushToastMessage(ToastMessagePage::create(appContext, INFO_TOAST, message->message, message->actionWithParam, message->actionLabel, message->actionParam, message->dismissed));
        break;
    case DEFERRED_ERROR:
        appContext->pushToastMessage(ToastMessagePage::create(appContext, ERROR_TOAST, message->message, message->autoDismiss));
        sound::playBeep();
        break;
    case DEFERRED_ERROR_WITH_ACTION:
        appContext->pushToastMessage(ToastMessagePage::create(appContext, ERROR_TOAST, message->message, message->action, message->actionLabel));
        sound::playBeep();
        break;
    case DEFERRED_ERROR_WITH_ACTION_PARAM:
        appContext->pushToastMessage(ToastMessagePage::create(appContext, ERROR_TOAST, message->message, message->actionWithParam, message->actionLabel, message->actionParam, message->dismissed));
        sound::playBeep();
        break;
    case DEFERRED_YES_NO:
        appContext->yesNoDialog(message->yesNoPageId, message->message, message->yesCallback, message->noCallback, message->cancelCallback);
        break;
    case DEFERRED_QUESTION:
        appContext->questionDialog(message->message, message->buttons, message->userParam, message->questionCallback, message->questionDismissed);
        break;
    }

    ObjectAllocator<DeferredMessage>::deallocate(message);
}

// a DeferredMessage for a call on another thread, nullptr on the GUI thread
static DeferredMessage *newDeferredMessage(AppContext *appContext, DeferredMessageKind kind, const Value &message) {
    if (isGuiThread()) {
        return nullptr;
    }
    auto deferredMessage = ObjectAllocator<DeferredMessage>::allocate(0x2c6f3a9d);
    if (!deferredMessage) {
        return nullptr;
    }
    deferredMessage->kind = kind;
    deferredMessage->appContext = appContext;
    deferredMessage->message = copyForGuiThread(message);
    return deferredMessage;
}

// a string copied into a string Value, for a message from another thread (it can
// be in a buffer of the caller)
static Value stringForGuiThread(const char *message) {
    return isGuiThread() ? Value(message) : Value::makeStringRef(message ? message : "", -1, 0x1f7c3e55);
}

void AppContext::yesNoDialog(int yesNoPageId, const char *message, void (*yes_callback)(), void (*no_callback)(), void (*cancel_callback)()) {
    if (!isGuiThread()) {
        // a copy of the message for the GUI thread
        yesNoDialog(yesNoPageId, Value::makeStringRef(message ? message : "", -1, 0x5d2a7c13), yes_callback, no_callback, cancel_callback);
        return;
    }

    set(WidgetCursor(), EEZ_DATA_ID_ALERT_MESSAGE, Value(message));

    m_dialogYesCallback = yes_callback;
    m_dialogNoCallback = no_callback;
    m_dialogCancelCallback = cancel_callback;

    pushPage(yesNoPageId);
}

void AppContext::yesNoDialog(int yesNoPageId, Value value, void(*yes_callback)(), void(*no_callback)(), void(*cancel_callback)()) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_YES_NO, value);
    if (deferredMessage) {
        deferredMessage->yesNoPageId = yesNoPageId;
        deferredMessage->yesCallback = yes_callback;
        deferredMessage->noCallback = no_callback;
        deferredMessage->cancelCallback = cancel_callback;
        if (deferMessage(deferredMessage)) {
            return;
        }
    }

	set(WidgetCursor(), EEZ_DATA_ID_ALERT_MESSAGE, value);

	m_dialogYesCallback = yes_callback;
	m_dialogNoCallback = no_callback;
	m_dialogCancelCallback = cancel_callback;

	pushPage(yesNoPageId);
}

void AppContext::infoMessage(const char *message) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_INFO, stringForGuiThread(message));
    if (deferredMessage && deferMessage(deferredMessage)) {
        return;
    }
    pushToastMessage(ToastMessagePage::create(this, INFO_TOAST, message));
}

void AppContext::infoMessage(Value value) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_INFO, value);
    if (deferredMessage && deferMessage(deferredMessage)) {
        return;
    }
    pushToastMessage(ToastMessagePage::create(this, INFO_TOAST, value));
}

void AppContext::infoMessage(const char *message, void (*action)(), const char *actionLabel) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_INFO_WITH_ACTION, stringForGuiThread(message));
    if (deferredMessage) {
        deferredMessage->action = action;
        deferredMessage->actionLabel = actionLabel;
        if (deferMessage(deferredMessage)) {
            return;
        }
    }
    pushToastMessage(ToastMessagePage::create(this, INFO_TOAST, message, action, actionLabel));
}

void AppContext::errorMessage(const char *message, bool autoDismiss) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_ERROR, stringForGuiThread(message));
    if (deferredMessage) {
        deferredMessage->autoDismiss = autoDismiss;
        if (deferMessage(deferredMessage)) {
            return;
        }
    }
    AppContext::pushToastMessage(ToastMessagePage::create(this, ERROR_TOAST, message, autoDismiss));
    sound::playBeep();
}

void AppContext::errorMessage(Value value) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_ERROR, value);
    if (deferredMessage) {
        // the Value overload of create has no auto dismiss (a Close button)
        deferredMessage->autoDismiss = false;
        if (deferMessage(deferredMessage)) {
            return;
        }
    }
    AppContext::pushToastMessage(ToastMessagePage::create(this, ERROR_TOAST, value));
    sound::playBeep();
}

void AppContext::infoMessageWithAction(Value value, void (*action)(int param), const char *actionLabel, int actionParam, void (*dismissed)(int param)) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_INFO_WITH_ACTION_PARAM, value);
    if (deferredMessage) {
        deferredMessage->actionWithParam = action;
        deferredMessage->actionLabel = actionLabel;
        deferredMessage->actionParam = actionParam;
        deferredMessage->dismissed = dismissed;
        if (deferMessage(deferredMessage)) {
            return;
        }
    }
    pushToastMessage(ToastMessagePage::create(this, INFO_TOAST, value, action, actionLabel, actionParam, dismissed));
}

void AppContext::errorMessageWithAction(Value value, void (*action)(int param), const char *actionLabel, int actionParam, void (*dismissed)(int param)) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_ERROR_WITH_ACTION_PARAM, value);
    if (deferredMessage) {
        deferredMessage->actionWithParam = action;
        deferredMessage->actionLabel = actionLabel;
        deferredMessage->actionParam = actionParam;
        deferredMessage->dismissed = dismissed;
        if (deferMessage(deferredMessage)) {
            return;
        }
    }
    AppContext::pushToastMessage(ToastMessagePage::create(this, ERROR_TOAST, value, action, actionLabel, actionParam, dismissed));
    sound::playBeep();
}

void AppContext::errorMessageWithAction(const char *message, void (*action)(), const char *actionLabel) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_ERROR_WITH_ACTION, stringForGuiThread(message));
    if (deferredMessage) {
        deferredMessage->action = action;
        deferredMessage->actionLabel = actionLabel;
        if (deferMessage(deferredMessage)) {
            return;
        }
    }
    AppContext::pushToastMessage(ToastMessagePage::create(this, ERROR_TOAST, message, action, actionLabel));
    sound::playBeep();
}

void AppContext::pushToastMessage(ToastMessagePage *toastMessage) {
    pushPage(INTERNAL_PAGE_ID_TOAST_MESSAGE, toastMessage);
}

void AppContext::questionDialog(Value message, Value buttons, void *userParam, void (*callback)(void *userParam, unsigned buttonIndex), void (*dismissed)(void *userParam)) {
    auto deferredMessage = newDeferredMessage(this, DEFERRED_QUESTION, message);
    if (deferredMessage) {
        deferredMessage->buttons = copyForGuiThread(buttons);
        deferredMessage->userParam = userParam;
        deferredMessage->questionCallback = callback;
        deferredMessage->questionDismissed = dismissed;
        if (deferMessage(deferredMessage)) {
            return;
        }
    }

    // the question page is one object: the question shown before is closed (it is
    // dismissed)
    removePageFromStack(INTERNAL_PAGE_ID_QUESTION);

    pushPage(INTERNAL_PAGE_ID_QUESTION, QuestionPage::create(this, message, buttons, userParam, callback, dismissed));
}

void AppContext::getBoundingRect(Rect &rectBounding) {
    if (m_pageNavigationStackPointer >= 0) {
        int x1 = rect.x + rect.w;
        int y1 = rect.y + rect.h;
        int x2 = rect.x;
        int y2 = rect.y;

        for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
            if (!isPageInternal(m_pageNavigationStack[i].pageId)) {
                int xPage, yPage, wPage, hPage;
                getPageRect(m_pageNavigationStack[i].pageId, m_pageNavigationStack[i].page, xPage, yPage, wPage, hPage);
                if (xPage < x1) x1 = xPage;
                if (xPage + wPage > x2) x2 = xPage + wPage;
                if (yPage < y1) y1 = yPage;
                if (yPage + hPage > y2) y2 = yPage + hPage;
            }
        }

        rectBounding.x = x1;
        rectBounding.y = y1;
        rectBounding.w = x2 - x1;
        rectBounding.h = y2 - y1;
    } else {
        rectBounding.x = rect.x;
        rectBounding.y = rect.y;
        rectBounding.w = rect.w;
        rectBounding.h = rect.h;
    }
}

AppContext *getRootAppContext() {
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
    return getAppContextFromId(APP_CONTEXT_ID_SIMULATOR_FRONT_PANEL);
#else
    return getAppContextFromId(APP_CONTEXT_ID_DEVICE);
#endif
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
