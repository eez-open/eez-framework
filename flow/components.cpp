/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
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
#include <math.h>

#include <eez/core/os.h>

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/queue.h>
#include <eez/flow/hooks.h>

#if defined(__EMSCRIPTEN__)
#include <eez/flow/dashboard_api.h>
#endif

using namespace eez::gui;

namespace eez {
namespace flow {

void executeStartComponent(FlowState *flowState, unsigned componentIndex);
void executeEndComponent(FlowState *flowState, unsigned componentIndex);
void executeInputComponent(FlowState *flowState, unsigned componentIndex);
void executeOutputComponent(FlowState *flowState, unsigned componentIndex);
void executeWatchVariableComponent(FlowState *flowState, unsigned componentIndex);
void executeEvalExprComponent(FlowState *flowState, unsigned componentIndex);
void executeSetVariableComponent(FlowState *flowState, unsigned componentIndex);
void executeSwitchComponent(FlowState *flowState, unsigned componentIndex);
void executeCompareComponent(FlowState *flowState, unsigned componentIndex);
void executeIsTrueComponent(FlowState *flowState, unsigned componentIndex);
void executeConstantComponent(FlowState *flowState, unsigned componentIndex);
void executeLogComponent(FlowState *flowState, unsigned componentIndex);
void executeCallActionComponent(FlowState *flowState, unsigned componentIndex);
void executeDelayComponent(FlowState *flowState, unsigned componentIndex);
void executeErrorComponent(FlowState *flowState, unsigned componentIndex);
void executeCatchErrorComponent(FlowState *flowState, unsigned componentIndex);
void executeCounterComponent(FlowState *flowState, unsigned componentIndex);
void executeLoopComponent(FlowState *flowState, unsigned componentIndex);
void executeShowPageComponent(FlowState *flowState, unsigned componentIndex);
void executeShowMessageBoxComponent(FlowState *flowState, unsigned componentIndex);
void executeShowKeyboardComponent(FlowState *flowState, unsigned componentIndex);
void executeShowKeypadComponent(FlowState *flowState, unsigned componentIndex);
void executeSelectLanguageComponent(FlowState *flowState, unsigned componentIndex);
void executeSetPageDirectionComponent(FlowState *flowState, unsigned componentIndex);
void executeAnimateComponent(FlowState *flowState, unsigned componentIndex);
void executeNoopComponent(FlowState *flowState, unsigned componentIndex);
void executeOnEventComponent(FlowState *flowState, unsigned componentIndex);

void executeLayoutViewWidgetComponent(FlowState *flowState, unsigned componentIndex);
void executeRollerWidgetComponent(FlowState *flowState, unsigned componentIndex);

typedef void (*ExecuteComponentFunctionType)(FlowState *flowState, unsigned componentIndex);

static ExecuteComponentFunctionType g_executeComponentFunctions[] = {
	executeStartComponent,
	executeEndComponent,
	executeInputComponent,
	executeOutputComponent,
	executeWatchVariableComponent,
	executeEvalExprComponent,
	executeSetVariableComponent,
	executeSwitchComponent,
	executeCompareComponent,
	executeIsTrueComponent,
	executeConstantComponent,
	executeLogComponent,
	executeCallActionComponent,
	executeDelayComponent,
	executeErrorComponent,
	executeCatchErrorComponent,
	executeCounterComponent, // COMPONENT_TYPE_COUNTER_ACTION
	executeLoopComponent,
	executeShowPageComponent,
	nullptr, // COMPONENT_TYPE_SCPIACTION
	executeShowMessageBoxComponent,
	executeShowKeyboardComponent,
	executeShowKeypadComponent,
	executeNoopComponent, // COMPONENT_TYPE_NOOP_ACTION
	nullptr, // COMPONENT_TYPE_COMMENT_ACTION
    executeSelectLanguageComponent, // COMPONENT_TYPE_SELECT_LANGUAGE_ACTION
    executeSetPageDirectionComponent, // COMPONENT_TYPE_SET_PAGE_DIRECTION_ACTION
    executeAnimateComponent, // COMPONENT_TYPE_ANIMATE_ACTION
    executeOnEventComponent, // COMPONENT_TYPE_ON_EVENT_ACTION
};

void registerComponent(ComponentTypes componentType, ExecuteComponentFunctionType executeComponentFunction) {
	if (componentType >= defs_v3::COMPONENT_TYPE_START_ACTION) {
		g_executeComponentFunctions[componentType - defs_v3::COMPONENT_TYPE_START_ACTION] = executeComponentFunction;
	}
}

void executeComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = flowState->flow->components[componentIndex];

#if defined(__EMSCRIPTEN__)
	if (component->type >= defs_v3::FIRST_DASHBOARD_COMPONENT_TYPE) {
        if (executeDashboardComponentHook) {
            executeDashboardComponentHook(component->type, getFlowStateIndex(flowState), componentIndex);
            return;
        }
    } else
#endif // __EMSCRIPTEN__
    if (component->type >= defs_v3::COMPONENT_TYPE_START_ACTION) {
		auto executeComponentFunction = g_executeComponentFunctions[component->type - defs_v3::COMPONENT_TYPE_START_ACTION];
		if (executeComponentFunction != nullptr) {
			executeComponentFunction(flowState, componentIndex);
			return;
		}
	} else if (component->type < 1000) {
		if (component->type == defs_v3::COMPONENT_TYPE_LAYOUT_VIEW_WIDGET) {
            executeLayoutViewWidgetComponent(flowState, componentIndex);
        } else if (component->type == defs_v3::COMPONENT_TYPE_ROLLER_WIDGET) {
			executeRollerWidgetComponent(flowState, componentIndex);
		}
		return;
	}

	char errorMessage[100];
	snprintf(errorMessage, sizeof(errorMessage), "Unknown component at index = %d, type = %d\n", componentIndex, component->type);
	throwError(flowState, componentIndex, errorMessage);
}

} // namespace flow
} // namespace eez
