/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Rows part: the icon parts of the Text widget (Text, DisplayData, Button) and
// of the Input, the label of the DropDownList (tests/render/rows_test.cpp).
//
// Every name starts with "rows_". A row of a settings group is one widget: an
// icon at the left, the label, the value and the unit, an icon at the right
// (eez-framework ValueParts). The whole widget is the touch target.
//
// The tests set the native variables rows_icon and rows_value from C++ (render/
// rows_test.cpp implements them and the native action rows_on_click).

import { eventHandler, nativeAction, page, style, variable, widget } from "../fwlib.mjs";

export const COLORS = [
    ["rows_bg", "#202020"],
    ["rows_page_bg", "#406080"],
    ["rows_white", "#f8fcf8"],
    ["rows_red", "#f80000"],
    ["rows_green", "#00fc00"],
    ["rows_blue", "#0000f8"],
    ["rows_yellow", "#f8fc00"],
    ["rows_pressed", "#804000"],
];

export const STYLES = [
    // the row: the value at the right, in white
    style("rows_row", { backgroundColor: "rows_bg", color: "rows_white", alignHorizontal: "right", padding: "0 16" }),
    style("rows_row_pressed", { state: "pressed", useStyle: "rows_row", backgroundColor: "rows_pressed" }),
    style("rows_row_left", { backgroundColor: "rows_bg", color: "rows_white", alignHorizontal: "left", padding: "0 16" }),
    style("rows_row_left_pressed", { state: "pressed", useStyle: "rows_row_left", backgroundColor: "rows_pressed" }),
    style("rows_row_ellipsis", {
        backgroundColor: "rows_bg", color: "rows_white", alignHorizontal: "right", padding: "0 16",
        ellipsis: true,
    }),
    // the parts: each in a color of its own
    style("rows_label", { color: "rows_yellow", backgroundColor: "rows_bg" }),
    style("rows_icon", { color: "rows_red", backgroundColor: "rows_bg", padding: "0 12 0 0" }),
    style("rows_trailing", { color: "rows_green", backgroundColor: "rows_bg", padding: "0 0 0 12" }),
    style("rows_unit", { color: "rows_blue", backgroundColor: "rows_bg", padding: "0 0 0 4" }),
    // the row of a dropdown with a label: the box is inside its padding
    style("rows_ddl_row", { color: "rows_yellow", backgroundColor: "rows_bg", alignHorizontal: "left", padding: "5 16" }),
    // a dropdown box
    style("rows_box", { backgroundColor: "rows_blue", color: "rows_white", borderSize: "1", borderColor: "rows_white" }),
    style("rows_page", { backgroundColor: "rows_page_bg" }),
];

export const GLOBALS = [
    variable("rows_icon", "string", null, { native: true }),
    variable("rows_value", "float", null, { native: true }),
    variable("rows_number", "float", "1.5"),
    variable("rows_choice", "integer", "1"),
    variable("rows_options", "array:string", '["zero", "one", "two"]'),
];

export const ACTIONS = [nativeAction("rows_on_click")];

const LABEL = { useStyle: "rows_label" };
const DDL_ROW = { useStyle: "rows_ddl_row" };
const ICON = { useStyle: "rows_icon" };
const TRAILING = { useStyle: "rows_trailing" };

function w(type, name, top, style_ = "rows_row", props = {}) {
    return widget(type, ["rows", name], 20, top, 400, 40, style_, props);
}

function rowsPage() {
    return page("rows_page", [
        // the two icons around a label and a value
        w("DisplayDataWidget", "both", 10, "rows_row", {
            data: '"value"', displayOption: 0, refreshRate: "0",
            label: '"Label"', labelPosition: "left", labelStyle: LABEL, unitStyle: {},
            icon: '"#"', iconStyle: ICON, trailingIcon: '">"', trailingIconStyle: TRAILING,
        }),
        // only the trailing icon, with an action
        w("ButtonWidget", "nav", 60, "rows_row_left", {
            data: '"Open"',
            trailingIcon: '">"', trailingIconStyle: TRAILING,
            eventHandlers: [eventHandler(["rows", "nav"], "CLICKED", "rows_on_click")],
        }),
        // only the icon, by data (an empty text is no icon)
        w("TextWidget", "icon", 110, "rows_row_left", {
            data: '"Text"', fitToContent: "none",
            icon: "rows_icon", iconStyle: ICON,
        }),
        // a unit before the trailing icon
        w("DisplayDataWidget", "unit", 160, "rows_row", {
            data: "rows_value", displayOption: 7, refreshRate: "0",
            label: '"Voltage"', labelPosition: "left", labelStyle: LABEL, unitStyle: { useStyle: "rows_unit" },
            trailingIcon: '">"', trailingIconStyle: TRAILING,
        }),
        // the icons without their styles: in the font and color of the widget
        w("TextWidget", "plain", 210, "rows_row", { data: '"mid"', fitToContent: "none", icon: '"<"', trailingIcon: '">"' }),
        // an Input
        w("InputEmbeddedWidget", "input", 260, "rows_row", {
            data: "rows_number", inputType: "number", min: "0", max: "10",
            precision: "0.1", unit: '"V"', info: '""', defaultValue: "0",
            label: '"Limit"', labelPosition: "left", labelStyle: LABEL,
            icon: '"#"', iconStyle: ICON, trailingIcon: '">"', trailingIconStyle: TRAILING,
        }),
        // a DropDownList with a label: a row in the label style, the box in its
        // own width at the right, inside the padding of the row
        w("DropDownListWidget", "dropdown", 310, "rows_box", {
            data: "rows_choice", options: "rows_options",
            label: '"Range"', labelStyle: DDL_ROW, boxWidth: 150,
        }),
        // without a box width: half of the content box of the row
        w("DropDownListWidget", "dropdown_half", 360, "rows_box", {
            data: "rows_choice", options: "rows_options",
            label: '"Range"', labelStyle: DDL_ROW,
        }),
        // without a label: as before
        w("DropDownListWidget", "dropdown_plain", 410, "rows_box", { data: "rows_choice", options: "rows_options" }),

        // fit to content: the icons are in the width
        widget("TextWidget", ["rows", "fit"], 450, 10, 300, 40, "rows_row_left", {
            data: '"Fit"', fitToContent: "width",
            icon: '"#"', iconStyle: ICON, trailingIcon: '">"', trailingIconStyle: TRAILING,
        }),

        // a value that doesn't fit: cut with an ellipsis, or clipped at the label
        widget("DisplayDataWidget", ["rows", "long_ellipsis"], 450, 60, 330, 40, "rows_row_ellipsis", {
            data: '"a very long value that does not fit in this row"', displayOption: 0, refreshRate: "0",
            label: '"Label"', labelPosition: "left", labelStyle: LABEL, unitStyle: {},
            trailingIcon: '">"', trailingIconStyle: TRAILING,
        }),
        widget("DisplayDataWidget", ["rows", "long_clip"], 450, 110, 330, 40, "rows_row", {
            data: '"a very long value that does not fit in this row"', displayOption: 0, refreshRate: "0",
            label: '"Label"', labelPosition: "left", labelStyle: LABEL, unitStyle: {},
        }),
    ], [], { style: "rows_page" });
}

export const PAGES = [rowsPage()];
