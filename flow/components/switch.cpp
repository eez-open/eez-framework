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

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>

#include <eez/flow/components/switch.h>

namespace eez {
namespace flow {

void executeSwitchComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (SwitchActionComponent *)flowState->flow->components[componentIndex];

    for (uint32_t testIndex = 0; testIndex < component->tests.count; testIndex++) {
        auto test = component->tests[testIndex];

        char strMessage[256];
        snprintf(strMessage, sizeof(strMessage), "Failed to evaluate test condition no. %d in Switch", (int)(testIndex + 1));

        Value conditionValue;
        if (!evalExpression(flowState, componentIndex, test->conditionInstructions, conditionValue, strMessage)) {
            return;
        }

        int err;
        bool result = conditionValue.toBool(&err);
        if (err == 0) {
            if (result) {
                propagateValue(flowState, componentIndex, test->outputIndex);
                break;
            }
        } else {
            char strMessage[256];
            snprintf(strMessage, sizeof(strMessage), "Failed to convert Value no. %d to boolean in Switch\n", (int)(testIndex + 1));
            throwError(flowState, componentIndex, strMessage);
            return;
        }
    }

	propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
