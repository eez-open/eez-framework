#!/usr/bin/env python3
"""Generates the external test project of the eez-framework tests.

    python make_fwtest_ext.py

writes fwtest_ext/fwtest_ext.eez-project: a module project with fwtest as its
master project, like a BB3+ module project (settings.general.masterProject) with
the BB3+ project. Its own pages, styles, fonts, bitmaps and colors get negative
indexes in the assets (styles, fonts, bitmaps -1, -2, ...; colors 65534 - k;
font tags U+E002), the master styles it uses keep their ids. It has no
bb3ModuleMetadata, so Studio compresses the assets with LZ4 (not LZMA) and the
framework's decompressAssetsData can load them.

Build it with EEZ Studio afterwards (build_projects.ps1, after fwtest), which
writes fwtest_ext/generated/ext_assets.h and ext_assets.cpp: the assets array
and the enums in the namespace fwtest_ext, so they link next to the main
assets. The tests (tests/flow/external) load them like the BB3+ loads module
assets.

Names: the project items start with mod_, the master items the project uses
start with ext_ (parts/external.py). The tags of the project items (fwlib.tags)
have the area "external", like the items of parts/external.py.
"""

import copy
import json
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)

from fwlib import (  # noqa: E402
    DISPLAY_HEIGHT, DISPLAY_WIDTH, component, event_handler, flow_action, line, load_font, oid,
    page, rectangle, style, tag_items, tags, text, variable, widget,
)

# the area tag of the project items
AREA = "external"

################################################################################
# build files

EXT_ASSETS_H = """\
#pragma once

#include <stdint.h>

namespace fwtest_ext {

//${eez-studio GUI_PAGES_ENUM}

//${eez-studio GUI_STYLES_ENUM}

//${eez-studio GUI_FONTS_ENUM}

//${eez-studio GUI_BITMAPS_ENUM}

//${eez-studio FLOW_GLOBAL_VARIABLES_ENUM}

//${eez-studio GUI_ASSETS_DECL_COMPRESSED}

} // namespace fwtest_ext
"""

EXT_ASSETS_CPP = """\
#include "ext_assets.h"

namespace fwtest_ext {

//${eez-studio GUI_ASSETS_DEF_COMPRESSED}

} // namespace fwtest_ext
"""

################################################################################
# colors: (name, value for the master theme "Default"), 32-bit colors

COLORS = [
    ("mod_fill", "#40c080"),
    ("mod_line", "#c04000"),
    # with alpha 128
    ("mod_fill_a128", "#40c08080"),
]

# a color that is not a project color: a custom color of the external assets
CUSTOM_COLOR = "#8040c0"

# the color of the bitmap
BITMAP_COLOR = (0x80, 0xC0, 0x40)

STYLES = [
    style("mod_fill_box", backgroundColor="mod_fill"),
    style("mod_line_box", backgroundColor="mod_line"),
    style("mod_custom_box", backgroundColor=CUSTOM_COLOR),
    style("mod_alpha_box", backgroundColor="mod_fill_a128"),
    style("mod_master_color_box", backgroundColor="bookmark"),
    style("mod_icons_text", font="mod_icons"),
    # the style of the dynamic user widget of mod_host
    style("mod_uw_frame", backgroundColor="mod_line"),
]

GLOBALS = [
    variable("mod_counter", "integer", "0"),
    variable("mod_opened", "integer", "0"),
    variable("mod_main_layout", "integer", "0"),
    variable("mod_uw_started", "integer", "0"),
    variable("mod_trigger", "integer", "0"),
    variable("mod_watch_hits", "integer", "0"),
    variable("mod_caught", "string", "\"\""),
]


################################################################################
# fonts and bitmaps

def rekey(obj, prefix):
    """A copy of obj with new objIDs (the copied fonts must not share them)."""
    obj = copy.deepcopy(obj)

    def walk(o):
        if isinstance(o, dict):
            if "objID" in o:
                o["objID"] = oid(prefix, o["objID"])
            for v in o.values():
                walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)

    walk(obj)
    return obj


def font(name, source):
    f = rekey(load_font(source), ("font", name))
    f["name"] = name
    f["alwaysBuild"] = True
    return f


def png(width, height, rgb):
    """A solid color RGB PNG as a data URL."""
    row = b"\x00" + bytes(rgb) * width
    raw = row * height

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw))
    data += chunk(b"IEND", b"")
    import base64
    return "data:image/png;base64," + base64.b64encode(data).decode("ascii")


BITMAPS = [
    {
        "objID": oid("bitmap", "mod_bitmap"), "name": "mod_bitmap",
        "image": png(16, 16, BITMAP_COLOR), "bpp": 16, "alwaysBuild": True,
    },
]


################################################################################
# actions

def _action_inc():
    start = ("mod_inc", "start")
    inc = ("mod_inc", "set")
    return flow_action("mod_inc", [
        component("StartActionComponent", start, width=100, height=40),
        component(
            "SetVariableActionComponent", inc, left=200, width=300, height=70,
            entries=[{"objID": oid(*inc, "entry"), "variable": "mod_counter", "value": "mod_counter + 1"}],
        ),
    ], [line(start, "@seqout", inc, "@seqin")])


def _action_fail():
    start = ("mod_fail", "start")
    error = ("mod_fail", "error")
    return flow_action("mod_fail", [
        component("StartActionComponent", start, width=100, height=40),
        component("ErrorActionComponent", error, left=200, width=200, height=50, message="\"mod failure\""),
    ], [line(start, "@seqout", error, "@seqin")])


ACTIONS = [_action_inc(), _action_fail()]


################################################################################
# pages

def probes(prefix, cases, left=600):
    """Probe components: SetVariable components on the page_close event, see
    parts/external.py."""
    event_key = (prefix, "probe_event")
    components = [component("OnEventActionComponent", event_key, left=left, top=0, event="page_close")]
    lines = []
    for i, (name, expression) in enumerate(cases):
        key = (prefix, "probe", name)
        components.append(component(
            "SetVariableActionComponent", key, left=left + 200, top=i * 80, width=300, height=70,
            entries=[
                {"objID": oid(*key, "label"), "variable": "probe_label", "value": "\"probe:%s\"" % name},
                {"objID": oid(*key, "value"), "variable": "probe_result", "value": expression},
            ],
        ))
        lines.append(line(event_key, "@seqout", key, "@seqin"))
    local_variables = [variable(name, "any", key=(prefix, "probe_var")) for name in ("probe_label", "probe_result")]
    return components, lines, local_variables


def on_event_set(prefix, event, variable_name, value):
    """OnEvent -> SetVariable variable_name = value"""
    event_key = (prefix, event)
    set_key = (prefix, event, "set")
    return [
        component("OnEventActionComponent", event_key, left=600, top=400, event=event),
        component(
            "SetVariableActionComponent", set_key, left=800, top=400, width=300, height=70,
            entries=[{"objID": oid(*set_key, "entry"), "variable": variable_name, "value": value}],
        ),
    ], [line(event_key, "@seqout", set_key, "@seqin")]


def user_widget(key, left, top, width, height, style_="default", data=None, page_name=""):
    props = {
        "userWidgetPageName": page_name,
        "userPropertyValues": {"objID": oid(key, "values"), "values": {}},
    }
    if data:
        props["data"] = data
    return widget("UserWidgetWidget", key, left, top, width, height, style_, **props)


def button(key, left, top, width, height, label, action):
    return widget(
        "ButtonWidget", key, left, top, width, height, "default", text=label,
        eventHandlers=[event_handler(key, "CLICKED", action=action)],
    )


def _mod_host():
    # the first page (index 0): its page id 1 is also PAGE_ID_MAIN of the
    # main assets
    probe_components, probe_lines, probe_locals = probes("mod_host", [
        ("active", "Flow.isPageActive()"),
    ])
    return page(
        "mod_host",
        [
            # siblings before and after a user widget with a main layout
            rectangle("mod_host/before", 300, 100, 100, 50, "mod_fill_box"),
            user_widget("mod_host/uw", 0, 0, 200, 100, "mod_uw_frame", data="mod_main_layout"),
            rectangle("mod_host/after", 300, 0, 100, 50, "mod_fill_box"),
        ] + probe_components,
        probe_lines,
        local_variables=probe_locals,
    )


def _mod_main():
    probe_components, probe_lines, probe_locals = probes("mod_main", [
        ("active", "Flow.isPageActive()"),
        ("bitmap", "Flow.getBitmapIndex(\"mod_bitmap\")"),
        ("local", "mod_local"),
        ("counter", "mod_counter"),
    ])
    open_components, open_lines = on_event_set("mod_main", "page_open", "mod_opened", "mod_opened + 1")
    return page(
        "mod_main",
        [
            rectangle("mod_main/fill", 10, 10, 100, 50, "mod_fill_box"),
            rectangle("mod_main/master_style", 120, 10, 100, 50, "ext_m_box"),
            rectangle("mod_main/custom", 230, 10, 100, 50, "mod_custom_box"),
            rectangle("mod_main/master_color", 340, 10, 100, 50, "mod_master_color_box"),
            widget("BitmapWidget", "mod_main/bitmap", 450, 10, 16, 16, "default", bitmap="mod_bitmap"),
            # U+00C3 is a glyph of mod_icons (Icons_WaveformS), not of mod_roboto
            text("mod_main/icons", 10, 80, 200, 40, "Ã", "mod_icons_text"),
            widget(
                "TextWidget", "mod_main/tag", 220, 80, 200, 40, "default",
                data="\"<font name='mod_icons'>Ã</font>\"",
            ),
            text("mod_main/no_glyph", 430, 80, 200, 40, "Ã"),
            button("mod_main/inc", 10, 150, 150, 50, "inc", "mod_inc"),
            button("mod_main/fail", 170, 150, 150, 50, "fail", "mod_fail"),
        ] + open_components + probe_components,
        open_lines + probe_lines,
        local_variables=[variable("mod_local", "integer", "7", key=("mod_main", "var"))] + probe_locals,
    )


def _mod_catch():
    catch = ("mod_catch", "catch")
    set_key = ("mod_catch", "set")
    return page(
        "mod_catch",
        [
            button("mod_catch/fail", 170, 150, 150, 50, "fail", "mod_fail"),
            component("CatchErrorActionComponent", catch, left=600, top=0, width=150, height=50),
            component(
                "SetVariableActionComponent", set_key, left=800, top=0, width=300, height=70,
                customInputs=[{"objID": oid(*set_key, "msg"), "name": "msg", "type": "string"}],
                entries=[{"objID": oid(*set_key, "entry"), "variable": "mod_caught", "value": "msg"}],
            ),
        ],
        [line(catch, "Message", set_key, "msg")],
    )


def _mod_end():
    event_key = ("mod_end", "page_open")
    end_key = ("mod_end", "end")
    return page(
        "mod_end",
        [
            component("OnEventActionComponent", event_key, left=600, top=0, event="page_open"),
            component("EndActionComponent", end_key, left=800, top=0, width=100, height=40),
        ],
        [line(event_key, "@seqout", end_key, "@seqin")],
    )


def _mod_err_page():
    event_key = ("mod_err_page", "page_open")
    error = ("mod_err_page", "error")
    return page(
        "mod_err_page",
        [
            component("OnEventActionComponent", event_key, left=600, top=0, event="page_open"),
            component("ErrorActionComponent", error, left=800, top=0, width=200, height=50, message="\"page failure\""),
        ],
        [line(event_key, "@seqout", error, "@seqin")],
    )


def _mod_uw():
    probe_components, probe_lines, probe_locals = probes("mod_uw", [("active", "Flow.isPageActive()")])
    start = ("mod_uw", "start")
    started = ("mod_uw", "started")
    watch = ("mod_uw", "watch")
    hits = ("mod_uw", "hits")
    return page(
        "mod_uw",
        [
            rectangle("mod_uw/box", 0, 0, 100, 50, "mod_fill_box"),
            button("mod_uw/inc", 100, 50, 100, 50, "inc", "mod_inc"),
            component("StartActionComponent", start, left=300, top=0, width=100, height=40),
            component(
                "SetVariableActionComponent", started, left=450, top=0, width=300, height=70,
                entries=[{"objID": oid(*started, "entry"), "variable": "mod_uw_started", "value": "mod_uw_started + 1"}],
            ),
            # the watched expression fails for mod_trigger > 3 (array index)
            component(
                "WatchVariableActionComponent", watch, left=300, top=100, width=150, height=50,
                variable="[0, 1, 2, 3][mod_trigger]",
            ),
            component(
                "SetVariableActionComponent", hits, left=450, top=100, width=300, height=70,
                entries=[{"objID": oid(*hits, "entry"), "variable": "mod_watch_hits", "value": "mod_watch_hits + 1"}],
            ),
        ] + probe_components,
        [line(start, "@seqout", started, "@seqin"), line(watch, "variable", hits, "@seqin")] + probe_lines,
        local_variables=probe_locals,
        width=200, height=100, isUsedAsUserWidget=True,
    )


PAGES = [_mod_host(), _mod_main(), _mod_catch(), _mod_end(), _mod_err_page()]

USER_WIDGETS = [_mod_uw()]


################################################################################

def project():
    return {
        "objID": oid("ext", "project"),
        "settings": {
            "objID": oid("ext", "settings"),
            "general": {
                "objID": oid("ext", "settings", "general"),
                "projectVersion": "v3",
                "projectType": "firmware",
                "masterProject": "..\\fwtest\\fwtest.eez-project",
                "flowSupport": True,
                "displayWidth": DISPLAY_WIDTH,
                "displayHeight": DISPLAY_HEIGHT,
                "colorBpp": "32",
                "bitmapColorFormat": "RGB",
            },
            "build": {
                "objID": oid("ext", "settings", "build"),
                "configurations": [{"objID": oid("ext", "configuration", "default"), "name": "default"}],
                "files": [
                    {"objID": oid("ext", "file", "ext_assets.h"), "fileName": "ext_assets.h", "template": EXT_ASSETS_H},
                    {"objID": oid("ext", "file", "ext_assets.cpp"), "fileName": "ext_assets.cpp", "template": EXT_ASSETS_CPP},
                ],
                "destinationFolder": "generated",
            },
        },
        "variables": {"globalVariables": tag_items(GLOBALS, AREA), "structures": [], "enums": []},
        "actions": tag_items(ACTIONS, AREA),
        "userPages": tag_items(PAGES, AREA),
        "userWidgets": tag_items(USER_WIDGETS, AREA),
        # new objIDs: the master project has a "default" style too
        "styles": tag_items([rekey(style(
            "default",
            font="mod_roboto",
            color="text",
            backgroundColor="background",
            alignHorizontal="center",
            alignVertical="center",
            children=STYLES,
        ), ("ext", "style"))], AREA),
        "fonts": tag_items([font("mod_roboto", "RobotoCondensed"), font("mod_icons", "Icons_WaveformS")], AREA),
        "bitmaps": tag_items(BITMAPS, AREA),
        # module colors: a value for each master theme (by name)
        "colors": [
            {"objID": oid("ext", "color", name), "name": name, "tags": tags(name, AREA), "themeValues": {"Default": value}}
            for name, value in COLORS
        ],
        "themes": [],
    }


def main():
    path = os.path.join(HERE, "fwtest_ext", "fwtest_ext.eez-project")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(project(), indent=2, ensure_ascii=False) + "\n")
    print("wrote", os.path.relpath(path, HERE))


if __name__ == "__main__":
    main()
