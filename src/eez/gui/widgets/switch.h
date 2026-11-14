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

namespace eez {
namespace gui {

// SwitchWidget::flags: "On" or "Off" left of the track
#define SWITCH_WIDGET_FLAG_STATE_TEXT (1 << 0)

// A switch: the track and the knob in the widget style (the background of the
// style is the off part of the track, the background of its checked state the
// on part, the color the knob). With a label or the state text the widget is a
// row: the label at the left (from the left padding of the style), the state
// text and the track at the right, the track in the height of the box without
// the top and bottom padding and two times as wide; else the track is the
// whole box without the padding. The whole widget toggles the data.
struct SwitchWidget : public Widget {
    // the text left of the track (a flow expression, 0: none)
    int16_t label;
    // the style of the label and of the state text (0: the widget style); the
    // state text: the checked state when on, both: the disabled state
    int16_t labelStyle;
    int16_t stateTextStyle;
    // the switch can be changed (a flow expression, 0: always); not enabled:
    // drawn in the disabled state styles, a touch doesn't change it
    int16_t enabled;
    uint16_t flags; // SWITCH_WIDGET_FLAG_...
    int16_t reserved;
};

struct SwitchWidgetState : public WidgetState {
	WidgetStateFlags flags;
	Value data;
	Value label;

	uint32_t changeStartTime = 0;

    bool updateState() override;
    void render() override;

    bool hasOnTouch() override;
	void onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) override;
};

} // namespace gui
} // namespace eez
