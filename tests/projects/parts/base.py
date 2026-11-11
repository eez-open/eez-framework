"""Base part: what the framework itself needs (harness/app.cpp maps these to the
EEZ_* ids), the main page the harness shows at start and the globals of the
expression cases."""

from fwlib import native_action, page, rectangle, style, text, variable

COLORS = [
    ("background", "#000000"),
    ("text", "#ffffff"),
    ("accent", "#2060c0"),
    ("bookmark", "#ffff00"),
    ("backdrop", "#202020"),
    ("error", "#c00000"),
]

# children of the "default" style (font RobotoCondensed, color text on background)
STYLES = [
    style("text_left", alignHorizontal="left"),
    style("accent_box", backgroundColor="accent", borderSize="2", borderColor="text", borderRadius="8"),
    # used by the framework (EEZ_STYLE_ID_* in harness/app.cpp)
    style("info_alert", backgroundColor="accent", borderSize="2", borderColor="text", padding="8"),
    style("error_alert", backgroundColor="error", borderSize="2", borderColor="text", padding="8"),
    style("error_alert_button", backgroundColor="background", borderSize="1", borderColor="text", padding="4"),
    style("select_enum_item_popup_container", borderSize="1", borderColor="text", padding="4"),
    style("select_enum_item_popup_container_s", borderSize="1", borderColor="text", padding="2"),
    style("select_enum_item_popup_item", alignHorizontal="left", padding="4"),
    style("select_enum_item_popup_item_s", alignHorizontal="left", padding="2"),
    style("select_enum_item_popup_disabled_item", alignHorizontal="left", color="backdrop", padding="4"),
    style("select_enum_item_popup_disabled_item_s", alignHorizontal="left", color="backdrop", padding="2"),
    style("menu_with_buttons_container", borderSize="1", borderColor="text", padding="8"),
    style("menu_with_buttons_message", padding="4"),
    style("menu_with_buttons_button", backgroundColor="accent", padding="4"),
]

GLOBALS = [
    # used by the expression cases
    variable("g_int", "integer", "5"),
    variable("g_double", "double", "2.5"),
    variable("g_string", "string", "\"text\""),
    variable("g_bool", "boolean", "true"),
    variable("g_int_array", "array:integer", "[10, 20, 30]"),
    # native variables the framework implements (keypad, alerts)
    variable("keypad_text", "string", native=True),
    variable("keypad_edit_unit", "string", native=True),
    variable("alert_message", "string", native=True),
]

# the framework implements drag_overlay and scroll, harness/app.cpp edit
ACTIONS = [native_action(name) for name in ("drag_overlay", "edit", "scroll")]

# the first pages, in this order (PAGE_ID_MAIN is the page the harness shows)
PAGES = [
    page("main", [
        text("main/hello", 10, 10, 200, 40, "Hello"),
        rectangle("main/box", 10, 60, 100, 50, "accent_box"),
    ]),
    page("numeric_keypad", [text("numeric_keypad/title", 10, 10, 200, 40, "Keypad")]),
    page("async_operation_in_progress", [text("async/title", 10, 10, 300, 40, "Please wait")]),
]
