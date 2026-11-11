"""Flow part: pages, actions and user widgets of the flow engine tests
(tests/flow/). Every name starts with "flow_".

Most pages are small flows that start with a Start component and report what
they do with Log components (harness::logMessages()). A test creates the page
flow state (flow_helpers.h) or shows the page, and runs frames. The page
"flow_vars" is never shown: its SetVariable components only give the tests a
pointer to each global variable of this part (entry 0 is the label, entry 1 the
variable), like the expressions part does.
"""

from fwlib import component, line, native_action, oid, page, variable, widget

PREFIX = "flow_"


def _id(*key):
    return oid("flow-part", *key)


class Flow:
    """A page, action or user widget being built: components, lines, locals."""

    def __init__(self, name):
        self.name = PREFIX + name
        self.components = []
        self.lines = []
        self.locals = []
        self.user_properties = []
        self._n = 0

    def key(self, k):
        return (self.name, k)

    def add(self, type_, k, custom_inputs=(), custom_outputs=(), **props):
        self._n += 1
        c = component(
            type_, self.key(k),
            left=40 + (self._n % 6) * 260, top=40 + (self._n // 6) * 140, width=200, height=80,
            customInputs=[{"objID": _id(self.name, k, "in", name), "name": name, "type": t} for name, t in custom_inputs],
            customOutputs=[{"objID": _id(self.name, k, "out", name), "name": name, "type": t} for name, t in custom_outputs],
            **props)
        self.components.append(c)
        return k

    def add_widget(self, type_, k, left, top, width, height, **props):
        w = widget(type_, self.key(k), left, top, width, height, **props)
        self.components.append(w)
        return k

    def wire(self, src, dst, out="@seqout", inp="@seqin"):
        self.lines.append(line(self.key(src), out, self.key(dst), inp))

    def chain(self, *keys):
        for a, b in zip(keys, keys[1:]):
            self.wire(a, b)

    def local(self, name, type_, default=None):
        self.locals.append(variable(name, type_, default, key=(self.name, "local")))

    def user_property(self, name, type_, assignable=False):
        self.user_properties.append({
            "objID": _id(self.name, "prop", name), "id": self.prop_id(name), "name": name,
            "type": type_, "assignable": assignable, "displayName": name, "description": ""
        })

    def prop_id(self, name):
        return _id(self.name, "prop-id", name)

    def inner_id(self, k):
        """objID of a component, e.g. an Input/Output component (CallAction input/output name)."""
        return oid(self.key(k))

    # components

    def start(self, k="start"):
        return self.add("StartActionComponent", k)

    def end(self, k="end"):
        return self.add("EndActionComponent", k)

    def log(self, k, value, custom_inputs=()):
        return self.add("LogActionComponent", k, custom_inputs, value=value)

    def setvar(self, k, *entries):
        return self.add("SetVariableActionComponent", k, entries=[
            {"objID": _id(self.name, k, "entry", i), "variable": var, "value": value}
            for i, (var, value) in enumerate(entries)])

    def eval(self, k, expression, custom_inputs=(), **props):
        return self.add("EvalExprActionComponent", k, custom_inputs, expression=expression, **props)

    def call(self, k, action, values=None, custom_outputs=(), **props):
        callee = ACTION_FLOWS[action]
        return self.add("CallActionActionComponent", k, (), custom_outputs, action=PREFIX + action, userPropertyValues={
            "objID": _id(self.name, k, "values"),
            "values": {callee.prop_id(name): expr for name, expr in (values or {}).items()}
        }, **props)

    # results

    def page(self, **props):
        p = page(self.name, self.components, self.lines, self.locals, **props)
        p["userProperties"] = self.user_properties
        return p

    def user_widget(self, width, height):
        return self.page(isUsedAsUserWidget=True, width=width, height=height)

    def action(self):
        return {
            "objID": oid("action", self.name), "name": self.name, "implementationType": "flow",
            "components": self.components, "connectionLines": self.lines,
            "localVariables": self.locals, "componentGroups": [],
            "userProperties": self.user_properties
        }


################################################################################
# globals and structures

STRUCTURES = [
    {"objID": _id("struct", "point"), "name": "flow_point", "fields": [
        {"objID": _id("struct", "point", "x"), "name": "x", "type": "integer"},
        {"objID": _id("struct", "point", "y"), "name": "y", "type": "integer"},
    ]},
    {"objID": _id("struct", "item"), "name": "flow_item", "fields": [
        {"objID": _id("struct", "item", "name"), "name": "name", "type": "string"},
        {"objID": _id("struct", "item", "value"), "name": "value", "type": "integer"},
    ]},
]

# (name, type, default value)
_GLOBALS = [
    ("flow_g_int", "integer", "0"),
    ("flow_g_arr", "array:integer", "[1, 2, 3]"),
    ("flow_g_point", "struct:flow_point", "{x: 1, y: 2}"),
    ("flow_g_watch", "integer", "0"),
    ("flow_g_lock", "boolean", "false"),
    ("flow_g_layout", "integer", "0"),
    ("flow_g_result", "integer", "0"),
    ("flow_g_list", "any", "null"),
]

GLOBALS = [variable(name, type_, default) for name, type_, default in _GLOBALS]


def _vars_page():
    """flow_vars: one SetVariable per global (entry 0: label, entry 1: the variable),
    hanging on a page_open event that never fires (the page is never shown)."""
    f = Flow("vars")
    f.local("label", "any")
    f.add("OnEventActionComponent", "on_event", event="page_open")
    for name, _, _ in _GLOBALS:
        f.setvar(name, ("label", '"%s"' % name), (name, name))
        f.wire("on_event", name)
    return f.page()


################################################################################
# actions

ACTION_FLOWS = {}
ACTIONS = []


def _action(f):
    ACTION_FLOWS[f.name[len(PREFIX):]] = f
    return f


def _native(name, props=()):
    a = native_action(PREFIX + name)
    f = Flow(name)
    for prop_name, type_, assignable in props:
        f.user_property(prop_name, type_, assignable)
    a["userProperties"] = f.user_properties
    ACTION_FLOWS[name] = f
    ACTIONS.append(a)


# native actions, implemented in tests/flow/flow_native.cpp
_native("native_props", [("a", "integer", False), ("out", "integer", True)])
_native("native_async")
_native("native_async_out", [("res", "integer", True)])
_native("native_sync")
_native("native_getprop")

# flow actions

f = _action(Flow("act_error"))
f.start()
f.add("ErrorActionComponent", "error", message='"boom"')
f.wire("start", "error")

f = _action(Flow("act_eval_error"))
f.start()
f.eval("eval", "flow_g_arr[10]")
f.log("log", '"act eval result " + v', [("v", "any")])
f.wire("start", "eval")
f.wire("eval", "log", "result", "v")

f = _action(Flow("act_watch"))
f.start()
f.add("WatchVariableActionComponent", "watch", variable="flow_g_watch")
f.log("log", '"aw " + v', [("v", "integer")])
f.wire("start", "watch")
f.wire("watch", "log", "variable", "v")

# the Noop is patched by the test into a custom component (registerComponent)
f = _action(Flow("act_custom_err"))
f.start()
f.add("NoopActionComponent", "custom", name="custom")
f.add("ErrorActionComponent", "error", message='"custom boom"')
f.wire("start", "custom")
f.wire("start", "error")

f = _action(Flow("act_outer"))
f.start()
f.call("call", "act_error")
f.add("CatchErrorActionComponent", "catch")
f.log("log", '"outer caught " + m', [("m", "string")])
f.wire("start", "call")
f.wire("catch", "log", "Message", "m")

f = _action(Flow("act_err_then"))
f.start()
f.add("ErrorActionComponent", "error", message='"stop here"')
f.log("log", '"act continues"')
f.wire("start", "error")
f.wire("start", "log")

f = _action(Flow("act_props"))
f.user_property("in_val", "integer")
f.user_property("out_ref", "integer", assignable=True)
f.start()
f.log("log", '"props in " + in_val')
f.setvar("set", ("out_ref", "in_val * 2"))
f.end()
f.chain("start", "log", "set", "end")

f = _action(Flow("act_io"))
f.add("InputActionComponent", "input", name="a", inputType="integer")
f.eval("eval", "v * 10", [("v", "integer")])
f.add("OutputActionComponent", "output", name="r", outputType="integer")
f.wire("input", "eval", "@seqout", "v")
f.wire("eval", "output", "result", "@seqin")

f = _action(Flow("act_inner"))
f.start()
f.log("log", '"inner"')
f.end()
f.chain("start", "log", "end")

f = _action(Flow("act_outer2"))
f.start()
f.log("log1", '"outer start"')
f.call("call", "act_inner")
f.log("log2", '"outer after inner"')
f.end()
f.chain("start", "log1", "call", "log2", "end")

f = _action(Flow("act_noend"))
f.start()
f.log("log", '"noend body"')
f.chain("start", "log")

f = _action(Flow("act_delay"))
f.start()
f.add("DelayActionComponent", "delay", milliseconds="50")
f.log("log", '"act delay done"')
f.end()
f.chain("start", "delay", "log", "end")

f = _action(Flow("act_click"))
f.start()
f.log("log", '"click action"')
f.add("InputActionComponent", "input", name="e", inputType="struct:$ClickEvent")
f.log("log_event", '"click index " + e.index', [("e", "struct:$ClickEvent")])
f.wire("start", "log")
f.wire("input", "log_event", "@seqout", "e")

f = _action(Flow("act_keypad"))
f.start()
f.add("ShowKeypadActionComponent", "keypad", label='"Amps"', initalValue="0.5", min="0", max="5",
      precision="0.1", unit='"A"')
f.log("log", '"akp " + v', [("v", "float")])
f.end()
f.wire("start", "keypad")
f.wire("keypad", "log", "result", "v")
f.wire("log", "end")

f = _action(Flow("act_keypad_bad"))
f.start()
f.add("ShowKeypadActionComponent", "keypad", label='"Bad"', initalValue="1", min="0", max="5",
      precision="flow_g_arr[10]", unit='"V"')
f.log("log", '"bad kp " + v', [("v", "float")])
f.wire("start", "keypad")
f.wire("keypad", "log", "result", "v")

f = _action(Flow("act_msgbox"))
f.start()
f.add("ShowMessageBoxActionComponent", "msgbox", messageType=1, message='"in action"')
f.end()
f.chain("start", "msgbox", "end")

f = _action(Flow("act_msgbox_bad"))
f.local("bad", "any", "5")
f.start()
f.add("ShowMessageBoxActionComponent", "msgbox", messageType=3, message='"Question?"', buttons="bad")
f.wire("start", "msgbox")

# queue flood: Start -> A -> B1..B11 -> C1..C11 -> D1..D11, every B is wired to
# every C and every C to every D, so the C tasks queue 1331 D tasks (> 1000)
FLOOD_WIDTH = 11


def _flood(name, d_component):
    f = _action(Flow(name))
    f.start()
    f.add("NoopActionComponent", "a", name="a")
    f.wire("start", "a")
    for i in range(FLOOD_WIDTH):
        f.add("NoopActionComponent", "b%d" % i, name="b%d" % i)
        f.add("NoopActionComponent", "c%d" % i, name="c%d" % i)
        d_component(f, "d%d" % i)
        f.wire("a", "b%d" % i)
    for i in range(FLOOD_WIDTH):
        for j in range(FLOOD_WIDTH):
            f.wire("b%d" % i, "c%d" % j)
            f.wire("c%d" % i, "d%d" % j)
    return f


_flood("act_flood", lambda f, k: f.add("NoopActionComponent", k, name=k))

f = _action(Flow("act_two"))
f.add("NoopActionComponent", "n1", name="n1")
f.add("NoopActionComponent", "n2", name="n2")


def _flood2_d(f, k):
    f.call(k, "act_two", catchError=True)
    f.add("NoopActionComponent", k + "_sink", name=k + "_sink")
    f.wire(k, k + "_sink", "@error", "@seqin")


_flood("act_flood2", _flood2_d)

ACTIONS += [f.action() for f in ACTION_FLOWS.values() if f.name[len(PREFIX):] not in
            ("native_props", "native_async", "native_async_out", "native_sync", "native_getprop")]


################################################################################
# pages

PAGES = [_vars_page()]
USER_WIDGETS = []


def _page(f, **props):
    PAGES.append(f.page(**props))
    return f


# sequence, fan-out, components without a connected sequence input
f = Flow("seq")
f.start()
f.log("l1", '"seq 1"')
f.log("l2", '"seq 2"')
f.log("l3", '"seq 3"')
f.log("free", '"seq free"')
f.chain("start", "l1", "l2", "l3")
_page(f)

f = Flow("fanout")
f.start()
f.log("a", '"fan a"')
f.log("b", '"fan b"')
f.wire("start", "a")
f.wire("start", "b")
_page(f)

# SetVariable
f = Flow("setvar")
f.local("loc", "integer", "0")
f.start()
f.setvar("set", ("flow_g_int", "41 + 1"), ("loc", "flow_g_int + 1"), ("flow_g_arr[1]", "99"),
         ("flow_g_point.x", "7"))
f.log("log", '"set " + flow_g_int + " " + loc + " " + flow_g_arr[1] + " " + flow_g_point.x')
f.chain("start", "set", "log")
_page(f)

f = Flow("setvar_oob")
f.start()
f.setvar("set", ("flow_g_arr[10]", "5"))
f.log("log", '"oob after"')
f.chain("start", "set", "log")
_page(f)

f = Flow("eval_oob")
f.start()
f.eval("eval", "flow_g_arr[10]")
f.log("log", '"eval oob " + v', [("v", "any")])
f.wire("start", "eval")
f.wire("eval", "log", "result", "v")
_page(f)

# Switch
f = Flow("switch")
f.local("x", "integer", "7")
f.start()
f.add("SwitchActionComponent", "switch", tests=[
    {"objID": _id("switch", 0), "condition": "x > 10", "outputName": "big", "outputValue": '"big"'},
    {"objID": _id("switch", 1), "condition": "x > 5", "outputName": "mid", "outputValue": '"mid"'},
    {"objID": _id("switch", 2), "condition": "true", "outputName": "small", "outputValue": '"small"'},
])
f.log("value", '"switch " + v', [("v", "string")])
f.log("done", '"switch done"')
f.wire("start", "switch")
for name in ("big", "mid", "small"):
    f.wire("switch", "value", name, "v")
f.wire("switch", "done")
_page(f)

f = Flow("switch_none")
f.local("x", "integer", "1")
f.start()
f.add("SwitchActionComponent", "switch", tests=[
    {"objID": _id("switch_none", 0), "condition": "x > 10", "outputName": "big", "outputValue": '"big"'},
])
f.log("value", '"none " + v', [("v", "string")])
f.log("done", '"switch none done"')
f.wire("start", "switch")
f.wire("switch", "value", "big", "v")
f.wire("switch", "done")
_page(f)

# Compare, IsTrue
f = Flow("compare")
f.start()
f.add("CompareActionComponent", "lt", A="3", B="5", operator="<")
f.add("CompareActionComponent", "between", A="5", B="1", C="10", operator="BETWEEN")
f.add("CompareActionComponent", "eq", A="1", B="2", operator="=")
for k in ("lt", "between", "eq"):
    f.log(k + "_t", '"%s true"' % k)
    f.log(k + "_f", '"%s false"' % k)
    f.wire(k, k + "_t", "True")
    f.wire(k, k + "_f", "False")
f.chain("start", "lt", "between", "eq")
_page(f)

f = Flow("istrue")
f.start()
f.add("IsTrueActionComponent", "t1", value="1 > 2")
f.add("IsTrueActionComponent", "t2", value='"x" == "x"')
for k in ("t1", "t2"):
    f.log(k + "_t", '"%s true"' % k)
    f.log(k + "_f", '"%s false"' % k)
    f.wire(k, k + "_t", "True")
    f.wire(k, k + "_f", "False")
f.log("done", '"istrue done"')
f.chain("start", "t1", "t2", "done")
_page(f)

# Loop, Counter
f = Flow("loop")
f.local("i", "integer", "0")
f.start()
f.add("LoopActionComponent", "loop", variable="i", **{"from": "1"}, to="3", step="1", version=1)
f.log("body", '"loop " + i')
f.log("done", '"loop done"')
f.wire("start", "loop", "@seqout", "start")
f.wire("loop", "body")
f.wire("body", "loop", "@seqout", "next")
f.wire("loop", "done", "done")
_page(f)

f = Flow("loop_down")
f.local("i", "integer", "0")
f.start()
f.add("LoopActionComponent", "loop", variable="i", **{"from": "3"}, to="1", step="-1", version=1)
f.log("body", '"down " + i')
f.log("done", '"down done"')
f.wire("start", "loop", "@seqout", "start")
f.wire("loop", "body")
f.wire("body", "loop", "@seqout", "next")
f.wire("loop", "done", "done")
_page(f)

f = Flow("counter")
f.start()
f.add("CounterActionComponent", "counter", countValue="3")
f.log("body", '"count"')
f.log("done", '"count done"')
f.wire("start", "counter")
f.wire("counter", "body")
f.wire("body", "counter")
f.wire("counter", "done", "done")
_page(f)

# Delay
f = Flow("delay")
f.start()
f.log("before", '"delay start"')
f.add("DelayActionComponent", "delay", milliseconds="100")
f.log("after", '"delay done"')
f.chain("start", "before", "delay", "after")
_page(f)

f = Flow("delay_nan")
f.local("bad", "string", '"abc"')
f.start()
f.add("DelayActionComponent", "delay", milliseconds="bad")
f.log("after", '"nan delay done"')
f.chain("start", "delay", "after")
_page(f)

f = Flow("delay_action")
f.start()
f.call("call", "act_delay")
f.log("after", '"delay action after"')
f.chain("start", "call", "after")
_page(f)

# Watch
f = Flow("watch")
f.start()
f.add("WatchVariableActionComponent", "watch", variable="flow_g_watch")
f.log("log", '"watch " + v', [("v", "integer")])
f.wire("start", "watch")
f.wire("watch", "log", "variable", "v")
_page(f)

f = Flow("watch_action")
f.start()
f.call("call", "act_watch")
f.chain("start", "call")
_page(f)

f = Flow("custom_err")
f.start()
f.call("call", "act_custom_err")
f.add("CatchErrorActionComponent", "catch")
f.log("log", '"custom caught " + m', [("m", "string")])
f.wire("start", "call")
f.wire("catch", "log", "Message", "m")
_page(f)

# Log
f = Flow("log")
f.start()
f.log("number", "40 + 2")
f.log("empty", '""')
f.log("end", '"log end"')
f.chain("start", "number", "empty", "end")
_page(f)

# Error, CatchError, @error
f = Flow("err_callaction_catch")
f.start()
f.call("call", "act_error", catchError=True)
f.log("log", '"caught " + m', [("m", "string")])
f.wire("start", "call")
f.wire("call", "log", "@error", "m")
_page(f)

f = Flow("err_callaction_eval")
f.start()
f.call("call", "act_eval_error", catchError=True)
f.log("log", '"eval caught " + m', [("m", "string")])
f.wire("start", "call")
f.wire("call", "log", "@error", "m")
_page(f)

f = Flow("err_catch_page")
f.start()
f.call("call", "act_error")
f.add("CatchErrorActionComponent", "catch")
f.log("log", '"catch " + m', [("m", "string")])
f.wire("start", "call")
f.wire("catch", "log", "Message", "m")
_page(f)

f = Flow("err_page")
f.start()
f.add("ErrorActionComponent", "error", message='"page boom"')
f.add("CatchErrorActionComponent", "catch")
f.log("log", '"page catch " + m', [("m", "string")])
f.wire("start", "error")
f.wire("catch", "log", "Message", "m")
_page(f)

f = Flow("err_uncaught_action")
f.start()
f.call("call", "act_error")
f.log("after", '"uncaught after"')
f.chain("start", "call", "after")
_page(f)

f = Flow("catch_eval")
f.start()
f.eval("eval", "flow_g_arr[10]")
f.log("result", '"catch eval result " + v', [("v", "any")])
f.add("CatchErrorActionComponent", "catch")
f.log("log", '"catch eval " + m', [("m", "string")])
f.wire("start", "eval")
f.wire("eval", "result", "result", "v")
f.wire("catch", "log", "Message", "m")
_page(f)

f = Flow("eval_error_output")
f.start()
f.eval("eval", "flow_g_arr[10]", catchError=True)
f.log("result", '"error output result " + v', [("v", "any")])
f.log("error", '"error output " + m', [("m", "string")])
f.wire("start", "eval")
f.wire("eval", "result", "result", "v")
f.wire("eval", "error", "@error", "m")
_page(f)

f = Flow("catch_twice")
f.start()
f.eval("e1", "flow_g_arr[10]")
f.eval("e2", "flow_g_arr[11]")
f.log("r1", '"twice result " + v', [("v", "any")])
f.log("r2", '"twice result " + v', [("v", "any")])
f.add("CatchErrorActionComponent", "catch")
f.log("log", '"twice " + m', [("m", "string")])
f.wire("start", "e1")
f.wire("start", "e2")
f.wire("e1", "r1", "result", "v")
f.wire("e2", "r2", "result", "v")
f.wire("catch", "log", "Message", "m")
_page(f)

f = Flow("err_nested")
f.start()
f.call("call", "act_outer")
f.chain("start", "call")
_page(f)

f = Flow("err_drop")
f.start()
f.call("call", "act_err_then")
f.add("CatchErrorActionComponent", "catch")
f.log("log", '"drop caught " + m', [("m", "string")])
f.wire("start", "call")
f.wire("catch", "log", "Message", "m")
_page(f)

f = Flow("err_nonstring")
f.local("num", "any", "42")
f.start()
f.add("ErrorActionComponent", "error", message="num")
f.wire("start", "error")
_page(f)

# LabelOut/LabelIn, Noop, TestAndSet
f = Flow("label")
f.start()
f.add("LabelOutActionComponent", "out", label="flow_l1")
f.add("LabelInActionComponent", "in", label="flow_l1")
f.log("log", '"label in"')
f.wire("start", "out")
f.wire("in", "log")
_page(f)

f = Flow("noop")
f.start()
f.add("NoopActionComponent", "noop", name="pass")
f.log("log", '"noop"')
f.chain("start", "noop", "log")
_page(f)

f = Flow("tas")
f.start()
f.add("TestAndSetActionComponent", "tas", variable="flow_g_lock")
f.log("log", '"tas got"')
f.chain("start", "tas", "log")
_page(f)

# SortArray
f = Flow("sort")
f.local("ints", "array:integer", "[3, 1, 2]")
f.local("big", "array:integer", "[16777217, 16777216]")
f.local("strs", "array:string", '["b", "a", "C"]')
f.local("bad", "any", "5")
f.start()
f.add("SortArrayActionComponent", "asc", array="ints", ascending=True, ignoreCase=True)
f.add("SortArrayActionComponent", "desc", array="ints", ascending=False, ignoreCase=True)
f.add("SortArrayActionComponent", "big", array="big", ascending=True, ignoreCase=True)
f.add("SortArrayActionComponent", "icase", array="strs", ascending=True, ignoreCase=True)
f.add("SortArrayActionComponent", "case", array="strs", ascending=True, ignoreCase=False)
for k, t in (("asc", "array:integer"), ("desc", "array:integer"), ("icase", "array:string"), ("case", "array:string")):
    f.log(k + "_log", '"%s " + v[0] + "," + v[1] + "," + v[2]' % k, [("v", t)])
    f.wire(k, k + "_log", "result", "v")
f.log("big_log", '"big " + v[0] + "," + v[1]', [("v", "array:integer")])
f.wire("big", "big_log", "result", "v")
# SortArray doesn't fire @seqout (see "sort_seq"), so every sort hangs on Start
for k in ("asc", "desc", "big", "icase", "case"):
    f.wire("start", k)
_page(f)

f = Flow("sort_seq")
f.local("ints", "array:integer", "[2, 1]")
f.start()
f.add("SortArrayActionComponent", "sort", array="ints", ascending=True, ignoreCase=True)
f.log("seq", '"sort seqout"')
f.log("result", '"sort result " + v[0] + "," + v[1]', [("v", "array:integer")])
f.wire("start", "sort")
f.wire("sort", "seq")
f.wire("sort", "result", "result", "v")
_page(f)

f = Flow("sort_catch")
f.local("ints", "array:integer", "[2, 1]")
f.start()
f.add("SortArrayActionComponent", "sort", array="ints", ascending=True, ignoreCase=True, catchError=True)
f.log("result", '"sort catch result " + v[0] + "," + v[1]', [("v", "array:integer")])
f.log("error", '"sort catch error"', [("m", "string")])
f.wire("start", "sort")
f.wire("sort", "result", "result", "v")
f.wire("sort", "error", "@error", "m")
_page(f)

f = Flow("sort_struct")
f.local("items", "array:struct:flow_item", '[{name: "x", value: 3}, {name: "y", value: 1}, {name: "z", value: 2}]')
f.start()
f.add("SortArrayActionComponent", "sort", array="items", structureName="flow_item",
      structureFieldName="value", ascending=True, ignoreCase=True)
f.log("log", '"by value " + v[0].name + v[1].name + v[2].name', [("v", "array:struct:flow_item")])
f.wire("start", "sort")
f.wire("sort", "log", "result", "v")
_page(f)

f = Flow("sort_bad")
f.local("bad", "any", "5")
f.start()
f.add("SortArrayActionComponent", "sort", array="bad", ascending=True, ignoreCase=True)
f.log("log", '"sort bad result"', [("v", "any")])
f.wire("start", "sort")
f.wire("sort", "log", "result", "v")
_page(f)

# Constant, EvalExpr
f = Flow("const")
f.start()
f.add("ConstantActionComponent", "const", value='"const ok"')
f.log("const_log", "v", [("v", "string")])
f.eval("eval", "6 * 7")
f.log("eval_log", '"eval " + v', [("v", "integer")])
f.wire("start", "const")
f.wire("const", "const_log", "value", "v")
f.wire("const", "eval")
f.wire("eval", "eval_log", "result", "v")
_page(f)

# CallAction
f = Flow("call_props")
f.local("res", "integer", "0")
f.start()
f.call("call", "act_props", {"in_val": "21", "out_ref": "res"})
f.log("log", '"props res " + res')
f.chain("start", "call", "log")
_page(f)

f = Flow("call_props_bad")
f.local("res", "integer", "0")
f.start()
f.call("call", "act_props", {"in_val": "flow_g_arr[10]", "out_ref": "res"})
f.log("log", '"props bad after"')
f.chain("start", "call", "log")
_page(f)

f = Flow("call_io")
io = ACTION_FLOWS["act_io"]
f.start()
f.eval("four", "4")
f.call("call", "act_io")
f.log("log", '"io " + v', [("v", "integer")])
f.wire("start", "four")
f.wire("four", "call", "result", io.inner_id("input"))
f.wire("call", "log", io.inner_id("output"), "v")
_page(f)

f = Flow("call_nested")
f.start()
f.call("call", "act_outer2")
f.log("log", '"nested done"')
f.chain("start", "call", "log")
_page(f)

f = Flow("call_noend")
f.start()
f.call("call", "act_noend")
f.log("log", '"noend after"')
f.chain("start", "call", "log")
_page(f)

f = Flow("end_page")
f.start()
f.end()
f.chain("start", "end")
_page(f)

# native actions
f = Flow("native_call")
f.local("r", "integer", "0")
f.start()
f.call("call", "native_props", {"a": "5", "out": "r"})
f.log("log", '"native r " + r')
f.chain("start", "call", "log")
_page(f)

f = Flow("native_async_call")
f.start()
f.call("call", "native_async")
f.log("log", '"async after"')
f.chain("start", "call", "log")
_page(f)

f = Flow("native_async_out_call")
f.start()
f.call("call", "native_async_out", {"res": "out"}, [("out", "integer")])
f.log("log", '"async out " + v', [("v", "integer")])
f.wire("start", "call")
f.wire("call", "log", "out", "v")
_page(f)

f = Flow("native_sync_call")
f.start()
f.call("call", "native_sync", None, [("out2", "integer")])
f.log("log", '"sync out " + v', [("v", "integer")])
f.wire("start", "call")
f.wire("call", "log", "out2", "v")
_page(f)

# OnEvent
f = Flow("events")
f.add("OnEventActionComponent", "open", event="page_open")
f.add("OnEventActionComponent", "close", event="page_close")
f.log("open_log", '"evt open"')
f.log("close_log", '"evt close"')
f.wire("open", "open_log")
f.wire("close", "close_log")
_page(f)

f = Flow("keys")
f.add("OnEventActionComponent", "key", event="keydown")
f.log("log", '"key " + v', [("v", "any")])
f.add("NoopActionComponent", "seq", name="seq")
f.wire("key", "log", "event", "v")
f.wire("key", "seq")
_page(f)

# ShowPage
f = Flow("showpage")
f.start()
f.add("ShowPageActionComponent", "show", page="flow_target")
f.log("log", '"showpage done"')
f.chain("start", "show", "log")
_page(f)

f = Flow("target")
f.add("OnEventActionComponent", "open", event="page_open")
f.log("log", '"target open"')
f.wire("open", "log")
_page(f)

# dialogs
f = Flow("keypad")
f.start()
f.add("ShowKeypadActionComponent", "keypad", label='"Volts"', initalValue="1.5", min="0", max="10",
      precision="0.01", unit='"V"')
f.log("ok", '"kp " + v', [("v", "float")])
f.log("cancel", '"kp canceled"')
f.wire("start", "keypad")
f.wire("keypad", "ok", "result", "v")
f.wire("keypad", "cancel", "canceled")
_page(f)

f = Flow("keypad_action")
f.start()
f.call("call", "act_keypad")
f.log("log", '"akp after"')
f.chain("start", "call", "log")
_page(f)

f = Flow("keypad_bad")
f.start()
f.call("call", "act_keypad_bad")
f.chain("start", "call")
_page(f)

f = Flow("keyboard")
f.start()
f.add("ShowKeyboardActionComponent", "keyboard", label='"Name"', initalText='"abc"', minChars="1",
      maxChars="8", password=False)
f.log("ok", '"kb " + v', [("v", "string")])
f.log("cancel", '"kb canceled"')
f.wire("start", "keyboard")
f.wire("keyboard", "ok", "result", "v")
f.wire("keyboard", "cancel", "canceled")
_page(f)

f = Flow("msgbox_info")
f.start()
f.add("ShowMessageBoxActionComponent", "msgbox", messageType=1, message='"hello info"')
f.log("log", '"info closed"')
f.chain("start", "msgbox", "log")
_page(f)

f = Flow("msgbox_error")
f.start()
f.add("ShowMessageBoxActionComponent", "msgbox", messageType=2, message='"hello error"')
f.log("log", '"error closed"')
f.chain("start", "msgbox", "log")
_page(f)

f = Flow("msgbox_question")
f.start()
f.add("ShowMessageBoxActionComponent", "msgbox", messageType=3, message='"Sure?"', buttons='["Yes", "No"]',
      custom_outputs=[("Yes", "any"), ("No", "any")])
f.log("yes", '"q yes"')
f.log("no", '"q no"')
f.wire("start", "msgbox")
f.wire("msgbox", "yes", "Yes")
f.wire("msgbox", "no", "No")
_page(f)

f = Flow("msgbox_action")
f.start()
f.call("call", "act_msgbox")
f.log("log", '"mb action after"')
f.chain("start", "call", "log")
_page(f)

f = Flow("msgbox_bad")
f.start()
f.call("call", "act_msgbox_bad")
f.chain("start", "call")
_page(f)

# queue
f = Flow("flood")
f.start()
f.call("call", "act_flood")
f.chain("start", "call")
_page(f)

f = Flow("flood_catch")
f.start()
f.call("call", "act_flood")
f.add("CatchErrorActionComponent", "catch")
f.log("log", '"flood caught " + m', [("m", "string")])
f.wire("start", "call")
f.wire("catch", "log", "Message", "m")
_page(f)

f = Flow("flood2")
f.start()
f.call("call", "act_flood2")
f.chain("start", "call")
_page(f)

# components patched by the tests (only one component of that type in the page)
f = Flow("patch")
f.start()
f.add("NoopActionComponent", "noop", name="patched")
f.eval("eval", "1 + 2")
f.log("log", '"patch " + v', [("v", "integer")])
f.wire("start", "noop")
f.wire("noop", "eval")
f.wire("eval", "log", "result", "v")
_page(f)

# float list (dataOperation)
f = Flow("floatlist")
f.start()
f.setvar("set", ("flow_g_list", "Array.allocate(300)"))
f.add_widget("DisplayDataWidget", "display", 10, 10, 200, 40, data="flow_g_list")
f.chain("start", "set")
_page(f)

# references kept by the evaluation stack
f = Flow("refs")
f.start()
f.setvar("set", ("flow_g_list", "Array.allocate(5)"))
f.chain("start", "set")
_page(f)

# evaluation stack drift
f = Flow("drift")
f.start()
f.eval("eval", "Math.min(1, flow_g_arr[10], 2)", catchError=True)
f.log("result", '"drift result " + v', [("v", "any")])
f.setvar("set", ("flow_g_int", "5"))
f.log("log", '"drift " + flow_g_int')
f.wire("start", "eval")
f.wire("eval", "result", "result", "v")
f.wire("eval", "set", "@error", "@seqin")
f.wire("set", "log")
_page(f)

# Animate
f = Flow("animate")
f.start()
f.add("AnimateActionComponent", "animate", **{"from": "0"}, to="1", speed="1")
f.log("log", '"anim done"')
f.chain("start", "animate", "log")
_page(f)

f = Flow("animate_neg")
f.start()
f.add("AnimateActionComponent", "animate", **{"from": "0"}, to="1", speed="-1")
f.log("log", '"anim neg done"')
f.chain("start", "animate", "log")
_page(f)

# LineChart widget component (its execution state collects the points)
f = Flow("linechart")
f.start()
f.eval("five", "5")
f.add_widget("LineChartEmbeddedWidget", "chart", 10, 10, 320, 160, xValue="1", maxPoints=40,
             lines=[{"objID": _id("linechart", "line"), "label": '"y"', "color": "text", "width": 1.5, "value": "value"}],
             title='""', showTitle="false", showLegend="false", showXAxis="false", showYAxis="false",
             showGrid="false", yAxisRangeOption="floating", yAxisRangeFrom="0", yAxisRangeTo="10",
             margin={"top": 10, "right": 10, "bottom": 10, "left": 10}, marker="null",
             customInputs=[{"objID": _id("linechart", "value"), "name": "value", "type": "any"}],
             titleStyle={"useStyle": "default"}, legendStyle={"useStyle": "default"},
             xAxisStyle={"useStyle": "default"}, yAxisStyle={"useStyle": "default"},
             markerStyle={"useStyle": "default"})
f.wire("start", "five")
f.wire("five", "chart", "result", "value")
_page(f)

# widget event handlers
f = Flow("button")
f.add_widget("ButtonWidget", "button", 100, 100, 200, 60, data='"Click"',
             eventHandlers=[{"objID": _id("button", "handler"), "eventName": "CLICKED", "handlerType": "flow"}])
f.log("log", '"clicked"')
f.wire("button", "log", "CLICKED")
_page(f)

f = Flow("button_action")
f.add_widget("ButtonWidget", "button", 100, 100, 200, 60, data='"Act"',
             eventHandlers=[{"objID": _id("button_action", "handler"), "eventName": "CLICKED",
                             "handlerType": "action", "action": "flow_act_click"}])
_page(f)

f = Flow("dropdown")
f.add_widget("DropDownListWidget", "dropdown", 100, 100, 200, 40, data="flow_g_int", options='["a", "b"]',
             eventHandlers=[{"objID": _id("dropdown", "handler"), "eventName": "ON_CHANGE", "handlerType": "flow"}])
f.log("log", '"dropdown changed"')
f.wire("dropdown", "log", "ON_CHANGE")
_page(f)

# user widgets
f = Flow("uw_a")
f.user_property("label_text", "string")
f.start()
f.log("log", '"uw " + label_text')
f.chain("start", "log")
USER_WIDGETS.append(f.user_widget(200, 50))

f = Flow("uw_assign")
f.user_property("target", "integer", assignable=True)
f.start()
f.setvar("set", ("target", "77"))
f.chain("start", "set")
USER_WIDGETS.append(f.user_widget(200, 50))

f = Flow("uw_in")
f.add("InputActionComponent", "input", name="x", inputType="integer")
f.log("log", '"uw in " + v', [("v", "integer")])
f.wire("input", "log", "@seqout", "v")
USER_WIDGETS.append(f.user_widget(200, 50))
UW_IN = f

NUM_LAYOUTS = 10
for k in range(NUM_LAYOUTS):
    f = Flow("uw_l%d" % k)
    f.start()
    f.log("log", '"layout %d"' % k)
    f.chain("start", "log")
    if k < 2:
        f.add("WatchVariableActionComponent", "watch", variable="flow_g_watch")
        f.log("watch_log", '"w%d " + v' % k, [("v", "integer")])
        f.wire("watch", "watch_log", "variable", "v")
    USER_WIDGETS.append(f.user_widget(200, 50))

f = Flow("uw_host2")
uw_a = Flow("uw_a")
for i, value in enumerate(('"one"', '"two"')):
    f.add_widget("UserWidgetWidget", "uw%d" % i, 10, 10 + i * 60, 200, 50, userWidgetPageName="flow_uw_a",
                 userPropertyValues={"objID": _id("uw_host2", i), "values": {uw_a.prop_id("label_text"): value}})
_page(f)

f = Flow("uw_host_assign")
uw_assign = Flow("uw_assign")
f.add_widget("UserWidgetWidget", "uw", 10, 10, 200, 50, userWidgetPageName="flow_uw_assign",
             userPropertyValues={"objID": _id("uw_host_assign"),
                                 "values": {uw_assign.prop_id("target"): "flow_g_result"}})
_page(f)

f = Flow("uw_host_in")
f.add("WatchVariableActionComponent", "watch", variable="flow_g_watch")
f.add_widget("UserWidgetWidget", "uw", 10, 10, 200, 50, userWidgetPageName="flow_uw_in",
             userPropertyValues={"objID": _id("uw_host_in"), "values": {}})
f.wire("watch", "uw", "variable", UW_IN.inner_id("input"))
_page(f)

f = Flow("uw_dyn")
f.add_widget("UserWidgetWidget", "uw", 10, 10, 200, 50, data="flow_g_layout",
             userPropertyValues={"objID": _id("uw_dyn"), "values": {}})
_page(f)
