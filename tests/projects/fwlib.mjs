/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Helpers that build .eez-project JSON objects for the test projects.
//
// See the eez-project skill (studio/.claude/skills/eez-project) for the format.
// Every object gets an objID derived from its key, so a regenerated project is the
// same file. Names of pages, actions, variables, styles and colors are global in
// the project: a part prefixes them with its area (flow_, gui_, ...).
//
// A key is a string, a number or an array of keys. An array is written into the
// objID name as a tuple, e.g. ('layout', 'row_start') for ["layout", "row_start"].

import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const HERE = dirname(fileURLToPath(import.meta.url));
const NAMESPACE = "6b1f3c1e-6c39-4a5e-9f63-2d1b9a1e0001";

export const DISPLAY_WIDTH = 800;
export const DISPLAY_HEIGHT = 480;

// RFC 4122 version 5 UUID: SHA-1 of the namespace bytes and the UTF-8 name
function uuid5(namespace, name) {
    const hash = createHash("sha1")
        .update(Buffer.from(namespace.replace(/-/g, ""), "hex"))
        .update(Buffer.from(name, "utf8"))
        .digest();
    const bytes = hash.subarray(0, 16);
    bytes[6] = (bytes[6] & 0x0f) | 0x50;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    const hex = bytes.toString("hex");
    return [hex.slice(0, 8), hex.slice(8, 12), hex.slice(12, 16), hex.slice(16, 20), hex.slice(20)].join("-");
}

// the text of a string inside a key: in single quotes, or in double quotes when
// it has a single quote and no double quote
function quote(s) {
    const q = s.includes("'") && !s.includes('"') ? '"' : "'";
    let out = "";
    for (const c of s) {
        const code = c.codePointAt(0);
        if (c === "\\" || c === q) {
            out += "\\" + c;
        } else if (c === "\n") {
            out += "\\n";
        } else if (c === "\r") {
            out += "\\r";
        } else if (c === "\t") {
            out += "\\t";
        } else if (code < 0x20 || (code >= 0x7f && code < 0xa0)) {
            out += "\\x" + code.toString(16).padStart(2, "0");
        } else if (code >= 0xe000 && code <= 0xf8ff) {
            out += "\\u" + code.toString(16).padStart(4, "0");
        } else {
            out += c;
        }
    }
    return q + out + q;
}

function keyText(k, inTuple) {
    if (Array.isArray(k)) {
        const items = k.map(item => keyText(item, true));
        return "(" + items.join(", ") + (items.length === 1 ? ",)" : ")");
    }
    if (typeof k === "boolean") {
        return k ? "True" : "False";
    }
    if (k === null || k === undefined) {
        return "None";
    }
    if (typeof k === "string" && inTuple) {
        return quote(k);
    }
    return String(k);
}

export function oid(...key) {
    return uuid5(NAMESPACE, key.map(k => keyText(k, false)).join("/"));
}

export function loadFont(name) {
    return JSON.parse(readFileSync(join(HERE, "fonts", name + ".json"), "utf8"));
}

////////////////////////////////////////////////////////////////////////////////
// tags: the "tags" property of the project items, used only for searching in
// Studio ("tag:flow" finds the items of the flow part)

// topic tags by the item name, after the area tag
const TOPICS = [
    [/err|catch|fail/, "errors"],
    [/(^|_)uw(_|\d|$)|user_widget/, "user-widget"],
    [/native/, "native"],
    [/keypad|keyboard/, "keypad"],
    [/msgbox|yes_no|popup|dialog|alert|menu_with_buttons|async_operation/, "popups"],
    [/probe/, "probe"],
];

// The tags of an item: the area (base, flow, gui, ...) and its topics.
export function tags(name, area) {
    const result = [area];
    for (const [pattern, topic] of TOPICS) {
        if (pattern.test(name) && !result.includes(topic)) {
            result.push(topic);
        }
    }
    return result.join(", ");
}

// Puts obj.tags right after the anchor key, where Studio writes it.
export function setTags(obj, value, anchor = "name") {
    const items = Object.entries(obj).filter(([k]) => k !== "tags");
    for (const k of Object.keys(obj)) {
        delete obj[k];
    }
    for (const [k, v] of items) {
        obj[k] = v;
        if (k === anchor) {
            obj.tags = value;
        }
    }
    return obj;
}

// Tags the items (with a name) of an area, their local variables and child
// styles too. Returns items.
export function tagItems(items, area) {
    for (const item of items) {
        setTags(item, tags(item.name, area));
        for (const v of item.localVariables ?? []) {
            setTags(v, tags(v.name, area));
        }
        tagItems(item.childStyles ?? [], area);
    }
    return items;
}

////////////////////////////////////////////////////////////////////////////////
// styles, variables, actions

// A style; props are style keys (font, color, backgroundColor, borderSize, ...)
// and children, the child styles.
export function style(name, props = {}) {
    const { children = [], ...styleProps } = props;
    const s = { objID: oid("style", name), name, alwaysBuild: true };
    Object.assign(s, styleProps);
    s.childStyles = [...children];
    return s;
}

export function variable(name, type, defaultValue = null, { native = false, key = "var" } = {}) {
    const v = { objID: oid(key, name), name, type, native };
    if (defaultValue !== null && defaultValue !== undefined) {
        v.defaultValue = defaultValue;
    }
    return v;
}

export function nativeAction(name) {
    return {
        objID: oid("action", name), name, implementationType: "native",
        components: [], connectionLines: [], localVariables: [],
        componentGroups: [], userProperties: [],
    };
}

export function flowAction(name, components, connectionLines = [], { localVariables = [], userProperties = [] } = {}) {
    return {
        objID: oid("action", name), name, implementationType: "flow",
        components: [...components], connectionLines: [...connectionLines],
        localVariables: [...localVariables], componentGroups: [],
        userProperties: [...userProperties],
    };
}

////////////////////////////////////////////////////////////////////////////////
// pages and widgets

// A page; props are its keys (style, left, width, isUsedAsUserWidget, ...) and
// localVariables.
export function page(name, components, connectionLines = [], props = {}) {
    const { localVariables = [], ...pageProps } = props;
    const p = {
        objID: oid("page", name), name,
        left: 0, top: 0, width: DISPLAY_WIDTH, height: DISPLAY_HEIGHT,
        components: [...components], connectionLines: [...connectionLines],
        localVariables: [...localVariables], userProperties: [], componentGroups: [],
    };
    Object.assign(p, pageProps);
    return p;
}

// Any widget; props are its keys (data, text, widgets, eventHandlers, ...).
export function widget(type, key, left, top, width, height, style_ = "default", props = {}) {
    const w = {
        type, objID: oid(key), left, top,
        width, height, style: { useStyle: style_ },
    };
    Object.assign(w, props);
    return w;
}

export function text(key, left, top, width, height, text_, style_ = "default", props = {}) {
    return widget("TextWidget", key, left, top, width, height, style_, { text: text_, ...props });
}

export function rectangle(key, left, top, width, height, style_, props = {}) {
    return widget("RectangleWidget", key, left, top, width, height, style_, props);
}

// A widget event handler: to an action (by name), or a flow output when action is null.
export function eventHandler(key, eventName = "CLICKED", action = null) {
    if (action) {
        return { objID: oid(key, "handler", eventName), eventName, handlerType: "action", action };
    }
    return { objID: oid(key, "handler", eventName), eventName, handlerType: "flow" };
}

////////////////////////////////////////////////////////////////////////////////
// flow components

// Any action component (StartActionComponent, LogActionComponent, ...); props
// are its keys and left, top, width, height.
export function component(type, key, props = {}) {
    const { left = 0, top = 0, width = 150, height = 50, ...componentProps } = props;
    const c = { type, objID: oid(key), left, top, width, height };
    Object.assign(c, componentProps);
    return c;
}

// A connection line between components given by their keys.
export function line(sourceKey, output, targetKey, input) {
    return {
        objID: oid("line", sourceKey, output, targetKey, input),
        source: oid(sourceKey), output,
        target: oid(targetKey), input,
    };
}
