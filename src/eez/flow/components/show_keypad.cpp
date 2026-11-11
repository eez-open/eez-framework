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

    // the precision for the keypad units (e.g. uA for a value below 1 mA), also
    // evaluated before the keypad is opened: an error must not leave the
    // component waiting
    Value precisionValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_PRECISION, precisionValue, FlowError::Property("ShowKeypad", "Precision"))) {
        return;
    }

	// the ShowKeypad components waiting for the keypad they opened: a weak
	// reference, the flow state can be freed while the keypad is open (its flow
	// stopped), then OK and Cancel only close the keypad
	static ComponentRefStack<8> g_keypadRefs;

	g_keypadRefs.push(ComponentRef(flowState, componentIndex));

	startAsyncExecution(flowState, componentIndex);

	auto onOk = [](float value) {
		auto ref = g_keypadRefs.top();
		auto flowState = ref.getFlowState();
		if (!flowState) {
			g_keypadRefs.pop();
			getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
			return;
		}
		auto componentIndex = ref.componentIndex;

        Value precisionValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_PRECISION, precisionValue, FlowError::Property("ShowKeypad", "Precision"))) {
            return;
        }

        float precision = precisionValue.toFloat();

        Value unitValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_UNIT, unitValue, FlowError::Property("ShowKeypad", "Unit"))) {
            return;
        }

		Unit unit = getUnitFromName(unitValue.getString());

        value = roundPrec(value, precision) / getUnitFactor(unit);

		g_keypadRefs.pop();
		propagateValue(flowState, componentIndex, 0, Value(value, VALUE_TYPE_FLOAT));
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
        endAsyncExecution(flowState, componentIndex);
	};

	auto onCancel = []() {
		auto ref = g_keypadRefs.pop();
		auto flowState = ref.getFlowState();
		if (flowState) {
			propagateValue(flowState, ref.componentIndex, 1);
		}
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
		if (flowState) {
			endAsyncExecution(flowState, ref.componentIndex);
		}
	};

	Unit unit = getUnitFromName(unitValue.getString());

    initialValue.unit = unit;

	showKeypadHook(labelValue, initialValue, minValue, maxValue, unit, precisionValue.toFloat(), onOk, onCancel);
}

} // namespace flow
} // namespace eez

#endif // EEZ_OPTION_GUI
