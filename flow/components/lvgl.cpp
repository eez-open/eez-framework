/*
 * EEZ Framework
 * Copyright (C) 2022-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <eez/core/os.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>
#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>
#include <eez/flow/hooks.h>

#include <eez/flow/components/lvgl.h>

#if defined(EEZ_FOR_LVGL)

namespace eez {
namespace flow {

////////////////////////////////////////////////////////////////////////////////

void anim_callback_set_x(lv_anim_t * a, int32_t v) { lv_obj_set_x((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_x(lv_anim_t * a) { return lv_obj_get_x_aligned((lv_obj_t *)a->user_data); }

void anim_callback_set_y(lv_anim_t * a, int32_t v) { lv_obj_set_y((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_y(lv_anim_t * a) { return lv_obj_get_y_aligned((lv_obj_t *)a->user_data); }

void anim_callback_set_width(lv_anim_t * a, int32_t v) { lv_obj_set_width((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_width(lv_anim_t * a) { return lv_obj_get_width((lv_obj_t *)a->user_data); }

void anim_callback_set_height(lv_anim_t * a, int32_t v) { lv_obj_set_height((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_height(lv_anim_t * a) { return lv_obj_get_height((lv_obj_t *)a->user_data); }

void anim_callback_set_opacity(lv_anim_t * a, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)a->user_data, v, 0); }
int32_t anim_callback_get_opacity(lv_anim_t * a) { return lv_obj_get_style_opa((lv_obj_t *)a->user_data, 0); }

void anim_callback_set_image_zoom(lv_anim_t * a, int32_t v) { lv_img_set_zoom((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_image_zoom(lv_anim_t * a) { return lv_img_get_zoom((lv_obj_t *)a->user_data); }

void anim_callback_set_image_angle(lv_anim_t * a, int32_t v) { lv_img_set_angle((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_image_angle(lv_anim_t * a) { return lv_img_get_angle((lv_obj_t *)a->user_data); }

void (*anim_set_callbacks[])(lv_anim_t *a, int32_t v) = {
    anim_callback_set_x,
    anim_callback_set_y,
    anim_callback_set_width,
    anim_callback_set_height,
    anim_callback_set_opacity,
    anim_callback_set_image_zoom,
    anim_callback_set_image_angle
};

int32_t (*anim_get_callbacks[])(lv_anim_t *a) = {
    anim_callback_get_x,
    anim_callback_get_y,
    anim_callback_get_width,
    anim_callback_get_height,
    anim_callback_get_opacity,
    anim_callback_get_image_zoom,
    anim_callback_get_image_angle
};

int32_t (*anim_path_callbacks[])(const lv_anim_t *a) = {
    lv_anim_path_linear,
    lv_anim_path_ease_in,
    lv_anim_path_ease_out,
    lv_anim_path_ease_in_out,
    lv_anim_path_overshoot,
    lv_anim_path_bounce
};

////////////////////////////////////////////////////////////////////////////////

enum PropertyCode {
    NONE,

    BAR_VALUE,

    BASIC_X,
    BASIC_Y,
    BASIC_WIDTH,
    BASIC_HEIGHT,

    DROPDOWN_SELECTED,

    IMAGE_IMAGE,
    IMAGE_ANGLE,
    IMAGE_ZOOM,

    LABEL_TEXT,

    ROLLER_SELECTED,

    SLIDER_VALUE
};

////////////////////////////////////////////////////////////////////////////////

void executeLVGLComponent(FlowState *flowState, unsigned componentIndex) {
    auto componentGeneral = (LVGLComponent *)flowState->flow->components[componentIndex];
    if (componentGeneral->action == CHANGE_SCREEN) {
        auto componentSpecific = (LVGLComponent_ChangeScreen *)componentGeneral;
        replacePageHook(componentSpecific->screen, componentSpecific->fadeMode, componentSpecific->speed, componentSpecific->delay);
    } else if (componentGeneral->action == PLAY_ANIMATION) {
        auto componentSpecific = (LVGLComponent_PlayAnimation *)componentGeneral;

        auto target = getLvglObjectFromIndexHook(componentSpecific->target);
        auto delay = componentSpecific->delay;

        for (uint32_t itemIndex = 0; itemIndex < componentSpecific->items.count; itemIndex++) {
            auto item = componentSpecific->items[itemIndex];

            lv_anim_t anim;

            lv_anim_init(&anim);
            lv_anim_set_time(&anim, item->time);
            lv_anim_set_user_data(&anim, target);
            lv_anim_set_custom_exec_cb(&anim, anim_set_callbacks[item->property]);
            lv_anim_set_values(&anim, item->start, item->end);
            lv_anim_set_path_cb(&anim, anim_path_callbacks[item->path]);
            lv_anim_set_delay(&anim, delay + item->delay);
            lv_anim_set_early_apply(&anim, item->flags & ANIMATION_ITEM_FLAG_INSTANT ? true : false);
            if (item->flags & ANIMATION_ITEM_FLAG_RELATIVE) {
                lv_anim_set_get_value_cb(&anim, anim_get_callbacks[item->property]);
            }

            lv_anim_start(&anim);
        }
    } else if (componentGeneral->action == SET_PROPERTY) {
        auto componentSpecific = (LVGLComponent_SetProperty *)componentGeneral;

		Value value;
		if (!evalExpression(flowState, componentIndex, componentSpecific->value, value, "Failed to evaluate Value in LVGL Set Property")) {
			return;
		}

        auto target = getLvglObjectFromIndexHook(componentSpecific->target);

        if (componentSpecific->property == IMAGE_IMAGE || componentSpecific->property == LABEL_TEXT) {
            const char *strValue = value.toString(0xe42b3ca2).getString();
            if (componentSpecific->property == IMAGE_IMAGE) {
                const void *src = getLvglImageByNameHook(strValue);
                if (src) {
                    lv_img_set_src(target, src);
                } else {
                    char errorMessage[256];
                    snprintf(errorMessage, sizeof(errorMessage), "Image \"%s\" not found in LVGL Set Property", strValue);
                    throwError(flowState, componentIndex, errorMessage);
                }
            } else {
                lv_label_set_text(target, strValue);
            }
        } else {
            int err;
            int32_t intValue = value.toInt32(&err);
            if (err) {
                throwError(flowState, componentIndex, "Failed to convert value to integer in LVGL Set Property");
                return;
            }

            if (componentSpecific->property == BAR_VALUE) {
                lv_bar_set_value(target, intValue, componentSpecific->animated ? LV_ANIM_ON : LV_ANIM_OFF);
            } else if (componentSpecific->property == BASIC_X) {
                lv_obj_set_x(target, intValue);
            } else if (componentSpecific->property == BASIC_Y) {
                lv_obj_set_y(target, intValue);
            } else if (componentSpecific->property == BASIC_WIDTH) {
                lv_obj_set_width(target, intValue);
            } else if (componentSpecific->property == BASIC_HEIGHT) {
                lv_obj_set_height(target, intValue);
            } else if (componentSpecific->property == DROPDOWN_SELECTED) {
                lv_dropdown_set_selected(target, intValue);
            } else if (componentSpecific->property == IMAGE_ANGLE) {
                lv_img_set_angle(target, intValue);
            } else if (componentSpecific->property == IMAGE_ZOOM) {
                lv_img_set_zoom(target, intValue);
            } else if (componentSpecific->property == ROLLER_SELECTED) {
                lv_roller_set_selected(target, intValue, componentSpecific->animated ? LV_ANIM_ON : LV_ANIM_OFF);
            } else if (componentSpecific->property == SLIDER_VALUE) {
                lv_slider_set_value(target, intValue, componentSpecific->animated ? LV_ANIM_ON : LV_ANIM_OFF);
            }
        }
    }

    propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez


#else

namespace eez {
namespace flow {

void executeLVGLComponent(FlowState *flowState, unsigned componentIndex) {
    throwError(flowState, componentIndex, "Not implemented");
}

} // namespace flow
} // namespace eez

#endif
