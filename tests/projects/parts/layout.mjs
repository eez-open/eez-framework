/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Layout part: rows and columns (tests/render/layout_test.cpp).
//
// Every name starts with "layout_". Containers with the layout horizontal (a
// row) or vertical (a column), see eez-framework gui/layout.h: the gap, the
// alignment on the main axis (justifyContent) and on the cross axis
// (alignItems, alignSelf of a child), growing children, a hidden child, sizes
// from the content (a Text, a row in a column), the baseline of texts in two
// fonts and the right-to-left mirroring. The page layout_parts: a DisplayData and
// an Input with a label, a unit and a quantity line (eez-framework
// drawValueParts), alone and in rows on the baseline, with the label above or
// left of the value.
//
// The children are rectangles or texts with a background of their own color, so
// the test finds their boxes in the frame. The tests set the native variables
// layout_visible and layout_text from C++ (render/layout_test.cpp).

import { oid, page, rectangle, style, text, variable, widget } from "../fwlib.mjs";

export const COLORS = [
    ["layout_page", "#405060"],
    ["layout_bg", "#202020"],
    ["layout_gray", "#808080"],
    ["layout_white", "#f8fcf8"],
    ["layout_red", "#f80000"],
    ["layout_green", "#00fc00"],
    ["layout_blue", "#0000f8"],
    ["layout_yellow", "#f8fc00"],
    ["layout_orange", "#f88000"],
    ["layout_purple", "#8000f8"],
];

export const STYLES = [
    style("layout_page", { backgroundColor: "layout_page" }),
    // a row or column: the content box is 5 px inside
    style("layout_box", { backgroundColor: "layout_bg", padding: "5" }),
    // a row inside a column: the content box is 3 px inside
    style("layout_inner", { backgroundColor: "layout_gray", padding: "3" }),
    style("layout_r", { backgroundColor: "layout_red" }),
    style("layout_g", { backgroundColor: "layout_green" }),
    style("layout_b", { backgroundColor: "layout_blue" }),
    // texts at the top left, no padding
    style("layout_text", {
        backgroundColor: "layout_purple", color: "layout_white", alignHorizontal: "left",
        alignVertical: "top", padding: "0",
    }),
    style("layout_text_small", {
        backgroundColor: "layout_orange", color: "layout_white", alignHorizontal: "left",
        alignVertical: "top", padding: "0",
    }),
    style("layout_text_big", {
        backgroundColor: "layout_yellow", color: "layout_bg", alignHorizontal: "left",
        alignVertical: "top", padding: "0", font: "VRobotoBig",
    }),
    // a value with a label and a unit: the value in VRobotoBig (height 52,
    // ascent 42) at the top left inside the padding 4, the label in VRoboto
    // (height 26, ascent 21) with the bottom padding 2, the unit in VRoboto 6 px
    // after the value, the quantity line a 2 px bottom border 3 px under it
    style("layout_pv", {
        backgroundColor: "layout_blue", color: "layout_white", alignHorizontal: "left",
        alignVertical: "top", padding: "4", font: "VRobotoBig",
    }),
    style("layout_pl", { color: "layout_green", alignHorizontal: "left", padding: "0 0 2 0", font: "VRoboto" }),
    style("layout_pu", { color: "layout_yellow", padding: "0 0 0 6", font: "VRoboto" }),
    // a set field: the quantity line is the bottom border, 2 px red, the other
    // borders 1 px purple (a color for each side)
    style("layout_pfield", {
        backgroundColor: "layout_blue", color: "layout_white", alignHorizontal: "left",
        alignVertical: "top", padding: "4", font: "VRobotoBig", borderSize: "1 1 2 1",
        borderColor: "layout_purple layout_purple layout_red layout_purple",
    }),
    // rounded, the bottom border 4 px red, the others 2 px purple
    style("layout_psides", {
        backgroundColor: "layout_blue", borderSize: "2 2 4 2", borderRadius: "12",
        borderColor: "layout_purple layout_purple layout_red layout_purple",
    }),
    // right aligned: the right padding 40 is the room for the unit
    style("layout_pv_right", {
        backgroundColor: "layout_blue", color: "layout_white", alignHorizontal: "right",
        alignVertical: "top", padding: "4 40 4 4", font: "VRobotoBig",
    }),
];

export const GLOBALS = [
    variable("layout_visible", "boolean", null, { native: true }),
    variable("layout_text", "string", null, { native: true }),
    variable("layout_value", "float", null, { native: true }),
];

// Roboto Regular (fonts/Roboto-Regular.ttf) at the double size of VRoboto (part
// render), the ascent and the descent doubled
export const FONTS = [{
    objID: oid("font", "VRobotoBig"), name: "VRobotoBig", description: "",
    renderingEngine: "freetype",
    source: { objID: oid("font", "VRobotoBig", "source"), filePath: "../fonts/Roboto-Regular.ttf", size: 28 },
    bpp: 8, threshold: 128,
    height: 52, ascent: 42, descent: 10,
    glyphs: [], screenOrientation: "all", alwaysBuild: true,
    vector: true,
}];

// a rectangle child of a row or column (the position is not used)
function rgb(key, w, h, color, props = {}) {
    return rectangle(["layout", key], 0, 0, w, h, "layout_" + color, props);
}

// red 40x20, green 60x30, blue 30x40
function children(key) {
    return [rgb(key + "_r", 40, 20, "r"), rgb(key + "_g", 60, 30, "g"), rgb(key + "_b", 30, 40, "b")];
}

function row(key, left, top, width, height, children_, props = {}) {
    props = { ...props };
    if (!("gap" in props)) {
        props.gap = 10;
    }
    return widget("ContainerWidget", ["layout", key], left, top, width, height, "layout_box",
        { layout: "horizontal", widgets: children_, ...props });
}

function column(key, left, top, width, height, children_, props = {}) {
    props = { ...props };
    if (!("gap" in props)) {
        props.gap = 10;
    }
    return widget("ContainerWidget", ["layout", key], left, top, width, height, "layout_box",
        { layout: "vertical", widgets: children_, ...props });
}

// the rows of the page layout_rows: 400 x 50 at x = 20, y = 10 + 56 * k
const ROWS = [
    ["start", {}],
    ["center", { justifyContent: "center" }],
    ["end", { justifyContent: "end" }],
    ["between", { justifyContent: "space-between" }],
    ["grow", null],
    ["align_center", { alignItems: "center" }],
    ["align_end", { alignItems: "end" }],
    ["stretch", { alignItems: "stretch" }],
];

function rowsPage() {
    const components = [];
    ROWS.forEach(([key, props], k) => {
        let children_;
        if (props === null) {
            children_ = [rgb(key + "_r", 40, 20, "r"), rgb(key + "_g", 60, 30, "g", { flexGrow: true }),
                rgb(key + "_b", 30, 40, "b")];
            props = {};
        } else {
            children_ = children(key);
        }
        components.push(row("row_" + key, 20, 10 + 56 * k, 400, 50, children_, props));
    });

    // the right side: x = 440, 340 wide
    components.push(
        // align self overrides align items
        row("row_self", 440, 10, 340, 50, [
            rgb("self_r", 40, 20, "r"),
            rgb("self_g", 60, 30, "g", { alignSelf: "end" }),
            rgb("self_b", 30, 20, "b", { alignSelf: "center" }),
        ]),
        // the green child is hidden by layout_visible
        row("row_hidden", 440, 66, 340, 50, [
            rgb("hidden_r", 40, 20, "r"),
            rgb("hidden_g", 60, 30, "g", { visible: "layout_visible" }),
            rgb("hidden_b", 30, 40, "b"),
        ]),
        // the text is as wide as layout_text
        row("row_fit", 440, 122, 340, 50, [
            widget("TextWidget", ["layout", "fit_text"], 0, 0, 100, 40, "layout_text", {
                data: "layout_text",
                fitToContent: "width",
            }),
            rgb("fit_b", 30, 40, "b"),
        ]),
        // the first lines of the texts on one baseline, the rectangle with its
        // bottom on it
        row("row_baseline", 440, 178, 340, 70, [
            text(["layout", "base_small"], 0, 0, 10, 10, "Ag", "layout_text_small", { fitToContent: "both" }),
            text(["layout", "base_big"], 0, 0, 10, 10, "Ag", "layout_text_big", { fitToContent: "both" }),
            rgb("base_r", 40, 20, "r"),
        ], { alignItems: "baseline" }),
        // a column with a row as big as its children and a row as wide as the
        // column
        column("col_rows", 440, 260, 340, 200, [
            widget("ContainerWidget", ["layout", "inner_fit"], 0, 0, 10, 10, "layout_inner", {
                layout: "horizontal",
                gap: 4, fitToContent: "both", widgets: [rgb("inner_r", 40, 20, "r"), rgb("inner_g", 60, 30, "g")],
            }),
            widget("ContainerWidget", ["layout", "inner_wide"], 0, 0, 10, 10, "layout_inner", {
                layout: "horizontal",
                fitToContent: "height", alignSelf: "stretch", widgets: [rgb("inner_b", 30, 40, "b")],
            }),
        ], { gap: 6 }),
    );
    return page("layout_rows", components, [], { style: "layout_page" });
}

function columnsPage() {
    return page("layout_columns", [
        column("col", 20, 20, 120, 300, children("col"), { justifyContent: "end", alignItems: "center" }),
    ], [], { style: "layout_page" });
}

// "LBL" above "12.345" and "mA" (the value and unit option)
function partsDd(key, left, top, width, height, props = {}) {
    return widget("DisplayDataWidget", ["layout", key], left, top, width, height, "layout_pv", {
        data: '"12.345 mA"', displayOption: 7, refreshRate: "0", label: '"LBL"',
        labelStyle: { useStyle: "layout_pl" }, unitStyle: { useStyle: "layout_pu" }, ...props,
    });
}

function partsPage() {
    return page("layout_parts", [
        partsDd("parts_dd", 20, 20, 360, 110),
        // as big as its content, the value on the baseline of the text
        row("row_parts_dd", 20, 150, 420, 110, [
            text(["layout", "parts_text1"], 0, 0, 10, 10, "Ag", "layout_text_small", { fitToContent: "both" }),
            partsDd("parts_dd_fit", 0, 0, 10, 10, { fitToContent: "both" }),
        ], { alignItems: "baseline" }),
        // "SET" above "12.5 V", the quantity line is the bottom border of the
        // style, on the baseline of the text
        row("row_parts_input", 20, 280, 420, 120, [
            text(["layout", "parts_text2"], 0, 0, 10, 10, "Ag", "layout_text_small", { fitToContent: "both" }),
            widget("InputEmbeddedWidget", ["layout", "parts_input"], 0, 0, 200, 100, "layout_pfield", {
                data: "12.5", inputType: "number", min: "0", max: "100", precision: "0.1", unit: '"V"',
                info: '""', defaultValue: "0", label: '"SET"', labelStyle: { useStyle: "layout_pl" },
            }),
        ], { alignItems: "baseline" }),
        // a rounded box with a color for each side
        rectangle(["layout", "parts_sides"], 460, 280, 200, 100, "layout_psides"),
        // right aligned values with different units end at one x
        widget("DisplayDataWidget", ["layout", "parts_right_v"], 460, 20, 300, 60, "layout_pv_right", {
            data: '"12.000 V"', displayOption: 7, refreshRate: "0", unitStyle: { useStyle: "layout_pu" },
        }),
        widget("DisplayDataWidget", ["layout", "parts_right_ma"], 460, 90, 300, 60, "layout_pv_right", {
            data: '"0.750 mA"', displayOption: 7, refreshRate: "0", unitStyle: { useStyle: "layout_pu" },
        }),
        // "12.50" fits the content box (150 - 4 - 40), "V" only the right padding
        widget("DisplayDataWidget", ["layout", "parts_right_fit"], 460, 160, 150, 60, "layout_pv_right", {
            data: "layout_value", displayOption: 7, precision: "0.01", refreshRate: "0",
            unitStyle: { useStyle: "layout_pu" },
        }),
        // "SET" left of "12.5 V" (right aligned)
        widget("InputEmbeddedWidget", ["layout", "parts_input_left"], 20, 410, 360, 60, "layout_pv_right", {
            data: "12.5", inputType: "number", min: "0", max: "100", precision: "0.1", unit: '"V"',
            info: '""', defaultValue: "0", label: '"SET"', labelPosition: "left",
            labelStyle: { useStyle: "layout_pl" },
        }),
        // "LBL" left of "12.345" and "mA", as big as its content, the value on the
        // baseline of the text
        row("row_parts_left", 400, 400, 390, 76, [
            text(["layout", "parts_text3"], 0, 0, 10, 10, "Ag", "layout_text_small", { fitToContent: "both" }),
            partsDd("parts_dd_left", 0, 0, 10, 10, { fitToContent: "both", labelPosition: "left" }),
        ], { alignItems: "baseline" }),
    ], [], { style: "layout_page" });
}

export const PAGES = [
    rowsPage(),
    columnsPage(),
    partsPage(),
];
