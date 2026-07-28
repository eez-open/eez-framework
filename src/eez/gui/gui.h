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

#include <eez/conf-internal.h>

#ifndef MAX_KEYPAD_TEXT_LENGTH
#define MAX_KEYPAD_TEXT_LENGTH 128
#endif

#ifndef MAX_KEYPAD_LABEL_LENGTH
#define MAX_KEYPAD_LABEL_LENGTH 64
#endif

#include <eez/core/assets.h>
#include <eez/gui/display.h>
#include <eez/gui/widget.h>

enum {
    FIRST_INTERNAL_PAGE_ID = 32000,
    INTERNAL_PAGE_ID_SELECT_FROM_ENUM,
    INTERNAL_PAGE_ID_TOAST_MESSAGE,
    INTERNAL_PAGE_ID_MENU_WITH_BUTTONS,
    INTERNAL_PAGE_ID_QUESTION
};

enum InternalActionsEnum {
    FIRST_INTERNAL_ACTION_ID = 32000,
    ACTION_ID_INTERNAL_SELECT_ENUM_ITEM,
    ACTION_ID_INTERNAL_DIALOG_CLOSE,
    ACTION_ID_INTERNAL_TOAST_ACTION,
    ACTION_ID_INTERNAL_TOAST_ACTION_WITHOUT_PARAM,
    ACTION_ID_INTERNAL_MENU_WITH_BUTTONS,
    ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON
};

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

extern WidgetCursor g_activeWidget;
extern bool g_isBlinkTime;
extern uint8_t g_selectedThemeIndex;

////////////////////////////////////////////////////////////////////////////////

void guiInit();
void guiTick();

WidgetCursor &getFoundWidgetAtDown();
void setFoundWidgetAtDown(WidgetCursor &widgetCursor);
void clearFoundWidgetAtDown();
bool isFocusWidget(const WidgetCursor &widgetCursor);
void refreshScreen();
inline bool isPageInternal(int pageId) { return pageId > FIRST_INTERNAL_PAGE_ID; }

int getWidgetAction(const WidgetCursor &widgetCursor);

void executeAction(const WidgetCursor &widgetCursor, int actionId, void *param = nullptr);
void executeInternalAction(int actionId);

AppContext *getAppContextFromId(int16_t id);

extern const char *g_discardMessage;

void setOverrideStyleRule(int16_t fromStyle, int16_t toStyle);
int overrideStyle(const WidgetCursor &widgetCursor, int styleId);

#if !EEZ_OPTION_THREADS
extern bool g_updateDisplay;
#endif

extern const uint8_t *eezAssets;
extern const uint32_t eezAssetsSize;

static const int EEZ_DATA_ID_NONE = 0;
extern const int EEZ_DATA_ID_KEYPAD_EDIT_UNIT;
extern const int EEZ_DATA_ID_KEYPAD_TEXT;
extern const int EEZ_DATA_ID_ALERT_MESSAGE;

static const int EEZ_ACTION_ID_NONE = 0;
extern const int EEZ_ACTION_ID_DRAG_OVERLAY;
extern const int EEZ_ACTION_ID_EDIT;
extern const int EEZ_ACTION_ID_SCROLL;

static const int EEZ_PAGE_ID_NONE = 0;
extern const int EEZ_PAGE_ID_ASYNC_OPERATION_IN_PROGRESS;
extern const int EEZ_PAGE_ID_NUMERIC_KEYPAD;

static const int EEZ_STYLE_ID_NONE = 0;
extern const int EEZ_STYLE_ID_DEFAULT;
extern const int EEZ_STYLE_ID_INFO_ALERT;
extern const int EEZ_STYLE_ID_INFO_ALERT_BUTTON;
extern const int EEZ_STYLE_ID_ERROR_ALERT;
extern const int EEZ_STYLE_ID_ERROR_ALERT_BUTTON;
extern const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER_S;
extern const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER;
extern const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM_S;
extern const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM;
extern const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM_S;
extern const int EEZ_STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM;
extern const int EEZ_STYLE_ID_MENU_WITH_BUTTONS_CONTAINER;
extern const int EEZ_STYLE_ID_MENU_WITH_BUTTONS_MESSAGE;
extern const int EEZ_STYLE_ID_MENU_WITH_BUTTONS_BUTTON;

extern const int EEZ_THEME_ID_LEGACY;
extern const int EEZ_THEME_ID_DEFAULT;
extern const int EEZ_COLOR_ID_BOOKMARK;
extern const int EEZ_COLOR_ID_BACKDROP;
extern const int EEZ_FONT_ID_SHADOW;

} // namespace gui
} // namespace eez

#include <eez/gui/app_context.h>
#include <eez/gui/update.h>
#include <eez/gui/overlay.h>
#include <eez/gui/font.h>
#include <eez/gui/draw.h>
#include <eez/gui/touch.h>
#include <eez/gui/page.h>
#include <eez/gui/hooks.h>

#define DATA_OPERATION_FUNCTION(id, operation, widgetCursor, value) (id >= 0 ? g_dataOperationsFunctions[id](operation, widgetCursor, value) : g_hooks.externalData(id, operation, widgetCursor, value))
