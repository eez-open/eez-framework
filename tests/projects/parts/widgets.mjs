/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Widgets part: the pages of the widget tests (tests/widgets/).
//
// Every name has the prefix w_. The tests set the global variables w_a ... from
// C++ and find them after the marker variable w_marker (the global variables of
// the parts are numbered in project order, see tests/widgets/wtest.h, which
// mirrors the order of GLOBALS below). Native variables and actions are
// implemented in tests/widgets/native.cpp.
//
// Pages are full screen (800x480, "default" style: black background). Widgets do
// not overlap (except on the pages that test it), the positions
// are mirrored as constants in the C++ tests.

import { crc32, zlibCompress } from "../deflate.mjs";

import {
    component, eventHandler, line, nativeAction, oid, page,
    style, variable, widget,
} from "../fwlib.mjs";

export const COLORS = [
    ["w_red", "#f80000"],
    ["w_green", "#00fc00"],
    ["w_blue", "#0000f8"],
    ["w_yellow", "#f8fc00"],
    ["w_gray", "#808080"],
    ["w_magenta", "#f800f8"],
    ["w_cyan", "#00fcf8"],
    ["w_white", "#f8fcf8"],
    ["w_black", "#000000"],
];

export const STYLES = [
    // text: yellow on gray, centered
    style("w_txt", { color: "w_yellow", backgroundColor: "w_gray" }),
    style("w_txt_left", { color: "w_yellow", backgroundColor: "w_gray", alignHorizontal: "left" }),
    // override target of the W8 tests
    style("w_txt_alt", { color: "w_green", backgroundColor: "w_blue" }),
    // opaque, rounded, no border (D4)
    style("w_rounded", { color: "w_white", backgroundColor: "w_red", borderRadius: "12" }),
    style("w_red_box", { backgroundColor: "w_red" }),
    style("w_green_box", { backgroundColor: "w_green" }),
    style("w_blue_box", { backgroundColor: "w_blue" }),
    style("w_gray_box", { backgroundColor: "w_gray" }),
    style("w_transparent", { backgroundColor: "transparent" }),
    // buttons: blue, red while pressed
    style("w_btn", {
        color: "w_white", backgroundColor: "w_blue",
        activeColor: "w_white", activeBackgroundColor: "w_red",
    }),
    style("w_btn_dis", { color: "w_white", backgroundColor: "w_gray" }),
    style("w_sel", { color: "w_white", backgroundColor: "w_green" }),
    // bar graph: green bar on blue
    style("w_bar", {
        color: "w_green", backgroundColor: "w_blue",
        activeColor: "w_green", activeBackgroundColor: "w_blue",
    }),
    style("w_bar_text", { color: "w_white", backgroundColor: "w_blue", activeColor: "w_black" }),
    style("w_line1", { color: "w_red" }),
    style("w_line2", { color: "w_yellow" }),
    // YT graph (background, grid in border color) and its trace
    style("w_ytg", { color: "w_white", backgroundColor: "w_black", borderColor: "w_gray" }),
    style("w_trace", { color: "w_red", backgroundColor: "w_black" }),
    // list graph
    style("w_lg", { backgroundColor: "w_black" }),
    style("w_lg_cursor", { backgroundColor: "w_gray" }),
    // progress: blue, green part
    style("w_prog", { backgroundColor: "w_blue", activeBackgroundColor: "w_green" }),
    // scroll bar: track gray, thumb green (their background colors, the red
    // color isn't used)
    style("w_track", { color: "w_red", backgroundColor: "w_gray" }),
    style("w_thumb", { color: "w_red", backgroundColor: "w_green" }),
    // switch, slider
    style("w_switch", {
        color: "w_white", backgroundColor: "w_gray", activeBackgroundColor: "w_green",
        borderColor: "w_gray", padding: "2",
    }),
    style("w_slider", { color: "w_white", backgroundColor: "w_gray", borderColor: "w_gray", padding: "2" }),
    // check box / radio
    style("w_check", { color: "w_white", backgroundColor: "w_black", alignHorizontal: "left" }),
    // containers, lists
    style("w_cont", { backgroundColor: "w_gray" }),
    style("w_list", { backgroundColor: "w_blue" }),
    style("w_item", { backgroundColor: "w_green" }),
    style("w_dim", { color: "w_gray", backgroundColor: "w_black" }),
    // gauge
    style("w_gauge_bar", { color: "w_green", backgroundColor: "w_black" }),
];

export const GLOBALS = [
    // the tests find the variables of this part after the marker
    variable("w_marker", "string", '"w_marker"'),
    variable("w_a", "any", "0"),
    variable("w_b", "any", "0"),
    variable("w_c", "any", "0"),
    variable("w_d", "any", "0"),
    variable("w_e", "any", "0"),
    variable("w_int", "integer", "0"),
    variable("w_int2", "integer", "0"),
    variable("w_bool", "boolean", "true"),
    variable("w_bool2", "boolean", "true"),
    variable("w_str", "string", '""'),
    variable("w_double", "double", "0"),
    variable("w_sb", "struct:$ScrollbarState",
        "{numItems: 100, itemsPerPage: 10, positionIncrement: 1, position: 0}"),
    variable("w_lc_points", "array:any", "[]"),
    variable("w_strs", "array:string", "[]"),
    // native (tests/widgets/native.cpp)
    variable("w_ell", "string", null, { native: true }),
    variable("w_ref", "string", null, { native: true }),
    variable("w_xscroll", "string", null, { native: true }),
    variable("w_counter_a", "any", null, { native: true }),
    variable("w_counter_b", "any", null, { native: true }),
    variable("w_yt", "any", null, { native: true }),
    variable("w_canvas", "any", null, { native: true }),
    variable("w_rol", "integer", null, { native: true }),
    variable("w_probe", "any", null, { native: true }),
    variable("w_counter_arr", "array:integer", null, { native: true }),
];

export const ACTIONS = ["w_on_click", "w_on_click2"].map(name => nativeAction(name));

////////////////////////////////////////////////////////////////////////////////
// helpers

function w(type, key, x, y, w_, h, style_ = "default", props = {}) {
    return widget(type, ["w", key], x, y, w_, h, style_, props);
}

// Secondary style properties: name -> {useStyle: style}.
function styles(props) {
    return Object.fromEntries(Object.entries(props).map(([name, value]) => [name, { useStyle: value }]));
}

// Timeline keyframe, props: left/top/width/height/opacity = [value, easing].
function keyframe(key, start, end, props = {}) {
    const k = { objID: oid("w", key, "keyframe", start, end), start, end };
    for (const name of ["left", "top", "width", "height", "rotate", "opacity", "scale", "scaleX", "scaleY"]) {
        if (name in props) {
            const [value, easing] = props[name];
            k[name] = { enabled: true, value, easingFunction: easing };
        } else {
            k[name] = { enabled: false, easingFunction: "linear" };
        }
    }
    k.controlPoints = "";
    return k;
}

// PNG (RGBA) with pixel(x, y) -> [r, g, b, a].
function png(width, height, pixel) {
    const raw = [];
    for (let y = 0; y < height; y++) {
        raw.push(0);
        for (let x = 0; x < width; x++) {
            raw.push(...pixel(x, y));
        }
    }

    function chunk(tag, data) {
        const length = Buffer.alloc(4);
        length.writeUInt32BE(data.length);
        const crc = Buffer.alloc(4);
        crc.writeUInt32BE(crc32(Buffer.concat([tag, data])));
        return Buffer.concat([length, tag, data, crc]);
    }

    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header.set([8, 6, 0, 0, 0], 8);
    const data = Buffer.concat([
        Buffer.from("\x89PNG\r\n\x1a\n", "latin1"),
        chunk(Buffer.from("IHDR"), header),
        chunk(Buffer.from("IDAT"), zlibCompress(Buffer.from(raw))),
        chunk(Buffer.from("IEND"), Buffer.alloc(0)),
    ]);
    return "data:image/png;base64," + data.toString("base64");
}

// 40x20: left half red, right half blue
export const BITMAPS = [{
    objID: oid("bitmap", "w_bmp"),
    name: "w_bmp",
    image: png(40, 20, (x, y) => x < 20 ? [248, 0, 0, 255] : [0, 0, 248, 255]),
    bpp: 32,
    alwaysBuild: true,
}];

////////////////////////////////////////////////////////////////////////////////
// pages

function addPage(name, components, lines = [], props = {}) {
    return page(name, components, lines, props);
}

export const PAGES = [];

// Text
PAGES.push(addPage("w_text", [
    w("TextWidget", "text/static", 10, 10, 200, 40, "w_txt", { text: "Hello" }),
    w("TextWidget", "text/data", 10, 60, 200, 40, "w_txt", { data: "w_a" }),
    w("TextWidget", "text/override", 10, 110, 200, 40, "w_txt", { text: "Hello" }),
    w("TextWidget", "text/ellipsis", 10, 160, 120, 40, "w_txt_left", { data: "w_ell" }),
    w("TextWidget", "text/ref", 10, 210, 300, 40, "w_txt_left", { data: "w_ref" }),
    w("TextWidget", "text/fit", 300, 10, 400, 40, "w_txt_left", { data: "w_b", fitToContent: "width" }),
    w("TextWidget", "text/nothing", 400, 110, 200, 40, "w_txt"),
]));

// MultilineText
PAGES.push(addPage("w_mtext", [
    w("MultilineTextWidget", "mtext/static", 10, 10, 150, 120, "w_txt_left", {
        text: "one two three four five six",
    }),
    w("MultilineTextWidget", "mtext/data", 200, 10, 150, 120, "w_txt_left", { data: "w_a" }),
    w("MultilineTextWidget", "mtext/override", 400, 10, 150, 120, "w_txt_left", { text: "one two" }),
]));

// DisplayData: the same value with each display option, a reference text,
// a rounded opaque style (D4), x scroll (W3), refresh rate (W20)
PAGES.push(addPage("w_dd", [
    w("DisplayDataWidget", "dd/all", 10, 10, 200, 40, "w_txt", { data: "w_a", displayOption: 0, refreshRate: "0" }),
    w("DisplayDataWidget", "dd/integer", 10, 60, 200, 40, "w_txt", { data: "w_a", displayOption: 1, refreshRate: "0" }),
    w("DisplayDataWidget", "dd/opt2", 10, 110, 200, 40, "w_txt", { data: "w_a", displayOption: 2, refreshRate: "0" }),
    w("DisplayDataWidget", "dd/opt3", 10, 160, 200, 40, "w_txt", { data: "w_a", displayOption: 3, refreshRate: "0" }),
    w("DisplayDataWidget", "dd/unit", 10, 210, 200, 40, "w_txt", { data: "w_a", displayOption: 4, refreshRate: "0" }),
    w("DisplayDataWidget", "dd/intfrac", 10, 260, 200, 40, "w_txt", { data: "w_a", displayOption: 5, refreshRate: "0" }),
    w("TextWidget", "dd/ref", 250, 10, 200, 40, "w_txt", { data: "w_ref" }),
    w("DisplayDataWidget", "dd/rounded", 500, 10, 200, 40, "w_rounded", { data: "w_b", displayOption: 0, refreshRate: "0" }),
    w("DisplayDataWidget", "dd/xscroll", 500, 60, 200, 40, "w_txt_left", { data: "w_xscroll", displayOption: 0, refreshRate: "0" }),
    w("DisplayDataWidget", "dd/refresh", 500, 110, 200, 40, "w_txt", { data: "w_c", displayOption: 0, refreshRate: "1000" }),
    w("DisplayDataWidget", "dd/iuf", 500, 160, 250, 40, "w_txt", {
        data: "w_d", displayOption: 6, refreshRate: "0",
        unitAndFractionFont: "RobotoCondensed",
    }),
    // a fixed unit and number of decimals (roadmap item 6): in mA with the
    // precision 0.005 mA, in the unit of the value with 0.0005, the integer and
    // fraction option with 0.0005
    w("DisplayDataWidget", "dd/fixed_ma", 500, 210, 200, 40, "w_txt", {
        data: "w_e", displayOption: 0, refreshRate: "0",
        displayUnit: '"mA"', precision: "0.005",
    }),
    w("DisplayDataWidget", "dd/fixed_a", 500, 260, 200, 40, "w_txt", {
        data: "w_e", displayOption: 0, refreshRate: "0",
        precision: "0.0005",
    }),
    w("DisplayDataWidget", "dd/fixed_intfrac", 500, 310, 200, 40, "w_txt", {
        data: "w_e", displayOption: 5,
        refreshRate: "0", precision: "0.0005",
    }),
]));

// Rectangle, Bitmap, visibility
PAGES.push(addPage("w_rect", [
    w("RectangleWidget", "rect/red", 10, 10, 100, 50, "w_red_box"),
    w("RectangleWidget", "rect/vis", 150, 10, 100, 50, "w_green_box", { visible: "w_bool" }),
    w("BitmapWidget", "rect/bitmap", 300, 10, 40, 20, "default", { bitmap: "w_bmp" }),
    // hidden widget with counted data (WP2)
    w("TextWidget", "rect/hidden", 10, 100, 100, 40, "w_txt", { data: "w_counter_a", visible: "false" }),
    // hidden container with a child with counted data
    w("ContainerWidget", "rect/hidden_cont", 150, 100, 200, 100, "w_cont", {
        visible: "false", widgets: [
            w("TextWidget", "rect/hidden_child", 10, 10, 100, 40, "w_txt", { data: "w_counter_b" }),
        ],
    }),
]));

// Button
PAGES.push(addPage("w_button", [
    w("ButtonWidget", "button/click", 10, 10, 150, 50, "w_btn", {
        data: '"Click"',
        eventHandlers: [eventHandler(["w", "button/click"], "CLICKED", "w_on_click")],
        ...styles({ disabledStyle: "w_btn_dis" }),
    }),
    w("ButtonWidget", "button/enabled", 200, 10, 150, 50, "w_btn", {
        data: '"Off"', enabled: "w_bool",
        eventHandlers: [eventHandler(["w", "button/enabled"], "CLICKED", "w_on_click2")],
        ...styles({ disabledStyle: "w_btn_dis" }),
    }),
    w("ButtonWidget", "button/override", 400, 10, 150, 50, "w_btn", {
        data: '"OvR"',
        ...styles({ disabledStyle: "w_btn_dis" }),
    }),
]));

// ToggleButton
PAGES.push(addPage("w_toggle", [
    w("ToggleButtonWidget", "toggle", 10, 10, 150, 50, "w_btn", {
        data: "w_int", text1: "OFF", text2: "ON",
        ...styles({ checkedStyle: "w_sel" }),
    }),
]));

// ButtonGroup
PAGES.push(addPage("w_bgroup", [
    w("ButtonGroupWidget", "bgroup", 10, 10, 300, 50, "w_btn", {
        data: "w_a", selectedButton: "w_int",
        ...styles({ selectedStyle: "w_sel" }),
    }),
]));

// BarGraph
PAGES.push(addPage("w_bar", [
    w("BarGraphWidget", "bar/lr", 10, 10, 200, 30, "w_bar", {
        data: "w_a", min: "0", max: "100",
        line1Data: "w_b", line2Data: "w_c", refreshRate: "0", orientation: "left-right", displayValue: false,
        ...styles({ textStyle: "w_bar_text", line1Style: "w_line1", line2Style: "w_line2" }),
    }),
    w("BarGraphWidget", "bar/bt", 10, 60, 40, 200, "w_bar", {
        data: "w_a", min: "0", max: "100",
        refreshRate: "0", orientation: "bottom-top", displayValue: false,
        ...styles({ textStyle: "w_bar_text", line1Style: "w_line1", line2Style: "w_line2" }),
    }),
    w("BarGraphWidget", "bar/text", 300, 10, 200, 30, "w_bar", {
        data: "w_a", min: "0", max: "100",
        refreshRate: "0", orientation: "left-right", displayValue: true,
        ...styles({ textStyle: "w_bar_text", line1Style: "w_line1", line2Style: "w_line2" }),
    }),
]));

// YTGraph (native data)
PAGES.push(addPage("w_yt", [
    w("YTGraphWidget", "yt", 10, 10, 400, 200, "w_ytg", {
        data: "w_yt",
        ...styles({ y1Style: "w_trace", y2Style: "w_trace" }),
    }),
]));

// YTGraph as the item of a list (widget cursor 0, not -1)
PAGES.push(addPage("w_yt_list", [
    w("ListWidget", "yt_list", 10, 10, 400, 200, "default", {
        data: "w_strs", listType: "vertical", gap: 0,
        itemWidget: w("YTGraphWidget", "yt_list/yt", 0, 0, 400, 200, "w_ytg", {
            data: "w_yt",
            ...styles({ y1Style: "w_trace", y2Style: "w_trace" }),
        }),
    }),
]));

// UpDown
PAGES.push(addPage("w_updown", [
    w("UpDownWidget", "updown", 10, 10, 200, 50, "w_txt", {
        data: "w_int", min: "0", max: "3",
        downButtonText: "<", upButtonText: ">", ...styles({ buttonsStyle: "w_btn" }),
    }),
]));

// ListGraph
PAGES.push(addPage("w_lgraph", [
    w("ListGraphWidget", "lgraph", 10, 10, 300, 150, "w_lg", {
        data: "w_a", dwellData: "w_b", y1Data: "w_c",
        y2Data: "w_d", cursorData: "w_int",
        ...styles({ y1Style: "w_line1", y2Style: "w_line2", cursorStyle: "w_lg_cursor" }),
    }),
]));

// Progress
PAGES.push(addPage("w_progress", [
    w("ProgressWidget", "progress/h", 10, 10, 200, 30, "w_prog", {
        data: "w_a", min: "0", max: "100",
        orientation: "horizontal",
    }),
    w("ProgressWidget", "progress/v", 10, 60, 30, 200, "w_prog", {
        data: "w_a", min: "0", max: "100",
        orientation: "vertical",
    }),
]));

// ScrollBar
PAGES.push(addPage("w_sbar", [
    w("ScrollBarWidget", "sbar", 10, 10, 300, 40, "w_track", {
        data: "w_sb", leftButtonText: "<",
        rightButtonText: ">", ...styles({ thumbStyle: "w_thumb", buttonsStyle: "w_btn" }),
    }),
]));

// Canvas (native data)
PAGES.push(addPage("w_canvas", [
    w("CanvasWidget", "canvas", 10, 10, 100, 100, "default", { data: "w_canvas" }),
]));

// LineChart: the points come from w_lc_points ([x, y] arrays) when the page opens
function lineChartPage(name, charts) {
    const components = [component("OnEventActionComponent", ["w", name, "on_open"], { left: 600, top: 300, event: "page_open" })];
    const lines = [];
    charts.forEach(([key, x, y, w_, h, props], i) => {
        const chart = w(
            "LineChartEmbeddedWidget", key, x, y, w_, h, "default", {
                xValue: "value[0]",
                lines: [{
                    objID: oid("w", key, "line"), label: '"A"', color: "w_red", width: 2,
                    value: "value[1]",
                }],
                title: '""', showTitle: "false", showLegend: props.showLegend ?? "false",
                showXAxis: "true", showYAxis: "true", showGrid: "true",
                yAxisRangeOption: "floating", yAxisRangeFrom: "0", yAxisRangeTo: "10",
                maxPoints: 40, marker: "null",
                margin: {
                    objID: oid("w", key, "margin"), top: props.top ?? 10, right: 10,
                    bottom: props.bottom ?? 30, left: 50,
                },
                customInputs: [{ objID: oid("w", key, "input"), name: "value", type: "array:double" }],
                ...styles({
                    titleStyle: "default", legendStyle: "w_txt_left", xAxisStyle: "default",
                    yAxisStyle: "default", markerStyle: "default",
                }),
            },
        );
        const evalKey = ["w", name, "eval", i];
        components.push(chart);
        components.push(component("EvalExprActionComponent", evalKey, {
            left: 600, top: 380 + i * 60,
            expression: "w_lc_points",
        }));
        lines.push(line(["w", name, "on_open"], "@seqout", evalKey, "@seqin"));
        lines.push(line(evalKey, "result", ["w", key], "value"));
    });
    return addPage(name, components, lines);
}

PAGES.push(lineChartPage("w_lchart", [
    ["lchart", 10, 10, 400, 200, {}],
]));
// grid height below the height of one y label: no y ticks (W5)
PAGES.push(lineChartPage("w_lchart_small", [
    ["lchart_small", 10, 10, 400, 50, { top: 10, bottom: 30 }],
]));
// legend of a chart at the top and of a chart lower on the page (W16)
PAGES.push(lineChartPage("w_lchart_legend", [
    ["lchart_legend_top", 10, 10, 400, 150, { showLegend: "true" }],
    ["lchart_legend_low", 10, 300, 400, 150, { showLegend: "true" }],
]));

// Gauge
PAGES.push(addPage("w_gauge", [
    w("GaugeEmbeddedWidget", "gauge", 10, 10, 300, 200, "default", {
        data: "w_a", min: "w_b", max: "w_c",
        threshold: "w_d", unit: "w_e",
        ...styles({ barStyle: "w_gauge_bar", valueStyle: "default", ticksStyle: "default", thresholdStyle: "w_line1" }),
    }),
]));

// Input
PAGES.push(addPage("w_input", [
    w("InputEmbeddedWidget", "input/text", 10, 10, 200, 40, "w_txt", {
        data: "w_a", inputType: "text",
        min: "0", max: "100",
    }),
    w("InputEmbeddedWidget", "input/password", 10, 60, 200, 40, "w_txt", {
        data: "w_b", inputType: "text",
        password: true, min: "0", max: "100",
    }),
    w("InputEmbeddedWidget", "input/number", 10, 110, 200, 40, "w_txt", {
        data: "w_c", inputType: "number",
        min: "0", max: "100", precision: "0.01", unit: '"V"', info: '""', defaultValue: "0",
    }),
    w("InputEmbeddedWidget", "input/password2", 10, 160, 200, 40, "w_txt", {
        data: "w_d", inputType: "text",
        password: true, min: "0", max: "100",
    }),
    // a number in A shown in mA with the decimals of the precision (0.0005 A
    // is 0.5 mA: 1 decimal), next to a reference text (a flow string: reading
    // it doesn't allocate, see the allocation test of the page)
    w("InputEmbeddedWidget", "input/shown", 10, 210, 200, 40, "w_txt", {
        data: "w_e", inputType: "number",
        min: "0", max: "10", precision: "0.0005", unit: '"A"', displayUnit: '"mA"', info: '""', defaultValue: "0",
    }),
    w("TextWidget", "input/ref", 250, 210, 200, 40, "w_txt", { data: "w_str" }),
    // a number with five decimals
    w("InputEmbeddedWidget", "input/decimals", 10, 260, 200, 40, "w_txt", {
        data: "w_e", inputType: "number",
        min: "0", max: "10", precision: "0.00001", unit: '"A"', info: '""', defaultValue: "0",
    }),
    // a number of native data (it has no min, max, ...: those of the Input)
    w("InputEmbeddedWidget", "input/native", 10, 310, 200, 40, "w_txt", {
        data: "w_rol", inputType: "number",
        min: "2", max: "7", precision: "1", unit: '""', info: '""', defaultValue: "3",
    }),
]));

// Roller: native data (counted writes) and flow data
PAGES.push(addPage("w_roller", [
    w("RollerWidget", "roller/native", 10, 10, 120, 200, "w_dim", {
        data: "w_rol", min: "0", max: "9",
        text: '"Item " + w_rol', ...styles({ selectedValueStyle: "w_txt", unselectedValueStyle: "w_dim" }),
    }),
    w("RollerWidget", "roller/flow", 200, 10, 120, 200, "w_dim", {
        data: "w_int", min: "0", max: "9",
        text: '"Item " + w_int', ...styles({ selectedValueStyle: "w_txt", unselectedValueStyle: "w_dim" }),
    }),
]));

// Switch, Slider
PAGES.push(addPage("w_switch", [
    w("SwitchWidget", "switch", 10, 10, 100, 40, "w_switch", { data: "w_int" }),
    w("SliderWidget", "slider", 10, 100, 300, 40, "w_slider", { data: "w_double", min: "w_b", max: "w_c" }),
]));

// DropDownList
PAGES.push(addPage("w_ddl", [
    w("DropDownListWidget", "ddl", 10, 10, 200, 40, "w_txt", { data: "w_int", options: "w_a" }),
]));

// QRCode
PAGES.push(addPage("w_qr", [
    w("QRCodeWidget", "qr", 10, 10, 200, 200, "default", { data: "w_a", errorCorrection: "low" }),
]));

// Checkbox, Radio
PAGES.push(addPage("w_check", [
    w("CheckboxWidget", "check", 10, 10, 200, 40, "w_check", { data: "w_bool", text: '"Check"' }),
    w("CheckboxWidget", "check/override", 10, 60, 200, 40, "w_check", { data: "w_bool2", text: '"Other"' }),
    w("RadioWidget", "radio/0", 300, 10, 200, 40, "w_check", { data: "w_int", value: "0", text: '"Zero"' }),
    w("RadioWidget", "radio/1", 300, 60, 200, 40, "w_check", { data: "w_int", value: "1", text: '"One"' }),
    w("RadioWidget", "radio/s", 300, 110, 200, 40, "w_check", { data: "w_str", value: '"b"', text: '"B"' }),
]));

// Containers: static, horizontal, vertical
PAGES.push(addPage("w_cont", [
    w("ContainerWidget", "cont/static", 10, 10, 300, 100, "w_cont", {
        visible: "w_bool", widgets: [
            w("TextWidget", "cont/static/text", 10, 10, 100, 40, "w_txt", { text: "In" }),
        ],
    }),
    w("ContainerWidget", "cont/h", 10, 150, 300, 50, "w_cont", {
        layout: "horizontal", widgets: [
            w("RectangleWidget", "cont/h/a", 0, 0, 50, 50, "w_red_box", { visible: "w_bool2" }),
            w("RectangleWidget", "cont/h/b", 50, 0, 50, 50, "w_green_box"),
        ],
    }),
    w("ContainerWidget", "cont/v", 400, 150, 100, 200, "w_cont", {
        layout: "vertical", widgets: [
            w("RectangleWidget", "cont/v/a", 0, 0, 100, 50, "w_red_box", { visible: "w_bool2" }),
            w("RectangleWidget", "cont/v/b", 0, 50, 100, 50, "w_green_box"),
        ],
    }),
]));

// Horizontal layout with timelines (W19): the container grows to 400 px, the
// child of the second container grows to 100 px (timeline position 1 s)
PAGES.push(addPage("w_cont_tl", [
    w("ContainerWidget", "conttl/grow", 10, 10, 300, 50, "w_cont", {
        layout: "horizontal",
        timeline: [keyframe("conttl/grow", 0, 1, { width: [400, "linear"] })],
        widgets: [
            w("RectangleWidget", "conttl/grow/a", 0, 0, 200, 50, "w_red_box", { visible: "w_bool2" }),
            w("RectangleWidget", "conttl/grow/b", 200, 0, 150, 50, "w_green_box", { allowOutside: true }),
        ],
    }),
    w("ContainerWidget", "conttl/child", 10, 100, 300, 50, "w_cont", {
        layout: "horizontal", widgets: [
            w("RectangleWidget", "conttl/child/a", 0, 0, 50, 50, "w_red_box", {
                timeline: [keyframe("conttl/child/a", 0, 1, { width: [100, "linear"] })],
            }),
            w("RectangleWidget", "conttl/child/b", 50, 0, 50, 50, "w_green_box"),
        ],
    }),
]));

// List, Grid
PAGES.push(addPage("w_list", [
    w("ListWidget", "list/v", 10, 10, 200, 100, "w_list", {
        data: "w_a", listType: "vertical", gap: 5,
        itemWidget: w("RectangleWidget", "list/v/item", 0, 0, 200, 50, "w_item"),
    }),
    w("ListWidget", "list/h", 300, 10, 300, 50, "w_list", {
        data: "w_strs", listType: "horizontal", gap: 0,
        itemWidget: w("TextWidget", "list/h/item", 0, 0, 60, 50, "w_txt", { data: "w_strs[$index]" }),
    }),
    w("GridWidget", "grid", 10, 150, 200, 100, "default", {
        data: "w_c", gridFlow: "row",
        itemWidget: w("RectangleWidget", "grid/item", 0, 0, 100, 50, "w_item"),
    }),
]));

// List with an expression as data (evaluations counted by the native variable, WP2)
PAGES.push(addPage("w_list_count", [
    w("ListWidget", "list_count", 10, 10, 200, 100, "w_list", {
        data: "(w_counter_arr)", listType: "vertical",
        gap: 0, itemWidget: w("RectangleWidget", "list_count/item", 0, 0, 200, 50, "w_item"),
    }),
]));

// more widget states than fit into the GUI state buffer (128 KB)
PAGES.push(addPage("w_many", [
    w("GridWidget", "many", 0, 0, 800, 480, "default", {
        data: "w_a", gridFlow: "row",
        itemWidget: w("RectangleWidget", "many/item", 0, 0, 10, 10, "w_item"),
    }),
]));

// a widget without its style key
function withoutStyle(widget_) {
    const { style: _, ...rest } = widget_;
    return rest;
}

// Select
PAGES.push(addPage("w_select", [
    w("SelectWidget", "select", 10, 10, 200, 50, "default", {
        data: "w_int", widgets: [
            w("RectangleWidget", "select/0", 0, 0, 200, 50, "w_red_box"),
            w("RectangleWidget", "select/1", 0, 0, 200, 50, "w_green_box"),
            w("RectangleWidget", "select/2", 0, 0, 200, 50, "w_blue_box"),
        ],
    }),
    // a Select with a Spacer as its second child, on a blue container: the
    // Spacer has no style (a widget made without one)
    w("ContainerWidget", "spacer/cont", 10, 100, 200, 50, "w_blue_box", {
        widgets: [
            w("SelectWidget", "spacer/select", 0, 0, 200, 50, "w_transparent", {
                data: "w_int2", widgets: [
                    w("RectangleWidget", "spacer/0", 0, 0, 200, 50, "w_red_box"),
                    withoutStyle(w("SpacerWidget", "spacer/1", 0, 0, 200, 50)),
                ],
            }),
        ],
    }),
    // a Spacer with Grow between two boxes of a row
    w("ContainerWidget", "spacer/row", 10, 200, 300, 50, "w_blue_box", {
        layout: "horizontal", widgets: [
            w("RectangleWidget", "spacer/row/a", 0, 0, 50, 50, "w_red_box"),
            withoutStyle(w("SpacerWidget", "spacer/row/s", 0, 0, 10, 50, "default", { flexGrow: true })),
            w("RectangleWidget", "spacer/row/b", 0, 0, 50, 50, "w_green_box"),
        ],
    }),
]));

// user widgets
const UW_LABEL_ID = oid("w", "uw_text", "label", "id");

export const USER_WIDGETS = [
    page("w_uw_button", [
        w("ButtonWidget", "uw_button/button", 0, 0, 150, 50, "w_btn", {
            data: '"B"',
            eventHandlers: [eventHandler(["w", "uw_button/button"], "CLICKED", "w_on_click")],
            ...styles({ disabledStyle: "w_btn_dis" }),
        }),
    ], [], { width: 150, height: 50, isUsedAsUserWidget: true }),
    page("w_uw_text", [
        w("TextWidget", "uw_text/text", 0, 0, 150, 50, "w_txt", { data: "label" }),
    ], [], {
        width: 150, height: 50, isUsedAsUserWidget: true, userProperties: [{
            objID: oid("w", "uw_text", "label"), id: UW_LABEL_ID, name: "label", type: "string",
            assignable: false, displayName: "Label", description: "",
        }],
    }),
];

function userWidget(key, x, y, w_, h, name, values = null) {
    const props = { userWidgetPageName: name };
    if (values !== null) {
        props.userPropertyValues = { objID: oid("w", key, "values"), values };
    }
    return w("UserWidgetWidget", key, x, y, w_, h, "default", props);
}

PAGES.push(addPage("w_uw", [
    userWidget("uw/b0", 10, 10, 150, 50, "w_uw_button"),
    userWidget("uw/b1", 200, 10, 150, 50, "w_uw_button"),
    userWidget("uw/t0", 10, 100, 150, 50, "w_uw_text", { [UW_LABEL_ID]: '"One"' }),
    userWidget("uw/t1", 200, 100, 150, 50, "w_uw_text", { [UW_LABEL_ID]: '"Two"' }),
    // data of this text is read after the user widgets (W9)
    w("TextWidget", "uw/probe", 400, 100, 150, 50, "w_txt", { data: "w_probe" }),
]));

// overlapping children: text under a rectangle
PAGES.push(addPage("w_overlap", [
    w("TextWidget", "overlap/text", 10, 10, 150, 60, "w_txt", { data: "w_a" }),
    w("RectangleWidget", "overlap/rect", 60, 40, 150, 60, "w_blue_box"),
]));

// timelines: position (left 10 -> 210), opacity to 0 with easeOutBack (W18)
PAGES.push(addPage("w_timeline", [
    w("RectangleWidget", "timeline/move", 10, 10, 50, 50, "w_red_box", {
        timeline: [keyframe("timeline/move", 0, 1, { left: [210, "linear"] })],
    }),
    w("RectangleWidget", "timeline/fade", 10, 100, 50, 50, "w_green_box", {
        timeline: [keyframe("timeline/fade", 0, 1, { opacity: [0, "easeOutBack"] })],
    }),
]));

// a widget whose type the test changes (W17)
PAGES.push(addPage("w_badtype", [
    w("RectangleWidget", "badtype/rect", 10, 10, 100, 50, "w_red_box"),
]));
