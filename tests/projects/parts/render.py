"""Render part: the pages, styles, colors, bitmaps and native variables of the
drawing tests (tests/render/).

Every name starts with "render_". The pages are drawn and compared with the
golden images in tests/render/golden/ (render/golden_test.cpp), the dynamic
page and the popups are used by the composition tests (render/compose_test.cpp).

Native variables render_text_a .. render_text_f are strings the tests set from
C++ (render/render_util.cpp implements data_render_text_*).
"""

import base64
import struct
import zlib

from fwlib import oid, page, rectangle, style, text, variable, widget

################################################################################
# small PNG encoder (RGBA, 8 bit), so the part needs no imaging library


def _png(width, height, pixel):
    """pixel(x, y) -> (r, g, b, a)"""
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            raw.extend(pixel(x, y))

    def chunk(kind, data):
        c = struct.pack(">I", len(data)) + kind + data
        return c + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    data += chunk(b"IEND", b"")
    return "data:image/png;base64," + base64.b64encode(data).decode("ascii")


def _bmp16_pixel(x, y):
    # 4 color bars on top, a gray ramp below (values exact in RGB565)
    if y < 8:
        return [(248, 0, 0, 255), (0, 252, 0, 255), (0, 0, 248, 255), (248, 252, 248, 255)][x // 6]
    v = (x * 10) & 0xF8
    return (v, v, v, 255)


def _bmp32_pixel(x, y):
    # top: yellow with alpha falling from 255 to 0, bottom: opaque red/blue
    # checkers with a fully transparent hole in the middle
    if y < 8:
        return (255, 255, 0, 255 - x * 255 // 23)
    if 8 <= x < 16 and 10 <= y < 14:
        return (0, 0, 0, 0)
    return (255, 0, 0, 255) if ((x // 4) + (y // 4)) % 2 == 0 else (0, 0, 255, 255)


def _bmp_big_pixel(x, y):
    # diagonal stripes
    return (248, 128, 0, 255) if ((x + y) // 6) % 2 == 0 else (0, 128, 128, 255)


def _bitmap(name, width, height, pixel, bpp):
    return {
        "objID": oid("bitmap", name), "name": name, "description": "",
        "image": _png(width, height, pixel), "bpp": bpp, "alwaysBuild": True,
    }


################################################################################
# vector fonts: Roboto Regular (fonts/Roboto-Regular.ttf, OFL) drawn at runtime,
# the ascent and descent as Studio gives them for the size


def _vector_font(name, size, ascent, descent, features=None, letter_spacing=None):
    font = {
        "objID": oid("font", name), "name": name, "description": "",
        "renderingEngine": "freetype",
        "source": {"objID": oid("font", name, "source"), "filePath": "../fonts/Roboto-Regular.ttf", "size": size},
        "bpp": 8, "threshold": 128,
        "height": ascent + descent, "ascent": ascent, "descent": descent,
        "glyphs": [], "screenOrientation": "all", "alwaysBuild": True,
        "vector": True,
    }
    if features:
        font["vectorFeatures"] = features
    if letter_spacing:
        font["vectorLetterSpacing"] = letter_spacing
    return font


# both from the same font file, which is in the assets once
FONTS = [
    _vector_font("VRoboto", 14, 21, 5),
    _vector_font("VRobotoTnum", 14, 21, 5, features="tnum", letter_spacing=1),
]

BITMAPS = [
    _bitmap("render_bmp16", 24, 16, _bmp16_pixel, 16),
    _bitmap("render_bmp32", 24, 16, _bmp32_pixel, 32),
    _bitmap("render_bmp_big", 64, 48, _bmp_big_pixel, 16),
]

################################################################################

# the project has 32-bit colors, so a pixel is the color
COLORS = [
    ("render_navy", "#000080"),
    ("render_red", "#f80000"),
    ("render_green", "#00fc00"),
    ("render_blue", "#0000f8"),
    ("render_gray", "#808080"),
    ("render_yellow", "#f8fc00"),
    ("render_orange", "#f88000"),
    ("render_purple", "#8000f8"),
    ("render_teal", "#008080"),
    ("render_dark_green", "#004000"),
    # with alpha (render_alpha)
    ("render_red_a128", "#ff000080"),
    ("render_white_a64", "#ffffff40"),
    ("render_blue_a0", "#0000ff00"),
]

_CELL = dict(backgroundColor="render_navy", borderSize="1", borderColor="render_gray", padding="4")

STYLES = [
    # text alignment (render_text_align)
    style("render_cell", children=[
        style("render_cell_lt", alignHorizontal="left", alignVertical="top"),
        style("render_cell_ct", alignVertical="top"),
        style("render_cell_rt", alignHorizontal="right", alignVertical="top"),
        style("render_cell_lc", alignHorizontal="left"),
        style("render_cell_cc"),
        style("render_cell_rc", alignHorizontal="right"),
        style("render_cell_lb", alignHorizontal="left", alignVertical="bottom"),
        style("render_cell_cb", alignVertical="bottom"),
        style("render_cell_rb", alignHorizontal="right", alignVertical="bottom"),
    ], **_CELL),
    style("render_pad", backgroundColor="render_teal", padding="2 8 4 16", alignHorizontal="left", alignVertical="top",
          children=[style("render_pad_rb", alignHorizontal="right", alignVertical="bottom")]),
    style("render_clip", backgroundColor="render_purple",
          children=[style("render_clip_r", alignHorizontal="right")]),
    style("render_text_transparent", backgroundColor="transparent", color="render_yellow"),
    style("render_text_red", color="render_red"),
    style("render_text_right", alignHorizontal="right"),
    style("render_green_bg", backgroundColor="render_dark_green"),
    style("render_purple_bg", backgroundColor="render_purple"),

    # borders
    style("render_border1", backgroundColor="render_navy", borderSize="1", borderColor="render_yellow"),
    style("render_border3", backgroundColor="render_navy", borderSize="3", borderColor="render_red"),
    style("render_border_uneven", backgroundColor="render_navy", borderSize="1 4 2 8", borderColor="render_green"),
    style("render_border_nobg", backgroundColor="transparent", borderSize="2", borderColor="text"),
    style("render_border_tcolor", backgroundColor="render_navy", borderSize="4", borderColor="transparent"),

    # border radius
    style("render_r_border", backgroundColor="render_blue", borderSize="2", borderColor="text", borderRadius="8"),
    style("render_r_noborder", backgroundColor="render_blue", borderRadius="8"),
    style("render_r_ellipse", backgroundColor="render_green", borderSize="1", borderColor="text", borderRadius="20/8",
          color="background"),
    style("render_r_huge", backgroundColor="render_orange", borderSize="1", borderColor="text", borderRadius="100"),
    style("render_r_corners", backgroundColor="render_red", borderSize="2", borderColor="text", borderRadius="0 6 16 24"),
    # only the bottom right corner is rounded (CSS order: TL TR BR BL)
    style("render_r_br", backgroundColor="render_red", borderRadius="0 0 20 0"),
    style("render_r_thick", backgroundColor="render_navy", borderSize="6", borderColor="render_yellow", borderRadius="14"),
    style("render_r_nofill", backgroundColor="transparent", borderSize="2", borderColor="render_green", borderRadius="10"),

    # opacity
    style("render_op_red128", backgroundColor="render_red", opacity=128),
    style("render_op_blue64", backgroundColor="render_blue", opacity=64),
    style("render_op_text", backgroundColor="render_navy", color="render_yellow", opacity=128),
    style("render_op_radius", backgroundColor="render_green", borderSize="2", borderColor="text", borderRadius="10",
          opacity=160),
    style("render_bg_green", backgroundColor="render_green"),
    style("render_bg_blue", backgroundColor="render_blue"),

    # blink: normal colors, active colors at blink time
    style("render_blink", backgroundColor="render_navy", color="text", activeBackgroundColor="render_red",
          activeColor="render_yellow", blink=True),
    style("render_blink_r", backgroundColor="render_blue", color="text", activeBackgroundColor="render_red",
          activeColor="render_yellow", borderRadius="10", blink=True),

    # dynamic texts (invariants, damage)
    style("render_dyn", backgroundColor="render_navy"),
    style("render_dyn_r_nb", backgroundColor="render_blue", borderRadius="10"),
    style("render_dyn_r_b", backgroundColor="render_blue", borderSize="1", borderColor="text", borderRadius="10"),
    style("render_dyn_op", backgroundColor="render_red", opacity=128),

    # pages
    style("render_popup", backgroundColor="render_navy", borderSize="2", borderColor="text"),
    style("render_popup2", backgroundColor="render_teal", borderSize="2", borderColor="render_yellow"),
    style("render_popup_box", backgroundColor="render_orange"),
    style("render_transparent_page", backgroundColor="transparent"),

    # multiline text
    style("render_ml", backgroundColor="render_navy", borderSize="1", borderColor="render_gray", padding="4",
          alignHorizontal="left", alignVertical="top", children=[
              style("render_ml_center", alignHorizontal="center", alignVertical="center"),
              style("render_ml_right", alignHorizontal="right", alignVertical="bottom"),
          ]),

    # bitmaps
    style("render_bmp", backgroundColor="render_gray", children=[
        style("render_bmp_lt", alignHorizontal="left", alignVertical="top", padding="2"),
        style("render_bmp_rb", alignHorizontal="right", alignVertical="bottom", padding="2"),
    ]),
    style("render_bgimg16", backgroundColor="render_navy", backgroundImage="render_bmp_big"),
    style("render_bgimg32", backgroundColor="render_navy", backgroundImage="render_bmp32"),

    # colors with alpha (render_alpha)
    style("render_a_bg", backgroundColor="render_red_a128"),
    style("render_a_text", backgroundColor="transparent", color="render_white_a64"),
    style("render_a_radius", backgroundColor="render_red_a128", borderSize="2", borderColor="render_white_a64",
          borderRadius="10", color="text"),
    style("render_a_op", backgroundColor="render_red_a128", opacity=128),
    style("render_a_zero", backgroundColor="render_blue_a0"),
    style("render_a_dyn", backgroundColor="render_red_a128", color="text"),
    style("render_a_page", backgroundColor="render_red_a128"),

    # vector fonts (render_vector)
    style("render_v", font="VRoboto", backgroundColor="render_navy", color="text", alignHorizontal="left",
          padding="4", children=[
              style("render_v_center", alignHorizontal="center"),
              style("render_v_right", alignHorizontal="right"),
              style("render_v_tnum", font="VRobotoTnum"),
              style("render_v_ml", alignVertical="top"),
          ]),

    # drawn directly by the tests (checkbox/radio, text measuring)
    style("render_cb", backgroundColor="render_navy", color="render_yellow", alignHorizontal="left", padding="4"),
    style("render_left_top", alignHorizontal="left", alignVertical="top"),
]

GLOBALS = [variable("render_text_" + c, "string", native=True) for c in "abcdef"]

################################################################################
# pages


def _box(key, x, y, w, h, style_, text_):
    return text(key, x, y, w, h, text_, style_)


def _dyn(key, x, y, w, h, style_, var):
    return widget("TextWidget", key, x, y, w, h, style_, data=var)


def _text_align_page():
    names = ["lt", "ct", "rt", "lc", "cc", "rc", "lb", "cb", "rb"]
    widgets = []
    for i, n in enumerate(names):
        col, row = i % 3, i // 3
        widgets.append(_box(("render_ta", n), 20 + col * 200, 20 + row * 80, 180, 60, "render_cell_" + n, "Align Wy"))
    widgets += [
        _box(("render_ta", "pad"), 620, 20, 160, 60, "render_pad", "Pad"),
        _box(("render_ta", "pad_rb"), 620, 100, 160, 60, "render_pad_rb", "Pad"),
        _box(("render_ta", "clip"), 620, 180, 120, 30, "render_clip", "This text is longer than its box"),
        _box(("render_ta", "clip_r"), 620, 220, 120, 30, "render_clip_r", "This text is longer than its box"),
        widget("ContainerWidget", ("render_ta", "green"), 20, 280, 360, 100, "render_green_bg", widgets=[
            _box(("render_ta", "on_green"), 10, 10, 160, 40, "render_text_transparent", "On green"),
            _box(("render_ta", "opaque"), 190, 10, 160, 40, "render_text_red", "Opaque"),
            _box(("render_ta", "empty"), 10, 60, 160, 30, "render_text_transparent", ""),
        ]),
        _box(("render_ta", "one"), 400, 280, 1, 1, "render_cell_cc", "X"),
        _box(("render_ta", "small"), 420, 280, 12, 8, "render_cell_cc", "Wide"),
    ]
    return page("render_text_align", widgets)


def _borders_page():
    return page("render_borders", [
        _box(("render_b", 1), 20, 20, 160, 80, "render_border1", "1px"),
        _box(("render_b", 3), 200, 20, 160, 80, "render_border3", "3px"),
        _box(("render_b", "uneven"), 380, 20, 160, 80, "render_border_uneven", "1 4 2 8"),
        widget("ContainerWidget", ("render_b", "green"), 560, 20, 160, 80, "render_green_bg", widgets=[
            _box(("render_b", "nobg"), 0, 0, 160, 80, "render_border_nobg", "no bg"),
        ]),
        _box(("render_b", "tcolor"), 20, 120, 160, 80, "render_border_tcolor", "transparent border"),
        rectangle(("render_b", "rect"), 200, 120, 60, 40, "render_border1"),
        rectangle(("render_b", "tiny"), 280, 120, 4, 4, "render_border3"),
        rectangle(("render_b", "pixel"), 300, 120, 1, 1, "render_border1"),
        rectangle(("render_b", "thin"), 320, 120, 100, 2, "render_border3"),
    ])


def _radius_page():
    return page("render_radius", [
        _box(("render_r", "border"), 20, 20, 160, 80, "render_r_border", "r8 b2"),
        _box(("render_r", "noborder"), 200, 20, 160, 80, "render_r_noborder", "r8"),
        _box(("render_r", "ellipse"), 380, 20, 160, 80, "render_r_ellipse", "20/8"),
        _box(("render_r", "huge"), 560, 20, 160, 80, "render_r_huge", "r100"),
        _box(("render_r", "corners"), 20, 120, 160, 80, "render_r_corners", "0 6 16 24"),
        rectangle(("render_r", "br"), 200, 120, 160, 80, "render_r_br"),
        _box(("render_r", "thick"), 380, 120, 160, 80, "render_r_thick", "b6 r14"),
        widget("ContainerWidget", ("render_r", "purple"), 560, 120, 160, 80, "render_purple_bg", widgets=[
            _box(("render_r", "nofill"), 10, 10, 140, 60, "render_r_nofill", "no fill"),
        ]),
        rectangle(("render_r", "circle"), 20, 220, 20, 20, "render_r_huge"),
        rectangle(("render_r", "pill"), 60, 220, 30, 12, "render_r_huge"),
        rectangle(("render_r", "small8"), 100, 220, 8, 8, "render_r_border"),
        rectangle(("render_r", "small3"), 120, 220, 3, 3, "render_r_border"),
        rectangle(("render_r", "small1"), 140, 220, 1, 1, "render_r_huge"),
        widget("ContainerWidget", ("render_r", "green"), 20, 280, 360, 120, "render_green_bg", widgets=[
            _box(("render_r", "g_nb"), 20, 20, 140, 40, "render_r_noborder", "on green"),
            _box(("render_r", "g_b"), 180, 20, 140, 40, "render_r_border", "on green"),
        ]),
    ])


def _opacity_page():
    return page("render_opacity", [
        rectangle(("render_o", "green"), 20, 20, 200, 200, "render_bg_green"),
        rectangle(("render_o", "blue"), 120, 120, 200, 200, "render_bg_blue"),
        rectangle(("render_o", "red128"), 60, 60, 200, 120, "render_op_red128"),
        rectangle(("render_o", "blue64"), 160, 100, 200, 120, "render_op_blue64"),
        widget("ContainerWidget", ("render_o", "cgreen"), 400, 20, 240, 80, "render_green_bg", widgets=[
            _box(("render_o", "text"), 20, 10, 200, 60, "render_op_text", "Opacity text"),
        ]),
        widget("ContainerWidget", ("render_o", "cblue"), 400, 120, 240, 100, "render_bg_blue", widgets=[
            _box(("render_o", "radius"), 20, 10, 200, 80, "render_op_radius", "op 160 r10"),
        ]),
    ], allowOverlappingChildren=True)


def _colors_page():
    names = [
        "background", "text", "accent", "bookmark", "backdrop", "error",
        "render_navy", "render_red", "render_green", "render_blue", "render_gray", "render_yellow",
        "render_orange", "render_purple", "render_teal", "render_dark_green",
        "#ffffff", "#336699", "#123456", "#fedcba",
    ]
    widgets = []
    for i, name in enumerate(names):
        w = rectangle(("render_c", i), 20 + (i % 8) * 95, 20 + (i // 8) * 80, 80, 60, "default")
        w["style"]["backgroundColor"] = name
        widgets.append(w)
    return page("render_colors", widgets)


ICON_ON = "\u0002"
ICON_OFF = ""


def _fonts_page():
    lines = [
        "Default " + ICON_ON + "ÃÄ" + ICON_OFF + " back to default",
        "Tag at the end " + ICON_ON + "Å",
        "cUnknown font index 99 is the default font",
        "Grüße, €5, ½, ✓ – missing glyphs",
        "Glyph 127:\u007f 128:\u0080 end",
        "\u0002External font tag in main assets",
    ]
    widgets = [_box(("render_f", i), 20, 20 + i * 50, 760, 40, "text_left", t) for i, t in enumerate(lines)]
    widgets += [
        _box(("render_f", "center"), 20, 320, 760, 40, "default",
             ICON_ON + "ÆÇÈ" + ICON_OFF + " centered icons"),
        _box(("render_f", "right"), 20, 370, 760, 40, "render_text_right",
             "right " + ICON_ON + "ÉÊË"),
    ]
    return page("render_fonts", widgets)


LOREM = ("The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs.\\n"
         "Second paragraph here.")


def _multiline(key, x, y, w, h, style_, text_, first=0, hanging=0):
    return widget("MultilineTextWidget", key, x, y, w, h, style_, text=text_,
                  firstLineIndent=first, hangingIndent=hanging)


def _multiline_page():
    return page("render_multiline", [
        _multiline(("render_ml", 1), 20, 20, 240, 200, "render_ml", LOREM),
        _multiline(("render_ml", 2), 280, 20, 240, 200, "render_ml_center", LOREM, 20, 10),
        _multiline(("render_ml", 3), 540, 20, 240, 200, "render_ml_right", LOREM),
        _multiline(("render_ml", 4), 20, 240, 240, 100, "render_ml",
                   "Supercalifragilisticexpialidocious_and_more fits?"),
        _multiline(("render_ml", 5), 280, 240, 240, 100, "render_ml",
                   "Start " + ICON_ON + "ÃÄ ÅÆ ÇÈ" + ICON_OFF + " end of text here"),
        _multiline(("render_ml", 6), 540, 240, 240, 60, "render_ml", LOREM),
        _multiline(("render_ml", 7), 20, 360, 240, 100, "render_ml", ""),
    ])


def _bitmap_widget(key, x, y, w, h, style_, bitmap):
    return widget("BitmapWidget", key, x, y, w, h, style_, bitmap=bitmap)


def _bitmaps_page():
    return page("render_bitmaps", [
        _bitmap_widget(("render_bm", 1), 20, 20, 60, 40, "render_bmp", "render_bmp16"),
        _bitmap_widget(("render_bm", 2), 100, 20, 60, 40, "render_bmp_lt", "render_bmp16"),
        _bitmap_widget(("render_bm", 3), 180, 20, 60, 40, "render_bmp_rb", "render_bmp16"),
        _bitmap_widget(("render_bm", 4), 260, 20, 60, 40, "render_bmp", "render_bmp32"),
        _bitmap_widget(("render_bm", 5), 340, 20, 60, 40, "render_bmp_lt", "render_bmp32"),
        _bitmap_widget(("render_bm", 6), 420, 20, 24, 16, "render_bmp", "render_bmp16"),
        _bitmap_widget(("render_bm", 7), 560, 40, 40, 30, "render_bmp", "render_bmp_big"),
        _box(("render_bm", "bg16"), 20, 120, 200, 80, "render_bgimg16", "bg image 16"),
        _box(("render_bm", "bg32"), 240, 120, 200, 80, "render_bgimg32", "bg image 32"),
        _box(("render_bm", "bgsmall"), 460, 120, 40, 30, "render_bgimg16", ""),
        # DK2: the box is 20 px wider than the bitmap
        _bitmap_widget(("render_bm", "dk2"), 20, 240, 44, 36, "render_bmp", "render_bmp16"),
    ])


def _blink_page():
    return page("render_blink", [
        _box(("render_bl", 1), 20, 20, 200, 50, "render_blink", "Blink"),
        _box(("render_bl", 2), 240, 20, 200, 50, "render_blink_r", "Blink r"),
        _box(("render_bl", 3), 460, 20, 200, 50, "render_dyn", "Static"),
    ])


def _dynamic_page():
    return page("render_dynamic", [
        _dyn(("render_d", "a"), 10, 10, 120, 30, "render_dyn", "render_text_a"),
        _dyn(("render_d", "b"), 670, 440, 120, 30, "render_dyn", "render_text_b"),
        widget("ContainerWidget", ("render_d", "green"), 180, 80, 440, 200, "render_green_bg", widgets=[
            _dyn(("render_d", "c"), 20, 20, 140, 40, "render_dyn_r_nb", "render_text_c"),
            _dyn(("render_d", "d"), 200, 20, 140, 40, "render_dyn_r_b", "render_text_d"),
            _dyn(("render_d", "e"), 20, 100, 140, 40, "render_dyn_op", "render_text_e"),
        ]),
        # transparent texts over the background image of their container (the
        # background stack draws the part of the image when they change)
        widget("ContainerWidget", ("render_d", "image"), 180, 300, 300, 120, "render_bgimg16", widgets=[
            _dyn(("render_d", "d_img1"), 10, 10, 40, 30, "render_text_transparent", "render_text_d"),
            _dyn(("render_d", "d_img2"), 60, 40, 140, 40, "render_text_transparent", "render_text_d"),
        ]),
    ])


def _vector_page():
    return page("render_vector", [
        _box(("render_v", "kern"), 20, 20, 360, 40, "render_v", "AVATAR Wave Ty fi ffi 0123456789"),
        _box(("render_v", "tnum"), 400, 20, 380, 40, "render_v_tnum", "tnum 0123456789 1111 8888"),
        _box(("render_v", "tags"), 20, 80, 360, 40, "render_v", "Vector " + ICON_ON + "ÃÄ" + ICON_OFF + " and icons"),
        _box(("render_v", "center"), 400, 80, 380, 40, "render_v_center", "Centered vector text"),
        _box(("render_v", "right"), 400, 140, 380, 40, "render_v_right", "Right 12.345 V"),
        _box(("render_v", "utf8"), 20, 140, 360, 40, "render_v", "Grüße, €5, ½, ✓ – Ωμ"),
        _multiline(("render_v", "ml"), 20, 200, 360, 120, "render_v_ml",
                   "The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs."),
        _dyn(("render_v", "dyn"), 400, 200, 380, 40, "render_v", "render_text_a"),
    ])


def _alpha_page():
    # everything with alpha is on an opaque green container, so the color of a
    # pixel is the color blended over green
    return page("render_alpha", [
        widget("ContainerWidget", ("render_a", "green"), 20, 20, 760, 300, "render_bg_green", widgets=[
            rectangle(("render_a", "bg"), 20, 20, 160, 80, "render_a_bg"),
            _box(("render_a", "text"), 200, 20, 160, 80, "render_a_text", "Alpha text"),
            _box(("render_a", "radius"), 380, 20, 160, 80, "render_a_radius", "r10"),
            rectangle(("render_a", "op"), 560, 20, 160, 80, "render_a_op"),
            rectangle(("render_a", "zero"), 20, 120, 160, 80, "render_a_zero"),
            _dyn(("render_a", "dyn"), 200, 120, 160, 80, "render_a_dyn", "render_text_a"),
        ]),
    ])


def _popup_page(name, x, y, w, h, style_, widgets):
    return page(name, widgets, left=x, top=y, width=w, height=h, style=style_)


def _popup_pages():
    pages = [
        _popup_page("render_popup1", 200, 120, 400, 240, "render_popup", [
            _box(("render_p1", "title"), 20, 20, 360, 40, "default", "Popup"),
            rectangle(("render_p1", "box"), 20, 80, 100, 60, "render_popup_box"),
            _dyn(("render_p1", "f"), 140, 80, 200, 40, "render_dyn", "render_text_f"),
        ]),
        _popup_page("render_popup2", 300, 60, 300, 200, "render_popup2", [
            _box(("render_p2", "title"), 20, 20, 260, 40, "default", "Second popup"),
        ]),
        _popup_page("render_transparent", 0, 0, 800, 480, "render_transparent_page", [
            _box(("render_tp", "box"), 300, 200, 200, 80, "render_popup_box", "transparent page"),
        ]),
        # a page with a background color with alpha is a transparent page (over
        # the green container of render_alpha)
        _popup_page("render_alpha_popup", 400, 200, 300, 100, "render_a_page", [
            rectangle(("render_ap", "box"), 20, 20, 60, 40, "render_popup_box"),
        ]),
    ]
    # D5: small popups, none of them covers another one
    for i in range(6):
        pages.append(_popup_page("render_stack%d" % (i + 1), 20 + i * 125, 300, 110, 60, "render_popup", [
            _box(("render_s", i), 10, 10, 90, 40, "render_popup_box", "S%d" % (i + 1)),
        ]))
    return pages


PAGES = [
    _text_align_page(),
    _borders_page(),
    _radius_page(),
    _opacity_page(),
    _colors_page(),
    _fonts_page(),
    _multiline_page(),
    _bitmaps_page(),
    _blink_page(),
    _dynamic_page(),
    _alpha_page(),
    _vector_page(),
] + _popup_pages()
