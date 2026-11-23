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

// Generates the main test project of the eez-framework tests.
//
//     node make_fwtest.mjs
//
// writes
//     fwtest/fwtest.eez-project             main assets of the test executable
//     fwtest/generated/expression_cases.inc the expression cases for the C++ test
//
// Build the project with EEZ Studio afterwards (build_projects.mjs), which writes
// fwtest/generated/document.h and document.cpp. Commit all of these files, so the
// tests can be built without Studio.
//
// The project is put together from the parts in parts/: base.mjs first, then the
// others by file name. A part is a module that may export any of
//     COLORS        [[name, "#rrggbb"]]
//     STYLES        styles under the "default" style (fwlib style)
//     GLOBALS       global variables (fwlib variable)
//     STRUCTURES    structures (see the eez-project skill, variables.md)
//     ENUMS         enums
//     ACTIONS       actions (fwlib nativeAction, flowAction)
//     PAGES         pages (fwlib page)
//     USER_WIDGETS  user widgets (pages with isUsedAsUserWidget)
//     BITMAPS       bitmaps
//     TEXTS         {languages: [languageID], resources: [[resourceID, {languageID: text}]]}
//
// Every item gets tags (fwlib tags): the area of its part (the part name, see
// PART_AREAS) and topics by its name.

import { mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { basename, dirname, extname, join, relative } from "node:path";
import { pathToFileURL } from "node:url";

import { CASES, UNDEFINED, F } from "./expression_cases.mjs";
import { DISPLAY_HEIGHT, DISPLAY_WIDTH, HERE, loadFont, oid, setTags, style, tagItems, tags } from "./fwlib.mjs";

// the area (tag) of a part when it isn't the part name
const PART_AREAS = { expr_extra: "expressions" };

// The parts: {name, module}, base first.
async function loadParts() {
    const names = readdirSync(join(HERE, "parts"))
        .filter(file => extname(file) === ".mjs")
        .map(file => basename(file, ".mjs"))
        .sort();
    names.splice(names.indexOf("base"), 1);
    const parts = [];
    for (const name of ["base", ...names]) {
        parts.push({ name, module: await import(pathToFileURL(join(HERE, "parts", name + ".mjs")).href) });
    }
    return parts;
}

function area(part) {
    return PART_AREAS[part.name] ?? part.name;
}

function collect(parts, attribute) {
    const result = [];
    for (const part of parts) {
        result.push(...tagItems(part.module[attribute] ?? [], area(part)));
    }
    return result;
}

// [name, value, area] of the COLORS of the parts
function collectColors(parts) {
    return parts.flatMap(part => (part.module.COLORS ?? []).map(([name, value]) => [name, value, area(part)]));
}

////////////////////////////////////////////////////////////////////////////////
// build files

const DOCUMENT_H = `#pragma once

#include <eez/gui/data.h>
#include <eez/gui/widget.h>

namespace eez {
namespace gui {

//\${eez-studio DATA_ENUM}

//\${eez-studio DATA_FUNCS_DECL}

//\${eez-studio DATA_ARRAY_DECL}

//\${eez-studio ACTIONS_ENUM}

//\${eez-studio ACTIONS_FUNCS_DECL}

//\${eez-studio ACTIONS_ARRAY_DECL}

//\${eez-studio GUI_FONTS_ENUM}

//\${eez-studio GUI_BITMAPS_ENUM}

//\${eez-studio GUI_STYLES_ENUM}

//\${eez-studio GUI_THEMES_ENUM}

//\${eez-studio GUI_COLORS_ENUM}

//\${eez-studio GUI_PAGES_ENUM}

//\${eez-studio FLOW_STRUCTS}

//\${eez-studio GUI_ASSETS_DECL_COMPRESSED}

} // namespace gui
} // namespace eez
`;

const DOCUMENT_CPP = `#include <eez/conf-internal.h>

#include "document.h"

namespace eez {
namespace gui {

//\${eez-studio DATA_ARRAY_DEF}

//\${eez-studio ACTIONS_ARRAY_DEF}

//\${eez-studio GUI_ASSETS_DEF_COMPRESSED}

} // namespace gui
} // namespace eez
`;

// The project texts from the TEXTS of the parts, null when no part has any.
function texts(parts) {
    const languages = new Map();
    const resources = [];
    for (const part of parts) {
        const partTexts = part.module.TEXTS;
        if (partTexts) {
            for (const language of partTexts.languages) {
                if (!languages.has(language)) {
                    languages.set(language, area(part));
                }
            }
            resources.push(...partTexts.resources.map(([resourceID, translations]) => [resourceID, translations, area(part)]));
        }
    }
    if (languages.size === 0) {
        return null;
    }
    return {
        objID: oid("texts"),
        languages: [...languages].map(([language, area_]) => (
            { objID: oid("language", language), languageID: language, tags: tags(language, area_) }
        )),
        resources: resources.map(([resourceID, translations, area_]) => ({
            objID: oid("text", resourceID),
            resourceID,
            tags: tags(resourceID, area_),
            translations: Object.entries(translations).map(([language, text_]) => (
                { objID: oid("text", resourceID, language), languageID: language, text: text_ }
            )),
        })),
    };
}

function project(parts) {
    const colors = collectColors(parts);
    const result = {
        objID: oid("project"),
        settings: {
            objID: oid("settings"),
            general: {
                objID: oid("settings", "general"),
                projectVersion: "v3",
                projectType: "firmware",
                flowSupport: true,
                displayWidth: DISPLAY_WIDTH,
                displayHeight: DISPLAY_HEIGHT,
                colorBpp: "32",
                bitmapColorFormat: "RGB",
            },
            eezGui: {
                objID: oid("settings", "eezGui"),
                styleVariants: parts.flatMap(part => (part.module.VARIANTS ?? []).map(([id, name]) => (
                    { objID: oid("style variant", name), id, name }
                ))),
            },
            build: {
                objID: oid("settings", "build"),
                configurations: [{ objID: oid("configuration", "default"), name: "default" }],
                files: [
                    { objID: oid("file", "document.h"), fileName: "document.h", template: DOCUMENT_H },
                    { objID: oid("file", "document.cpp"), fileName: "document.cpp", template: DOCUMENT_CPP },
                ],
                destinationFolder: "generated",
            },
        },
        variables: {
            globalVariables: collect(parts, "GLOBALS"),
            structures: collect(parts, "STRUCTURES"),
            enums: collect(parts, "ENUMS"),
        },
        actions: collect(parts, "ACTIONS"),
        userPages: collect(parts, "PAGES"),
        userWidgets: collect(parts, "USER_WIDGETS"),
        styles: [
            setTags(style("default", {
                font: "RobotoCondensed",
                color: "text",
                backgroundColor: "background",
                alignHorizontal: "center",
                alignVertical: "center",
                children: collect(parts, "STYLES"),
            }), tags("default", "base")),
        ],
        fonts: [...tagItems([loadFont("RobotoCondensed"), loadFont("Icons_WaveformS")], "base"), ...collect(parts, "FONTS")],
        bitmaps: collect(parts, "BITMAPS"),
        colors: colors.map(([name, , area_]) => ({ objID: oid("color", name), name, tags: tags(name, area_) })),
        themes: [{
            objID: oid("theme", "Default"), name: "Default", tags: tags("Default", "base"),
            colors: colors.map(([, value]) => value),
        }],
    };
    const projectTexts = texts(parts);
    if (projectTexts) {
        result.texts = projectTexts;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// expression cases for the C++ test

// expected type of a case -> [check, value type], see expression_cases.mjs
const CHECKS = {
    int32: ["CASE_VALUE", "VALUE_TYPE_INT32"],
    uint32: ["CASE_VALUE", "VALUE_TYPE_UINT32"],
    int64: ["CASE_VALUE", "VALUE_TYPE_INT64"],
    float: ["CASE_VALUE", "VALUE_TYPE_FLOAT"],
    double: ["CASE_VALUE", "VALUE_TYPE_DOUBLE"],
    boolean: ["CASE_VALUE", "VALUE_TYPE_BOOLEAN"],
    string: ["CASE_VALUE", "VALUE_TYPE_STRING"],
    undefined: ["CASE_VALUE", "VALUE_TYPE_UNDEFINED"],
    null: ["CASE_VALUE", "VALUE_TYPE_NULL"],
    date: ["CASE_VALUE", "VALUE_TYPE_DATE"],
    number: ["CASE_NUMBER", "VALUE_TYPE_UNDEFINED"],
    prefix: ["CASE_PREFIX", "VALUE_TYPE_STRING"],
    error: ["CASE_ERROR", "VALUE_TYPE_ERROR"],
    array: ["CASE_ARRAY", "VALUE_TYPE_ARRAY"],
    any: ["CASE_ANY", "VALUE_TYPE_UNDEFINED"],
};

// A C string literal of the UTF-8 bytes of s (octal escapes, they never
// run into the next character).
function cString(s) {
    let out = "";
    for (const byte of Buffer.from(s, "utf8")) {
        const c = String.fromCharCode(byte);
        if (c === '"' || c === "\\") {
            out += "\\" + c;
        } else if (byte >= 32 && byte < 127) {
            out += c;
        } else {
            out += "\\" + byte.toString(8).padStart(3, "0");
        }
    }
    return '"' + out + '"';
}

// The shortest digits that give the double x back: [digits, exponent of the
// first digit], x finite and not 0.
function shortestDigits(x) {
    const [mantissa, exponent] = Math.abs(x).toExponential().split("e");
    return [mantissa.replace(".", ""), Number(exponent)];
}

// The exact decimal digits of the double x: [digits, exponent of the first
// digit], x finite and not 0.
function exactDigits(x) {
    const view = new DataView(new ArrayBuffer(8));
    view.setFloat64(0, Math.abs(x));
    const bits = view.getBigUint64(0);
    const biasedExponent = Number(bits >> 52n);
    let mantissa = bits & ((1n << 52n) - 1n);
    let exponent;
    if (biasedExponent === 0) {
        exponent = -1074;
    } else {
        mantissa |= 1n << 52n;
        exponent = biasedExponent - 1075;
    }
    // x = mantissa * 2^exponent
    let digits;
    let pointShift;
    if (exponent >= 0) {
        digits = (mantissa << BigInt(exponent)).toString();
        pointShift = 0;
    } else {
        digits = (mantissa * 5n ** BigInt(-exponent)).toString();
        pointShift = -exponent;
    }
    // x = digits * 10^-pointShift
    const firstExponent = digits.length - 1 - pointShift;
    return [digits.replace(/0+$/, ""), firstExponent];
}

// x rounded to precision significant digits, half to even: [digits, exponent
// of the first digit], x finite and not 0.
function roundedDigits(x, precision) {
    let [digits, exponent] = exactDigits(x);
    if (digits.length <= precision) {
        return [digits.padEnd(precision, "0"), exponent];
    }
    const rest = digits.slice(precision);
    let kept = BigInt(digits.slice(0, precision));
    const roundUp = rest[0] > "5" || (rest[0] === "5" && (/[1-9]/.test(rest.slice(1)) || kept % 2n === 1n));
    if (roundUp) {
        kept += 1n;
    }
    let result = kept.toString();
    if (result.length > precision) {
        result = result.slice(0, precision);
        exponent += 1;
    }
    return [result, exponent];
}

function exponentText(exponent) {
    return "e" + (exponent < 0 ? "-" : "+") + String(Math.abs(exponent)).padStart(2, "0");
}

// The text of a double as the C++ source needs it: a "." or an exponent, the
// shortest digits that give the double back.
function floatRepr(x) {
    if (x === 0) {
        return Object.is(x, -0) ? "-0.0" : "0.0";
    }
    const sign = x < 0 ? "-" : "";
    const [digits, exponent] = shortestDigits(x);
    if (exponent < -4 || exponent >= 16) {
        const mantissa = digits.length > 1 ? digits[0] + "." + digits.slice(1) : digits;
        return sign + mantissa + exponentText(exponent);
    }
    if (exponent < 0) {
        return sign + "0." + "0".repeat(-exponent - 1) + digits;
    }
    const intPart = digits.slice(0, exponent + 1).padEnd(exponent + 1, "0");
    const fraction = digits.slice(exponent + 1) || "0";
    return sign + intPart + "." + fraction;
}

// printf("%.<precision>g", x)
function formatG(x, precision) {
    if (Number.isNaN(x)) {
        return "nan";
    }
    if (!Number.isFinite(x)) {
        return x > 0 ? "inf" : "-inf";
    }
    const sign = x < 0 || Object.is(x, -0) ? "-" : "";
    if (x === 0) {
        return sign + "0";
    }
    const [digits, exponent] = roundedDigits(x, precision);
    if (exponent < -4 || exponent >= precision) {
        const fraction = digits.slice(1).replace(/0+$/, "");
        return sign + digits[0] + (fraction ? "." + fraction : "") + exponentText(exponent);
    }
    let intPart;
    let fraction;
    if (exponent < 0) {
        intPart = "0";
        fraction = "0".repeat(-exponent - 1) + digits;
    } else {
        intPart = digits.slice(0, exponent + 1);
        fraction = digits.slice(exponent + 1);
    }
    fraction = fraction.replace(/0+$/, "");
    return sign + intPart + (fraction ? "." + fraction : "");
}

function cNumber(value) {
    value = Number(value);
    if (Number.isNaN(value)) {
        return "NAN";
    }
    if (!Number.isFinite(value)) {
        return value > 0 ? "INFINITY" : "-INFINITY";
    }
    return floatRepr(value);
}

// The text of an array element as expressions_test.cpp encodes it.
function encodeValue(value) {
    if (value === UNDEFINED) {
        return "undefined";
    }
    if (value === null) {
        return "null";
    }
    if (typeof value === "boolean") {
        return value ? "true" : "false";
    }
    if (typeof value === "number" && Number.isInteger(value)) {
        return String(value);
    }
    if (value instanceof F) {
        return "f:" + formatG(Math.fround(value.value), 9);
    }
    if (typeof value === "number") {
        return "d:" + formatG(value, 15);
    }
    if (typeof value === "string") {
        return '"' + value + '"';
    }
    if (Array.isArray(value)) {
        return "[" + value.map(encodeValue).join(",") + "]";
    }
    throw new Error(`can't encode ${value}`);
}

function casesInc() {
    const lines = [
        "// Generated by make_fwtest.mjs from expression_cases.mjs, do not edit.",
        "// { expression, check, expected type, number, string, known bug }",
    ];
    for (const case_ of CASES) {
        const [expression, type, value] = case_;
        const bug = case_.length > 3 ? case_[3] : null;
        const [check, valueType] = CHECKS[type];
        let number = "0";
        let string = "nullptr";
        if (type === "string" || type === "prefix") {
            string = cString(value);
        } else if (type === "error") {
            if (value !== null) {
                string = cString(value);
            }
        } else if (type === "array") {
            if (Array.isArray(value)) {
                number = cNumber(value.length);
                string = cString(encodeValue(value));
            } else {
                number = cNumber(value);
            }
        } else if (type === "boolean") {
            number = value ? "1" : "0";
        } else if (value !== null) {
            number = cNumber(value);
        }
        lines.push(`{ ${cString(expression)}, ${check}, ${valueType}, ${number}, ${string}, ${bug ? cString(bug) : "nullptr"} },`);
    }
    return lines.join("\n") + "\n";
}

function write(path, text_) {
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, text_, "utf8");
    console.log("wrote", relative(HERE, path));
}

async function main() {
    const parts = await loadParts();
    write(join(HERE, "fwtest", "fwtest.eez-project"), JSON.stringify(project(parts), null, 2) + "\n");
    write(join(HERE, "fwtest", "generated", "expression_cases.inc"), casesInc());
}

await main();
