"""Variants part: the style variants (tests/render/variants_test.cpp).

Every name starts with "variants_". A variant is a look of a style that a
widget chooses: a variant style is a project style with a variant that uses
its base style and holds the properties of the variant (also of a state of the
variant). A widget has a fixed variant, rules that give the variant by data,
or the variant of its container.

The tests set the native variable variants_state from C++ (render/
variants_test.cpp implements it and the native action variants_on_click).
"""

from fwlib import event_handler, native_action, oid, page, style, text, variable, widget

COLORS = [
    ("variants_bg", "#202020"),
    ("variants_white", "#f8fcf8"),
    ("variants_gray", "#808080"),
    ("variants_red", "#f80000"),
    ("variants_green", "#00fc00"),
    ("variants_blue", "#0000f8"),
    ("variants_orange", "#f88000"),
    ("variants_page_bg", "#406080"),
]

# the variants of the project (settings.general.styleVariants)
VARIANTS = [
    (1, "variants_warn"),
    (2, "variants_volt"),
    # no style has a style for this one
    (3, "variants_unused"),
]

BOX = dict(backgroundColor="variants_bg", color="variants_white", borderSize="1", borderColor="variants_gray")


def variant_style(name, base, variant, state=None, **props):
    s = style(name, useStyle=base, variant=variant, **props)
    del s["alwaysBuild"]
    if state:
        s["state"] = state
    return s


STYLES = [
    style("variants_box", **BOX),
    style("variants_box_pressed", state="pressed", useStyle="variants_box", backgroundColor="variants_green"),
    variant_style("variants_box__warn", "variants_box", "variants_warn", backgroundColor="variants_orange"),
    # the pressed state of the variant
    variant_style("variants_box__warn_pressed", "variants_box", "variants_warn", state="pressed",
                  backgroundColor="variants_red"),
    # without its own pressed state: the pressed state of the base style
    variant_style("variants_box__volt", "variants_box", "variants_volt", backgroundColor="variants_blue"),

    # a style without variants
    style("variants_plain", backgroundColor="variants_page_bg", color="variants_white"),
    style("variants_page", backgroundColor="variants_page_bg"),
]

GLOBALS = [
    variable("variants_state", "integer", native=True),
]

ACTIONS = [native_action("variants_on_click")]


def rule(key, variant, condition):
    return {"objID": oid(key, "rule", variant, condition), "variant": variant, "condition": condition}


def button(name, left, top, **props):
    key = ("variants", name)
    return widget("ButtonWidget", key, left, top, 160, 50, "variants_box", data='"%s"' % name,
                  eventHandlers=[event_handler(key, "CLICKED", "variants_on_click")], **props)


def box(name, left, top, **props):
    return text(("variants", name), left, top, 160, 50, name, "variants_box", **props)


def _page():
    return page("variants_page", [
        button("fixed", 20, 20, variant="variants_warn"),
        button("base", 200, 20),
        button("volt", 380, 20, variant="variants_volt"),
        button("unused", 560, 20, variant="variants_unused"),

        # by data
        button("rules", 20, 90, variantRules=[
            rule("rules", "variants_warn", "variants_state == 1"),
            rule("rules", "variants_volt", "variants_state == 2"),
        ]),
        # no rule applies: the fixed variant
        button("rules_fixed", 200, 90, variant="variants_volt", variantRules=[
            rule("rules_fixed", "variants_warn", "variants_state == 1"),
            rule("rules_fixed", "none", "variants_state == 2"),
        ]),

        # the children of a container with a variant
        widget("ContainerWidget", ("variants", "container"), 20, 160, 760, 70, "variants_plain",
               variant="variants_warn", widgets=[
                   box("inherit", 0, 10),
                   box("none", 180, 10, variant="none"),
                   box("own", 360, 10, variant="variants_volt"),
                   # through a container without a variant
                   widget("ContainerWidget", ("variants", "inner"), 540, 0, 220, 70, "variants_plain", widgets=[
                       box("deep", 0, 10),
                   ]),
               ]),

        # the variant of the container changes
        widget("ContainerWidget", ("variants", "container_rules"), 20, 250, 760, 70, "variants_plain",
               variantRules=[rule("container_rules", "variants_warn", "variants_state == 1")], widgets=[
                   box("follows", 0, 10),
                   box("follows_none", 180, 10, variant="none"),
               ]),
    ], style="variants_page")


PAGES = [_page()]
