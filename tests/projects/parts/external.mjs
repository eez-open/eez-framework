/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// External part: the items of the main project that the external assets tests
// use (tests/flow/external). The external project is projects/fwtest_ext
// (make_fwtest_ext.mjs), a module project with this project as its master
// project, like a BB3+ module project with the BB3+ project.
//
// - ext_m_box, ext_m_text: master styles used by the external project. A module
//   project can use only the master styles that have an id, and the id is the
//   index of the style in the main assets, so these two take the ids 1 and 2
//   (every other style of the main project comes after them).
// - ext_host: a main page with a dynamic user widget (data ext_layout) that shows
//   an external layout, like the BB3+ slot views show the module layouts.
// - ext_uw_main: a main user widget that external pages show through a dynamic
//   user widget.
//
// The probe components (SetVariable, entry 0 "probe:<name>", entry 1 the
// expression) hang on the page_close event: the tests evaluate their expressions
// in the flow state of the page or user widget, or assign to the variable of
// entry 1 (flow/external/ext.cpp).

import { component, line, oid, page, rectangle, style, variable, widget } from "../fwlib.mjs";

export const STYLES = [
    style("ext_m_box", { id: 1, backgroundColor: "accent" }),
    style("ext_m_text", { id: 2, color: "bookmark", alignHorizontal: "left" }),
];

export const GLOBALS = [
    // layout id of the dynamic user widget of ext_host (an external page id)
    variable("ext_layout", "integer", "0"),
    // the Start component of ext_uw_main increments it
    variable("ext_uw_started", "integer", "0"),
];

// Probe components: SetVariable components on the page_close event. A case
// is [name, expression] or [name, expression, variable of entry 1].
function probes(prefix, cases, left = 600) {
    const eventKey = [prefix, "probe_event"];
    const components = [component("OnEventActionComponent", eventKey, { left, top: 0, event: "page_close" })];
    const lines = [];
    cases.forEach((case_, i) => {
        const [name, expression] = case_;
        const target = case_.length > 2 ? case_[2] : "probe_result";
        const key = [prefix, "probe", name];
        components.push(component("SetVariableActionComponent", key, {
            left: left + 200, top: i * 80, width: 300, height: 70,
            entries: [
                { objID: oid(...key, "label"), variable: "probe_label", value: `"probe:${name}"` },
                { objID: oid(...key, "value"), variable: target, value: expression },
            ],
        }));
        lines.push(line(eventKey, "@seqout", key, "@seqin"));
    });
    const localVariables = ["probe_label", "probe_result"].map(name => variable(name, "any", null, { key: [prefix, "probe_var"] }));
    return [components, lines, localVariables];
}

function userWidget(key, left, top, width, height, style_ = "default", data = null, pageName = "") {
    const props = {
        userWidgetPageName: pageName,
        userPropertyValues: { objID: oid(key, "values"), values: {} },
    };
    if (data) {
        props.data = data;
    }
    return widget("UserWidgetWidget", key, left, top, width, height, style_, props);
}

function extHost() {
    const [probeComponents, probeLines, probeLocals] = probes("ext_host", [
        ["active", "Flow.isPageActive()"],
        // the tests set ext_layout through this one
        ["layout", "ext_layout", "ext_layout"],
        ["uw_started", "ext_uw_started"],
    ]);
    return page(
        "ext_host",
        [
            userWidget("ext_host/uw", 0, 0, 200, 100, "default", "ext_layout"),
            rectangle("ext_host/after", 300, 0, 100, 50, "ext_m_box"),
            ...probeComponents,
        ],
        probeLines,
        { localVariables: probeLocals },
    );
}

function extUwMain() {
    const [probeComponents, probeLines, probeLocals] = probes("ext_uw_main", [["active", "Flow.isPageActive()"]]);
    const start = ["ext_uw_main", "start"];
    const inc = ["ext_uw_main", "inc"];
    return page(
        "ext_uw_main",
        [
            rectangle("ext_uw_main/box", 0, 0, 100, 50, "ext_m_box"),
            component("StartActionComponent", start, { left: 300, top: 0, width: 100, height: 40 }),
            component("SetVariableActionComponent", inc, {
                left: 300, top: 60, width: 300, height: 70,
                entries: [{ objID: oid(...inc, "entry"), variable: "ext_uw_started", value: "ext_uw_started + 1" }],
            }),
            ...probeComponents,
        ],
        [line(start, "@seqout", inc, "@seqin"), ...probeLines],
        { localVariables: probeLocals, width: 200, height: 100, isUsedAsUserWidget: true },
    );
}

export const PAGES = [extHost()];

export const USER_WIDGETS = [extUwMain()];
