#!/usr/bin/env node
/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Generates the external test project of the eez-framework tests.
//
//     node make_fwtest_ext.mjs
//
// writes fwtest_ext/fwtest_ext.eez-project: a module project with fwtest as its
// master project, like a BB3+ module project (settings.general.masterProject) with
// the BB3+ project. Its own pages, styles, fonts, bitmaps and colors get negative
// indexes in the assets (styles, fonts, bitmaps -1, -2, ...; colors 65534 - k;
// font tags U+E002), the master styles it uses keep their ids. It has no
// bb3ModuleMetadata, so Studio compresses the assets with LZ4 (not LZMA) and the
// framework's decompressAssetsData can load them.
//
// Build it with EEZ Studio afterwards (build_projects.mjs, after fwtest), which
// writes fwtest_ext/generated/ext_assets.h and ext_assets.cpp: the assets array
// and the enums in the namespace fwtest_ext, so they link next to the main
// assets. The tests (tests/flow/external) load them like the BB3+ loads module
// assets.
//
// Names: the project items start with mod_, the master items the project uses
// start with ext_ (parts/external.mjs). The tags of the project items (fwlib tags)
// have the area "external", like the items of parts/external.mjs.

import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";

import { crc32, zlibCompress } from "./deflate.mjs";
import {
    DISPLAY_HEIGHT, DISPLAY_WIDTH, HERE, component, eventHandler, flowAction, line, loadFont, oid,
    page, rectangle, style, tagItems, tags, text, variable, widget,
} from "./fwlib.mjs";

// the area tag of the project items
const AREA = "external";

////////////////////////////////////////////////////////////////////////////////
// build files

const EXT_ASSETS_H = `#pragma once

#include <stdint.h>

namespace fwtest_ext {

//\${eez-studio GUI_PAGES_ENUM}

//\${eez-studio GUI_STYLES_ENUM}

//\${eez-studio GUI_FONTS_ENUM}

//\${eez-studio GUI_BITMAPS_ENUM}

//\${eez-studio FLOW_GLOBAL_VARIABLES_ENUM}

//\${eez-studio GUI_ASSETS_DECL_COMPRESSED}

} // namespace fwtest_ext
`;

const EXT_ASSETS_CPP = `#include "ext_assets.h"

namespace fwtest_ext {

//\${eez-studio GUI_ASSETS_DEF_COMPRESSED}

} // namespace fwtest_ext
`;

////////////////////////////////////////////////////////////////////////////////
// colors: [name, value for the master theme "Default"], 32-bit colors

const COLORS = [
    ["mod_fill", "#40c080"],
    ["mod_line", "#c04000"],
    // with alpha 128
    ["mod_fill_a128", "#40c08080"],
];

// a color that is not a project color: a custom color of the external assets
const CUSTOM_COLOR = "#8040c0";

// the color of the bitmap
const BITMAP_COLOR = [0x80, 0xC0, 0x40];

const STYLES = [
    style("mod_fill_box", { backgroundColor: "mod_fill" }),
    style("mod_line_box", { backgroundColor: "mod_line" }),
    style("mod_custom_box", { backgroundColor: CUSTOM_COLOR }),
    style("mod_alpha_box", { backgroundColor: "mod_fill_a128" }),
    style("mod_master_color_box", { backgroundColor: "bookmark" }),
    style("mod_icons_text", { font: "mod_icons" }),
    // the style of the dynamic user widget of mod_host
    style("mod_uw_frame", { backgroundColor: "mod_line" }),
    // a module style with variant styles for variants of the master project
    // (variants_warn, variants_volt of parts/variants.mjs)
    style("mod_variant_box", { backgroundColor: "mod_fill" }),
    variantStyle("mod_variant_box__warn", "mod_variant_box", "variants_warn", { backgroundColor: "mod_line" }),
    variantStyle("mod_variant_box__volt", "mod_variant_box", "variants_volt", { backgroundColor: CUSTOM_COLOR }),
    // the variant styles for the variant of the module (MODULE_VARIANTS): of a
    // module style and of a master style (ext_m_var_box of parts/external.mjs)
    variantStyle("mod_variant_box__mod_var", "mod_variant_box", "mod_var", { backgroundColor: "bookmark" }),
    variantStyle("ext_m_var_box__mod_var", "ext_m_var_box", "mod_var", { backgroundColor: "mod_line" }),
];

// the variants the module adds to the variants of the master project (ids
// from 128)
const MODULE_VARIANTS = [[128, "mod_var"]];

// A variant style: the properties of the base style in a variant.
function variantStyle(name, base, variant, props) {
    const s = style(name, { useStyle: base, variant, ...props });
    delete s.alwaysBuild;
    return s;
}

const GLOBALS = [
    variable("mod_counter", "integer", "0"),
    variable("mod_opened", "integer", "0"),
    variable("mod_main_layout", "integer", "0"),
    variable("mod_uw_started", "integer", "0"),
    variable("mod_trigger", "integer", "0"),
    variable("mod_watch_hits", "integer", "0"),
    variable("mod_caught", "string", "\"\""),
];

////////////////////////////////////////////////////////////////////////////////
// fonts and bitmaps

// A copy of obj with new objIDs (the copied fonts must not share them).
function rekey(obj, prefix) {
    obj = structuredClone(obj);

    function walk(o) {
        if (Array.isArray(o)) {
            for (const v of o) {
                walk(v);
            }
        } else if (o !== null && typeof o === "object") {
            if ("objID" in o) {
                o.objID = oid(prefix, o.objID);
            }
            for (const v of Object.values(o)) {
                walk(v);
            }
        }
    }

    walk(obj);
    return obj;
}

function font(name, source) {
    const f = rekey(loadFont(source), ["font", name]);
    f.name = name;
    f.alwaysBuild = true;
    return f;
}

// A solid color RGB PNG as a data URL.
function png(width, height, rgb) {
    const row = Buffer.concat([Buffer.from([0]), ...Array(width).fill(Buffer.from(rgb))]);
    const raw = Buffer.concat(Array(height).fill(row));

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
    header.set([8, 2, 0, 0, 0], 8);
    const data = Buffer.concat([
        Buffer.from("\x89PNG\r\n\x1a\n", "latin1"),
        chunk(Buffer.from("IHDR"), header),
        chunk(Buffer.from("IDAT"), zlibCompress(raw)),
        chunk(Buffer.from("IEND"), Buffer.alloc(0)),
    ]);
    return "data:image/png;base64," + data.toString("base64");
}

const BITMAPS = [
    {
        objID: oid("bitmap", "mod_bitmap"), name: "mod_bitmap",
        image: png(16, 16, BITMAP_COLOR), bpp: 16, alwaysBuild: true,
    },
];

////////////////////////////////////////////////////////////////////////////////
// actions

function actionInc() {
    const start = ["mod_inc", "start"];
    const inc = ["mod_inc", "set"];
    return flowAction("mod_inc", [
        component("StartActionComponent", start, { width: 100, height: 40 }),
        component("SetVariableActionComponent", inc, {
            left: 200, width: 300, height: 70,
            entries: [{ objID: oid(...inc, "entry"), variable: "mod_counter", value: "mod_counter + 1" }],
        }),
    ], [line(start, "@seqout", inc, "@seqin")]);
}

function actionFail() {
    const start = ["mod_fail", "start"];
    const error = ["mod_fail", "error"];
    return flowAction("mod_fail", [
        component("StartActionComponent", start, { width: 100, height: 40 }),
        component("ErrorActionComponent", error, { left: 200, width: 200, height: 50, message: "\"mod failure\"" }),
    ], [line(start, "@seqout", error, "@seqin")]);
}

const ACTIONS = [actionInc(), actionFail()];

////////////////////////////////////////////////////////////////////////////////
// pages

// Probe components: SetVariable components on the page_close event, see
// parts/external.mjs.
function probes(prefix, cases, left = 600) {
    const eventKey = [prefix, "probe_event"];
    const components = [component("OnEventActionComponent", eventKey, { left, top: 0, event: "page_close" })];
    const lines = [];
    cases.forEach(([name, expression], i) => {
        const key = [prefix, "probe", name];
        components.push(component("SetVariableActionComponent", key, {
            left: left + 200, top: i * 80, width: 300, height: 70,
            entries: [
                { objID: oid(...key, "label"), variable: "probe_label", value: `"probe:${name}"` },
                { objID: oid(...key, "value"), variable: "probe_result", value: expression },
            ],
        }));
        lines.push(line(eventKey, "@seqout", key, "@seqin"));
    });
    const localVariables = ["probe_label", "probe_result"].map(name => variable(name, "any", null, { key: [prefix, "probe_var"] }));
    return [components, lines, localVariables];
}

// OnEvent -> SetVariable variableName = value
function onEventSet(prefix, event, variableName, value) {
    const eventKey = [prefix, event];
    const setKey = [prefix, event, "set"];
    return [[
        component("OnEventActionComponent", eventKey, { left: 600, top: 400, event }),
        component("SetVariableActionComponent", setKey, {
            left: 800, top: 400, width: 300, height: 70,
            entries: [{ objID: oid(...setKey, "entry"), variable: variableName, value }],
        }),
    ], [line(eventKey, "@seqout", setKey, "@seqin")]];
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

function button(key, left, top, width, height, label, action) {
    return widget("ButtonWidget", key, left, top, width, height, "default", {
        text: label,
        eventHandlers: [eventHandler(key, "CLICKED", action)],
    });
}

function modHost() {
    // the first page (index 0): its page id 1 is also PAGE_ID_MAIN of the
    // main assets
    const [probeComponents, probeLines, probeLocals] = probes("mod_host", [
        ["active", "Flow.isPageActive()"],
    ]);
    return page(
        "mod_host",
        [
            // siblings before and after a user widget with a main layout
            rectangle("mod_host/before", 300, 100, 100, 50, "mod_fill_box"),
            userWidget("mod_host/uw", 0, 0, 200, 100, "mod_uw_frame", "mod_main_layout"),
            rectangle("mod_host/after", 300, 0, 100, 50, "mod_fill_box"),
            ...probeComponents,
        ],
        probeLines,
        { localVariables: probeLocals },
    );
}

function modMain() {
    const [probeComponents, probeLines, probeLocals] = probes("mod_main", [
        ["active", "Flow.isPageActive()"],
        ["bitmap", "Flow.getBitmapIndex(\"mod_bitmap\")"],
        ["local", "mod_local"],
        ["counter", "mod_counter"],
    ]);
    const [openComponents, openLines] = onEventSet("mod_main", "page_open", "mod_opened", "mod_opened + 1");
    return page(
        "mod_main",
        [
            rectangle("mod_main/fill", 10, 10, 100, 50, "mod_fill_box"),
            rectangle("mod_main/master_style", 120, 10, 100, 50, "ext_m_box"),
            rectangle("mod_main/custom", 230, 10, 100, 50, "mod_custom_box"),
            rectangle("mod_main/master_color", 340, 10, 100, 50, "mod_master_color_box"),
            widget("BitmapWidget", "mod_main/bitmap", 450, 10, 16, 16, "default", { bitmap: "mod_bitmap" }),
            // U+00C3 is a glyph of mod_icons (Icons_WaveformS), not of mod_roboto
            text("mod_main/icons", 10, 80, 200, 40, "Ã", "mod_icons_text"),
            widget("TextWidget", "mod_main/tag", 220, 80, 200, 40, "default", {
                data: "\"<font name='mod_icons'>Ã</font>\"",
            }),
            text("mod_main/no_glyph", 430, 80, 200, 40, "Ã"),
            button("mod_main/inc", 10, 150, 150, 50, "inc", "mod_inc"),
            button("mod_main/fail", 170, 150, 150, 50, "fail", "mod_fail"),
            ...openComponents,
            ...probeComponents,
        ],
        [...openLines, ...probeLines],
        { localVariables: [variable("mod_local", "integer", "7", { key: ["mod_main", "var"] }), ...probeLocals] },
    );
}

function modCatch() {
    const catch_ = ["mod_catch", "catch"];
    const setKey = ["mod_catch", "set"];
    return page(
        "mod_catch",
        [
            button("mod_catch/fail", 170, 150, 150, 50, "fail", "mod_fail"),
            component("CatchErrorActionComponent", catch_, { left: 600, top: 0, width: 150, height: 50 }),
            component("SetVariableActionComponent", setKey, {
                left: 800, top: 0, width: 300, height: 70,
                customInputs: [{ objID: oid(...setKey, "msg"), name: "msg", type: "string" }],
                entries: [{ objID: oid(...setKey, "entry"), variable: "mod_caught", value: "msg" }],
            }),
        ],
        [line(catch_, "Message", setKey, "msg")],
    );
}

function modEnd() {
    const eventKey = ["mod_end", "page_open"];
    const endKey = ["mod_end", "end"];
    return page(
        "mod_end",
        [
            component("OnEventActionComponent", eventKey, { left: 600, top: 0, event: "page_open" }),
            component("EndActionComponent", endKey, { left: 800, top: 0, width: 100, height: 40 }),
        ],
        [line(eventKey, "@seqout", endKey, "@seqin")],
    );
}

function modErrPage() {
    const eventKey = ["mod_err_page", "page_open"];
    const error = ["mod_err_page", "error"];
    return page(
        "mod_err_page",
        [
            component("OnEventActionComponent", eventKey, { left: 600, top: 0, event: "page_open" }),
            component("ErrorActionComponent", error, { left: 800, top: 0, width: 200, height: 50, message: "\"page failure\"" }),
        ],
        [line(eventKey, "@seqout", error, "@seqin")],
    );
}

function modUw() {
    const [probeComponents, probeLines, probeLocals] = probes("mod_uw", [["active", "Flow.isPageActive()"]]);
    const start = ["mod_uw", "start"];
    const started = ["mod_uw", "started"];
    const watch = ["mod_uw", "watch"];
    const hits = ["mod_uw", "hits"];
    return page(
        "mod_uw",
        [
            rectangle("mod_uw/box", 0, 0, 100, 50, "mod_fill_box"),
            button("mod_uw/inc", 100, 50, 100, 50, "inc", "mod_inc"),
            component("StartActionComponent", start, { left: 300, top: 0, width: 100, height: 40 }),
            component("SetVariableActionComponent", started, {
                left: 450, top: 0, width: 300, height: 70,
                entries: [{ objID: oid(...started, "entry"), variable: "mod_uw_started", value: "mod_uw_started + 1" }],
            }),
            // the watched expression fails for mod_trigger > 3 (array index)
            component("WatchVariableActionComponent", watch, {
                left: 300, top: 100, width: 150, height: 50,
                variable: "[0, 1, 2, 3][mod_trigger]",
            }),
            component("SetVariableActionComponent", hits, {
                left: 450, top: 100, width: 300, height: 70,
                entries: [{ objID: oid(...hits, "entry"), variable: "mod_watch_hits", value: "mod_watch_hits + 1" }],
            }),
            ...probeComponents,
        ],
        [line(start, "@seqout", started, "@seqin"), line(watch, "variable", hits, "@seqin"), ...probeLines],
        { localVariables: probeLocals, width: 200, height: 100, isUsedAsUserWidget: true },
    );
}

function modVariants() {
    return page("mod_variants", [
        rectangle("mod_variants/base", 10, 10, 100, 50, "mod_variant_box"),
        rectangle("mod_variants/warn", 120, 10, 100, 50, "mod_variant_box", { variant: "variants_warn" }),
        rectangle("mod_variants/volt", 230, 10, 100, 50, "mod_variant_box", { variant: "variants_volt" }),
        // the style has no variant style for this variant
        rectangle("mod_variants/unused", 340, 10, 100, 50, "mod_variant_box", { variant: "variants_unused" }),
        // the children of a container with a variant
        // the variant of the module: a module style, a master style the module
        // adds a variant style to, the same master style without a variant
        rectangle("mod_variants/module_var", 450, 10, 100, 50, "mod_variant_box", { variant: "mod_var" }),
        rectangle("mod_variants/master_var", 560, 10, 100, 50, "ext_m_var_box", { variant: "mod_var" }),
        rectangle("mod_variants/master_plain", 670, 10, 100, 50, "ext_m_var_box"),
        // the children of a container with the variant of the module
        widget("ContainerWidget", "mod_variants/container_mod", 10, 160, 450, 70, "default", {
            variant: "mod_var",
            widgets: [
                rectangle("mod_variants/mod_inherit", 0, 10, 100, 50, "mod_variant_box"),
                rectangle("mod_variants/mod_master", 110, 10, 100, 50, "ext_m_var_box"),
                // a master variant inside: the master variant styles of the copy
                rectangle("mod_variants/mod_warn", 220, 10, 100, 50, "mod_variant_box", { variant: "variants_warn" }),
            ],
        }),
        widget("ContainerWidget", "mod_variants/container", 10, 80, 450, 70, "default", {
            variant: "variants_volt",
            widgets: [
                rectangle("mod_variants/inherit", 0, 10, 100, 50, "mod_variant_box"),
                rectangle("mod_variants/none", 110, 10, 100, 50, "mod_variant_box", { variant: "none" }),
                // a master style without a variant style for the variant
                rectangle("mod_variants/master", 220, 10, 100, 50, "ext_m_box"),
            ],
        }),
    ]);
}

const PAGES = [modHost(), modMain(), modCatch(), modEnd(), modErrPage(), modVariants()];

const USER_WIDGETS = [modUw()];

////////////////////////////////////////////////////////////////////////////////

function project() {
    return {
        objID: oid("ext", "project"),
        settings: {
            objID: oid("ext", "settings"),
            general: {
                objID: oid("ext", "settings", "general"),
                projectVersion: "v3",
                projectType: "firmware",
                masterProject: "..\\fwtest\\fwtest.eez-project",
                flowSupport: true,
                displayWidth: DISPLAY_WIDTH,
                displayHeight: DISPLAY_HEIGHT,
                colorBpp: "32",
                bitmapColorFormat: "RGB",
            },
            eezGui: {
                objID: oid("ext", "settings", "eezGui"),
                styleVariants: MODULE_VARIANTS.map(([id, name]) => ({ objID: oid("ext", "style variant", name), id, name })),
            },
            build: {
                objID: oid("ext", "settings", "build"),
                configurations: [{ objID: oid("ext", "configuration", "default"), name: "default" }],
                files: [
                    { objID: oid("ext", "file", "ext_assets.h"), fileName: "ext_assets.h", template: EXT_ASSETS_H },
                    { objID: oid("ext", "file", "ext_assets.cpp"), fileName: "ext_assets.cpp", template: EXT_ASSETS_CPP },
                ],
                destinationFolder: "generated",
            },
        },
        variables: { globalVariables: tagItems(GLOBALS, AREA), structures: [], enums: [] },
        actions: tagItems(ACTIONS, AREA),
        userPages: tagItems(PAGES, AREA),
        userWidgets: tagItems(USER_WIDGETS, AREA),
        // new objIDs: the master project has a "default" style too
        styles: tagItems([rekey(style("default", {
            font: "mod_roboto",
            color: "text",
            backgroundColor: "background",
            alignHorizontal: "center",
            alignVertical: "center",
            children: STYLES,
        }), ["ext", "style"])], AREA),
        fonts: tagItems([font("mod_roboto", "RobotoCondensed"), font("mod_icons", "Icons_WaveformS")], AREA),
        bitmaps: tagItems(BITMAPS, AREA),
        // module colors: a value for each master theme (by name)
        colors: COLORS.map(([name, value]) => (
            { objID: oid("ext", "color", name), name, tags: tags(name, AREA), themeValues: { Default: value } }
        )),
        themes: [],
    };
}

function main() {
    const path = join(HERE, "fwtest_ext", "fwtest_ext.eez-project");
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, JSON.stringify(project(), null, 2) + "\n", "utf8");
    console.log("wrote", relative(HERE, path));
}

main();
