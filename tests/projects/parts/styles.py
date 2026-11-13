"""Styles part: the style properties of EEZ-GUI v3 (tests/render/styles_test.cpp).

Every name starts with "styles_". The states (state styles: a project style
with a state that uses its base style), the focus ring, the shadow, borders
with a radius and a different width on each side, children clipped to the
rounded corners of a container, padding of centered text, letter spacing, line
height and the ellipsis. The styles are written as Studio saves them: flat and
complete, without child styles (the other parts use the older style tree,
which Studio flattens when the project is loaded).

The tests set the native variables styles_visible and styles_text from C++
(render/styles_test.cpp implements them and the native action
styles_on_click).
"""

from fwlib import event_handler, native_action, page, rectangle, style, text, variable, widget

COLORS = [
    ("styles_bg", "#202020"),
    ("styles_white", "#f8fcf8"),
    ("styles_gray", "#808080"),
    ("styles_red", "#f80000"),
    ("styles_green", "#00fc00"),
    ("styles_blue", "#0000f8"),
    ("styles_yellow", "#f8fc00"),
    ("styles_orange", "#f88000"),
    ("styles_purple", "#8000f8"),
    ("styles_page_bg", "#406080"),
    ("styles_shadow", "#000000a0"),
]

BOX = dict(backgroundColor="styles_bg", color="styles_white", borderSize="1", borderColor="styles_gray")

# the states of styles_btn, also of styles_btn_primary (pressed: its own color
# with the background of styles_btn)
BTN_STATES = [
    ("disabled", dict(backgroundColor="styles_gray", color="styles_bg")),
    ("checked", dict(backgroundColor="styles_green")),
    ("selected", dict(backgroundColor="styles_orange")),
    ("focused", dict(borderColor="styles_yellow", focusRingWidth=2, focusRingOffset=3,
                     focusRingColor="styles_yellow")),
]

STYLES = [
    style("styles_box", **BOX),
    # a style with all the states
    style("styles_btn", **dict(BOX, backgroundColor="styles_blue")),
    style("styles_btn_pressed", state="pressed", useStyle="styles_btn", backgroundColor="styles_red"),
    *[style("styles_btn_" + state, state=state, useStyle="styles_btn", **props) for state, props in BTN_STATES],
    style("styles_btn_primary", **dict(BOX, backgroundColor="styles_purple")),
    style("styles_btn_primary_pressed", state="pressed", useStyle="styles_btn_primary",
          backgroundColor="styles_red", color="styles_yellow"),
    *[style("styles_btn_primary_" + state, state=state, useStyle="styles_btn_primary", **props)
      for state, props in BTN_STATES],
    # without states: pressed swaps the colors, not over a transparent
    # background
    style("styles_plain", **BOX),
    style("styles_plain_transparent", **dict(BOX, backgroundColor="transparent")),

    # page background, the shadows and the clipped corners are seen on it
    style("styles_page", backgroundColor="styles_page_bg"),

    # shadow of a rounded box
    style("styles_card", backgroundColor="styles_bg", borderRadius="12", shadowColor="styles_shadow",
          shadowBlur=8, shadowOffsetY=4, shadowSpread=0),

    # a different border width on each side, with a radius
    style("styles_sides", backgroundColor="styles_bg", borderSize="1 4 2 8", borderColor="styles_yellow",
          borderRadius="12"),
    style("styles_sides_same", backgroundColor="styles_bg", borderSize="4", borderColor="styles_yellow",
          borderRadius="12"),

    # rounded container, the children are inside of the corners
    style("styles_rounded", backgroundColor="styles_bg", borderSize="2", borderColor="styles_white",
          borderRadius="16"),
    style("styles_red_fill", backgroundColor="styles_red", color="styles_white"),

    # padding of centered text
    style("styles_center", backgroundColor="styles_bg", color="styles_white"),
    style("styles_center_pad", backgroundColor="styles_bg", color="styles_white", padding="0 0 0 60"),

    # letter spacing, line height, ellipsis
    style("styles_spacing", backgroundColor="styles_bg", color="styles_white", alignHorizontal="left", padding="4",
          letterSpacing=3),
    style("styles_lines", backgroundColor="styles_bg", color="styles_white", alignHorizontal="left",
          alignVertical="top", lineHeight=30),
    style("styles_ellipsis", backgroundColor="styles_bg", color="styles_white", alignHorizontal="left", padding="4",
          ellipsis=True),
    style("styles_ellipsis_v", backgroundColor="styles_bg", color="styles_white", alignHorizontal="left",
          padding="4", ellipsis=True, font="VRoboto"),
    style("styles_ellipsis_center", backgroundColor="styles_bg", color="styles_white", alignHorizontal="center",
          padding="4", ellipsis=True),
]

GLOBALS = [
    variable("styles_visible", "boolean", native=True),
    variable("styles_text", "string", native=True),
]

ACTIONS = [native_action("styles_on_click")]


def _states_page():
    return page("styles_states", [
        widget("ButtonWidget", ("styles", "btn"), 20, 20, 160, 50, "styles_btn", data='"Button"',
               eventHandlers=[event_handler(("styles", "btn"), "CLICKED", "styles_on_click")]),
        widget("ButtonWidget", ("styles", "disabled"), 200, 20, 160, 50, "styles_btn", data='"Disabled"',
               enabled="false"),
        widget("ToggleButtonWidget", ("styles", "toggle"), 380, 20, 160, 50, "styles_btn", data="1",
               text1="OFF", text2="ON"),
        widget("ButtonGroupWidget", ("styles", "group"), 560, 20, 220, 50, "styles_btn",
               data='["A", "B", "C"]', selectedButton="1"),
        widget("CheckboxWidget", ("styles", "check"), 20, 90, 160, 50, "styles_btn", data="true",
               text='"Check"'),
        widget("ButtonWidget", ("styles", "primary"), 200, 90, 160, 50, "styles_btn_primary", data='"Primary"',
               eventHandlers=[event_handler(("styles", "primary"), "CLICKED", "styles_on_click")]),
        text(("styles", "focus"), 380, 90, 160, 50, "Focus", "styles_btn"),
        widget("ButtonWidget", ("styles", "plain"), 560, 90, 220, 50, "styles_plain", data='"Plain"',
               eventHandlers=[event_handler(("styles", "plain"), "CLICKED", "styles_on_click")]),
    ], style="styles_page")


def _shapes_page():
    return page("styles_shapes", [
        # shadow, hidden by styles_visible
        widget("ContainerWidget", ("styles", "card"), 40, 30, 200, 120, "styles_card", visible="styles_visible",
               widgets=[text(("styles", "card_text"), 20, 40, 160, 40, "Card", "styles_center")]),
        # borders
        rectangle(("styles", "sides"), 300, 30, 200, 120, "styles_sides"),
        rectangle(("styles", "sides_same"), 540, 30, 200, 120, "styles_sides_same"),
        # rounded container with a child over the whole box and a text that
        # changes
        widget("ContainerWidget", ("styles", "rounded"), 40, 200, 200, 120, "styles_rounded", widgets=[
            rectangle(("styles", "rounded_fill"), 0, 0, 200, 60, "styles_red_fill"),
            widget("TextWidget", ("styles", "rounded_text"), 0, 60, 200, 60, "styles_red_fill",
                   data="styles_text"),
        ]),
        # padding of centered text
        text(("styles", "center"), 300, 200, 200, 40, "Center", "styles_center"),
        text(("styles", "center_pad"), 300, 260, 200, 40, "Center", "styles_center_pad"),
        # letter spacing, line height
        text(("styles", "spacing"), 540, 200, 240, 40, "Spacing", "styles_spacing"),
        widget("MultilineTextWidget", ("styles", "lines"), 540, 260, 240, 100, "styles_lines",
               text="One two three four five six seven"),
        # ellipsis
        text(("styles", "ellipsis"), 40, 360, 160, 40, "A text that does not fit", "styles_ellipsis"),
        text(("styles", "ellipsis_v"), 220, 360, 160, 40, "A text that does not fit", "styles_ellipsis_v"),
        text(("styles", "ellipsis_center"), 400, 360, 120, 40, "Fits", "styles_ellipsis_center"),
        text(("styles", "ellipsis_short"), 540, 360, 40, 40, "Long", "styles_ellipsis"),
    ], style="styles_page")


PAGES = [
    _states_page(),
    _shapes_page(),
]
