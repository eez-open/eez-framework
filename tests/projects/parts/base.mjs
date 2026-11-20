/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Base part: what the framework itself needs (harness/app.cpp maps these to the
// EEZ_* ids), the main page the harness shows at start and the globals of the
// expression cases.

import { nativeAction, page, rectangle, style, text, variable } from "../fwlib.mjs";

export const COLORS = [
    ["background", "#000000"],
    ["text", "#ffffff"],
    ["accent", "#2060c0"],
    ["bookmark", "#ffff00"],
    ["backdrop", "#202020"],
    ["error", "#c00000"],
];

// children of the "default" style (font RobotoCondensed, color text on background)
export const STYLES = [
    style("text_left", { alignHorizontal: "left" }),
    style("accent_box", { backgroundColor: "accent", borderSize: "2", borderColor: "text", borderRadius: "8" }),
    // used by the framework (EEZ_STYLE_ID_* in harness/app.cpp)
    style("info_alert", { backgroundColor: "accent", borderSize: "2", borderColor: "text", padding: "8" }),
    style("error_alert", { backgroundColor: "error", borderSize: "2", borderColor: "text", padding: "8" }),
    style("error_alert_button", { backgroundColor: "background", borderSize: "1", borderColor: "text", padding: "4" }),
    style("select_enum_item_popup_container", { borderSize: "1", borderColor: "text", padding: "4" }),
    style("select_enum_item_popup_container_s", { borderSize: "1", borderColor: "text", padding: "2" }),
    // rounded: a pressed item keeps the popup background in its corners
    // (gui/popups_test.cpp)
    style("select_enum_item_popup_item", { alignHorizontal: "left", padding: "4", borderRadius: "6" }),
    style("select_enum_item_popup_item_s", { alignHorizontal: "left", padding: "2" }),
    style("select_enum_item_popup_disabled_item", { alignHorizontal: "left", color: "backdrop", padding: "4" }),
    style("select_enum_item_popup_disabled_item_s", { alignHorizontal: "left", color: "backdrop", padding: "2" }),
    // rounded: its corners show the page below (gui/popups_test.cpp)
    style("menu_with_buttons_container", { borderSize: "1", borderColor: "text", padding: "8", borderRadius: "12" }),
    style("menu_with_buttons_message", { padding: "4" }),
    style("menu_with_buttons_button", { backgroundColor: "accent", padding: "4" }),
];

export const GLOBALS = [
    // used by the expression cases
    variable("g_int", "integer", "5"),
    variable("g_double", "double", "2.5"),
    variable("g_string", "string", "\"text\""),
    variable("g_bool", "boolean", "true"),
    variable("g_int_array", "array:integer", "[10, 20, 30]"),
    // native variables the framework implements (keypad, alerts)
    variable("keypad_text", "string", null, { native: true }),
    variable("keypad_edit_unit", "string", null, { native: true }),
    variable("alert_message", "string", null, { native: true }),
];

// the framework implements drag_overlay and scroll, harness/app.cpp edit
export const ACTIONS = ["drag_overlay", "edit", "scroll"].map(name => nativeAction(name));

// the first pages, in this order (PAGE_ID_MAIN is the page the harness shows)
export const PAGES = [
    page("main", [
        text("main/hello", 10, 10, 200, 40, "Hello"),
        rectangle("main/box", 10, 60, 100, 50, "accent_box"),
    ]),
    page("numeric_keypad", [text("numeric_keypad/title", 10, 10, 200, 40, "Keypad")]),
    page("async_operation_in_progress", [text("async/title", 10, 10, 300, 40, "Please wait")]),
];
