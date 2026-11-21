/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// GUI part: pages, popups, keypads and lists of the GUI runtime tests
// (tests/gui/). The native actions and variables are implemented in
// tests/gui/gui_app.cpp: the gui_keypad_* actions, gui_yes, gui_no and gui_cancel
// call the framework actions of the same name without the prefix.

import { eventHandler, nativeAction, page, style, text, variable, widget } from "../fwlib.mjs";

export const STYLES = [
    style("gui_button", { backgroundColor: "accent", activeBackgroundColor: "error", borderSize: "1", borderColor: "text" }),
    style("gui_button_disabled", { backgroundColor: "backdrop", color: "background" }),
    style("gui_popup", { backgroundColor: "backdrop", borderSize: "2", borderColor: "text" }),
    // rounded: its corners show the page below (gui/page_stack_test.cpp)
    style("gui_rounded", { backgroundColor: "backdrop", borderSize: "2", borderColor: "text", borderRadius: "16" }),
    style("gui_transparent", { backgroundColor: "transparent" }),
    // a white page: the shadow of a popup over it shows (gui/popups_test.cpp)
    style("gui_light", { backgroundColor: "text" }),
    style("gui_keypad_text", { alignHorizontal: "left", padding: "4", borderSize: "1", borderColor: "text" }),
    style("gui_key", { backgroundColor: "accent", activeBackgroundColor: "error", borderSize: "1", borderColor: "text" }),
    style("gui_list", { color: "text" }),
    style("gui_item", { borderSize: "1", borderColor: "backdrop" }),
    // page dots: 8x8 white, the current one accent
    style("gui_dot", { backgroundColor: "text", padding: "4" }),
    style("gui_dot_selected", { useStyle: "gui_dot", state: "selected", backgroundColor: "accent" }),
    // a group of switch rows (gui/switch_rows_test.cpp): accent dividers; the
    // track 64x32 (backdrop, accent when on, error when disabled), the label
    // white (backdrop disabled), "Off" error and "On" bookmark, the row
    // bookmark while pressed
    style("gui_group", { borderSize: "1", borderColor: "accent" }),
    style("gui_switch_row", { backgroundColor: "backdrop", color: "text", padding: "19 16 19 16" }),
    style("gui_switch_row_checked", { useStyle: "gui_switch_row", state: "checked", backgroundColor: "accent" }),
    style("gui_switch_row_disabled", { useStyle: "gui_switch_row", state: "disabled", backgroundColor: "error" }),
    style("gui_switch_row_pressed", { useStyle: "gui_switch_row", state: "pressed", backgroundColor: "bookmark" }),
    style("gui_switch_label", { alignHorizontal: "left" }),
    style("gui_switch_label_disabled", { useStyle: "gui_switch_label", state: "disabled", color: "backdrop" }),
    style("gui_switch_state", { color: "error", alignHorizontal: "right", padding: "0 12 0 0" }),
    style("gui_switch_state_checked", { useStyle: "gui_switch_state", state: "checked", color: "bookmark" }),
];

// [0, ..., n - 1]
function range(n) {
    return Array.from({ length: n }, (_, i) => i);
}

export const GLOBALS = [
    variable("gui_list_items", "array:integer", `[${range(20).join(", ")}]`),
    variable("gui_grid_items", "array:integer", `[${range(60).join(", ")}]`),
    variable("gui_resize_items", "array:integer", `[${range(8).join(", ")}]`),
    // native, implemented in tests/gui/gui_app.cpp
    variable("gui_enabled", "boolean", null, { native: true }),
    variable("gui_switch", "boolean", null, { native: true }),
    variable("gui_switch2", "boolean", null, { native: true }),
    variable("gui_name", "string", null, { native: true }),
    variable("gui_overlay", "any", null, { native: true }),
    variable("gui_keypad_ok_enabled", "boolean", null, { native: true }),
    variable("gui_keypad_option1_text", "string", null, { native: true }),
    variable("gui_choice", "integer", null, { native: true }),
    variable("gui_check", "boolean", null, { native: true }),
];

export const ACTIONS = [
    "gui_click_a", "gui_click_b", "gui_click_c", "gui_click_d", "gui_click_p",
    "gui_list_item", "gui_grid_item",
    "gui_yes", "gui_no", "gui_cancel",
    "gui_keypad_key", "gui_keypad_space", "gui_keypad_back", "gui_keypad_clear",
    "gui_keypad_ok", "gui_keypad_cancel", "gui_keypad_sign", "gui_keypad_unit",
    "gui_toggle_keypad_mode", "gui_keypad_option1",
].map(name => nativeAction(name));

function button(key, left, top, width, height, label, action, style_ = "gui_button", props = {}) {
    return widget(
        "ButtonWidget", key, left, top, width, height, style_,
        { data: `"${label}"`, eventHandlers: [eventHandler(key, "CLICKED", action)], ...props },
    );
}

// A keypad key: a text widget whose data is the label (Keypad::key reads it).
function key(key_, left, top, label, action = "gui_keypad_key", width = 98, height = 78, props = {}) {
    return widget(
        "TextWidget", key_, left, top, width, height, "gui_key",
        { data: `"${label}"`, eventHandlers: [eventHandler(key_, "CLICKED", action)], ...props },
    );
}

function keypadText(key_) {
    return widget("DisplayDataWidget", key_, 0, 0, 800, 60, "gui_keypad_text", { data: "keypad_text", displayOption: 0 });
}

// page_a: buttons at known positions (gui_test.h has the coordinates)
const PAGE_A = page("gui_page_a", [
    button("gui/a/btn1", 20, 20, 200, 60, "A1", "gui_click_a"),
    button("gui/a/btn2", 300, 20, 200, 60, "A2", "gui_click_b"),
    button("gui/a/disabled", 20, 120, 200, 60, "Off", "gui_click_c", "gui_button",
        { enabled: "gui_enabled", disabledStyle: { useStyle: "gui_button_disabled" } }),
    widget("SwitchWidget", "gui/a/switch", 300, 120, 100, 50, "gui_button", { data: "gui_switch" }),
]);

// page_b: other widgets than page_a
const PAGE_B = page("gui_page_b", [
    text("gui/b/title", 20, 20, 760, 40, "Page B"),
    button("gui/b/btn1", 500, 300, 200, 60, "B1", "gui_click_d"),
]);

// popups closed by a touch outside of them
const POPUP = page("gui_popup", [
    button("gui/popup/btn", 20, 20, 160, 60, "P1", "gui_click_p"),
], [], { left: 200, top: 120, width: 400, height: 240, style: "gui_popup", closePageIfTouchedOutside: true });

const POPUP2 = page("gui_popup2", [
    text("gui/popup2/title", 10, 10, 180, 40, "Popup 2"),
], [], { left: 300, top: 200, width: 200, height: 80, style: "gui_popup", closePageIfTouchedOutside: true });

// smaller popup that is not closed by a touch outside of it
const DIALOG = page("gui_dialog", [
    text("gui/dialog/title", 10, 10, 280, 40, "Dialog"),
], [], { left: 250, top: 150, width: 300, height: 180, style: "gui_popup" });

// transparent full screen page
const TRANSPARENT = page("gui_transparent", [
    text("gui/transparent/title", 300, 220, 200, 40, "Transparent"),
], [], { style: "gui_transparent" });

// yes/no dialog (AppContext::yesNoDialog)
const YES_NO = page("gui_yes_no", [
    widget("TextWidget", "gui/yes_no/message", 20, 20, 460, 60, "default", { data: "alert_message" }),
    button("gui/yes_no/yes", 20, 180, 140, 60, "Yes", "gui_yes"),
    button("gui/yes_no/no", 180, 180, 140, 60, "No", "gui_no"),
    button("gui/yes_no/cancel", 340, 180, 140, 60, "Cancel", "gui_cancel"),
], [], { left: 150, top: 100, width: 500, height: 280, style: "gui_popup" });

function numericKeypad() {
    const components = [keypadText("gui/kn/text")];
    const rows = [["7", "8", "9"], ["4", "5", "6"], ["1", "2", "3"], ["0", "."]];
    rows.forEach((row, r) => {
        row.forEach((label, c) => {
            components.push(key(`gui/kn/key_${r}_${c}`, c * 100, 80 + r * 80, label));
        });
    });
    components.push(
        key("gui/kn/back", 320, 80, "<", "gui_keypad_back"),
        key("gui/kn/sign", 320, 160, "+/-", "gui_keypad_sign"),
        widget("TextWidget", "gui/kn/unit", 320, 240, 98, 78, "gui_key", {
            data: "keypad_edit_unit",
            eventHandlers: [eventHandler("gui/kn/unit", "CLICKED", "gui_keypad_unit")],
        }),
        key("gui/kn/clear", 320, 320, "C", "gui_keypad_clear"),
        key("gui/kn/ok", 440, 80, "OK", "gui_keypad_ok"),
        key("gui/kn/cancel", 440, 160, "Esc", "gui_keypad_cancel"),
        widget("TextWidget", "gui/kn/option1", 440, 240, 98, 78, "gui_key", {
            data: "gui_keypad_option1_text",
            eventHandlers: [eventHandler("gui/kn/option1", "CLICKED", "gui_keypad_option1")],
        }),
    );
    return page("gui_keypad_num", components);
}

function textKeypad() {
    const components = [keypadText("gui/kt/text")];
    ["a", "b", "c", "ñ"].forEach((label, c) => {
        components.push(key(`gui/kt/key_${c}`, c * 100, 80, label));
    });
    components.push(
        key("gui/kt/space", 0, 160, "_", "gui_keypad_space"),
        key("gui/kt/back", 100, 160, "<", "gui_keypad_back"),
        key("gui/kt/clear", 200, 160, "C", "gui_keypad_clear"),
        widget("ButtonWidget", "gui/kt/ok", 300, 160, 98, 78, "gui_key", {
            data: '"OK"', enabled: "gui_keypad_ok_enabled",
            disabledStyle: { useStyle: "gui_button_disabled" },
            eventHandlers: [eventHandler("gui/kt/ok", "CLICKED", "gui_keypad_ok")],
        }),
        key("gui/kt/cancel", 400, 160, "Esc", "gui_keypad_cancel"),
        key("gui/kt/mode", 500, 160, "Aa", "gui_toggle_keypad_mode"),
    );
    return page("gui_keypad_text", components);
}

// a vertical list (20 items of 60 px in 300 px), a grid (60 items, 4 in a
// row, rows of 60 px in 300 px) and a horizontal list with page dots (20 items
// of 100 px in 400 px)
const LISTS = page("gui_lists", [
    widget("ListWidget", "gui/lists/list", 0, 0, 400, 300, "gui_list", {
        data: "gui_list_items", listType: "vertical", gap: 0,
        itemWidget: widget("TextWidget", "gui/lists/item", 0, 0, 400, 60, "gui_item", {
            data: '"Item " + $index', eventHandlers: [eventHandler("gui/lists/item", "CLICKED", "gui_list_item")],
        }),
    }),
    widget("GridWidget", "gui/lists/grid", 420, 0, 360, 300, "gui_list", {
        data: "gui_grid_items", gridFlow: "row",
        itemWidget: widget("TextWidget", "gui/lists/grid_item", 0, 0, 90, 60, "gui_item", {
            data: '"" + $index', eventHandlers: [eventHandler("gui/lists/grid_item", "CLICKED", "gui_grid_item")],
        }),
    }),
    widget("ListWidget", "gui/lists/hlist", 0, 320, 400, 80, "gui_list", {
        data: "gui_list_items", listType: "horizontal", gap: 0, pageDotsStyle: { useStyle: "gui_dot" },
        itemWidget: widget("TextWidget", "gui/lists/hitem", 0, 0, 100, 80, "gui_item", { data: '"" + $index' }),
    }),
    button("gui/lists/btn", 500, 400, 200, 60, "L1", "gui_click_a"),
]);

// a draggable overlay: container with the overlay data and the drag_overlay action
const OVERLAY = page("gui_overlay_page", [
    widget("ContainerWidget", "gui/overlay/container", 500, 300, 200, 100, "gui_popup", {
        overlay: "gui_overlay", layout: "static",
        eventHandlers: [eventHandler("gui/overlay/container", "CLICKED", "drag_overlay")],
        widgets: [
            text("gui/overlay/title", 50, 30, 100, 40, "Drag", "gui_popup"),
        ],
    }),
]);

// a container that scrolls vertically: 10 rows of 60 px (static layout) in
// 300 px at 0, 0, 400x300
const SCROLL = page("gui_scroll", [
    widget("ContainerWidget", "gui/scroll/container", 0, 0, 400, 300, "gui_list", {
        layout: "static", scroll: "vertical",
        widgets: range(10).map(i => widget("TextWidget", `gui/scroll/row_${i}`, 0, 60 * i, 400, 60, "gui_item", {
            data: `"Row ${i}"`,
            eventHandlers: [eventHandler(`gui/scroll/row_${i}`, "CLICKED", "gui_click_a")],
        })),
    }),
]);

// widgets with their own touch handling in a container that scrolls
// vertically (gui/touch_widgets_test.cpp): rows of 60 px (static layout) in
// 300 px at 0, 0, 400x300: a switch, a drop down list, a checkbox, a button
// group (3 buttons), an up/down (left half down, right half up), text rows
const SCROLL_WIDGETS = page("gui_scroll_widgets", [
    widget("ContainerWidget", "gui/scroll_widgets/container", 0, 0, 400, 300, "gui_list", {
        layout: "static", scroll: "vertical",
        widgets: [
            widget("SwitchWidget", "gui/scroll_widgets/switch", 0, 0, 400, 60, "gui_button", { data: "gui_switch" }),
            widget("DropDownListWidget", "gui/scroll_widgets/dropdown", 0, 60, 400, 60, "gui_button", {
                data: "gui_choice", options: '["zero", "one", "two"]',
            }),
            widget("CheckboxWidget", "gui/scroll_widgets/check", 0, 120, 400, 60, "gui_button", {
                data: "gui_check", text: '"Check"',
            }),
            widget("ButtonGroupWidget", "gui/scroll_widgets/group", 0, 180, 400, 60, "gui_button", {
                data: '["x", "y", "z"]', selectedButton: "gui_choice",
                selectedStyle: { useStyle: "gui_button" },
            }),
            widget("UpDownWidget", "gui/scroll_widgets/updown", 0, 240, 400, 60, "gui_item", {
                data: "gui_choice", min: "0", max: "9",
                downButtonText: '"<"', upButtonText: '">"', buttonsStyle: { useStyle: "gui_button" },
            }),
            ...range(5).map(i => widget("TextWidget", `gui/scroll_widgets/row_${i}`, 0, 300 + 60 * i, 400, 60, "gui_item", {
                data: `"Row ${i}"`,
            })),
        ],
    }),
]);

// a popup with rounded corners where gui_popup2 is
const ROUNDED = page("gui_rounded_popup", [
    text("gui/rounded/title", 50, 20, 100, 40, "Round", "gui_rounded"),
], [], { left: 300, top: 200, width: 200, height: 80, style: "gui_rounded" });

// a white full screen page
const LIGHT = page("gui_page_light", [], [], { style: "gui_light" });

// a column with dividers of two switch rows (gui/switch_rows_test.cpp): the
// rows 598x70 at y 101 and 172, the divider at y 171
const SWITCH_ROWS = page("gui_switch_rows", [
    widget("ContainerWidget", "gui/rows/group", 100, 100, 600, 143, "gui_group", {
        layout: "vertical", alignItems: "stretch", gap: 0, dividers: true, widgets: [
            widget("SwitchWidget", "gui/rows/row1", 0, 0, 598, 70, "gui_switch_row", {
                data: "gui_switch",
                label: '"Row 1"', labelStyle: { useStyle: "gui_switch_label" },
                showStateText: true, stateTextStyle: { useStyle: "gui_switch_state" },
            }),
            widget("SwitchWidget", "gui/rows/row2", 0, 0, 598, 70, "gui_switch_row", {
                data: "gui_switch2",
                label: '"Row 2"', labelStyle: { useStyle: "gui_switch_label" }, enabled: "gui_enabled",
            }),
        ],
    }),
]);

// lists in a page that is shown smaller and bigger than designed (a user widget
// resizes its page, as BB3+ shows a module page in a slot view): 8 items of a
// list (100x40), a grid (50x80, 2 in a row) and a list pinned to the top and
// the bottom fill the 300x320 page exactly
function resizeItem(key_, w, h) {
    // an action, so that findWidget finds the item
    return widget("TextWidget", key_, 0, 0, w, h, "gui_item", {
        data: '"" + $index',
        eventHandlers: [eventHandler(key_, "CLICKED", "gui_list_item")],
    });
}

const RESIZE_VIEW = page("gui_resize_view", [
    widget("ListWidget", "gui/resize/list", 0, 0, 100, 320, "gui_list", {
        data: "gui_resize_items",
        listType: "vertical", gap: 0, itemWidget: resizeItem("gui/resize/list_item", 100, 40),
    }),
    widget("GridWidget", "gui/resize/grid", 100, 0, 100, 320, "gui_list", {
        data: "gui_resize_items",
        gridFlow: "row", itemWidget: resizeItem("gui/resize/grid_item", 50, 80),
    }),
    widget("ListWidget", "gui/resize/pinned", 200, 0, 100, 320, "gui_list", {
        data: "gui_resize_items",
        listType: "vertical", gap: 0, resizing: { pinToEdge: 12, fixSize: 0 },
        itemWidget: resizeItem("gui/resize/pinned_item", 100, 40),
    }),
], [], { width: 300, height: 320, isUsedAsUserWidget: true });

const RESIZE = page("gui_resize", [
    widget("UserWidgetWidget", "gui/resize/smaller", 0, 0, 300, 240, "default", { userWidgetPageName: "gui_resize_view" }),
    widget("UserWidgetWidget", "gui/resize/bigger", 400, 0, 300, 480, "default", { userWidgetPageName: "gui_resize_view" }),
]);

export const PAGES = [PAGE_A, PAGE_B, POPUP, POPUP2, DIALOG, TRANSPARENT, YES_NO, numericKeypad(), textKeypad(), LISTS, OVERLAY,
    SCROLL, SCROLL_WIDGETS, ROUNDED, LIGHT, SWITCH_ROWS, RESIZE];

export const USER_WIDGETS = [RESIZE_VIEW];
