#!/usr/bin/env python3
"""Generates the main test project of the eez-framework tests.

    python make_fwtest.py

writes
    fwtest/fwtest.eez-project             main assets of the test executable
    fwtest/generated/expression_cases.inc the expression cases for the C++ test

Build the project with EEZ Studio afterwards (build_projects.ps1), which writes
fwtest/generated/document.h and document.cpp. Commit all of these files, so the
tests can be built without Studio.

The project is put together from the parts in parts/: base.py first, then the
others by file name. A part is a module that may define any of
    COLORS        [(name, "#rrggbb")]
    STYLES        styles under the "default" style (fwlib.style)
    GLOBALS       global variables (fwlib.variable)
    STRUCTURES    structures (see the eez-project skill, variables.md)
    ENUMS         enums
    ACTIONS       actions (fwlib.native_action, fwlib.flow_action)
    PAGES         pages (fwlib.page)
    USER_WIDGETS  user widgets (pages with isUsedAsUserWidget)
    BITMAPS       bitmaps
    TEXTS         {"languages": [languageID], "resources": [(resourceID, {languageID: text})]}
"""

import glob
import importlib
import json
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "parts"))

from expression_cases import CASES, UNDEFINED, F  # noqa: E402
from fwlib import DISPLAY_HEIGHT, DISPLAY_WIDTH, load_font, oid, style  # noqa: E402


def load_parts():
    names = sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(HERE, "parts", "*.py")))
    names.remove("base")
    return [importlib.import_module(name) for name in ["base"] + names]


def collect(parts, attribute):
    result = []
    for part in parts:
        result.extend(getattr(part, attribute, []))
    return result


################################################################################
# build files

DOCUMENT_H = """\
#pragma once

#include <eez/gui/data.h>
#include <eez/gui/widget.h>

namespace eez {
namespace gui {

//${eez-studio DATA_ENUM}

//${eez-studio DATA_FUNCS_DECL}

//${eez-studio DATA_ARRAY_DECL}

//${eez-studio ACTIONS_ENUM}

//${eez-studio ACTIONS_FUNCS_DECL}

//${eez-studio ACTIONS_ARRAY_DECL}

//${eez-studio GUI_FONTS_ENUM}

//${eez-studio GUI_BITMAPS_ENUM}

//${eez-studio GUI_STYLES_ENUM}

//${eez-studio GUI_THEMES_ENUM}

//${eez-studio GUI_COLORS_ENUM}

//${eez-studio GUI_PAGES_ENUM}

//${eez-studio FLOW_STRUCTS}

//${eez-studio GUI_ASSETS_DECL_COMPRESSED}

} // namespace gui
} // namespace eez
"""

DOCUMENT_CPP = """\
#include <eez/conf-internal.h>

#include "document.h"

namespace eez {
namespace gui {

//${eez-studio DATA_ARRAY_DEF}

//${eez-studio ACTIONS_ARRAY_DEF}

//${eez-studio GUI_ASSETS_DEF_COMPRESSED}

} // namespace gui
} // namespace eez
"""


def texts(parts):
    """The project texts from the TEXTS of the parts, None when no part has any."""
    languages = []
    resources = []
    for part in parts:
        part_texts = getattr(part, "TEXTS", None)
        if part_texts:
            languages.extend(language for language in part_texts["languages"] if language not in languages)
            resources.extend(part_texts["resources"])
    if not languages:
        return None
    return {
        "objID": oid("texts"),
        "languages": [{"objID": oid("language", language), "languageID": language} for language in languages],
        "resources": [
            {
                "objID": oid("text", resource_id),
                "resourceID": resource_id,
                "translations": [
                    {"objID": oid("text", resource_id, language), "languageID": language, "text": text_}
                    for language, text_ in translations.items()
                ],
            }
            for resource_id, translations in resources
        ],
    }


def project(parts):
    colors = collect(parts, "COLORS")
    result = {
        "objID": oid("project"),
        "settings": {
            "objID": oid("settings"),
            "general": {
                "objID": oid("settings", "general"),
                "projectVersion": "v3",
                "projectType": "firmware",
                "flowSupport": True,
                "displayWidth": DISPLAY_WIDTH,
                "displayHeight": DISPLAY_HEIGHT,
                "colorBpp": "16",
                "bitmapColorFormat": "RGB",
            },
            "build": {
                "objID": oid("settings", "build"),
                "configurations": [{"objID": oid("configuration", "default"), "name": "default"}],
                "files": [
                    {"objID": oid("file", "document.h"), "fileName": "document.h", "template": DOCUMENT_H},
                    {"objID": oid("file", "document.cpp"), "fileName": "document.cpp", "template": DOCUMENT_CPP},
                ],
                "destinationFolder": "generated",
            },
        },
        "variables": {
            "globalVariables": collect(parts, "GLOBALS"),
            "structures": collect(parts, "STRUCTURES"),
            "enums": collect(parts, "ENUMS"),
        },
        "actions": collect(parts, "ACTIONS"),
        "userPages": collect(parts, "PAGES"),
        "userWidgets": collect(parts, "USER_WIDGETS"),
        "styles": [
            style(
                "default",
                font="RobotoCondensed",
                color="text",
                backgroundColor="background",
                alignHorizontal="center",
                alignVertical="center",
                children=collect(parts, "STYLES"),
            )
        ],
        "fonts": [load_font("RobotoCondensed"), load_font("Icons_WaveformS")],
        "bitmaps": collect(parts, "BITMAPS"),
        "colors": [{"objID": oid("color", name), "name": name} for name, _ in colors],
        "themes": [{"objID": oid("theme", "Default"), "name": "Default", "colors": [value for _, value in colors]}],
    }
    project_texts = texts(parts)
    if project_texts:
        result["texts"] = project_texts
    return result


################################################################################
# expression cases for the C++ test

# expected type of a case -> (check, value type), see expression_cases.py
CHECKS = {
    "int32": ("CASE_VALUE", "VALUE_TYPE_INT32"),
    "uint32": ("CASE_VALUE", "VALUE_TYPE_UINT32"),
    "int64": ("CASE_VALUE", "VALUE_TYPE_INT64"),
    "float": ("CASE_VALUE", "VALUE_TYPE_FLOAT"),
    "double": ("CASE_VALUE", "VALUE_TYPE_DOUBLE"),
    "boolean": ("CASE_VALUE", "VALUE_TYPE_BOOLEAN"),
    "string": ("CASE_VALUE", "VALUE_TYPE_STRING"),
    "undefined": ("CASE_VALUE", "VALUE_TYPE_UNDEFINED"),
    "null": ("CASE_VALUE", "VALUE_TYPE_NULL"),
    "date": ("CASE_VALUE", "VALUE_TYPE_DATE"),
    "number": ("CASE_NUMBER", "VALUE_TYPE_UNDEFINED"),
    "prefix": ("CASE_PREFIX", "VALUE_TYPE_STRING"),
    "error": ("CASE_ERROR", "VALUE_TYPE_ERROR"),
    "array": ("CASE_ARRAY", "VALUE_TYPE_ARRAY"),
    "any": ("CASE_ANY", "VALUE_TYPE_UNDEFINED"),
}


def c_string(s):
    """A C string literal of the UTF-8 bytes of s (octal escapes, they never
    run into the next character)."""
    out = []
    for byte in s.encode("utf-8"):
        c = chr(byte)
        if c == '"' or c == "\\":
            out.append("\\" + c)
        elif 32 <= byte < 127:
            out.append(c)
        else:
            out.append("\\%03o" % byte)
    return '"' + "".join(out) + '"'


def c_number(value):
    value = float(value)
    if math.isnan(value):
        return "NAN"
    if math.isinf(value):
        return "INFINITY" if value > 0 else "-INFINITY"
    return repr(value)


def encode_value(value):
    """The text of an array element as expressions_test.cpp encodes it."""
    if value is UNDEFINED:
        return "undefined"
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return "%d" % value
    if isinstance(value, F):
        return "f:%.9g" % struct.unpack("f", struct.pack("f", value))[0]
    if isinstance(value, float):
        return "d:%.15g" % value
    if isinstance(value, str):
        return '"' + value + '"'
    if isinstance(value, (list, tuple)):
        return "[" + ",".join(encode_value(element) for element in value) + "]"
    raise ValueError("can't encode %r" % (value,))


def cases_inc():
    lines = [
        "// Generated by make_fwtest.py from expression_cases.py, do not edit.",
        "// { expression, check, expected type, number, string, known bug }",
    ]
    for case in CASES:
        expression, type_, value = case[0], case[1], case[2]
        bug = case[3] if len(case) > 3 else None
        check, value_type = CHECKS[type_]
        number = "0"
        string = "nullptr"
        if type_ in ("string", "prefix"):
            string = c_string(value)
        elif type_ == "error":
            if value is not None:
                string = c_string(value)
        elif type_ == "array":
            if isinstance(value, (list, tuple)):
                number = c_number(len(value))
                string = c_string(encode_value(value))
            else:
                number = c_number(value)
        elif type_ == "boolean":
            number = "1" if value else "0"
        elif value is not None:
            number = c_number(value)
        lines.append("{ %s, %s, %s, %s, %s, %s }," % (
            c_string(expression), check, value_type, number, string, c_string(bug) if bug else "nullptr"))
    return "\n".join(lines) + "\n"


def write(path, text_):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text_)
    print("wrote", os.path.relpath(path, HERE))


def main():
    parts = load_parts()
    write(os.path.join(HERE, "fwtest", "fwtest.eez-project"), json.dumps(project(parts), indent=2, ensure_ascii=False) + "\n")
    write(os.path.join(HERE, "fwtest", "generated", "expression_cases.inc"), cases_inc())


if __name__ == "__main__":
    main()
