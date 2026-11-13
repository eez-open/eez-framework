"""Layout part: rows and columns (tests/render/layout_test.cpp).

Every name starts with "layout_". Containers with the layout horizontal (a
row) or vertical (a column), see eez-framework gui/layout.h: the gap, the
alignment on the main axis (justifyContent) and on the cross axis
(alignItems, alignSelf of a child), growing children, a hidden child, sizes
from the content (a Text, a row in a column), the baseline of texts in two
fonts and the right-to-left mirroring.

The children are rectangles or texts with a background of their own color, so
the test finds their boxes in the frame. The tests set the native variables
layout_visible and layout_text from C++ (render/layout_test.cpp).
"""

from fwlib import oid, page, rectangle, style, text, variable, widget

COLORS = [
    ("layout_page", "#405060"),
    ("layout_bg", "#202020"),
    ("layout_gray", "#808080"),
    ("layout_white", "#f8fcf8"),
    ("layout_red", "#f80000"),
    ("layout_green", "#00fc00"),
    ("layout_blue", "#0000f8"),
    ("layout_yellow", "#f8fc00"),
    ("layout_orange", "#f88000"),
    ("layout_purple", "#8000f8"),
]

STYLES = [
    style("layout_page", backgroundColor="layout_page"),
    # a row or column: the content box is 5 px inside
    style("layout_box", backgroundColor="layout_bg", padding="5"),
    # a row inside a column: the content box is 3 px inside
    style("layout_inner", backgroundColor="layout_gray", padding="3"),
    style("layout_r", backgroundColor="layout_red"),
    style("layout_g", backgroundColor="layout_green"),
    style("layout_b", backgroundColor="layout_blue"),
    # texts at the top left, no padding
    style("layout_text", backgroundColor="layout_purple", color="layout_white", alignHorizontal="left",
          alignVertical="top", padding="0"),
    style("layout_text_small", backgroundColor="layout_orange", color="layout_white", alignHorizontal="left",
          alignVertical="top", padding="0"),
    style("layout_text_big", backgroundColor="layout_yellow", color="layout_bg", alignHorizontal="left",
          alignVertical="top", padding="0", font="VRobotoBig"),
]

GLOBALS = [
    variable("layout_visible", "boolean", native=True),
    variable("layout_text", "string", native=True),
]

# Roboto Regular (fonts/Roboto-Regular.ttf) at the double size of VRoboto (part
# render), the ascent and the descent doubled
FONTS = [{
    "objID": oid("font", "VRobotoBig"), "name": "VRobotoBig", "description": "",
    "renderingEngine": "freetype",
    "source": {"objID": oid("font", "VRobotoBig", "source"), "filePath": "../fonts/Roboto-Regular.ttf", "size": 28},
    "bpp": 8, "threshold": 128,
    "height": 52, "ascent": 42, "descent": 10,
    "glyphs": [], "screenOrientation": "all", "alwaysBuild": True,
    "vector": True,
}]


def _rgb(key, w, h, color, **props):
    """a rectangle child of a row or column (the position is not used)"""
    return rectangle(("layout", key), 0, 0, w, h, "layout_" + color, **props)


def _children(key):
    """red 40x20, green 60x30, blue 30x40"""
    return [_rgb(key + "_r", 40, 20, "r"), _rgb(key + "_g", 60, 30, "g"), _rgb(key + "_b", 30, 40, "b")]


def _row(key, left, top, width, height, children, **props):
    props.setdefault("gap", 10)
    return widget("ContainerWidget", ("layout", key), left, top, width, height, "layout_box",
                  layout="horizontal", widgets=children, **props)


def _column(key, left, top, width, height, children, **props):
    props.setdefault("gap", 10)
    return widget("ContainerWidget", ("layout", key), left, top, width, height, "layout_box",
                  layout="vertical", widgets=children, **props)


# the rows of the page layout_rows: 400 x 50 at x = 20, y = 10 + 56 * k
ROWS = [
    ("start", {}),
    ("center", dict(justifyContent="center")),
    ("end", dict(justifyContent="end")),
    ("between", dict(justifyContent="space-between")),
    ("grow", None),
    ("align_center", dict(alignItems="center")),
    ("align_end", dict(alignItems="end")),
    ("stretch", dict(alignItems="stretch")),
]


def _rows_page():
    components = []
    for k, (key, props) in enumerate(ROWS):
        if props is None:
            children = [_rgb(key + "_r", 40, 20, "r"), _rgb(key + "_g", 60, 30, "g", flexGrow=True),
                        _rgb(key + "_b", 30, 40, "b")]
            props = {}
        else:
            children = _children(key)
        components.append(_row("row_" + key, 20, 10 + 56 * k, 400, 50, children, **props))

    # the right side: x = 440, 340 wide
    components += [
        # align self overrides align items
        _row("row_self", 440, 10, 340, 50, [
            _rgb("self_r", 40, 20, "r"),
            _rgb("self_g", 60, 30, "g", alignSelf="end"),
            _rgb("self_b", 30, 20, "b", alignSelf="center"),
        ]),
        # the green child is hidden by layout_visible
        _row("row_hidden", 440, 66, 340, 50, [
            _rgb("hidden_r", 40, 20, "r"),
            _rgb("hidden_g", 60, 30, "g", visible="layout_visible"),
            _rgb("hidden_b", 30, 40, "b"),
        ]),
        # the text is as wide as layout_text
        _row("row_fit", 440, 122, 340, 50, [
            widget("TextWidget", ("layout", "fit_text"), 0, 0, 100, 40, "layout_text", data="layout_text",
                   fitToContent="width"),
            _rgb("fit_b", 30, 40, "b"),
        ]),
        # the first lines of the texts on one baseline, the rectangle with its
        # bottom on it
        _row("row_baseline", 440, 178, 340, 70, [
            text(("layout", "base_small"), 0, 0, 10, 10, "Ag", "layout_text_small", fitToContent="both"),
            text(("layout", "base_big"), 0, 0, 10, 10, "Ag", "layout_text_big", fitToContent="both"),
            _rgb("base_r", 40, 20, "r"),
        ], alignItems="baseline"),
        # a column with a row as big as its children and a row as wide as the
        # column
        _column("col_rows", 440, 260, 340, 200, [
            widget("ContainerWidget", ("layout", "inner_fit"), 0, 0, 10, 10, "layout_inner", layout="horizontal",
                   gap=4, fitToContent="both", widgets=[_rgb("inner_r", 40, 20, "r"), _rgb("inner_g", 60, 30, "g")]),
            widget("ContainerWidget", ("layout", "inner_wide"), 0, 0, 10, 10, "layout_inner", layout="horizontal",
                   fitToContent="height", alignSelf="stretch", widgets=[_rgb("inner_b", 30, 40, "b")]),
        ], gap=6),
    ]
    return page("layout_rows", components, style="layout_page")


def _columns_page():
    return page("layout_columns", [
        _column("col", 20, 20, 120, 300, _children("col"), justifyContent="end", alignItems="center"),
    ], style="layout_page")


PAGES = [
    _rows_page(),
    _columns_page(),
]
