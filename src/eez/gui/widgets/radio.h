/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

namespace eez {
namespace gui {

// Checked if data is equal to value, the radio buttons of a group have the same
// data and different values. A touch sets data to value and executes the
// action, without data and action it can't be touched. It is drawn in the
// checked, pressed and disabled states of its style (see drawCheckboxOrRadio
// for the look).
struct RadioWidget : public Widget {
    int16_t value;
    int16_t text;
};

struct RadioWidgetState : public WidgetState {
    WidgetStateFlags flags;
    // style after overrideStyle
    int styleId;
    Value data;
    Value value;
    Value text;

    bool updateState() override;
    void render() override;

    bool hasOnTouch() override;
    void onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) override;
};

} // namespace gui
} // namespace eez
