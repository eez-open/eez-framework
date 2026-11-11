/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <eez/gui/widgets/button.h>
#include <eez/gui/widgets/rectangle.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/data.h>

namespace eez {
namespace gui {

extern Value g_alertMessage;

// Copies the text into the buffer of the given size (with the terminating 0). A
// longer text is cut at a UTF-8 character boundary. A null text is copied as an
// empty text.
void copyTextToBuffer(char *buffer, size_t bufferSize, const char *text);

////////////////////////////////////////////////////////////////////////////////

class Page {
    friend class AppContext;

public:
    virtual ~Page() {
    }

    virtual void pageAlloc();
    virtual void pageFree();

    virtual void pageWillAppear();

    virtual void onEncoder(int counter);
    virtual void onEncoderClicked();
    virtual Unit getEncoderUnit();

    virtual int getDirty();
    virtual void set();
    virtual void discard();

    virtual bool showAreYouSureOnDiscard();

    // the app context in which the page was shown last, the device app context if
    // the page was never shown
    AppContext *getPageAppContext();

private:
    // set by the app context when it shows the page
    AppContext *m_pageAppContext = nullptr;
};

////////////////////////////////////////////////////////////////////////////////

class SetPage : public Page {
public:
    virtual void edit();

protected:
    int16_t editDataId;

    static void onSetValue(float value);
    virtual void setValue(float value);
};

////////////////////////////////////////////////////////////////////////////////

class InternalPage : public Page {
public:
    virtual void updateInternalPage() = 0;
	virtual WidgetCursor findWidgetInternalPage(int x, int y, bool clicked) = 0;
    virtual bool canClickPassThrough();
    virtual bool closeIfTouchedOutside();

    int x;
    int y;
    int width;
    int height;

protected:
    Widget widget;
};

////////////////////////////////////////////////////////////////////////////////

enum ToastType {
    INFO_TOAST,
    ERROR_TOAST
};

// The toast page is one object: a new toast replaces the toast shown before.
// The texts (the message and the action label) are copied into the page.
class ToastMessagePage : public InternalPage {
    friend class AppContext;

    static ToastMessagePage *findFreePage();

public:
    static ToastMessagePage *create(AppContext *appContext, ToastType type, const char *message, bool autoDismiss = false);
    static ToastMessagePage *create(AppContext *appContext, ToastType type, Value message);
    static ToastMessagePage *create(AppContext *appContext, ToastType type, Value message, bool autoDismiss);
    // dismissed: called with actionParam when the toast is closed without its
    // button (touched outside, closed by code, replaced by another page or by a
    // newer toast)
    static ToastMessagePage *create(AppContext *appContext, ToastType type, Value message, void (*action)(int param), const char *actionLabel, int actionParam, void (*dismissed)(int param) = nullptr);
    static ToastMessagePage *create(AppContext *appContext, ToastType type, const char *message, void (*action)(), const char *actionLabel);
    static ToastMessagePage *create(AppContext *appContext, ToastType type, Value message, void (*action)(), const char *actionLabel);

    void init(AppContext *appContext, ToastType type, const Value& message);

    void pageFree();

    void onEncoder(int counter);
    void onEncoderClicked();

    void updateInternalPage();
    WidgetCursor findWidgetInternalPage(int x, int y, bool clicked);
    bool canClickPassThrough();
    bool closeIfTouchedOutside();

    bool hasAction() {
      return actionWidget.action != 0;
    }

    static void executeAction();
    static void executeActionWithoutParam();

	AppContext *appContext;
    Value messageValue;

private:
    ToastType type;

    static const size_t MAX_MESSAGE_LENGTH = 200;
    char messageBuffer[MAX_MESSAGE_LENGTH];

    static const size_t MAX_ACTION_LABEL_LENGTH = 48;
    char actionLabelBuffer[MAX_ACTION_LABEL_LENGTH];

    // actionLabelBuffer or nullptr (no button)
    const char *actionLabel;
    void (*action)(int param);
    int actionParam;
    void (*actionWithoutParam)();
    void (*dismissed)(int param);

    void setActionLabel(const char *label);
    void dismiss();

    RectangleWidget containerRectangleWidget;
    TextWidget line1Widget;
    TextWidget line2Widget;
    TextWidget line3Widget;
    ButtonWidget actionWidget;

    WidgetCursor lastActiveWidget;
};

////////////////////////////////////////////////////////////////////////////////

class SelectFromEnumPage : public InternalPage {
public:
    static void pushSelectFromEnumPage(
        AppContext *appContext,
        const EnumItem *enumItems,
        uint16_t currentValue,
        bool (*disabledCallback)(uint16_t value),
        void (*onSet)(uint16_t),
        bool smallFont = false,
        bool showRadioButtonIcon = true
    );

    static void pushSelectFromEnumPage(
        AppContext *appContext,
        void (*enumDefinitionFunc)(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value),
        uint16_t currentValue,
        bool (*disabledCallback)(uint16_t value),
        void (*onSet)(uint16_t),
        bool smallFont = false,
        bool showRadioButtonIcon = true
    );

    static const EnumItem *getActiveSelectEnumDefinition();
    static void selectEnumItem();
    static void popSelectFromEnumPage();

    void init(
        AppContext *appContext_,
        const EnumItem *enumDefinition_,
        uint16_t currentValue_,
    	bool (*disabledCallback_)(uint16_t value),
        void (*onSet_)(uint16_t),
        bool smallFont_,
        bool showRadioButtonIcon_
    );

    void init(
        AppContext *appContext_,
        void (*enumDefinitionFunc)(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value),
        uint16_t currentValue_,
        bool (*disabledCallback_)(uint16_t value),
        void (*onSet_)(uint16_t),
        bool smallFont_,
        bool showRadioButtonIcon_
    );

    void init();

    void updateInternalPage();
    WidgetCursor findWidgetInternalPage(int x, int y, bool clicked);

    const EnumItem *getEnumDefinition() {
        return enumDefinition;
    }

    AppContext *appContext;

private:
    const EnumItem *enumDefinition;
    void (*enumDefinitionFunc)(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);

    int numItems;
    int numColumns;
    int itemWidth;
    int itemHeight;

    int activeItemIndex;

    bool dirty;

    uint16_t getValue(int i);
    bool getLabel(int i, char *text, int count);

    uint16_t currentValue;
    bool (*disabledCallback)(uint16_t value);
    void (*onSet)(uint16_t);

    bool smallFont;
    bool showRadioButtonIcon;

    bool calcSize();
    void findPagePosition();

    bool isDisabled(int i);

    void getItemPosition(int itemIndex, int &x, int &y);

	void doSelectEnumItem();
};

////////////////////////////////////////////////////////////////////////////////

enum MenuType {
    MENU_TYPE_BUTTON
};

// the max. number of buttons of the menu with buttons and of the question page
static const size_t MAX_MENU_ITEMS = 4;

// sizes of the text buffers of the menu with buttons and of the question page
static const size_t MAX_MENU_MESSAGE_LENGTH = 200;
static const size_t MAX_MENU_ITEM_LENGTH = 48;

// menuItems: ends with nullptr, only the first MAX_MENU_ITEMS items are shown
void showMenu(AppContext *appContext, const char *message, MenuType menuType, const char **menuItems, void(*callback)(int));

// The menu page is one object: the texts are copied into it.
class MenuWithButtonsPage : public InternalPage {
public:
    static MenuWithButtonsPage *create(AppContext *appContext, const char *message, const char **menuItems, void (*callback)(int));

    void updateInternalPage();
    WidgetCursor findWidgetInternalPage(int x, int y, bool clicked);

    static void executeAction();

private:
    AppContext *m_appContext;
    RectangleWidget m_containerRectangleWidget;
    TextWidget m_messageTextWidget;
    TextWidget m_buttonTextWidgets[MAX_MENU_ITEMS];
    size_t m_numButtonTextWidgets;
    void (*m_callback)(int);

    char m_messageBuffer[MAX_MENU_MESSAGE_LENGTH];
    char m_buttonTextBuffers[MAX_MENU_ITEMS][MAX_MENU_ITEM_LENGTH];

    // drawn again only after init or when another button is pressed
    bool m_dirty;
    WidgetCursor m_lastActiveWidget;

    void init(AppContext *appContext, const char *message, const char **menuItems, void(*callback)(int));
};

////////////////////////////////////////////////////////////////////////////////

// The question page is one object: a new question replaces the question shown
// before. The texts are copied into it. It shows at most MAX_MENU_ITEMS buttons.
class QuestionPage : public InternalPage {
public:
    // dismissed: called when the page is closed without a button (closed by
    // code, replaced by a newer question)
    static QuestionPage *create(AppContext *appContext, const Value &message, const Value &buttons, void *userParam, void (*callback)(void *userParam, unsigned buttonIndex), void (*dismissed)(void *userParam) = nullptr);

    void pageFree();

    void updateInternalPage();
    WidgetCursor findWidgetInternalPage(int x, int y, bool clicked);

    static void executeAction();

private:
    AppContext *m_appContext;
    Value m_message;
    Value m_buttons;
    void *m_userParam;
    void (*m_callback)(void *userParam, unsigned buttonIndex);
    void (*m_dismissed)(void *userParam);

    RectangleWidget m_containerRectangleWidget;
    TextWidget m_messageTextWidget;
    TextWidget m_buttonTextWidgets[MAX_MENU_ITEMS];
    size_t m_numButtonTextWidgets;

    char m_messageBuffer[MAX_MENU_MESSAGE_LENGTH];
    char m_buttonTextBuffers[MAX_MENU_ITEMS][MAX_MENU_ITEM_LENGTH];

    // drawn again only after init or when another button is pressed
    bool m_dirty;
    WidgetCursor m_lastActiveWidget;

    void init(AppContext *appContext, const Value &message, const Value &buttons, void *userParam, void (*callback)(void *userParam, unsigned buttonIndex), void (*dismissed)(void *userParam));
    void dismiss();
};

} // namespace gui
} // namespace eez
