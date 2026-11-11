"""Widgets part: the pages of the widget tests (tests/widgets/).

Every name has the prefix w_. The tests set the global variables w_a ... from
C++ and find them after the marker variable w_marker (the global variables of
the parts are numbered in project order, see tests/widgets/wtest.h, which
mirrors the order of GLOBALS below). Native variables and actions are
implemented in tests/widgets/native.cpp.

Pages are full screen (800x480, "default" style: black background). Widgets do
not overlap (except where the page allows overlapping children), the positions
are mirrored as constants in the C++ tests.
"""

import base64
import struct
import zlib

from fwlib import (component, event_handler, line, native_action, oid, page,
                   style, variable, widget)

COLORS = [
    ("w_red", "#f80000"),
    ("w_green", "#00fc00"),
    ("w_blue", "#0000f8"),
    ("w_yellow", "#f8fc00"),
    ("w_gray", "#808080"),
    ("w_magenta", "#f800f8"),
    ("w_cyan", "#00fcf8"),
    ("w_white", "#f8fcf8"),
    ("w_black", "#000000"),
]

STYLES = [
    # text: yellow on gray, centered
    style("w_txt", color="w_yellow", backgroundColor="w_gray"),
    style("w_txt_left", color="w_yellow", backgroundColor="w_gray", alignHorizontal="left"),
    # override target of the W8 tests
    style("w_txt_alt", color="w_green", backgroundColor="w_blue"),
    # opaque, rounded, no border (D4)
    style("w_rounded", color="w_white", backgroundColor="w_red", borderRadius="12"),
    style("w_red_box", backgroundColor="w_red"),
    style("w_green_box", backgroundColor="w_green"),
    style("w_blue_box", backgroundColor="w_blue"),
    style("w_gray_box", backgroundColor="w_gray"),
    # buttons: blue, red while pressed
    style("w_btn", color="w_white", backgroundColor="w_blue",
          activeColor="w_white", activeBackgroundColor="w_red"),
    style("w_btn_dis", color="w_white", backgroundColor="w_gray"),
    style("w_sel", color="w_white", backgroundColor="w_green"),
    # bar graph: green bar on blue
    style("w_bar", color="w_green", backgroundColor="w_blue",
          activeColor="w_green", activeBackgroundColor="w_blue"),
    style("w_bar_text", color="w_white", backgroundColor="w_blue", activeColor="w_black"),
    style("w_line1", color="w_red"),
    style("w_line2", color="w_yellow"),
    # YT graph (background, grid in border color) and its trace
    style("w_ytg", color="w_white", backgroundColor="w_black", borderColor="w_gray"),
    style("w_trace", color="w_red", backgroundColor="w_black"),
    # list graph
    style("w_lg", backgroundColor="w_black"),
    style("w_lg_cursor", backgroundColor="w_gray"),
    # progress: blue, green part
    style("w_prog", backgroundColor="w_blue", activeBackgroundColor="w_green"),
    # scroll bar: track gray, thumb green (drawn in the style color)
    style("w_track", color="w_gray", backgroundColor="w_gray"),
    style("w_thumb", color="w_green", backgroundColor="w_green"),
    # switch, slider
    style("w_switch", color="w_white", backgroundColor="w_gray", activeBackgroundColor="w_green",
          borderColor="w_gray", padding="2"),
    style("w_slider", color="w_white", backgroundColor="w_gray", borderColor="w_gray", padding="2"),
    # check box / radio
    style("w_check", color="w_white", backgroundColor="w_black", alignHorizontal="left"),
    # containers, lists
    style("w_cont", backgroundColor="w_gray"),
    style("w_list", backgroundColor="w_blue"),
    style("w_item", backgroundColor="w_green"),
    style("w_dim", color="w_gray", backgroundColor="w_black"),
    # gauge
    style("w_gauge_bar", color="w_green", backgroundColor="w_black"),
]

GLOBALS = [
    # the tests find the variables of this part after the marker
    variable("w_marker", "string", '"w_marker"'),
    variable("w_a", "any", "0"),
    variable("w_b", "any", "0"),
    variable("w_c", "any", "0"),
    variable("w_d", "any", "0"),
    variable("w_e", "any", "0"),
    variable("w_int", "integer", "0"),
    variable("w_int2", "integer", "0"),
    variable("w_bool", "boolean", "true"),
    variable("w_bool2", "boolean", "true"),
    variable("w_str", "string", '""'),
    variable("w_double", "double", "0"),
    variable("w_sb", "struct:$ScrollbarState",
             "{numItems: 100, itemsPerPage: 10, positionIncrement: 1, position: 0}"),
    variable("w_lc_points", "array:any", "[]"),
    variable("w_strs", "array:string", "[]"),
    # native (tests/widgets/native.cpp)
    variable("w_ell", "string", native=True),
    variable("w_ref", "string", native=True),
    variable("w_xscroll", "string", native=True),
    variable("w_counter_a", "any", native=True),
    variable("w_counter_b", "any", native=True),
    variable("w_yt", "any", native=True),
    variable("w_canvas", "any", native=True),
    variable("w_rol", "integer", native=True),
    variable("w_probe", "any", native=True),
    variable("w_counter_arr", "array:integer", native=True),
]

ACTIONS = [native_action(name) for name in ("w_on_click", "w_on_click2")]


################################################################################
# helpers

def _w(type_, key, x, y, w, h, style_="default", **props):
    return widget(type_, ("w", key), x, y, w, h, style_, **props)


def _styles(**props):
    """Secondary style properties: name -> {"useStyle": style}."""
    return {name: {"useStyle": value} for name, value in props.items()}


def _keyframe(key, start, end, **props):
    """Timeline keyframe, props: left/top/width/height/opacity = (value, easing)."""
    k = {"objID": oid("w", key, "keyframe", start, end), "start": start, "end": end}
    for name in ("left", "top", "width", "height", "rotate", "opacity", "scale", "scaleX", "scaleY"):
        if name in props:
            value, easing = props[name]
            k[name] = {"enabled": True, "value": value, "easingFunction": easing}
        else:
            k[name] = {"enabled": False, "easingFunction": "linear"}
    k["controlPoints"] = ""
    return k


def _png(width, height, pixel):
    """PNG (RGBA) with pixel(x, y) -> (r, g, b, a)."""
    raw = b""
    for y in range(height):
        raw += b"\x00" + b"".join(struct.pack("BBBB", *pixel(x, y)) for x in range(width))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw))
    png += chunk(b"IEND", b"")
    return "data:image/png;base64," + base64.b64encode(png).decode("ascii")


# 40x20: left half red, right half blue
BITMAPS = [{
    "objID": oid("bitmap", "w_bmp"),
    "name": "w_bmp",
    "image": _png(40, 20, lambda x, y: (248, 0, 0, 255) if x < 20 else (0, 0, 248, 255)),
    "bpp": 32,
    "alwaysBuild": True,
}]


################################################################################
# pages

def _page(name, components, lines=(), **props):
    return page(name, components, lines, **props)


PAGES = []

# Text
PAGES.append(_page("w_text", [
    _w("TextWidget", "text/static", 10, 10, 200, 40, "w_txt", text="Hello"),
    _w("TextWidget", "text/data", 10, 60, 200, 40, "w_txt", data="w_a"),
    _w("TextWidget", "text/override", 10, 110, 200, 40, "w_txt", text="Hello"),
    _w("TextWidget", "text/ellipsis", 10, 160, 120, 40, "w_txt_left", data="w_ell"),
    _w("TextWidget", "text/ref", 10, 210, 300, 40, "w_txt_left", data="w_ref"),
    _w("TextWidget", "text/fit", 300, 10, 400, 40, "w_txt_left", data="w_b", fitToContent="width"),
    _w("TextWidget", "text/nothing", 400, 110, 200, 40, "w_txt"),
]))

# MultilineText
PAGES.append(_page("w_mtext", [
    _w("MultilineTextWidget", "mtext/static", 10, 10, 150, 120, "w_txt_left",
       text="one two three four five six"),
    _w("MultilineTextWidget", "mtext/data", 200, 10, 150, 120, "w_txt_left", data="w_a"),
    _w("MultilineTextWidget", "mtext/override", 400, 10, 150, 120, "w_txt_left", text="one two"),
]))

# DisplayData: the same value with each display option, a reference text,
# a rounded opaque style (D4), x scroll (W3), refresh rate (W20)
PAGES.append(_page("w_dd", [
    _w("DisplayDataWidget", "dd/all", 10, 10, 200, 40, "w_txt", data="w_a", displayOption=0, refreshRate="0"),
    _w("DisplayDataWidget", "dd/integer", 10, 60, 200, 40, "w_txt", data="w_a", displayOption=1, refreshRate="0"),
    _w("DisplayDataWidget", "dd/opt2", 10, 110, 200, 40, "w_txt", data="w_a", displayOption=2, refreshRate="0"),
    _w("DisplayDataWidget", "dd/opt3", 10, 160, 200, 40, "w_txt", data="w_a", displayOption=3, refreshRate="0"),
    _w("DisplayDataWidget", "dd/unit", 10, 210, 200, 40, "w_txt", data="w_a", displayOption=4, refreshRate="0"),
    _w("DisplayDataWidget", "dd/intfrac", 10, 260, 200, 40, "w_txt", data="w_a", displayOption=5, refreshRate="0"),
    _w("TextWidget", "dd/ref", 250, 10, 200, 40, "w_txt", data="w_ref"),
    _w("DisplayDataWidget", "dd/rounded", 500, 10, 200, 40, "w_rounded", data="w_b", displayOption=0, refreshRate="0"),
    _w("DisplayDataWidget", "dd/xscroll", 500, 60, 200, 40, "w_txt_left", data="w_xscroll", displayOption=0, refreshRate="0"),
    _w("DisplayDataWidget", "dd/refresh", 500, 110, 200, 40, "w_txt", data="w_c", displayOption=0, refreshRate="1000"),
    _w("DisplayDataWidget", "dd/iuf", 500, 160, 250, 40, "w_txt", data="w_d", displayOption=6, refreshRate="0",
       unitAndFractionFont="RobotoCondensed"),
]))

# Rectangle, Bitmap, visibility
PAGES.append(_page("w_rect", [
    _w("RectangleWidget", "rect/red", 10, 10, 100, 50, "w_red_box"),
    _w("RectangleWidget", "rect/vis", 150, 10, 100, 50, "w_green_box", visible="w_bool"),
    _w("BitmapWidget", "rect/bitmap", 300, 10, 40, 20, "default", bitmap="w_bmp"),
    # hidden widget with counted data (WP2)
    _w("TextWidget", "rect/hidden", 10, 100, 100, 40, "w_txt", data="w_counter_a", visible="false"),
    # hidden container with a child with counted data
    _w("ContainerWidget", "rect/hidden_cont", 150, 100, 200, 100, "w_cont", visible="false", widgets=[
        _w("TextWidget", "rect/hidden_child", 10, 10, 100, 40, "w_txt", data="w_counter_b"),
    ]),
]))

# Button
PAGES.append(_page("w_button", [
    _w("ButtonWidget", "button/click", 10, 10, 150, 50, "w_btn", data='"Click"',
       eventHandlers=[event_handler(("w", "button/click"), "CLICKED", "w_on_click")],
       **_styles(disabledStyle="w_btn_dis")),
    _w("ButtonWidget", "button/enabled", 200, 10, 150, 50, "w_btn", data='"Off"', enabled="w_bool",
       eventHandlers=[event_handler(("w", "button/enabled"), "CLICKED", "w_on_click2")],
       **_styles(disabledStyle="w_btn_dis")),
    _w("ButtonWidget", "button/override", 400, 10, 150, 50, "w_btn", data='"OvR"',
       **_styles(disabledStyle="w_btn_dis")),
]))

# ToggleButton
PAGES.append(_page("w_toggle", [
    _w("ToggleButtonWidget", "toggle", 10, 10, 150, 50, "w_btn", data="w_int", text1="OFF", text2="ON",
       **_styles(checkedStyle="w_sel")),
]))

# ButtonGroup
PAGES.append(_page("w_bgroup", [
    _w("ButtonGroupWidget", "bgroup", 10, 10, 300, 50, "w_btn", data="w_a", selectedButton="w_int",
       **_styles(selectedStyle="w_sel")),
]))

# BarGraph
PAGES.append(_page("w_bar", [
    _w("BarGraphWidget", "bar/lr", 10, 10, 200, 30, "w_bar", data="w_a", min="0", max="100",
       line1Data="w_b", line2Data="w_c", refreshRate="0", orientation="left-right", displayValue=False,
       **_styles(textStyle="w_bar_text", line1Style="w_line1", line2Style="w_line2")),
    _w("BarGraphWidget", "bar/bt", 10, 60, 40, 200, "w_bar", data="w_a", min="0", max="100",
       refreshRate="0", orientation="bottom-top", displayValue=False,
       **_styles(textStyle="w_bar_text", line1Style="w_line1", line2Style="w_line2")),
    _w("BarGraphWidget", "bar/text", 300, 10, 200, 30, "w_bar", data="w_a", min="0", max="100",
       refreshRate="0", orientation="left-right", displayValue=True,
       **_styles(textStyle="w_bar_text", line1Style="w_line1", line2Style="w_line2")),
]))

# YTGraph (native data)
PAGES.append(_page("w_yt", [
    _w("YTGraphWidget", "yt", 10, 10, 400, 200, "w_ytg", data="w_yt",
       **_styles(y1Style="w_trace", y2Style="w_trace")),
]))

# YTGraph as the item of a list (widget cursor 0, not -1)
PAGES.append(_page("w_yt_list", [
    _w("ListWidget", "yt_list", 10, 10, 400, 200, "default", data="w_strs", listType="vertical", gap=0,
       itemWidget=_w("YTGraphWidget", "yt_list/yt", 0, 0, 400, 200, "w_ytg", data="w_yt",
                     **_styles(y1Style="w_trace", y2Style="w_trace"))),
]))

# UpDown
PAGES.append(_page("w_updown", [
    _w("UpDownWidget", "updown", 10, 10, 200, 50, "w_txt", data="w_int", min="0", max="3",
       downButtonText="<", upButtonText=">", **_styles(buttonsStyle="w_btn")),
]))

# ListGraph
PAGES.append(_page("w_lgraph", [
    _w("ListGraphWidget", "lgraph", 10, 10, 300, 150, "w_lg", data="w_a", dwellData="w_b", y1Data="w_c",
       y2Data="w_d", cursorData="w_int",
       **_styles(y1Style="w_line1", y2Style="w_line2", cursorStyle="w_lg_cursor")),
]))

# Progress
PAGES.append(_page("w_progress", [
    _w("ProgressWidget", "progress/h", 10, 10, 200, 30, "w_prog", data="w_a", min="0", max="100",
       orientation="horizontal"),
    _w("ProgressWidget", "progress/v", 10, 60, 30, 200, "w_prog", data="w_a", min="0", max="100",
       orientation="vertical"),
]))

# ScrollBar
PAGES.append(_page("w_sbar", [
    _w("ScrollBarWidget", "sbar", 10, 10, 300, 40, "w_track", data="w_sb", leftButtonText="<",
       rightButtonText=">", **_styles(thumbStyle="w_thumb", buttonsStyle="w_btn")),
]))

# Canvas (native data)
PAGES.append(_page("w_canvas", [
    _w("CanvasWidget", "canvas", 10, 10, 100, 100, "default", data="w_canvas"),
]))


# LineChart: the points come from w_lc_points ([x, y] arrays) when the page opens
def _line_chart_page(name, charts):
    components = [component("OnEventActionComponent", ("w", name, "on_open"), left=600, top=300, event="page_open")]
    lines = []
    for i, (key, x, y, w, h, props) in enumerate(charts):
        chart = _w(
            "LineChartEmbeddedWidget", key, x, y, w, h, "default",
            xValue="value[0]",
            lines=[{"objID": oid("w", key, "line"), "label": '"A"', "color": "w_red", "width": 2,
                    "value": "value[1]"}],
            title='""', showTitle="false", showLegend=props.get("showLegend", "false"),
            showXAxis="true", showYAxis="true", showGrid="true",
            yAxisRangeOption="floating", yAxisRangeFrom="0", yAxisRangeTo="10",
            maxPoints=40, marker="null",
            margin={"objID": oid("w", key, "margin"), "top": props.get("top", 10), "right": 10,
                    "bottom": props.get("bottom", 30), "left": 50},
            customInputs=[{"objID": oid("w", key, "input"), "name": "value", "type": "array:double"}],
            **_styles(titleStyle="default", legendStyle="w_txt_left", xAxisStyle="default",
                      yAxisStyle="default", markerStyle="default"),
        )
        eval_key = ("w", name, "eval", i)
        components.append(chart)
        components.append(component("EvalExprActionComponent", eval_key, left=600, top=380 + i * 60,
                                    expression="w_lc_points"))
        lines.append(line(("w", name, "on_open"), "@seqout", eval_key, "@seqin"))
        lines.append(line(eval_key, "result", ("w", key), "value"))
    return _page(name, components, lines)


PAGES.append(_line_chart_page("w_lchart", [
    ("lchart", 10, 10, 400, 200, {}),
]))
# grid height below the height of one y label: no y ticks (W5)
PAGES.append(_line_chart_page("w_lchart_small", [
    ("lchart_small", 10, 10, 400, 50, {"top": 10, "bottom": 30}),
]))
# legend of a chart at the top and of a chart lower on the page (W16)
PAGES.append(_line_chart_page("w_lchart_legend", [
    ("lchart_legend_top", 10, 10, 400, 150, {"showLegend": "true"}),
    ("lchart_legend_low", 10, 300, 400, 150, {"showLegend": "true"}),
]))

# Gauge
PAGES.append(_page("w_gauge", [
    _w("GaugeEmbeddedWidget", "gauge", 10, 10, 300, 200, "default", data="w_a", min="w_b", max="w_c",
       threshold="w_d", unit="w_e",
       **_styles(barStyle="w_gauge_bar", valueStyle="default", ticksStyle="default", thresholdStyle="w_line1")),
]))

# Input
PAGES.append(_page("w_input", [
    _w("InputEmbeddedWidget", "input/text", 10, 10, 200, 40, "w_txt", data="w_a", inputType="text",
       min="0", max="100"),
    _w("InputEmbeddedWidget", "input/password", 10, 60, 200, 40, "w_txt", data="w_b", inputType="text",
       password=True, min="0", max="100"),
    _w("InputEmbeddedWidget", "input/number", 10, 110, 200, 40, "w_txt", data="w_c", inputType="number",
       min="0", max="100", precision="0.01", unit='"V"', info='""', defaultValue="0"),
    _w("InputEmbeddedWidget", "input/password2", 10, 160, 200, 40, "w_txt", data="w_d", inputType="text",
       password=True, min="0", max="100"),
]))

# Roller: native data (counted writes) and flow data
PAGES.append(_page("w_roller", [
    _w("RollerWidget", "roller/native", 10, 10, 120, 200, "w_dim", data="w_rol", min="0", max="9",
       text='"Item " + w_rol', **_styles(selectedValueStyle="w_txt", unselectedValueStyle="w_dim")),
    _w("RollerWidget", "roller/flow", 200, 10, 120, 200, "w_dim", data="w_int", min="0", max="9",
       text='"Item " + w_int', **_styles(selectedValueStyle="w_txt", unselectedValueStyle="w_dim")),
]))

# Switch, Slider
PAGES.append(_page("w_switch", [
    _w("SwitchWidget", "switch", 10, 10, 100, 40, "w_switch", data="w_int"),
    _w("SliderWidget", "slider", 10, 100, 300, 40, "w_slider", data="w_double", min="w_b", max="w_c"),
]))

# DropDownList
PAGES.append(_page("w_ddl", [
    _w("DropDownListWidget", "ddl", 10, 10, 200, 40, "w_txt", data="w_int", options="w_a"),
]))

# QRCode
PAGES.append(_page("w_qr", [
    _w("QRCodeWidget", "qr", 10, 10, 200, 200, "default", data="w_a", errorCorrection="low"),
]))

# Checkbox, Radio
PAGES.append(_page("w_check", [
    _w("CheckboxWidget", "check", 10, 10, 200, 40, "w_check", data="w_bool", text='"Check"'),
    _w("CheckboxWidget", "check/override", 10, 60, 200, 40, "w_check", data="w_bool2", text='"Other"'),
    _w("RadioWidget", "radio/0", 300, 10, 200, 40, "w_check", data="w_int", value="0", text='"Zero"'),
    _w("RadioWidget", "radio/1", 300, 60, 200, 40, "w_check", data="w_int", value="1", text='"One"'),
    _w("RadioWidget", "radio/s", 300, 110, 200, 40, "w_check", data="w_str", value='"b"', text='"B"'),
]))

# Containers: static, horizontal, vertical
PAGES.append(_page("w_cont", [
    _w("ContainerWidget", "cont/static", 10, 10, 300, 100, "w_cont", visible="w_bool", widgets=[
        _w("TextWidget", "cont/static/text", 10, 10, 100, 40, "w_txt", text="In"),
    ]),
    _w("ContainerWidget", "cont/h", 10, 150, 300, 50, "w_cont", layout="horizontal", widgets=[
        _w("RectangleWidget", "cont/h/a", 0, 0, 50, 50, "w_red_box", visible="w_bool2"),
        _w("RectangleWidget", "cont/h/b", 50, 0, 50, 50, "w_green_box"),
    ]),
    _w("ContainerWidget", "cont/v", 400, 150, 100, 200, "w_cont", layout="vertical", widgets=[
        _w("RectangleWidget", "cont/v/a", 0, 0, 100, 50, "w_red_box", visible="w_bool2"),
        _w("RectangleWidget", "cont/v/b", 0, 50, 100, 50, "w_green_box"),
    ]),
]))

# Horizontal layout with timelines (W19): the container grows to 400 px, the
# child of the second container grows to 100 px (timeline position 1 s)
PAGES.append(_page("w_cont_tl", [
    _w("ContainerWidget", "conttl/grow", 10, 10, 300, 50, "w_cont", layout="horizontal",
       timeline=[_keyframe("conttl/grow", 0, 1, width=(400, "linear"))],
       widgets=[
           _w("RectangleWidget", "conttl/grow/a", 0, 0, 200, 50, "w_red_box", visible="w_bool2"),
           _w("RectangleWidget", "conttl/grow/b", 200, 0, 150, 50, "w_green_box", allowOutside=True),
       ]),
    _w("ContainerWidget", "conttl/child", 10, 100, 300, 50, "w_cont", layout="horizontal", widgets=[
        _w("RectangleWidget", "conttl/child/a", 0, 0, 50, 50, "w_red_box",
           timeline=[_keyframe("conttl/child/a", 0, 1, width=(100, "linear"))]),
        _w("RectangleWidget", "conttl/child/b", 50, 0, 50, 50, "w_green_box"),
    ]),
]))

# List, Grid
PAGES.append(_page("w_list", [
    _w("ListWidget", "list/v", 10, 10, 200, 100, "w_list", data="w_a", listType="vertical", gap=5,
       itemWidget=_w("RectangleWidget", "list/v/item", 0, 0, 200, 50, "w_item")),
    _w("ListWidget", "list/h", 300, 10, 300, 50, "w_list", data="w_strs", listType="horizontal", gap=0,
       itemWidget=_w("TextWidget", "list/h/item", 0, 0, 60, 50, "w_txt", data="w_strs[$index]")),
    _w("GridWidget", "grid", 10, 150, 200, 100, "default", data="w_c", gridFlow="row",
       itemWidget=_w("RectangleWidget", "grid/item", 0, 0, 100, 50, "w_item")),
]))

# List with an expression as data (evaluations counted by the native variable, WP2)
PAGES.append(_page("w_list_count", [
    _w("ListWidget", "list_count", 10, 10, 200, 100, "w_list", data="(w_counter_arr)", listType="vertical",
       gap=0, itemWidget=_w("RectangleWidget", "list_count/item", 0, 0, 200, 50, "w_item")),
]))

# more widget states than fit into the GUI state buffer (128 KB)
PAGES.append(_page("w_many", [
    _w("GridWidget", "many", 0, 0, 800, 480, "default", data="w_a", gridFlow="row",
       itemWidget=_w("RectangleWidget", "many/item", 0, 0, 10, 10, "w_item")),
]))

# Select
PAGES.append(_page("w_select", [
    _w("SelectWidget", "select", 10, 10, 200, 50, "default", data="w_int", widgets=[
        _w("RectangleWidget", "select/0", 0, 0, 200, 50, "w_red_box"),
        _w("RectangleWidget", "select/1", 0, 0, 200, 50, "w_green_box"),
        _w("RectangleWidget", "select/2", 0, 0, 200, 50, "w_blue_box"),
    ]),
]))

# user widgets
_UW_LABEL_ID = oid("w", "uw_text", "label", "id")

USER_WIDGETS = [
    page("w_uw_button", [
        _w("ButtonWidget", "uw_button/button", 0, 0, 150, 50, "w_btn", data='"B"',
           eventHandlers=[event_handler(("w", "uw_button/button"), "CLICKED", "w_on_click")],
           **_styles(disabledStyle="w_btn_dis")),
    ], width=150, height=50, isUsedAsUserWidget=True),
    page("w_uw_text", [
        _w("TextWidget", "uw_text/text", 0, 0, 150, 50, "w_txt", data="label"),
    ], width=150, height=50, isUsedAsUserWidget=True, userProperties=[{
        "objID": oid("w", "uw_text", "label"), "id": _UW_LABEL_ID, "name": "label", "type": "string",
        "assignable": False, "displayName": "Label", "description": "",
    }]),
]


def _user_widget(key, x, y, w, h, name, values=None):
    props = {"userWidgetPageName": name}
    if values is not None:
        props["userPropertyValues"] = {"objID": oid("w", key, "values"), "values": values}
    return _w("UserWidgetWidget", key, x, y, w, h, "default", **props)


PAGES.append(_page("w_uw", [
    _user_widget("uw/b0", 10, 10, 150, 50, "w_uw_button"),
    _user_widget("uw/b1", 200, 10, 150, 50, "w_uw_button"),
    _user_widget("uw/t0", 10, 100, 150, 50, "w_uw_text", {_UW_LABEL_ID: '"One"'}),
    _user_widget("uw/t1", 200, 100, 150, 50, "w_uw_text", {_UW_LABEL_ID: '"Two"'}),
    # data of this text is read after the user widgets (W9)
    _w("TextWidget", "uw/probe", 400, 100, 150, 50, "w_txt", data="w_probe"),
]))

# overlapping children: text under a rectangle
PAGES.append(_page("w_overlap", [
    _w("TextWidget", "overlap/text", 10, 10, 150, 60, "w_txt", data="w_a"),
    _w("RectangleWidget", "overlap/rect", 60, 40, 150, 60, "w_blue_box"),
], allowOverlappingChildren=True))

# timelines: position (left 10 -> 210), opacity to 0 with easeOutBack (W18)
PAGES.append(_page("w_timeline", [
    _w("RectangleWidget", "timeline/move", 10, 10, 50, 50, "w_red_box",
       timeline=[_keyframe("timeline/move", 0, 1, left=(210, "linear"))]),
    _w("RectangleWidget", "timeline/fade", 10, 100, 50, 50, "w_green_box",
       timeline=[_keyframe("timeline/fade", 0, 1, opacity=(0, "easeOutBack"))]),
]))

# a widget whose type the test changes (W17)
PAGES.append(_page("w_badtype", [
    _w("RectangleWidget", "badtype/rect", 10, 10, 100, 50, "w_red_box"),
]))
