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

#include <eez/core/os.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>
#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>
#include <eez/flow/hooks.h>

#if defined(EEZ_FOR_LVGL)

namespace eez {
namespace flow {

enum LVGL_ACTIONS {
    CHANGE_SCREEN
};

struct LVGLComponent : public Component {
	uint32_t action;
};

struct LVGLComponent_ChangeScreen : public LVGLComponent {
    int32_t screen;
    uint32_t fadeMode;
    uint32_t speed;
    uint32_t delay;
};

void executeLVGLComponent(FlowState *flowState, unsigned componentIndex) {
    auto componentGeneral = (LVGLComponent *)flowState->flow->components[componentIndex];
    if (componentGeneral->action == CHANGE_SCREEN) {
        auto component = (LVGLComponent_ChangeScreen *)componentGeneral;
        replacePageHook(component->screen, component->fadeMode, component->speed, component->delay);
    }
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
