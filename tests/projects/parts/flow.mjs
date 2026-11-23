/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Flow part: pages, actions and user widgets of the flow engine tests
// (tests/flow/). Every name starts with "flow_".
//
// Most pages are small flows that start with a Start component and report what
// they do with Log components (harness::logMessages()). A test creates the page
// flow state (flow_helpers.h) or shows the page, and runs frames. The page
// "flow_vars" is never shown: its SetVariable components only give the tests a
// pointer to each global variable of this part (entry 0 is the label, entry 1 the
// variable), like the expressions part does.

import { component, line, nativeAction, oid, page, variable, widget } from "../fwlib.mjs";

const PREFIX = "flow_";

function id(...key) {
    return oid("flow-part", ...key);
}

// A page, action or user widget being built: components, lines, locals.
class Flow {
    constructor(name) {
        this.name = PREFIX + name;
        this.components = [];
        this.lines = [];
        this.locals = [];
        this.userProperties = [];
        this.n = 0;
    }

    key(k) {
        return [this.name, k];
    }

    add(type, k, customInputs = [], customOutputs = [], props = {}) {
        this.n += 1;
        const c = component(type, this.key(k), {
            left: 40 + (this.n % 6) * 260, top: 40 + Math.floor(this.n / 6) * 140, width: 200, height: 80,
            customInputs: customInputs.map(([name, t]) => ({ objID: id(this.name, k, "in", name), name, type: t })),
            customOutputs: customOutputs.map(([name, t]) => ({ objID: id(this.name, k, "out", name), name, type: t })),
            ...props,
        });
        this.components.push(c);
        return k;
    }

    addWidget(type, k, left, top, width, height, props = {}) {
        const w = widget(type, this.key(k), left, top, width, height, "default", props);
        this.components.push(w);
        return k;
    }

    wire(src, dst, out = "@seqout", inp = "@seqin") {
        this.lines.push(line(this.key(src), out, this.key(dst), inp));
    }

    chain(...keys) {
        for (let i = 0; i + 1 < keys.length; i++) {
            this.wire(keys[i], keys[i + 1]);
        }
    }

    local(name, type, defaultValue = null) {
        this.locals.push(variable(name, type, defaultValue, { key: [this.name, "local"] }));
    }

    userProperty(name, type, assignable = false) {
        this.userProperties.push({
            objID: id(this.name, "prop", name), id: this.propId(name), name,
            type, assignable, displayName: name, description: "",
        });
    }

    propId(name) {
        return id(this.name, "prop-id", name);
    }

    // objID of a component, e.g. an Input/Output component (CallAction input/output name).
    innerId(k) {
        return oid(this.key(k));
    }

    // components

    start(k = "start") {
        return this.add("StartActionComponent", k);
    }

    end(k = "end") {
        return this.add("EndActionComponent", k);
    }

    log(k, value, customInputs = []) {
        return this.add("LogActionComponent", k, customInputs, [], { value });
    }

    setvar(k, ...entries) {
        return this.add("SetVariableActionComponent", k, [], [], {
            entries: entries.map(([variable_, value], i) => (
                { objID: id(this.name, k, "entry", i), variable: variable_, value }
            )),
        });
    }

    eval(k, expression, customInputs = [], props = {}) {
        return this.add("EvalExprActionComponent", k, customInputs, [], { expression, ...props });
    }

    call(k, action, values = null, customOutputs = [], props = {}) {
        const callee = ACTION_FLOWS.get(action);
        return this.add("CallActionActionComponent", k, [], customOutputs, {
            action: PREFIX + action,
            userPropertyValues: {
                objID: id(this.name, k, "values"),
                values: Object.fromEntries(Object.entries(values ?? {}).map(([name, expr]) => [callee.propId(name), expr])),
            },
            ...props,
        });
    }

    // results

    page(props = {}) {
        const p = page(this.name, this.components, this.lines, { localVariables: this.locals, ...props });
        p.userProperties = this.userProperties;
        return p;
    }

    userWidget(width, height) {
        return this.page({ isUsedAsUserWidget: true, width, height });
    }

    action() {
        return {
            objID: oid("action", this.name), name: this.name, implementationType: "flow",
            components: this.components, connectionLines: this.lines,
            localVariables: this.locals, componentGroups: [],
            userProperties: this.userProperties,
        };
    }
}

////////////////////////////////////////////////////////////////////////////////
// globals and structures

export const STRUCTURES = [
    {
        objID: id("struct", "point"), name: "flow_point", fields: [
            { objID: id("struct", "point", "x"), name: "x", type: "integer" },
            { objID: id("struct", "point", "y"), name: "y", type: "integer" },
        ],
    },
    {
        objID: id("struct", "item"), name: "flow_item", fields: [
            { objID: id("struct", "item", "name"), name: "name", type: "string" },
            { objID: id("struct", "item", "value"), name: "value", type: "integer" },
        ],
    },
];

// [name, type, default value]
const GLOBAL_VALUES = [
    ["flow_g_int", "integer", "0"],
    ["flow_g_arr", "array:integer", "[1, 2, 3]"],
    ["flow_g_point", "struct:flow_point", "{x: 1, y: 2}"],
    ["flow_g_watch", "integer", "0"],
    ["flow_g_lock", "boolean", "false"],
    ["flow_g_layout", "integer", "0"],
    ["flow_g_result", "integer", "0"],
    ["flow_g_list", "any", "null"],
];

export const GLOBALS = GLOBAL_VALUES.map(([name, type, defaultValue]) => variable(name, type, defaultValue));

// flow_vars: one SetVariable per global (entry 0: label, entry 1: the variable),
// hanging on a page_open event that never fires (the page is never shown).
function varsPage() {
    const f = new Flow("vars");
    f.local("label", "any");
    f.add("OnEventActionComponent", "on_event", [], [], { event: "page_open" });
    for (const [name] of GLOBAL_VALUES) {
        f.setvar(name, ["label", `"${name}"`], [name, name]);
        f.wire("on_event", name);
    }
    return f.page();
}

////////////////////////////////////////////////////////////////////////////////
// actions

const ACTION_FLOWS = new Map();
export const ACTIONS = [];

function action(f) {
    ACTION_FLOWS.set(f.name.slice(PREFIX.length), f);
    return f;
}

function native(name, props = []) {
    const a = nativeAction(PREFIX + name);
    const f = new Flow(name);
    for (const [propName, type, assignable] of props) {
        f.userProperty(propName, type, assignable);
    }
    a.userProperties = f.userProperties;
    ACTION_FLOWS.set(name, f);
    ACTIONS.push(a);
}

// native actions, implemented in tests/flow/flow_native.cpp
native("native_props", [["a", "integer", false], ["out", "integer", true]]);
native("native_async");
native("native_async_out", [["res", "integer", true]]);
native("native_sync");
native("native_getprop");

// flow actions

let f = action(new Flow("act_error"));
f.start();
f.add("ErrorActionComponent", "error", [], [], { message: '"boom"' });
f.wire("start", "error");

f = action(new Flow("act_eval_error"));
f.start();
f.eval("eval", "flow_g_arr[10]");
f.log("log", '"act eval result " + v', [["v", "any"]]);
f.wire("start", "eval");
f.wire("eval", "log", "result", "v");

f = action(new Flow("act_watch"));
f.start();
f.add("WatchVariableActionComponent", "watch", [], [], { variable: "flow_g_watch" });
f.log("log", '"aw " + v', [["v", "integer"]]);
f.wire("start", "watch");
f.wire("watch", "log", "variable", "v");

// the Noop is patched by the test into a custom component (registerComponent)
f = action(new Flow("act_custom_err"));
f.start();
f.add("NoopActionComponent", "custom", [], [], { name: "custom" });
f.add("ErrorActionComponent", "error", [], [], { message: '"custom boom"' });
f.wire("start", "custom");
f.wire("start", "error");

f = action(new Flow("act_outer"));
f.start();
f.call("call", "act_error");
f.add("CatchErrorActionComponent", "catch");
f.log("log", '"outer caught " + m', [["m", "string"]]);
f.wire("start", "call");
f.wire("catch", "log", "Message", "m");

f = action(new Flow("act_err_then"));
f.start();
f.add("ErrorActionComponent", "error", [], [], { message: '"stop here"' });
f.log("log", '"act continues"');
f.wire("start", "error");
f.wire("start", "log");

f = action(new Flow("act_props"));
f.userProperty("in_val", "integer");
f.userProperty("out_ref", "integer", true);
f.start();
f.log("log", '"props in " + in_val');
f.setvar("set", ["out_ref", "in_val * 2"]);
f.end();
f.chain("start", "log", "set", "end");

f = action(new Flow("act_io"));
f.add("InputActionComponent", "input", [], [], { name: "a", inputType: "integer" });
f.eval("eval", "v * 10", [["v", "integer"]]);
f.add("OutputActionComponent", "output", [], [], { name: "r", outputType: "integer" });
f.wire("input", "eval", "@seqout", "v");
f.wire("eval", "output", "result", "@seqin");

f = action(new Flow("act_inner"));
f.start();
f.log("log", '"inner"');
f.end();
f.chain("start", "log", "end");

f = action(new Flow("act_outer2"));
f.start();
f.log("log1", '"outer start"');
f.call("call", "act_inner");
f.log("log2", '"outer after inner"');
f.end();
f.chain("start", "log1", "call", "log2", "end");

f = action(new Flow("act_noend"));
f.start();
f.log("log", '"noend body"');
f.chain("start", "log");

f = action(new Flow("act_delay"));
f.start();
f.add("DelayActionComponent", "delay", [], [], { milliseconds: "50" });
f.log("log", '"act delay done"');
f.end();
f.chain("start", "delay", "log", "end");

f = action(new Flow("act_click"));
f.start();
f.log("log", '"click action"');
f.add("InputActionComponent", "input", [], [], { name: "e", inputType: "struct:$ClickEvent" });
f.log("log_event", '"click index " + e.index', [["e", "struct:$ClickEvent"]]);
f.wire("start", "log");
f.wire("input", "log_event", "@seqout", "e");

f = action(new Flow("act_keypad"));
f.start();
f.add("ShowKeypadActionComponent", "keypad", [], [], {
    label: '"Amps"', initalValue: "0.5", min: "0", max: "5",
    precision: "0.1", unit: '"A"',
});
f.log("log", '"akp " + v', [["v", "float"]]);
f.end();
f.wire("start", "keypad");
f.wire("keypad", "log", "result", "v");
f.wire("log", "end");

f = action(new Flow("act_keypad_bad"));
f.start();
f.add("ShowKeypadActionComponent", "keypad", [], [], {
    label: '"Bad"', initalValue: "1", min: "0", max: "5",
    precision: "flow_g_arr[10]", unit: '"V"',
});
f.log("log", '"bad kp " + v', [["v", "float"]]);
f.wire("start", "keypad");
f.wire("keypad", "log", "result", "v");

f = action(new Flow("act_msgbox"));
f.start();
f.add("ShowMessageBoxActionComponent", "msgbox", [], [], { messageType: 1, message: '"in action"' });
f.end();
f.chain("start", "msgbox", "end");

f = action(new Flow("act_msgbox_bad"));
f.local("bad", "any", "5");
f.start();
f.add("ShowMessageBoxActionComponent", "msgbox", [], [], { messageType: 3, message: '"Question?"', buttons: "bad" });
f.wire("start", "msgbox");

// queue flood: Start -> A -> B1..B11 -> C1..C11 -> D1..D11, every B is wired to
// every C and every C to every D, so the C tasks queue 1331 D tasks (> 1000)
const FLOOD_WIDTH = 11;

function flood(name, dComponent) {
    const f = action(new Flow(name));
    f.start();
    f.add("NoopActionComponent", "a", [], [], { name: "a" });
    f.wire("start", "a");
    for (let i = 0; i < FLOOD_WIDTH; i++) {
        f.add("NoopActionComponent", `b${i}`, [], [], { name: `b${i}` });
        f.add("NoopActionComponent", `c${i}`, [], [], { name: `c${i}` });
        dComponent(f, `d${i}`);
        f.wire("a", `b${i}`);
    }
    for (let i = 0; i < FLOOD_WIDTH; i++) {
        for (let j = 0; j < FLOOD_WIDTH; j++) {
            f.wire(`b${i}`, `c${j}`);
            f.wire(`c${i}`, `d${j}`);
        }
    }
    return f;
}

flood("act_flood", (f, k) => f.add("NoopActionComponent", k, [], [], { name: k }));

f = action(new Flow("act_two"));
f.add("NoopActionComponent", "n1", [], [], { name: "n1" });
f.add("NoopActionComponent", "n2", [], [], { name: "n2" });

function flood2D(f, k) {
    f.call(k, "act_two", null, [], { catchError: true });
    f.add("NoopActionComponent", k + "_sink", [], [], { name: k + "_sink" });
    f.wire(k, k + "_sink", "@error", "@seqin");
}

flood("act_flood2", flood2D);

const NATIVE_ACTIONS = ["native_props", "native_async", "native_async_out", "native_sync", "native_getprop"];
ACTIONS.push(...[...ACTION_FLOWS.values()]
    .filter(f => !NATIVE_ACTIONS.includes(f.name.slice(PREFIX.length)))
    .map(f => f.action()));

////////////////////////////////////////////////////////////////////////////////
// pages

export const PAGES = [varsPage()];
export const USER_WIDGETS = [];

function addPage(f, props = {}) {
    PAGES.push(f.page(props));
    return f;
}

// sequence, fan-out, components without a connected sequence input
f = new Flow("seq");
f.start();
f.log("l1", '"seq 1"');
f.log("l2", '"seq 2"');
f.log("l3", '"seq 3"');
f.log("free", '"seq free"');
f.chain("start", "l1", "l2", "l3");
addPage(f);

f = new Flow("fanout");
f.start();
f.log("a", '"fan a"');
f.log("b", '"fan b"');
f.wire("start", "a");
f.wire("start", "b");
addPage(f);

// SetVariable
f = new Flow("setvar");
f.local("loc", "integer", "0");
f.start();
f.setvar("set", ["flow_g_int", "41 + 1"], ["loc", "flow_g_int + 1"], ["flow_g_arr[1]", "99"],
    ["flow_g_point.x", "7"]);
f.log("log", '"set " + flow_g_int + " " + loc + " " + flow_g_arr[1] + " " + flow_g_point.x');
f.chain("start", "set", "log");
addPage(f);

f = new Flow("setvar_oob");
f.start();
f.setvar("set", ["flow_g_arr[10]", "5"]);
f.log("log", '"oob after"');
f.chain("start", "set", "log");
addPage(f);

f = new Flow("eval_oob");
f.start();
f.eval("eval", "flow_g_arr[10]");
f.log("log", '"eval oob " + v', [["v", "any"]]);
f.wire("start", "eval");
f.wire("eval", "log", "result", "v");
addPage(f);

// Switch
f = new Flow("switch");
f.local("x", "integer", "7");
f.start();
f.add("SwitchActionComponent", "switch", [], [], {
    tests: [
        { objID: id("switch", 0), condition: "x > 10", outputName: "big", outputValue: '"big"' },
        { objID: id("switch", 1), condition: "x > 5", outputName: "mid", outputValue: '"mid"' },
        { objID: id("switch", 2), condition: "true", outputName: "small", outputValue: '"small"' },
    ],
});
f.log("value", '"switch " + v', [["v", "string"]]);
f.log("done", '"switch done"');
f.wire("start", "switch");
for (const name of ["big", "mid", "small"]) {
    f.wire("switch", "value", name, "v");
}
f.wire("switch", "done");
addPage(f);

f = new Flow("switch_none");
f.local("x", "integer", "1");
f.start();
f.add("SwitchActionComponent", "switch", [], [], {
    tests: [
        { objID: id("switch_none", 0), condition: "x > 10", outputName: "big", outputValue: '"big"' },
    ],
});
f.log("value", '"none " + v', [["v", "string"]]);
f.log("done", '"switch none done"');
f.wire("start", "switch");
f.wire("switch", "value", "big", "v");
f.wire("switch", "done");
addPage(f);

// Compare, IsTrue
f = new Flow("compare");
f.start();
f.add("CompareActionComponent", "lt", [], [], { A: "3", B: "5", operator: "<" });
f.add("CompareActionComponent", "between", [], [], { A: "5", B: "1", C: "10", operator: "BETWEEN" });
f.add("CompareActionComponent", "eq", [], [], { A: "1", B: "2", operator: "=" });
for (const k of ["lt", "between", "eq"]) {
    f.log(k + "_t", `"${k} true"`);
    f.log(k + "_f", `"${k} false"`);
    f.wire(k, k + "_t", "True");
    f.wire(k, k + "_f", "False");
}
f.chain("start", "lt", "between", "eq");
addPage(f);

f = new Flow("istrue");
f.start();
f.add("IsTrueActionComponent", "t1", [], [], { value: "1 > 2" });
f.add("IsTrueActionComponent", "t2", [], [], { value: '"x" == "x"' });
for (const k of ["t1", "t2"]) {
    f.log(k + "_t", `"${k} true"`);
    f.log(k + "_f", `"${k} false"`);
    f.wire(k, k + "_t", "True");
    f.wire(k, k + "_f", "False");
}
f.log("done", '"istrue done"');
f.chain("start", "t1", "t2", "done");
addPage(f);

// Loop, Counter
f = new Flow("loop");
f.local("i", "integer", "0");
f.start();
f.add("LoopActionComponent", "loop", [], [], { variable: "i", from: "1", to: "3", step: "1", version: 1 });
f.log("body", '"loop " + i');
f.log("done", '"loop done"');
f.wire("start", "loop", "@seqout", "start");
f.wire("loop", "body");
f.wire("body", "loop", "@seqout", "next");
f.wire("loop", "done", "done");
addPage(f);

f = new Flow("loop_down");
f.local("i", "integer", "0");
f.start();
f.add("LoopActionComponent", "loop", [], [], { variable: "i", from: "3", to: "1", step: "-1", version: 1 });
f.log("body", '"down " + i');
f.log("done", '"down done"');
f.wire("start", "loop", "@seqout", "start");
f.wire("loop", "body");
f.wire("body", "loop", "@seqout", "next");
f.wire("loop", "done", "done");
addPage(f);

f = new Flow("counter");
f.start();
f.add("CounterActionComponent", "counter", [], [], { countValue: "3" });
f.log("body", '"count"');
f.log("done", '"count done"');
f.wire("start", "counter");
f.wire("counter", "body");
f.wire("body", "counter");
f.wire("counter", "done", "done");
addPage(f);

// Delay
f = new Flow("delay");
f.start();
f.log("before", '"delay start"');
f.add("DelayActionComponent", "delay", [], [], { milliseconds: "100" });
f.log("after", '"delay done"');
f.chain("start", "before", "delay", "after");
addPage(f);

f = new Flow("delay_nan");
f.local("bad", "string", '"abc"');
f.start();
f.add("DelayActionComponent", "delay", [], [], { milliseconds: "bad" });
f.log("after", '"nan delay done"');
f.chain("start", "delay", "after");
addPage(f);

f = new Flow("delay_action");
f.start();
f.call("call", "act_delay");
f.log("after", '"delay action after"');
f.chain("start", "call", "after");
addPage(f);

// Watch
f = new Flow("watch");
f.start();
f.add("WatchVariableActionComponent", "watch", [], [], { variable: "flow_g_watch" });
f.log("log", '"watch " + v', [["v", "integer"]]);
f.wire("start", "watch");
f.wire("watch", "log", "variable", "v");
addPage(f);

f = new Flow("watch_action");
f.start();
f.call("call", "act_watch");
f.chain("start", "call");
addPage(f);

f = new Flow("custom_err");
f.start();
f.call("call", "act_custom_err");
f.add("CatchErrorActionComponent", "catch");
f.log("log", '"custom caught " + m', [["m", "string"]]);
f.wire("start", "call");
f.wire("catch", "log", "Message", "m");
addPage(f);

// Log
f = new Flow("log");
f.start();
f.log("number", "40 + 2");
f.log("empty", '""');
f.log("end", '"log end"');
f.chain("start", "number", "empty", "end");
addPage(f);

// Error, CatchError, @error
f = new Flow("err_callaction_catch");
f.start();
f.call("call", "act_error", null, [], { catchError: true });
f.log("log", '"caught " + m', [["m", "string"]]);
f.wire("start", "call");
f.wire("call", "log", "@error", "m");
addPage(f);

f = new Flow("err_callaction_eval");
f.start();
f.call("call", "act_eval_error", null, [], { catchError: true });
f.log("log", '"eval caught " + m', [["m", "string"]]);
f.wire("start", "call");
f.wire("call", "log", "@error", "m");
addPage(f);

f = new Flow("err_catch_page");
f.start();
f.call("call", "act_error");
f.add("CatchErrorActionComponent", "catch");
f.log("log", '"catch " + m', [["m", "string"]]);
f.wire("start", "call");
f.wire("catch", "log", "Message", "m");
addPage(f);

f = new Flow("err_page");
f.start();
f.add("ErrorActionComponent", "error", [], [], { message: '"page boom"' });
f.add("CatchErrorActionComponent", "catch");
f.log("log", '"page catch " + m', [["m", "string"]]);
f.wire("start", "error");
f.wire("catch", "log", "Message", "m");
addPage(f);

f = new Flow("err_uncaught_action");
f.start();
f.call("call", "act_error");
f.log("after", '"uncaught after"');
f.chain("start", "call", "after");
addPage(f);

f = new Flow("catch_eval");
f.start();
f.eval("eval", "flow_g_arr[10]");
f.log("result", '"catch eval result " + v', [["v", "any"]]);
f.add("CatchErrorActionComponent", "catch");
f.log("log", '"catch eval " + m', [["m", "string"]]);
f.wire("start", "eval");
f.wire("eval", "result", "result", "v");
f.wire("catch", "log", "Message", "m");
addPage(f);

f = new Flow("eval_error_output");
f.start();
f.eval("eval", "flow_g_arr[10]", [], { catchError: true });
f.log("result", '"error output result " + v', [["v", "any"]]);
f.log("error", '"error output " + m', [["m", "string"]]);
f.wire("start", "eval");
f.wire("eval", "result", "result", "v");
f.wire("eval", "error", "@error", "m");
addPage(f);

f = new Flow("catch_twice");
f.start();
f.eval("e1", "flow_g_arr[10]");
f.eval("e2", "flow_g_arr[11]");
f.log("r1", '"twice result " + v', [["v", "any"]]);
f.log("r2", '"twice result " + v', [["v", "any"]]);
f.add("CatchErrorActionComponent", "catch");
f.log("log", '"twice " + m', [["m", "string"]]);
f.wire("start", "e1");
f.wire("start", "e2");
f.wire("e1", "r1", "result", "v");
f.wire("e2", "r2", "result", "v");
f.wire("catch", "log", "Message", "m");
addPage(f);

f = new Flow("err_nested");
f.start();
f.call("call", "act_outer");
f.chain("start", "call");
addPage(f);

f = new Flow("err_drop");
f.start();
f.call("call", "act_err_then");
f.add("CatchErrorActionComponent", "catch");
f.log("log", '"drop caught " + m', [["m", "string"]]);
f.wire("start", "call");
f.wire("catch", "log", "Message", "m");
addPage(f);

f = new Flow("err_nonstring");
f.local("num", "any", "42");
f.start();
f.add("ErrorActionComponent", "error", [], [], { message: "num" });
f.wire("start", "error");
addPage(f);

// LabelOut/LabelIn, Noop, TestAndSet
f = new Flow("label");
f.start();
f.add("LabelOutActionComponent", "out", [], [], { label: "flow_l1" });
f.add("LabelInActionComponent", "in", [], [], { label: "flow_l1" });
f.log("log", '"label in"');
f.wire("start", "out");
f.wire("in", "log");
addPage(f);

f = new Flow("noop");
f.start();
f.add("NoopActionComponent", "noop", [], [], { name: "pass" });
f.log("log", '"noop"');
f.chain("start", "noop", "log");
addPage(f);

f = new Flow("tas");
f.start();
f.add("TestAndSetActionComponent", "tas", [], [], { variable: "flow_g_lock" });
f.log("log", '"tas got"');
f.chain("start", "tas", "log");
addPage(f);

// SortArray
f = new Flow("sort");
f.local("ints", "array:integer", "[3, 1, 2]");
f.local("big", "array:integer", "[16777217, 16777216]");
f.local("strs", "array:string", '["b", "a", "C"]');
f.local("bad", "any", "5");
f.start();
f.add("SortArrayActionComponent", "asc", [], [], { array: "ints", ascending: true, ignoreCase: true });
f.add("SortArrayActionComponent", "desc", [], [], { array: "ints", ascending: false, ignoreCase: true });
f.add("SortArrayActionComponent", "big", [], [], { array: "big", ascending: true, ignoreCase: true });
f.add("SortArrayActionComponent", "icase", [], [], { array: "strs", ascending: true, ignoreCase: true });
f.add("SortArrayActionComponent", "case", [], [], { array: "strs", ascending: true, ignoreCase: false });
for (const [k, t] of [["asc", "array:integer"], ["desc", "array:integer"], ["icase", "array:string"], ["case", "array:string"]]) {
    f.log(k + "_log", `"${k} " + v[0] + "," + v[1] + "," + v[2]`, [["v", t]]);
    f.wire(k, k + "_log", "result", "v");
}
f.log("big_log", '"big " + v[0] + "," + v[1]', [["v", "array:integer"]]);
f.wire("big", "big_log", "result", "v");
// SortArray doesn't fire @seqout (see "sort_seq"), so every sort hangs on Start
for (const k of ["asc", "desc", "big", "icase", "case"]) {
    f.wire("start", k);
}
addPage(f);

f = new Flow("sort_seq");
f.local("ints", "array:integer", "[2, 1]");
f.start();
f.add("SortArrayActionComponent", "sort", [], [], { array: "ints", ascending: true, ignoreCase: true });
f.log("seq", '"sort seqout"');
f.log("result", '"sort result " + v[0] + "," + v[1]', [["v", "array:integer"]]);
f.wire("start", "sort");
f.wire("sort", "seq");
f.wire("sort", "result", "result", "v");
addPage(f);

f = new Flow("sort_catch");
f.local("ints", "array:integer", "[2, 1]");
f.start();
f.add("SortArrayActionComponent", "sort", [], [], { array: "ints", ascending: true, ignoreCase: true, catchError: true });
f.log("result", '"sort catch result " + v[0] + "," + v[1]', [["v", "array:integer"]]);
f.log("error", '"sort catch error"', [["m", "string"]]);
f.wire("start", "sort");
f.wire("sort", "result", "result", "v");
f.wire("sort", "error", "@error", "m");
addPage(f);

f = new Flow("sort_struct");
f.local("items", "array:struct:flow_item", '[{name: "x", value: 3}, {name: "y", value: 1}, {name: "z", value: 2}]');
f.start();
f.add("SortArrayActionComponent", "sort", [], [], {
    array: "items", structureName: "flow_item",
    structureFieldName: "value", ascending: true, ignoreCase: true,
});
f.log("log", '"by value " + v[0].name + v[1].name + v[2].name', [["v", "array:struct:flow_item"]]);
f.wire("start", "sort");
f.wire("sort", "log", "result", "v");
addPage(f);

f = new Flow("sort_bad");
f.local("bad", "any", "5");
f.start();
f.add("SortArrayActionComponent", "sort", [], [], { array: "bad", ascending: true, ignoreCase: true });
f.log("log", '"sort bad result"', [["v", "any"]]);
f.wire("start", "sort");
f.wire("sort", "log", "result", "v");
addPage(f);

// Constant, EvalExpr
f = new Flow("const");
f.start();
f.add("ConstantActionComponent", "const", [], [], { value: '"const ok"' });
f.log("const_log", "v", [["v", "string"]]);
f.eval("eval", "6 * 7");
f.log("eval_log", '"eval " + v', [["v", "integer"]]);
f.wire("start", "const");
f.wire("const", "const_log", "value", "v");
f.wire("const", "eval");
f.wire("eval", "eval_log", "result", "v");
addPage(f);

// CallAction
f = new Flow("call_props");
f.local("res", "integer", "0");
f.start();
f.call("call", "act_props", { in_val: "21", out_ref: "res" });
f.log("log", '"props res " + res');
f.chain("start", "call", "log");
addPage(f);

f = new Flow("call_props_bad");
f.local("res", "integer", "0");
f.start();
f.call("call", "act_props", { in_val: "flow_g_arr[10]", out_ref: "res" });
f.log("log", '"props bad after"');
f.chain("start", "call", "log");
addPage(f);

f = new Flow("call_io");
const io = ACTION_FLOWS.get("act_io");
f.start();
f.eval("four", "4");
f.call("call", "act_io");
f.log("log", '"io " + v', [["v", "integer"]]);
f.wire("start", "four");
f.wire("four", "call", "result", io.innerId("input"));
f.wire("call", "log", io.innerId("output"), "v");
addPage(f);

f = new Flow("call_nested");
f.start();
f.call("call", "act_outer2");
f.log("log", '"nested done"');
f.chain("start", "call", "log");
addPage(f);

f = new Flow("call_noend");
f.start();
f.call("call", "act_noend");
f.log("log", '"noend after"');
f.chain("start", "call", "log");
addPage(f);

f = new Flow("end_page");
f.start();
f.end();
f.chain("start", "end");
addPage(f);

// native actions
f = new Flow("native_call");
f.local("r", "integer", "0");
f.start();
f.call("call", "native_props", { a: "5", out: "r" });
f.log("log", '"native r " + r');
f.chain("start", "call", "log");
addPage(f);

f = new Flow("native_async_call");
f.start();
f.call("call", "native_async");
f.log("log", '"async after"');
f.chain("start", "call", "log");
addPage(f);

f = new Flow("native_async_out_call");
f.start();
f.call("call", "native_async_out", { res: "out" }, [["out", "integer"]]);
f.log("log", '"async out " + v', [["v", "integer"]]);
f.wire("start", "call");
f.wire("call", "log", "out", "v");
addPage(f);

f = new Flow("native_sync_call");
f.start();
f.call("call", "native_sync", null, [["out2", "integer"]]);
f.log("log", '"sync out " + v', [["v", "integer"]]);
f.wire("start", "call");
f.wire("call", "log", "out2", "v");
addPage(f);

// OnEvent
f = new Flow("events");
f.add("OnEventActionComponent", "open", [], [], { event: "page_open" });
f.add("OnEventActionComponent", "close", [], [], { event: "page_close" });
f.log("open_log", '"evt open"');
f.log("close_log", '"evt close"');
f.wire("open", "open_log");
f.wire("close", "close_log");
addPage(f);

f = new Flow("keys");
f.add("OnEventActionComponent", "key", [], [], { event: "keydown" });
f.log("log", '"key " + v', [["v", "any"]]);
f.add("NoopActionComponent", "seq", [], [], { name: "seq" });
f.wire("key", "log", "event", "v");
f.wire("key", "seq");
addPage(f);

// ShowPage
f = new Flow("showpage");
f.start();
f.add("ShowPageActionComponent", "show", [], [], { page: "flow_target" });
f.log("log", '"showpage done"');
f.chain("start", "show", "log");
addPage(f);

f = new Flow("showpage_push");
f.start();
f.add("ShowPageActionComponent", "show", [], [], { page: "flow_target", mode: "push" });
f.log("log", '"showpage push done"');
f.chain("start", "show", "log");
addPage(f);

f = new Flow("showpage_set");
f.start();
f.add("ShowPageActionComponent", "show", [], [], { page: "flow_target", mode: "set" });
f.log("log", '"showpage set done"');
f.chain("start", "show", "log");
addPage(f);

f = new Flow("target");
f.add("OnEventActionComponent", "open", [], [], { event: "page_open" });
f.log("log", '"target open"');
f.wire("open", "log");
addPage(f);

// dialogs
f = new Flow("keypad");
f.start();
f.add("ShowKeypadActionComponent", "keypad", [], [], {
    label: '"Volts"', initalValue: "1.5", min: "0", max: "10",
    precision: "0.01", unit: '"V"',
});
f.log("ok", '"kp " + v', [["v", "float"]]);
f.log("cancel", '"kp canceled"');
f.wire("start", "keypad");
f.wire("keypad", "ok", "result", "v");
f.wire("keypad", "cancel", "canceled");
addPage(f);

f = new Flow("keypad_action");
f.start();
f.call("call", "act_keypad");
f.log("log", '"akp after"');
f.chain("start", "call", "log");
addPage(f);

f = new Flow("keypad_bad");
f.start();
f.call("call", "act_keypad_bad");
f.chain("start", "call");
addPage(f);

f = new Flow("keyboard");
f.start();
f.add("ShowKeyboardActionComponent", "keyboard", [], [], {
    label: '"Name"', initalText: '"abc"', minChars: "1",
    maxChars: "8", password: false,
});
f.log("ok", '"kb " + v', [["v", "string"]]);
f.log("cancel", '"kb canceled"');
f.wire("start", "keyboard");
f.wire("keyboard", "ok", "result", "v");
f.wire("keyboard", "cancel", "canceled");
addPage(f);

f = new Flow("msgbox_info");
f.start();
f.add("ShowMessageBoxActionComponent", "msgbox", [], [], { messageType: 1, message: '"hello info"' });
f.log("log", '"info closed"');
f.chain("start", "msgbox", "log");
addPage(f);

f = new Flow("msgbox_error");
f.start();
f.add("ShowMessageBoxActionComponent", "msgbox", [], [], { messageType: 2, message: '"hello error"' });
f.log("log", '"error closed"');
f.chain("start", "msgbox", "log");
addPage(f);

f = new Flow("msgbox_question");
f.start();
f.add("ShowMessageBoxActionComponent", "msgbox", [], [["Yes", "any"], ["No", "any"]], {
    messageType: 3, message: '"Sure?"', buttons: '["Yes", "No"]',
});
f.log("yes", '"q yes"');
f.log("no", '"q no"');
f.wire("start", "msgbox");
f.wire("msgbox", "yes", "Yes");
f.wire("msgbox", "no", "No");
addPage(f);

f = new Flow("msgbox_action");
f.start();
f.call("call", "act_msgbox");
f.log("log", '"mb action after"');
f.chain("start", "call", "log");
addPage(f);

f = new Flow("msgbox_bad");
f.start();
f.call("call", "act_msgbox_bad");
f.chain("start", "call");
addPage(f);

// queue
f = new Flow("flood");
f.start();
f.call("call", "act_flood");
f.chain("start", "call");
addPage(f);

f = new Flow("flood_catch");
f.start();
f.call("call", "act_flood");
f.add("CatchErrorActionComponent", "catch");
f.log("log", '"flood caught " + m', [["m", "string"]]);
f.wire("start", "call");
f.wire("catch", "log", "Message", "m");
addPage(f);

f = new Flow("flood2");
f.start();
f.call("call", "act_flood2");
f.chain("start", "call");
addPage(f);

// components patched by the tests (only one component of that type in the page)
f = new Flow("patch");
f.start();
f.add("NoopActionComponent", "noop", [], [], { name: "patched" });
f.eval("eval", "1 + 2");
f.log("log", '"patch " + v', [["v", "integer"]]);
f.wire("start", "noop");
f.wire("noop", "eval");
f.wire("eval", "log", "result", "v");
addPage(f);

// float list (dataOperation)
f = new Flow("floatlist");
f.start();
f.setvar("set", ["flow_g_list", "Array.allocate(300)"]);
f.addWidget("DisplayDataWidget", "display", 10, 10, 200, 40, { data: "flow_g_list" });
f.chain("start", "set");
addPage(f);

// references kept by the evaluation stack
f = new Flow("refs");
f.start();
f.setvar("set", ["flow_g_list", "Array.allocate(5)"]);
f.chain("start", "set");
addPage(f);

// evaluation stack drift
f = new Flow("drift");
f.start();
f.eval("eval", "Math.min(1, flow_g_arr[10], 2)", [], { catchError: true });
f.log("result", '"drift result " + v', [["v", "any"]]);
f.setvar("set", ["flow_g_int", "5"]);
f.log("log", '"drift " + flow_g_int');
f.wire("start", "eval");
f.wire("eval", "result", "result", "v");
f.wire("eval", "set", "@error", "@seqin");
f.wire("set", "log");
addPage(f);

// Animate
f = new Flow("animate");
f.start();
f.add("AnimateActionComponent", "animate", [], [], { from: "0", to: "1", speed: "1" });
f.log("log", '"anim done"');
f.chain("start", "animate", "log");
addPage(f);

f = new Flow("animate_neg");
f.start();
f.add("AnimateActionComponent", "animate", [], [], { from: "0", to: "1", speed: "-1" });
f.log("log", '"anim neg done"');
f.chain("start", "animate", "log");
addPage(f);

// LineChart widget component (its execution state collects the points)
f = new Flow("linechart");
f.start();
f.eval("five", "5");
f.addWidget("LineChartEmbeddedWidget", "chart", 10, 10, 320, 160, {
    xValue: "1", maxPoints: 40,
    lines: [{ objID: id("linechart", "line"), label: '"y"', color: "text", width: 1.5, value: "value" }],
    title: '""', showTitle: "false", showLegend: "false", showXAxis: "false", showYAxis: "false",
    showGrid: "false", yAxisRangeOption: "floating", yAxisRangeFrom: "0", yAxisRangeTo: "10",
    margin: { top: 10, right: 10, bottom: 10, left: 10 }, marker: "null",
    customInputs: [{ objID: id("linechart", "value"), name: "value", type: "any" }],
    titleStyle: { useStyle: "default" }, legendStyle: { useStyle: "default" },
    xAxisStyle: { useStyle: "default" }, yAxisStyle: { useStyle: "default" },
    markerStyle: { useStyle: "default" },
});
f.wire("start", "five");
f.wire("five", "chart", "result", "value");
addPage(f);

// widget event handlers
f = new Flow("button");
f.addWidget("ButtonWidget", "button", 100, 100, 200, 60, {
    data: '"Click"',
    eventHandlers: [{ objID: id("button", "handler"), eventName: "CLICKED", handlerType: "flow" }],
});
f.log("log", '"clicked"');
f.wire("button", "log", "CLICKED");
addPage(f);

f = new Flow("button_action");
f.addWidget("ButtonWidget", "button", 100, 100, 200, 60, {
    data: '"Act"',
    eventHandlers: [{
        objID: id("button_action", "handler"), eventName: "CLICKED",
        handlerType: "action", action: "flow_act_click",
    }],
});
addPage(f);

f = new Flow("dropdown");
f.addWidget("DropDownListWidget", "dropdown", 100, 100, 200, 40, {
    data: "flow_g_int", options: '["a", "b"]',
    eventHandlers: [{ objID: id("dropdown", "handler"), eventName: "ON_CHANGE", handlerType: "flow" }],
});
f.log("log", '"dropdown changed"');
f.wire("dropdown", "log", "ON_CHANGE");
addPage(f);

// user widgets
f = new Flow("uw_a");
f.userProperty("label_text", "string");
f.start();
f.log("log", '"uw " + label_text');
f.chain("start", "log");
USER_WIDGETS.push(f.userWidget(200, 50));

f = new Flow("uw_assign");
f.userProperty("target", "integer", true);
f.start();
f.setvar("set", ["target", "77"]);
f.chain("start", "set");
USER_WIDGETS.push(f.userWidget(200, 50));

f = new Flow("uw_in");
f.add("InputActionComponent", "input", [], [], { name: "x", inputType: "integer" });
f.log("log", '"uw in " + v', [["v", "integer"]]);
f.wire("input", "log", "@seqout", "v");
USER_WIDGETS.push(f.userWidget(200, 50));
const UW_IN = f;

const NUM_LAYOUTS = 10;
for (let k = 0; k < NUM_LAYOUTS; k++) {
    f = new Flow(`uw_l${k}`);
    f.start();
    f.log("log", `"layout ${k}"`);
    f.chain("start", "log");
    if (k < 2) {
        f.add("WatchVariableActionComponent", "watch", [], [], { variable: "flow_g_watch" });
        f.log("watch_log", `"w${k} " + v`, [["v", "integer"]]);
        f.wire("watch", "watch_log", "variable", "v");
    }
    USER_WIDGETS.push(f.userWidget(200, 50));
}

f = new Flow("uw_host2");
const uwA = new Flow("uw_a");
['"one"', '"two"'].forEach((value, i) => {
    f.addWidget("UserWidgetWidget", `uw${i}`, 10, 10 + i * 60, 200, 50, {
        userWidgetPageName: "flow_uw_a",
        userPropertyValues: { objID: id("uw_host2", i), values: { [uwA.propId("label_text")]: value } },
    });
});
addPage(f);

f = new Flow("uw_host_assign");
const uwAssign = new Flow("uw_assign");
f.addWidget("UserWidgetWidget", "uw", 10, 10, 200, 50, {
    userWidgetPageName: "flow_uw_assign",
    userPropertyValues: {
        objID: id("uw_host_assign"),
        values: { [uwAssign.propId("target")]: "flow_g_result" },
    },
});
addPage(f);

f = new Flow("uw_host_in");
f.add("WatchVariableActionComponent", "watch", [], [], { variable: "flow_g_watch" });
f.addWidget("UserWidgetWidget", "uw", 10, 10, 200, 50, {
    userWidgetPageName: "flow_uw_in",
    userPropertyValues: { objID: id("uw_host_in"), values: {} },
});
f.wire("watch", "uw", "variable", UW_IN.innerId("input"));
addPage(f);

f = new Flow("uw_dyn");
f.addWidget("UserWidgetWidget", "uw", 10, 10, 200, 50, {
    data: "flow_g_layout",
    userPropertyValues: { objID: id("uw_dyn"), values: {} },
});
addPage(f);
