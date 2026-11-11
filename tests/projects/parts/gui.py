"""GUI part: pages, popups, keypads and lists of the GUI runtime tests
(tests/gui/). The native actions and variables are implemented in
tests/gui/gui_app.cpp: the gui_keypad_* actions, gui_yes, gui_no and gui_cancel
call the framework actions of the same name without the prefix."""

from fwlib import event_handler, native_action, page, style, text, variable, widget

STYLES = [
    style("gui_button", backgroundColor="accent", activeBackgroundColor="error", borderSize="1", borderColor="text"),
    style("gui_button_disabled", backgroundColor="backdrop", color="background"),
    style("gui_popup", backgroundColor="backdrop", borderSize="2", borderColor="text"),
    style("gui_transparent", backgroundColor="transparent"),
    style("gui_keypad_text", alignHorizontal="left", padding="4", borderSize="1", borderColor="text"),
    style("gui_key", backgroundColor="accent", activeBackgroundColor="error", borderSize="1", borderColor="text"),
    style("gui_list", color="text"),
    style("gui_item", borderSize="1", borderColor="backdrop"),
]

GLOBALS = [
    variable("gui_list_items", "array:integer", "[%s]" % ", ".join(str(i) for i in range(20))),
    variable("gui_grid_items", "array:integer", "[%s]" % ", ".join(str(i) for i in range(60))),
    # native, implemented in tests/gui/gui_app.cpp
    variable("gui_enabled", "boolean", native=True),
    variable("gui_switch", "boolean", native=True),
    variable("gui_name", "string", native=True),
    variable("gui_overlay", "any", native=True),
    variable("gui_keypad_ok_enabled", "boolean", native=True),
    variable("gui_keypad_option1_text", "string", native=True),
]

ACTIONS = [native_action(name) for name in (
    "gui_click_a", "gui_click_b", "gui_click_c", "gui_click_d", "gui_click_p",
    "gui_list_item", "gui_grid_item",
    "gui_yes", "gui_no", "gui_cancel",
    "gui_keypad_key", "gui_keypad_space", "gui_keypad_back", "gui_keypad_clear",
    "gui_keypad_ok", "gui_keypad_cancel", "gui_keypad_sign", "gui_keypad_unit",
    "gui_toggle_keypad_mode", "gui_keypad_option1",
)]


def _button(key, left, top, width, height, label, action, style_="gui_button", **props):
    return widget(
        "ButtonWidget", key, left, top, width, height, style_,
        data='"%s"' % label, eventHandlers=[event_handler(key, "CLICKED", action)], **props
    )


def _key(key, left, top, label, action="gui_keypad_key", width=98, height=78, **props):
    """A keypad key: a text widget whose data is the label (Keypad::key reads it)."""
    return widget(
        "TextWidget", key, left, top, width, height, "gui_key",
        data='"%s"' % label, eventHandlers=[event_handler(key, "CLICKED", action)], **props
    )


def _keypad_text(key):
    return widget("DisplayDataWidget", key, 0, 0, 800, 60, "gui_keypad_text", data="keypad_text", displayOption=0)


# page_a: buttons at known positions (gui_test.h has the coordinates)
_PAGE_A = page("gui_page_a", [
    _button("gui/a/btn1", 20, 20, 200, 60, "A1", "gui_click_a"),
    _button("gui/a/btn2", 300, 20, 200, 60, "A2", "gui_click_b"),
    _button("gui/a/disabled", 20, 120, 200, 60, "Off", "gui_click_c",
            enabled="gui_enabled", disabledStyle={"useStyle": "gui_button_disabled"}),
    widget("SwitchWidget", "gui/a/switch", 300, 120, 100, 50, "gui_button", data="gui_switch"),
])

# page_b: other widgets than page_a
_PAGE_B = page("gui_page_b", [
    text("gui/b/title", 20, 20, 760, 40, "Page B"),
    _button("gui/b/btn1", 500, 300, 200, 60, "B1", "gui_click_d"),
])

# popups closed by a touch outside of them
_POPUP = page("gui_popup", [
    _button("gui/popup/btn", 20, 20, 160, 60, "P1", "gui_click_p"),
], left=200, top=120, width=400, height=240, style="gui_popup", closePageIfTouchedOutside=True)

_POPUP2 = page("gui_popup2", [
    text("gui/popup2/title", 10, 10, 180, 40, "Popup 2"),
], left=300, top=200, width=200, height=80, style="gui_popup", closePageIfTouchedOutside=True)

# smaller popup that is not closed by a touch outside of it
_DIALOG = page("gui_dialog", [
    text("gui/dialog/title", 10, 10, 280, 40, "Dialog"),
], left=250, top=150, width=300, height=180, style="gui_popup")

# transparent full screen page
_TRANSPARENT = page("gui_transparent", [
    text("gui/transparent/title", 300, 220, 200, 40, "Transparent"),
], style="gui_transparent")

# yes/no dialog (AppContext::yesNoDialog)
_YES_NO = page("gui_yes_no", [
    widget("TextWidget", "gui/yes_no/message", 20, 20, 460, 60, "default", data="alert_message"),
    _button("gui/yes_no/yes", 20, 180, 140, 60, "Yes", "gui_yes"),
    _button("gui/yes_no/no", 180, 180, 140, 60, "No", "gui_no"),
    _button("gui/yes_no/cancel", 340, 180, 140, 60, "Cancel", "gui_cancel"),
], left=150, top=100, width=500, height=280, style="gui_popup")


def _numeric_keypad():
    components = [_keypad_text("gui/kn/text")]
    rows = [["7", "8", "9"], ["4", "5", "6"], ["1", "2", "3"], ["0", "."]]
    for r, row in enumerate(rows):
        for c, label in enumerate(row):
            components.append(_key("gui/kn/key_%d_%d" % (r, c), c * 100, 80 + r * 80, label))
    components += [
        _key("gui/kn/back", 320, 80, "<", "gui_keypad_back"),
        _key("gui/kn/sign", 320, 160, "+/-", "gui_keypad_sign"),
        widget("TextWidget", "gui/kn/unit", 320, 240, 98, 78, "gui_key", data="keypad_edit_unit",
               eventHandlers=[event_handler("gui/kn/unit", "CLICKED", "gui_keypad_unit")]),
        _key("gui/kn/clear", 320, 320, "C", "gui_keypad_clear"),
        _key("gui/kn/ok", 440, 80, "OK", "gui_keypad_ok"),
        _key("gui/kn/cancel", 440, 160, "Esc", "gui_keypad_cancel"),
        widget("TextWidget", "gui/kn/option1", 440, 240, 98, 78, "gui_key", data="gui_keypad_option1_text",
               eventHandlers=[event_handler("gui/kn/option1", "CLICKED", "gui_keypad_option1")]),
    ]
    return page("gui_keypad_num", components)


def _text_keypad():
    components = [_keypad_text("gui/kt/text")]
    for c, label in enumerate(["a", "b", "c", "ñ"]):
        components.append(_key("gui/kt/key_%d" % c, c * 100, 80, label))
    components += [
        _key("gui/kt/space", 0, 160, "_", "gui_keypad_space"),
        _key("gui/kt/back", 100, 160, "<", "gui_keypad_back"),
        _key("gui/kt/clear", 200, 160, "C", "gui_keypad_clear"),
        widget("ButtonWidget", "gui/kt/ok", 300, 160, 98, 78, "gui_key", data='"OK"', enabled="gui_keypad_ok_enabled",
               disabledStyle={"useStyle": "gui_button_disabled"},
               eventHandlers=[event_handler("gui/kt/ok", "CLICKED", "gui_keypad_ok")]),
        _key("gui/kt/cancel", 400, 160, "Esc", "gui_keypad_cancel"),
        _key("gui/kt/mode", 500, 160, "Aa", "gui_toggle_keypad_mode"),
    ]
    return page("gui_keypad_text", components)


# a vertical list (20 items of 60 px in 300 px) and a grid (60 items, 4 in a
# row, rows of 60 px in 300 px)
_LISTS = page("gui_lists", [
    widget(
        "ListWidget", "gui/lists/list", 0, 0, 400, 300, "gui_list",
        data="gui_list_items", listType="vertical", gap=0,
        itemWidget=widget(
            "TextWidget", "gui/lists/item", 0, 0, 400, 60, "gui_item",
            data='"Item " + $index', eventHandlers=[event_handler("gui/lists/item", "CLICKED", "gui_list_item")]
        ),
    ),
    widget(
        "GridWidget", "gui/lists/grid", 420, 0, 360, 300, "gui_list",
        data="gui_grid_items", gridFlow="row",
        itemWidget=widget(
            "TextWidget", "gui/lists/grid_item", 0, 0, 90, 60, "gui_item",
            data='"" + $index', eventHandlers=[event_handler("gui/lists/grid_item", "CLICKED", "gui_grid_item")]
        ),
    ),
    _button("gui/lists/btn", 500, 400, 200, 60, "L1", "gui_click_a"),
])

# a draggable overlay: container with the overlay data and the drag_overlay action
_OVERLAY = page("gui_overlay_page", [
    widget(
        "ContainerWidget", "gui/overlay/container", 500, 300, 200, 100, "gui_popup",
        overlay="gui_overlay", layout="static",
        eventHandlers=[event_handler("gui/overlay/container", "CLICKED", "drag_overlay")],
        widgets=[
            text("gui/overlay/title", 50, 30, 100, 40, "Drag", "gui_popup"),
        ],
    ),
])

PAGES = [_PAGE_A, _PAGE_B, _POPUP, _POPUP2, _DIALOG, _TRANSPARENT, _YES_NO, _numeric_keypad(), _text_keypad(), _LISTS, _OVERLAY]
