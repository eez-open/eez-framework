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
#include <stdio.h>

#include <eez/core/util.h>
#include <eez/core/sound.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/roller.h>

#include <eez/flow/flow.h>
#include <eez/flow/expression.h>
#include <eez/flow/components/roller_widget.h>

static const float FRICTION = 0.1f;
static const float BOUNCE_FORCE = 0.1f;

namespace eez {
namespace gui {

// The flow variable (local or global variable, also through the properties of
// user widgets) the data of the widget is bound to, nullptr if the data is
// not a flow variable (e.g. a native variable or a module field). type is the
// declared type of the variable, as for assignValue.
static Value *getFlowVariable(const WidgetCursor &widgetCursor, uint32_t &type) {
    using namespace flow;

    auto flowState = widgetCursor.flowState;
    int16_t dataId = widgetCursor.widget->data;
    if (dataId >= 0 || !flowState || isFlowStopped()) {
        return nullptr;
    }

    auto flowDataId = -dataId - 1;
    if (flowDataId >= (int)flowState->flow->widgetDataItems.count) {
        return nullptr;
    }
    auto widgetDataItem = flowState->flow->widgetDataItems[flowDataId];
    if (!widgetDataItem || widgetDataItem->componentIndex == -1 || widgetDataItem->propertyValueIndex == -1) {
        return nullptr;
    }

    Value dstValue;
    if (!evalAssignableProperty(flowState, widgetDataItem->componentIndex, widgetDataItem->propertyValueIndex, dstValue, FlowError::Plain("Roller data is not assignable"), nullptr, widgetCursor.iterators)) {
        return nullptr;
    }

    // a property of a user widget refers to the assignable expression of the
    // parent, at most a few levels deep
    for (int level = 0; level < 16 && dstValue.getType() == VALUE_TYPE_VALUE_PTR; level++) {
        type = dstValue.dstValueType;

        Value *pValue = dstValue.pValueValue;
        while (pValue->getType() == VALUE_TYPE_VALUE_PTR) {
            pValue = pValue->pValueValue;
        }

        if (pValue->getType() != VALUE_TYPE_PROPERTY_REF) {
            // an array element, a JSON member, ... are set through SET
            return pValue->isIndirectValueType() ? nullptr : pValue;
        }

        auto propertyRef = pValue->getPropertyRef();
        if (!evalAssignableProperty(propertyRef->flowState, propertyRef->componentIndex, propertyRef->propertyIndex, dstValue, FlowError::Plain("Roller data is not assignable"))) {
            return nullptr;
        }
    }

    return nullptr;
}

bool RollerWidgetState::updateState() {
    // also while hidden: a running roller (and the clear from the flow) sets the data
    WIDGET_STATE_START_EVEN_IF_HIDDEN(RollerWidget);

	const Style* selectedValueStyle = getStyle(widget->selectedValueStyle);
	font::Font fontSelectedValue = styleGetFont(selectedValueStyle);
	if (!fontSelectedValue) {
		return true;
	}

	textHeight = selectedValueStyle->paddingTop + fontSelectedValue.getHeight() + selectedValueStyle->paddingBottom;
	if (textHeight <= 0) {
		return true;
	}

    minValue = get(widgetCursor, widget->min).toInt32();
    maxValue = get(widgetCursor, widget->max).toInt32();

    Value dataValue = get(widgetCursor, widget->data);

	if (widgetCursor.flowState) {
		auto executionState = (flow::RollerWidgetComponenentExecutionState *)widgetCursor.flowState->componenentExecutionStates[widget->componentIndex];
		if (executionState && executionState->clear) {
			executionState->clear = false;
			isRunning = true;
			velocity = 0;
			applyForce(-1.5f * position * FRICTION);
		}
	}

    float oldPosition = position;

    if (isRunning) {
        updatePosition();

		// in 64 bits: the range can be wider than int
		int64_t newDataValue = (int64_t)minValue + llroundf(-position / textHeight);
		if (newDataValue < minValue) {
			newDataValue = minValue;
		} else if (newDataValue > maxValue) {
			newDataValue = maxValue;
		}

		if (newDataValue != dataValue.toInt32()) {
			dataValue = Value((int)newDataValue, VALUE_TYPE_INT32);
			set(widgetCursor, widget->data, dataValue);
			sound::playClick();
		}

        if (!isMoving()) {
            isRunning = false;
        }
    } else {
        position = -(float)((int64_t)dataValue.toInt32() - minValue) * textHeight;
    }

    if (oldPosition != position) {
        hasPreviousState = false;
    }

	WIDGET_STATE(data, dataValue);

    WIDGET_STATE_END()
}

void RollerWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const RollerWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);
    const Style* selectedValueStyle = getStyle(widget->selectedValueStyle);
    const Style* unselectedValueStyle = getStyle(widget->unselectedValueStyle);

    font::Font fontSelectedValue = styleGetFont(selectedValueStyle);
	if (!fontSelectedValue) {
		return;
	}

	font::Font fontUnselectedValue = styleGetFont(unselectedValueStyle);
	if (!fontUnselectedValue || textHeight <= 0) {
		return;
	}

	drawRectangle(
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        style
    );

	auto x = widgetCursor.x;

	int rectHeight = selectedValueStyle->borderSizeTop + textHeight + selectedValueStyle->borderSizeBottom;

	display::setColor(selectedValueStyle->borderColor);
	auto yCenter = widgetCursor.y + (widgetCursor.h - rectHeight) / 2;
	display::drawRoundedRect(
		x, yCenter, x + widgetCursor.w - 1, yCenter + rectHeight - 1, selectedValueStyle->borderSizeLeft,
		selectedValueStyle->borderRadiusTLX, selectedValueStyle->borderRadiusTLY, selectedValueStyle->borderRadiusTRX, selectedValueStyle->borderRadiusTRY,
		selectedValueStyle->borderRadiusBRX, selectedValueStyle->borderRadiusBRY, selectedValueStyle->borderRadiusBLX, selectedValueStyle->borderRadiusBLY
	);

	int clip1_x1 = widgetCursor.x;
	int clip1_y1 = widgetCursor.y;
	int clip1_x2 = widgetCursor.x + widgetCursor.w - 1;
	int clip1_y2 = yCenter - 1;

	int clip2_x1 = widgetCursor.x;
	int clip2_y1 = yCenter + selectedValueStyle->borderSizeTop;
	int clip2_x2 = widgetCursor.x + widgetCursor.w - 1;
	int clip2_y2 = yCenter + rectHeight - selectedValueStyle->borderSizeBottom;

	int clip3_x1 = widgetCursor.x;
	int clip3_y1 = yCenter + rectHeight;
	int clip3_x2 = widgetCursor.x + widgetCursor.w - 1;
	int clip3_y2 = widgetCursor.y + widgetCursor.h - 1;

	// y of the item minValue, in 64 bits: the range can be wider than int
	int64_t yFirstItem = widgetCursor.y + (widgetCursor.h - textHeight) / 2 + (int64_t)llroundf(position);

	// index (from minValue) of the first item that is not above the widget
	int64_t firstVisible = 0;
	if (yFirstItem + textHeight <= widgetCursor.y) {
		firstVisible = (widgetCursor.y - yFirstItem - textHeight) / textHeight + 1;
	}

	// The text expression of an item is evaluated while the data is the index
	// of the item. A flow variable is assigned directly (no change notification,
	// the flow doesn't run meanwhile) and restored after the items, other data
	// (a native variable) is set through SET and restored.
	uint32_t variableType = VALUE_TYPE_UNDEFINED;
	Value *variable = getFlowVariable(widgetCursor, variableType);
	Value savedValue = variable ? *variable : Value();
	bool isDataSet = false;

	display::setColor(unselectedValueStyle->color);

    for (int64_t k = firstVisible; (int64_t)minValue + k <= maxValue; k++) {
        int64_t yItem = yFirstItem + k * textHeight;
        if (yItem >= widgetCursor.y + widgetCursor.h) {
            break;
        }
        int y = (int)yItem;
        int i = (int)(minValue + k);

        if (variable) {
            assignValue(*variable, Value(i, VALUE_TYPE_INT32), variableType);
        } else {
            set(widgetCursor, widget->data, i);
        }
        isDataSet = true;

        Value textValue = get(widgetCursor, widget->text);

        char text[100];
        textValue.toText(text, sizeof(text));

		auto textLength = strlen(text);

		int textWidth = display::measureStr(text, (int)textLength, fontUnselectedValue, 0);
		if (y < clip1_y2) {
			display::drawStr(
				text, (int)textLength,
				x + (widgetCursor.w - textWidth) / 2, y,
				clip1_x1, clip1_y1, clip1_x2, clip1_y2,
				fontUnselectedValue,
				-1
			);
		} else {
			display::drawStr(
				text, (int)textLength,
				x + (widgetCursor.w - textWidth) / 2, y,
				clip3_x1, clip3_y1, clip3_x2, clip3_y2,
				fontUnselectedValue,
				-1
			);
		}

		if (y + textHeight >= clip2_y1 || y < clip2_y2) {
			int textWidth = display::measureStr(text, (int)textLength, fontSelectedValue, 0);

			display::setColor(selectedValueStyle->color);
			display::drawStr(
				text, (int)textLength,
				x + (widgetCursor.w - textWidth) / 2, y,
				clip2_x1, clip2_y1, clip2_x2, clip2_y2,
				fontSelectedValue,
				-1
			);
			display::setColor(unselectedValueStyle->color);
		}
    }

    if (isDataSet) {
        if (variable) {
            *variable = savedValue;
        } else {
            set(widgetCursor, widget->data, data);
        }
    }
}

bool RollerWidgetState::hasOnTouch() {
    return true;
}

void RollerWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        dragOrigin = touchEvent.y;
        dragStartPosition = position;
        dragPosition = dragStartPosition;
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
		if (abs(touchEvent.y - dragOrigin) > (int)(DISPLAY_HEIGHT / 25)) {
			isDragging = true;
			isRunning = true;
			dragPosition = dragStartPosition + (touchEvent.y - dragOrigin);
		}
	} else if (touchEvent.type == EVENT_TYPE_TOUCH_UP || touchEvent.type == EVENT_TYPE_AUTO_REPEAT) {
		if (!isDragging) {
			isRunning = true;
			if (touchEvent.y < widgetCursor.y + (widgetCursor.h - textHeight) / 2) {
				applyForce(textHeight * FRICTION);
			} else if (touchEvent.y > widgetCursor.y + (widgetCursor.h - textHeight) / 2 + textHeight) {
				applyForce(-textHeight * FRICTION);
			}
		} else {
			if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
				isDragging = false;
			}
		}
	}
}

void RollerWidgetState::updatePosition() {
    applyDragForce();
    applySnapForce();
    applyEdgeForce();

    auto inverseFriction = 1.0f - FRICTION;
    velocity *= inverseFriction;
    position += velocity;
}

bool RollerWidgetState::isMoving() {
    return isDragging || fabs(velocity) >= 0.01f;
}

void RollerWidgetState::applyDragForce() {
    if (!isDragging) {
        return;
    }
    auto dragVelocity = dragPosition - position;
    applyForce(dragVelocity - velocity);
}

void RollerWidgetState::applySnapForce() {
    if (isDragging) {
        return;
    }

    // Make sure position ends at multiply of textHeight
    auto restPosition = position + velocity / FRICTION;
    auto targetRestPosition = roundf(restPosition / textHeight) * textHeight;
    applyForce(FRICTION * (targetRestPosition - position) - velocity);
}

void RollerWidgetState::applyEdgeForce() {
    if (isDragging) {
        return;
    }

    float edgeFrom = -(float)((int64_t)maxValue - minValue) * textHeight;
    float edgeTo = 0;

    if (position > edgeTo) {
        auto distanceToEdge = edgeTo - position;
        auto force = distanceToEdge * BOUNCE_FORCE;
        auto restPosition = position + (velocity + force) / FRICTION;
        if (restPosition <= edgeTo) {
            force -= velocity;
        }
        applyForce(force);
    } else if (position < edgeFrom) {
        auto distanceToEdge = edgeFrom - position;
        auto force = distanceToEdge * BOUNCE_FORCE;
        auto restPosition = position + (velocity + force) / FRICTION;
        if (restPosition >= edgeFrom) {
            force -= velocity;
        }
        applyForce(force);
    }
}

void RollerWidgetState::applyForce(float force) {
    velocity += force;
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
