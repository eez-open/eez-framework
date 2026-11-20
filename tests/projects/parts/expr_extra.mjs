/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Expressions part, extra items: the globals, structures, enums, texts and the
// bitmap the expression cases use (expression_cases.mjs), the native variables
// the expression tests implement (expressions/expressions_native.cpp), and the
// pages of the expression tests that run a flow (expressions/expressions_flow_test.cpp).
//
// The globals g_* of base.mjs are used as they are.

import { crc32, zlibCompress } from "../deflate.mjs";

import { component, line, oid, page, variable } from "../fwlib.mjs";

////////////////////////////////////////////////////////////////////////////////
// structures and enums

export const STRUCTURES = [
    {
        objID: oid("struct", "expr_Point"),
        name: "expr_Point",
        fields: [
            { objID: oid("struct", "expr_Point", "x"), name: "x", type: "integer" },
            { objID: oid("struct", "expr_Point", "y"), name: "y", type: "integer" },
        ],
    },
    {
        objID: oid("struct", "expr_Rect"),
        name: "expr_Rect",
        fields: [
            { objID: oid("struct", "expr_Rect", "name"), name: "name", type: "string" },
            { objID: oid("struct", "expr_Rect", "p"), name: "p", type: "struct:expr_Point" },
            { objID: oid("struct", "expr_Rect", "tags"), name: "tags", type: "array:string" },
            { objID: oid("struct", "expr_Rect", "w"), name: "w", type: "double" },
        ],
    },
];

export const ENUMS = [
    {
        objID: oid("enum", "expr_Mode"),
        name: "expr_Mode",
        members: [
            { objID: oid("enum", "expr_Mode", "Off"), name: "Off", value: 0 },
            { objID: oid("enum", "expr_Mode", "On"), name: "On", value: 1 },
            { objID: oid("enum", "expr_Mode", "Auto"), name: "Auto", value: 5 },
            // a negative int32 constant (unary minus computes its value)
            { objID: oid("enum", "expr_Mode", "Neg"), name: "Neg", value: -7 },
        ],
    },
];

////////////////////////////////////////////////////////////////////////////////
// globals

export const GLOBALS = [
    variable("expr_float", "float", "2.5"),
    variable("expr_str_array", "array:string", "[\"a\", \"b\", \"c\"]"),
    variable("expr_dbl_array", "array:double", "[0.5, 1.5]"),
    variable("expr_empty_array", "array:integer", "[]"),
    variable("expr_matrix", "array:array:integer", "[[1, 2], [3, 4]]"),
    variable("expr_point", "struct:expr_Point", "{x: 1, y: 2}"),
    variable("expr_rect", "struct:expr_Rect", "{name: \"r\", p: {x: 3, y: 4}, tags: [\"t1\", \"t2\"], w: 1.5}"),
    variable("expr_points", "array:struct:expr_Point", "[{x: 1, y: 2}, {x: 5, y: 6}]"),
    variable("expr_mode", "enum:expr_Mode", "expr_Mode.Auto"),

    // native variables, implemented in expressions/expressions_native.cpp: they
    // give the expressions values the project types can't hold (uint32 above
    // 2^31, int64, the narrow integer types, a float with a unit, undefined,
    // a dynamic array)
    variable("expr_nat_u32", "integer", null, { native: true }),
    variable("expr_nat_i64", "integer", null, { native: true }),
    variable("expr_nat_u8", "integer", null, { native: true }),
    variable("expr_nat_i8", "integer", null, { native: true }),
    variable("expr_nat_i16", "integer", null, { native: true }),
    variable("expr_nat_volt", "float", null, { native: true }),
    variable("expr_nat_undef", "integer", null, { native: true }),
    variable("expr_nat_dyn", "array:integer", null, { native: true }),
];

////////////////////////////////////////////////////////////////////////////////
// texts (Flow.languages, Flow.translate, T"..." and SelectLanguage)

export const TEXTS = {
    languages: ["en", "de"],
    resources: [
        ["expr_hello", { en: "Hello", de: "Hallo" }],
        ["expr_bye", { en: "Bye", de: "Tschuess" }],
    ],
};

////////////////////////////////////////////////////////////////////////////////
// a bitmap for Flow.getBitmapIndex

function png(width, height, rgba) {
    function chunk(kind, data) {
        const length = Buffer.alloc(4);
        length.writeUInt32BE(data.length);
        const crc = Buffer.alloc(4);
        crc.writeUInt32BE(crc32(Buffer.concat([kind, data])));
        return Buffer.concat([length, kind, data, crc]);
    }

    const row = Buffer.concat([Buffer.from([0]), ...Array(width).fill(Buffer.from(rgba))]);
    const raw = Buffer.concat(Array(height).fill(row));
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header.set([8, 6, 0, 0, 0], 8);
    return Buffer.concat([
        Buffer.from("\x89PNG\r\n\x1a\n", "latin1"),
        chunk(Buffer.from("IHDR"), header),
        chunk(Buffer.from("IDAT"), zlibCompress(raw)),
        chunk(Buffer.from("IEND"), Buffer.alloc(0)),
    ]);
}

export const BITMAPS = [
    {
        objID: oid("bitmap", "expr_dot"),
        name: "expr_dot",
        image: "data:image/png;base64," + png(2, 2, [255, 0, 0, 255]).toString("base64"),
        bpp: 32,
        alwaysBuild: true,
    },
];

////////////////////////////////////////////////////////////////////////////////
// pages with a flow that runs when the page is shown

// A page whose "page open" event runs the target component.
function onOpenPage(name, target) {
    const onEvent = component("OnEventActionComponent", [name, "on_event"], { event: "page_open" });
    return [[onEvent, target], [line([name, "on_event"], "@seqout", [name, "target"], "@seqin")]];
}

function activePage() {
    const [components, lines] = onOpenPage("expr_active", component(
        "SetVariableActionComponent", ["expr_active", "target"], {
            left: 200, width: 400, height: 70,
            entries: [{ objID: oid("expr_active", "entry"), variable: "active", value: "Flow.isPageActive()" }],
        }));
    return page("expr_active", components, lines, {
        localVariables: [variable("active", "boolean", "false", { key: ["expr_active", "var"] })],
    });
}

function badThemePage() {
    // SetColorTheme with a theme expression that is not a string (O14)
    const [components, lines] = onOpenPage("expr_bad_theme", component(
        "SetColorThemeActionComponent", ["expr_bad_theme", "target"], { left: 200, theme: "5" }));
    return page("expr_bad_theme", components, lines);
}

function badLanguagePage() {
    // SelectLanguage with a language expression that is not a string (O14)
    const [components, lines] = onOpenPage("expr_bad_language", component(
        "SelectLanguageActionComponent", ["expr_bad_language", "target"], { left: 200, language: "5" }));
    return page("expr_bad_language", components, lines);
}

export const PAGES = [activePage(), badThemePage(), badLanguagePage()];
