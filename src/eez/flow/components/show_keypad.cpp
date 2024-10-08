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

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/operations.h>
#include <eez/flow/private.h>
#include <eez/flow/hooks.h>

#include <eez/gui/gui.h>

namespace eez {
namespace flow {

struct ShowKeyboardActionComponent : public Component {
	uint8_t password;
};

void executeShowKeypadComponent(FlowState *flowState, unsigned componentIndex) {
    Value labelValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_LABEL, labelValue, FlowError::Property("ShowKeypad", "Label"))) {
        return;
    }

    Value initialValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_INITAL_VALUE, initialValue, FlowError::Property("ShowKeypad", "Initial value"))) {
        return;
    }

    Value minValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_MIN, minValue, FlowError::Property("ShowKeypad", "Min"))) {
        return;
    }

    Value maxValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_MAX, maxValue, FlowError::Property("ShowKeypad", "Max"))) {
        return;
    }

    Value unitValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_UNIT, unitValue, FlowError::Property("ShowKeypad", "Unit"))) {
        return;
    }

	static FlowState *g_showKeyboardFlowState;
	static unsigned g_showKeyboardComponentIndex;

	g_showKeyboardFlowState = flowState;
	g_showKeyboardComponentIndex = componentIndex;

	startAsyncExecution(flowState, componentIndex);

	auto onOk = [](float value) {
        Value precisionValue;
        if (!evalProperty(g_showKeyboardFlowState, g_showKeyboardComponentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_PRECISION, precisionValue, FlowError::Property("ShowKeypad", "Precision"))) {
            return;
        }

        float precision = precisionValue.toFloat();

        Value unitValue;
        if (!evalProperty(g_showKeyboardFlowState, g_showKeyboardComponentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_UNIT, unitValue, FlowError::Property("ShowKeypad", "Unit"))) {
            return;
        }

		Unit unit = getUnitFromName(unitValue.getString());

        value = roundPrec(value, precision) / getUnitFactor(unit);

		propagateValue(g_showKeyboardFlowState, g_showKeyboardComponentIndex, 0, Value(value, VALUE_TYPE_FLOAT));
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
        endAsyncExecution(g_showKeyboardFlowState, g_showKeyboardComponentIndex);
	};

	auto onCancel = []() {
		propagateValue(g_showKeyboardFlowState, g_showKeyboardComponentIndex, 1);
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
		endAsyncExecution(g_showKeyboardFlowState, g_showKeyboardComponentIndex);
	};


	Unit unit = getUnitFromName(unitValue.getString());

    initialValue.unit = unit;

	showKeypadHook(labelValue, initialValue, minValue, maxValue, unit, onOk, onCancel);
}

} // namespace flow
} // namespace eez

#endif // EEZ_OPTION_GUI
