"""Expressions part, extra items: the globals, structures, enums, texts and the
bitmap the expression cases use (expression_cases.py), the native variables
the expression tests implement (expressions/expressions_native.cpp), and the
pages of the expression tests that run a flow (expressions/expressions_flow_test.cpp).

The globals g_* of base.py are used as they are.
"""

import base64
import struct
import zlib

from fwlib import component, line, oid, page, variable

################################################################################
# structures and enums

STRUCTURES = [
    {
        "objID": oid("struct", "expr_Point"),
        "name": "expr_Point",
        "fields": [
            {"objID": oid("struct", "expr_Point", "x"), "name": "x", "type": "integer"},
            {"objID": oid("struct", "expr_Point", "y"), "name": "y", "type": "integer"},
        ],
    },
    {
        "objID": oid("struct", "expr_Rect"),
        "name": "expr_Rect",
        "fields": [
            {"objID": oid("struct", "expr_Rect", "name"), "name": "name", "type": "string"},
            {"objID": oid("struct", "expr_Rect", "p"), "name": "p", "type": "struct:expr_Point"},
            {"objID": oid("struct", "expr_Rect", "tags"), "name": "tags", "type": "array:string"},
            {"objID": oid("struct", "expr_Rect", "w"), "name": "w", "type": "double"},
        ],
    },
]

ENUMS = [
    {
        "objID": oid("enum", "expr_Mode"),
        "name": "expr_Mode",
        "members": [
            {"objID": oid("enum", "expr_Mode", "Off"), "name": "Off", "value": 0},
            {"objID": oid("enum", "expr_Mode", "On"), "name": "On", "value": 1},
            {"objID": oid("enum", "expr_Mode", "Auto"), "name": "Auto", "value": 5},
            # a negative int32 constant (unary minus computes its value)
            {"objID": oid("enum", "expr_Mode", "Neg"), "name": "Neg", "value": -7},
        ],
    },
]

################################################################################
# globals

GLOBALS = [
    variable("expr_float", "float", "2.5"),
    variable("expr_str_array", "array:string", "[\"a\", \"b\", \"c\"]"),
    variable("expr_dbl_array", "array:double", "[0.5, 1.5]"),
    variable("expr_empty_array", "array:integer", "[]"),
    variable("expr_matrix", "array:array:integer", "[[1, 2], [3, 4]]"),
    variable("expr_point", "struct:expr_Point", "{x: 1, y: 2}"),
    variable("expr_rect", "struct:expr_Rect", "{name: \"r\", p: {x: 3, y: 4}, tags: [\"t1\", \"t2\"], w: 1.5}"),
    variable("expr_points", "array:struct:expr_Point", "[{x: 1, y: 2}, {x: 5, y: 6}]"),
    variable("expr_mode", "enum:expr_Mode", "expr_Mode.Auto"),

    # native variables, implemented in expressions/expressions_native.cpp: they
    # give the expressions values the project types can't hold (uint32 above
    # 2^31, int64, the narrow integer types, a float with a unit, undefined,
    # a dynamic array)
    variable("expr_nat_u32", "integer", native=True),
    variable("expr_nat_i64", "integer", native=True),
    variable("expr_nat_u8", "integer", native=True),
    variable("expr_nat_i8", "integer", native=True),
    variable("expr_nat_i16", "integer", native=True),
    variable("expr_nat_volt", "float", native=True),
    variable("expr_nat_undef", "integer", native=True),
    variable("expr_nat_dyn", "array:integer", native=True),
]

################################################################################
# texts (Flow.languages, Flow.translate, T"..." and SelectLanguage)

TEXTS = {
    "languages": ["en", "de"],
    "resources": [
        ("expr_hello", {"en": "Hello", "de": "Hallo"}),
        ("expr_bye", {"en": "Bye", "de": "Tschuess"}),
    ],
}

################################################################################
# a bitmap for Flow.getBitmapIndex


def _png(width, height, rgba):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(rgba) * width for _ in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


BITMAPS = [
    {
        "objID": oid("bitmap", "expr_dot"),
        "name": "expr_dot",
        "image": "data:image/png;base64," + base64.b64encode(_png(2, 2, (255, 0, 0, 255))).decode("ascii"),
        "bpp": 32,
        "alwaysBuild": True,
    }
]

################################################################################
# pages with a flow that runs when the page is shown


def _on_open_page(name, target):
    """A page whose "page open" event runs the target component."""
    on_event = component("OnEventActionComponent", (name, "on_event"), event="page_open")
    return [on_event, target], [line((name, "on_event"), "@seqout", (name, "target"), "@seqin")]


def _active_page():
    components, lines = _on_open_page("expr_active", component(
        "SetVariableActionComponent", ("expr_active", "target"), left=200, width=400, height=70,
        entries=[{"objID": oid("expr_active", "entry"), "variable": "active", "value": "Flow.isPageActive()"}],
    ))
    return page("expr_active", components, lines, local_variables=[
        variable("active", "boolean", "false", key=("expr_active", "var"))
    ])


def _bad_theme_page():
    # SetColorTheme with a theme expression that is not a string (O14)
    components, lines = _on_open_page("expr_bad_theme", component(
        "SetColorThemeActionComponent", ("expr_bad_theme", "target"), left=200, theme="5"))
    return page("expr_bad_theme", components, lines)


def _bad_language_page():
    # SelectLanguage with a language expression that is not a string (O14)
    components, lines = _on_open_page("expr_bad_language", component(
        "SelectLanguageActionComponent", ("expr_bad_language", "target"), left=200, language="5"))
    return page("expr_bad_language", components, lines)


PAGES = [_active_page(), _bad_theme_page(), _bad_language_page()]
