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

#if defined(EEZ_FOR_LVGL)

#include <stdio.h>

#include <eez/core/os.h>
#include <eez/core/action.h>
#include <eez/core/util.h>

#include <eez/flow/flow.h>
#include <eez/flow/expression.h>
#include <eez/flow/hooks.h>
#include <eez/flow/debugger.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/components/lvgl_user_widget.h>
#include <eez/flow/lvgl_api.h>

static void replacePageHook(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay);

extern "C" void create_screens();
extern "C" void tick_screen(int screen_index);

static const char **g_screenNames;
static size_t g_numScreens;

static lv_obj_t **g_objects;
static const char **g_objectNames;
static size_t g_numObjects;

static lv_group_t **g_groups;
static const char **g_groupNames;
static size_t g_numGroups;

static const char **g_styleNames;
static size_t g_numStyles;

static const ext_img_desc_t *g_images;
static size_t g_numImages;

static ActionExecFunc *g_actions;

int16_t g_currentScreen = -1;

static const char **g_themeNames;
static size_t g_numThemes;
static void (*g_changeColorTheme)(uint32_t themeIndex);
static uint32_t g_selectedThemeIndex;

static lv_obj_t *getLvglObjectFromIndex(int32_t index) {
    if (index >= 0 && index < g_numObjects) {
        return g_objects[index];
    }
    return 0;
}

static lv_group_t *getLvglGroupFromIndex(int32_t index) {
    if (index >= 0 && index < g_numGroups) {
        return g_groups[index];
    }
    return 0;
}

static int32_t getLvglScreenByName(const char *name) {
    for (size_t i = 0; i < g_numScreens; i++) {
        if (strcmp(g_screenNames[i], name) == 0) {
            return i + 1;
        }
    }
    return -1;
}

static int32_t getLvglObjectByName(const char *name) {
    for (size_t i = 0; i < g_numObjects; i++) {
        if (strcmp(g_objectNames[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int32_t getLvglGroupByName(const char *name) {
    for (size_t i = 0; i < g_numGroups; i++) {
        if (strcmp(g_groupNames[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int32_t getLvglStyleByName(const char *name) {
    for (size_t i = 0; i < g_numStyles; i++) {
        if (strcmp(g_styleNames[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static const void *getLvglImageByName(const char *name) {
    for (size_t i = 0; i < g_numImages; i++) {
        if (strcmp(g_images[i].name, name) == 0) {
            return g_images[i].img_dsc;
        }
    }
    return 0;
}

static void executeLvglAction(int actionIndex) {
    g_actions[actionIndex](0);
}

void eez_flow_init_themes(const char **themeNames, size_t numThemes, void (*changeColorTheme)(uint32_t themeIndex)) {
    g_themeNames = themeNames;
    g_numThemes = numThemes;
    g_changeColorTheme = changeColorTheme;
}

static void lvglSetColorTheme(const char *themeName) {
    for (uint32_t i = 0; i < g_numThemes; i++) {
        if (strcmp(themeName, g_themeNames[i]) == 0) {
            g_selectedThemeIndex = i;
            if (g_changeColorTheme) {
                g_changeColorTheme(g_selectedThemeIndex);
            }
            return;
        }
    }
}

#if !defined(EEZ_LVGL_SCREEN_STACK_SIZE)
#define EEZ_LVGL_SCREEN_STACK_SIZE 10
#endif

int16_t g_screenStack[EEZ_LVGL_SCREEN_STACK_SIZE];
unsigned g_screenStackPosition = 0;

extern "C" int16_t eez_flow_get_current_screen() {
    return g_currentScreen + 1;
}

extern "C" void eez_flow_set_screen(int16_t screenId, lv_scr_load_anim_t animType, uint32_t speed, uint32_t delay) {
    g_screenStackPosition = 0;
    eez::flow::replacePageHook(screenId, animType, speed, delay);
}

extern "C" void eez_flow_push_screen(int16_t screenId, lv_scr_load_anim_t animType, uint32_t speed, uint32_t delay) {
    // remove the oldest screen from the stack if the stack is full
    if (g_screenStackPosition == EEZ_LVGL_SCREEN_STACK_SIZE) {
        for (unsigned i = 1; i < EEZ_LVGL_SCREEN_STACK_SIZE; i++) {
            g_screenStack[i - 1] = g_screenStack[i];
        }
        g_screenStackPosition--;
    }

    g_screenStack[g_screenStackPosition++] = g_currentScreen + 1;

    eez::flow::replacePageHook(screenId, animType, speed, delay);
}

extern "C" void eez_flow_pop_screen(lv_scr_load_anim_t animType, uint32_t speed, uint32_t delay) {
    if (g_screenStackPosition > 0) {
        g_screenStackPosition--;
        eez::flow::replacePageHook(g_screenStack[g_screenStackPosition], animType, speed, delay);
    }
}

extern "C" void eez_flow_init(const uint8_t *assets, uint32_t assetsSize, lv_obj_t **objects, size_t numObjects, const ext_img_desc_t *images, size_t numImages, ActionExecFunc *actions) {
    g_objects = objects;
    g_numObjects = numObjects;
    g_images = images;
    g_numImages = numImages;
    g_actions = actions;

    eez::initAssetsMemory();
    eez::loadMainAssets(assets, assetsSize);
    eez::initOtherMemory();
    eez::initAllocHeap(eez::ALLOC_BUFFER, eez::ALLOC_BUFFER_SIZE);

    eez::flow::replacePageHook = replacePageHook;
    eez::flow::getLvglObjectFromIndexHook = getLvglObjectFromIndex;
    eez::flow::getLvglScreenByNameHook = getLvglScreenByName;
    eez::flow::getLvglObjectByNameHook = getLvglObjectByName;
    eez::flow::getLvglGroupByNameHook = getLvglGroupByName;
    eez::flow::getLvglStyleByNameHook = getLvglStyleByName;
    eez::flow::getLvglImageByNameHook = getLvglImageByName;
    eez::flow::executeLvglActionHook = executeLvglAction;
    eez::flow::getLvglGroupFromIndexHook = getLvglGroupFromIndex;
    eez::flow::lvglSetColorThemeHook = lvglSetColorTheme;

    eez::flow::start(eez::g_mainAssets);

    create_screens();
    replacePageHook(1, 0, 0, 0);
}

extern "C" void eez_flow_init_styles(
    void (*add_style)(lv_obj_t *obj, int32_t styleIndex),
    void (*remove_style)(lv_obj_t *obj, int32_t styleIndex)
) {
    eez::flow::lvglObjAddStyleHook = add_style;
    eez::flow::lvglObjRemoveStyleHook = remove_style;
}

void eez_flow_init_groups(lv_group_t **groups, size_t numGroups) {
    g_groups = groups;
    g_numGroups = numGroups;
}

void eez_flow_init_screen_names(const char **screenNames, size_t numScreens) {
    g_screenNames = screenNames;
    g_numScreens = numScreens;
}

void eez_flow_init_object_names(const char **objectNames, size_t numObjects) {
    g_objectNames = objectNames;
}

void eez_flow_init_group_names(const char **groupNames, size_t numGroups) {
    g_groupNames = groupNames;
}

void eez_flow_init_style_names(const char **styleNames, size_t numStyles) {
    g_styleNames = styleNames;
    g_numStyles = numStyles;
}


extern "C" void eez_flow_tick() {
    eez::flow::tick();
}

extern "C" bool eez_flow_is_stopped() {
    return eez::flow::isFlowStopped();
}

namespace eez {
ActionExecFunc g_actionExecFunctions[] = { 0 };
}

void replacePageHook(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay) {
    eez::flow::onPageChanged(g_currentScreen + 1, pageId);
    g_currentScreen = pageId - 1;
    lv_scr_load_anim(getLvglObjectFromIndex(g_currentScreen), (lv_scr_load_anim_t)animType, speed, delay, false);
}

extern "C" void flowOnPageLoaded(unsigned pageIndex) {
    eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);
}

extern "C" void flowPropagateValue(void *flowState, unsigned componentIndex, unsigned outputIndex) {
    eez::flow::propagateValue((eez::flow::FlowState *)flowState, componentIndex, outputIndex);
}

extern "C" void flowPropagateValueInt32(void *flowState, unsigned componentIndex, unsigned outputIndex, int32_t value) {
    eez::flow::propagateValue((eez::flow::FlowState *)flowState, componentIndex, outputIndex, eez::Value((int)value, eez::VALUE_TYPE_INT32));
}

extern "C" void flowPropagateValueUint32(void *flowState, unsigned componentIndex, unsigned outputIndex, uint32_t value) {
    eez::flow::propagateValue((eez::flow::FlowState *)flowState, componentIndex, outputIndex, eez::Value(value, eez::VALUE_TYPE_UINT32));
}

extern "C" void flowPropagateValueLVGLEvent(void *flowState, unsigned componentIndex, unsigned outputIndex, lv_event_t *event) {
    lv_event_code_t event_code = lv_event_get_code(event);

    uint32_t code = (uint32_t)event_code;
    void *currentTarget = (void *)lv_event_get_current_target(event);
    void *target = (void *)lv_event_get_target(event);

    void *userDataPointer = lv_event_get_user_data(event);
    int32_t userData = *((int32_t*)(&userDataPointer));

    uint32_t key = 0;
    if (event_code == LV_EVENT_KEY || (event_code == LV_EVENT_VALUE_CHANGED &&
#if LVGL_VERSION_MAJOR >= 9
        lv_obj_check_type((lv_obj_t*)target, &lv_buttonmatrix_class)
#else
        lv_obj_check_type((lv_obj_t*)target, &lv_btnmatrix_class)
#endif
    )) {
        uint32_t *param = (uint32_t *)lv_event_get_param(event);
        key = param ? *param : 0;
    }

    int32_t gestureDir = (int32_t)LV_DIR_NONE;
    if (event_code == LV_EVENT_GESTURE) {
#if LVGL_VERSION_MAJOR >= 9
        lv_indev_wait_release(lv_indev_active());
#else
        lv_indev_wait_release(lv_indev_get_act());
#endif

        gestureDir = (int32_t)lv_indev_get_gesture_dir(
#if LVGL_VERSION_MAJOR >= 9
            lv_indev_active()
#else
            lv_indev_get_act()
#endif
        );
    }

    int32_t rotaryDiff = 0;
#if LVGL_VERSION_MAJOR >= 9
    if (event_code == LV_EVENT_ROTARY) {
        rotaryDiff = lv_event_get_rotary_diff(event);
    }
#endif

    eez::flow::propagateValue(
        (eez::flow::FlowState *)flowState, componentIndex, outputIndex,
        eez::Value::makeLVGLEventRef(
            code, currentTarget, target, userData, key, gestureDir, rotaryDiff, 0xe7f23624
        )
    );
}

#ifndef EEZ_LVGL_TEMP_STRING_BUFFER_SIZE
#define EEZ_LVGL_TEMP_STRING_BUFFER_SIZE 1024
#endif

static char textValue[EEZ_LVGL_TEMP_STRING_BUFFER_SIZE];

extern "C" const char *evalTextProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage) {
    eez::Value value;
    if (!eez::flow::evalProperty((eez::flow::FlowState *)flowState, componentIndex, propertyIndex, value, eez::flow::FlowError::Plain(errorMessage))) {
        return "";
    }
    value.toText(textValue, sizeof(textValue));
    return textValue;
}

extern "C" int32_t evalIntegerProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage) {
    eez::Value value;
    if (!eez::flow::evalProperty((eez::flow::FlowState *)flowState, componentIndex, propertyIndex, value, eez::flow::FlowError::Plain(errorMessage))) {
        return 0;
    }
    int err;
    int32_t intValue = value.toInt32(&err);
    if (err) {
        eez::flow::throwError((eez::flow::FlowState *)flowState, componentIndex, errorMessage);
        return 0;
    }
    return intValue;
}

extern "C" bool evalBooleanProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage) {
    eez::Value value;
    if (!eez::flow::evalProperty((eez::flow::FlowState *)flowState, componentIndex, propertyIndex, value, eez::flow::FlowError::Plain(errorMessage))) {
        return 0;
    }
    int err;
    bool booleanValue = value.toBool(&err);
    if (err) {
        eez::flow::throwError((eez::flow::FlowState *)flowState, componentIndex, errorMessage);
        return 0;
    }
    return booleanValue;
}

const char *evalStringArrayPropertyAndJoin(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage, const char *separator) {
    eez::Value value;
    if (!eez::flow::evalProperty((eez::flow::FlowState *)flowState, componentIndex, propertyIndex, value, eez::flow::FlowError::Plain(errorMessage))) {
        return "";
    }

    if (value.isArray()) {
        auto array = value.getArray();

        textValue[0] = 0;
        size_t textPosition = 0;

        size_t separatorLength = strlen(separator);

        for (uint32_t elementIndex = 0; elementIndex < array->arraySize; elementIndex++) {
            if (elementIndex > 0) {
                eez::stringAppendString(textValue + textPosition, sizeof(textValue) - textPosition, separator);
                textPosition += separatorLength;
            }
            array->values[elementIndex].toText(textValue + textPosition, sizeof(textValue) - textPosition);
            textPosition = strlen(textValue);
        }

        return textValue;
    } else if (value.isString()) {
        value.toText(textValue, sizeof(textValue));
        return textValue;
    }

    return "";
}

extern "C" void assignStringProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *value, const char *errorMessage) {
    auto component = ((eez::flow::FlowState *)flowState)->flow->components[componentIndex];

    eez::Value dstValue;
    if (!eez::flow::evalAssignableExpression((eez::flow::FlowState *)flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, dstValue, eez::flow::FlowError::Plain(errorMessage))) {
        return;
    }

    eez::Value srcValue = eez::Value::makeStringRef(value, -1, 0x3eefcf0d);

    eez::flow::assignValue((eez::flow::FlowState *)flowState, componentIndex, dstValue, srcValue);
}

extern "C" void assignIntegerProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, int32_t value, const char *errorMessage) {
    auto component = ((eez::flow::FlowState *)flowState)->flow->components[componentIndex];

    eez::Value dstValue;
    if (!eez::flow::evalAssignableExpression((eez::flow::FlowState *)flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, dstValue, eez::flow::FlowError::Plain(errorMessage))) {
        return;
    }

    eez::Value srcValue((int)value, eez::VALUE_TYPE_INT32);

    eez::flow::assignValue((eez::flow::FlowState *)flowState, componentIndex, dstValue, srcValue);
}

extern "C" void assignBooleanProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, bool value, const char *errorMessage) {
    auto component = ((eez::flow::FlowState *)flowState)->flow->components[componentIndex];

    eez::Value dstValue;
    if (!eez::flow::evalAssignableExpression((eez::flow::FlowState *)flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, dstValue, eez::flow::FlowError::Plain(errorMessage))) {
        return;
    }

    eez::Value srcValue(value, eez::VALUE_TYPE_BOOLEAN);

    eez::flow::assignValue((eez::flow::FlowState *)flowState, componentIndex, dstValue, srcValue);
}

extern "C" float getTimelinePosition(void *flowState) {
    return ((eez::flow::FlowState *)flowState)->timelinePosition;
}

void *getFlowState(void *flowState, unsigned userWidgetComponentIndexOrPageIndex) {
    if (!flowState) {
        return eez::flow::getPageFlowState(eez::g_mainAssets, userWidgetComponentIndexOrPageIndex);
    }
    auto executionState = (eez::flow::LVGLUserWidgetExecutionState *)((eez::flow::FlowState *)flowState)->componenentExecutionStates[userWidgetComponentIndexOrPageIndex];
    if (!executionState) {
        executionState = eez::flow::createUserWidgetFlowState((eez::flow::FlowState *)flowState, userWidgetComponentIndexOrPageIndex);
    }
    return executionState->flowState;
}

bool compareRollerOptions(lv_roller_t *roller, const char *new_val, const char *cur_val, lv_roller_mode_t mode) {
    if (mode == LV_ROLLER_MODE_NORMAL) {
        return strcmp(new_val, cur_val) != 0;
    }

    auto n = strlen(new_val);

#if LVGL_VERSION_MAJOR >= 9
    size_t numPages = roller->inf_page_cnt;
#else
    size_t numPages = LV_ROLLER_INF_PAGES;
#endif

    for (size_t i = 0; i < numPages * (n + 1); i += n + 1) {
        if (strncmp(new_val, cur_val + i, n) != 0) {
            return true;
        }
    }

    return false;
}

#endif // EEZ_FOR_LVGL
