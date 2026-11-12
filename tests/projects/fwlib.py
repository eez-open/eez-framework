"""Helpers that build .eez-project JSON objects for the test projects.

See the eez-project skill (studio/.claude/skills/eez-project) for the format.
Every object gets an objID derived from its key, so a regenerated project is the
same file. Names of pages, actions, variables, styles and colors are global in
the project: a part prefixes them with its area (flow_, gui_, ...).
"""

import json
import os
import re
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
NAMESPACE = uuid.UUID("6b1f3c1e-6c39-4a5e-9f63-2d1b9a1e0001")

DISPLAY_WIDTH = 800
DISPLAY_HEIGHT = 480


def oid(*key):
    return str(uuid.uuid5(NAMESPACE, "/".join(str(k) for k in key)))


def load_font(name):
    with open(os.path.join(HERE, "fonts", name + ".json"), encoding="utf-8") as f:
        return json.load(f)


################################################################################
# tags: the "tags" property of the project items, used only for searching in
# Studio ("tag:flow" finds the items of the flow part)

# topic tags by the item name, after the area tag
TOPICS = [
    (r"err|catch|fail", "errors"),
    (r"(^|_)uw(_|\d|$)|user_widget", "user-widget"),
    (r"native", "native"),
    (r"keypad|keyboard", "keypad"),
    (r"msgbox|yes_no|popup|dialog|alert|menu_with_buttons|async_operation", "popups"),
    (r"probe", "probe"),
]


def tags(name, area):
    """The tags of an item: the area (base, flow, gui, ...) and its topics."""
    result = [area]
    for pattern, topic in TOPICS:
        if re.search(pattern, name) and topic not in result:
            result.append(topic)
    return ", ".join(result)


def set_tags(obj, value, anchor="name"):
    """Puts obj["tags"] right after the anchor key, where Studio writes it."""
    items = [(k, v) for k, v in obj.items() if k != "tags"]
    obj.clear()
    for k, v in items:
        obj[k] = v
        if k == anchor:
            obj["tags"] = value
    return obj


def tag_items(items, area):
    """Tags the items (with a name) of an area, their local variables and child
    styles too. Returns items."""
    for item in items:
        set_tags(item, tags(item["name"], area))
        for v in item.get("localVariables", []):
            set_tags(v, tags(v["name"], area))
        tag_items(item.get("childStyles", []), area)
    return items


################################################################################
# styles, variables, actions

def style(name, children=(), **props):
    """A style; props are style keys (font, color, backgroundColor, borderSize, ...)."""
    s = {"objID": oid("style", name), "name": name, "alwaysBuild": True}
    s.update(props)
    s["childStyles"] = list(children)
    return s


def variable(name, type_, default_value=None, native=False, key="var"):
    v = {"objID": oid(key, name), "name": name, "type": type_, "native": native}
    if default_value is not None:
        v["defaultValue"] = default_value
    return v


def native_action(name):
    return {
        "objID": oid("action", name), "name": name, "implementationType": "native",
        "components": [], "connectionLines": [], "localVariables": [],
        "componentGroups": [], "userProperties": []
    }


def flow_action(name, components, connection_lines=(), local_variables=(), user_properties=()):
    return {
        "objID": oid("action", name), "name": name, "implementationType": "flow",
        "components": list(components), "connectionLines": list(connection_lines),
        "localVariables": list(local_variables), "componentGroups": [],
        "userProperties": list(user_properties)
    }


################################################################################
# pages and widgets

def page(name, components, connection_lines=(), local_variables=(), **props):
    p = {
        "objID": oid("page", name), "name": name,
        "left": 0, "top": 0, "width": DISPLAY_WIDTH, "height": DISPLAY_HEIGHT,
        "components": list(components), "connectionLines": list(connection_lines),
        "localVariables": list(local_variables), "userProperties": [], "componentGroups": [],
    }
    p.update(props)
    return p


def widget(type_, key, left, top, width, height, style_="default", **props):
    """Any widget; props are its keys (data, text, widgets, eventHandlers, ...)."""
    w = {
        "type": type_, "objID": oid(key), "left": left, "top": top,
        "width": width, "height": height, "style": {"useStyle": style_}
    }
    w.update(props)
    return w


def text(key, left, top, width, height, text_, style_="default", **props):
    return widget("TextWidget", key, left, top, width, height, style_, text=text_, **props)


def rectangle(key, left, top, width, height, style_, **props):
    return widget("RectangleWidget", key, left, top, width, height, style_, **props)


def event_handler(key, event_name="CLICKED", action=None):
    """A widget event handler: to an action (by name), or a flow output when action is None."""
    if action:
        return {"objID": oid(key, "handler", event_name), "eventName": event_name, "handlerType": "action", "action": action}
    return {"objID": oid(key, "handler", event_name), "eventName": event_name, "handlerType": "flow"}


################################################################################
# flow components

def component(type_, key, left=0, top=0, width=150, height=50, **props):
    """Any action component (StartActionComponent, LogActionComponent, ...)."""
    c = {"type": type_, "objID": oid(key), "left": left, "top": top, "width": width, "height": height}
    c.update(props)
    return c


def line(source_key, output, target_key, input_):
    """A connection line between components given by their keys."""
    return {
        "objID": oid("line", source_key, output, target_key, input_),
        "source": oid(source_key), "output": output,
        "target": oid(target_key), "input": input_
    }
