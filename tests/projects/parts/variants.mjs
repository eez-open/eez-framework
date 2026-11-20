/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Variants part: the style variants (tests/render/variants_test.cpp).
//
// Every name starts with "variants_". A variant is a look of a style that a
// widget chooses: a variant style is a project style with a variant that uses
// its base style and holds the properties of the variant (also of a state of the
// variant). A widget has a fixed variant, rules that give the variant by data,
// or the variant of its container.
//
// The tests set the native variable variants_state from C++ (render/
// variants_test.cpp implements it and the native action variants_on_click).

import { eventHandler, nativeAction, oid, page, style, text, variable, widget } from "../fwlib.mjs";

export const COLORS = [
    ["variants_bg", "#202020"],
    ["variants_white", "#f8fcf8"],
    ["variants_gray", "#808080"],
    ["variants_red", "#f80000"],
    ["variants_green", "#00fc00"],
    ["variants_blue", "#0000f8"],
    ["variants_orange", "#f88000"],
    ["variants_page_bg", "#406080"],
];

// the variants of the project (settings.general.styleVariants)
export const VARIANTS = [
    [1, "variants_warn"],
    [2, "variants_volt"],
    // no style has a style for this one
    [3, "variants_unused"],
];

const BOX = { backgroundColor: "variants_bg", color: "variants_white", borderSize: "1", borderColor: "variants_gray" };

function variantStyle(name, base, variant, state = null, props = {}) {
    const s = style(name, { useStyle: base, variant, ...props });
    delete s.alwaysBuild;
    if (state) {
        s.state = state;
    }
    return s;
}

export const STYLES = [
    style("variants_box", { ...BOX }),
    style("variants_box_pressed", { state: "pressed", useStyle: "variants_box", backgroundColor: "variants_green" }),
    variantStyle("variants_box__warn", "variants_box", "variants_warn", null, { backgroundColor: "variants_orange" }),
    // the pressed state of the variant
    variantStyle("variants_box__warn_pressed", "variants_box", "variants_warn", "pressed",
        { backgroundColor: "variants_red" }),
    // without its own pressed state: the pressed state of the base style
    variantStyle("variants_box__volt", "variants_box", "variants_volt", null, { backgroundColor: "variants_blue" }),

    // a style without variants
    style("variants_plain", { backgroundColor: "variants_page_bg", color: "variants_white" }),
    style("variants_page", { backgroundColor: "variants_page_bg" }),
];

export const GLOBALS = [
    variable("variants_state", "integer", null, { native: true }),
];

export const ACTIONS = [nativeAction("variants_on_click")];

function rule(key, variant, condition) {
    return { objID: oid(key, "rule", variant, condition), variant, condition };
}

function button(name, left, top, props = {}) {
    const key = ["variants", name];
    return widget("ButtonWidget", key, left, top, 160, 50, "variants_box", {
        data: `"${name}"`,
        eventHandlers: [eventHandler(key, "CLICKED", "variants_on_click")], ...props,
    });
}

function box(name, left, top, props = {}) {
    return text(["variants", name], left, top, 160, 50, name, "variants_box", props);
}

function variantsPage() {
    return page("variants_page", [
        button("fixed", 20, 20, { variant: "variants_warn" }),
        button("base", 200, 20),
        button("volt", 380, 20, { variant: "variants_volt" }),
        button("unused", 560, 20, { variant: "variants_unused" }),

        // by data
        button("rules", 20, 90, {
            variantRules: [
                rule("rules", "variants_warn", "variants_state == 1"),
                rule("rules", "variants_volt", "variants_state == 2"),
            ],
        }),
        // no rule applies: the fixed variant
        button("rules_fixed", 200, 90, {
            variant: "variants_volt", variantRules: [
                rule("rules_fixed", "variants_warn", "variants_state == 1"),
                rule("rules_fixed", "none", "variants_state == 2"),
            ],
        }),

        // the children of a container with a variant
        widget("ContainerWidget", ["variants", "container"], 20, 160, 760, 70, "variants_plain", {
            variant: "variants_warn", widgets: [
                box("inherit", 0, 10),
                box("none", 180, 10, { variant: "none" }),
                box("own", 360, 10, { variant: "variants_volt" }),
                // through a container without a variant
                widget("ContainerWidget", ["variants", "inner"], 540, 0, 220, 70, "variants_plain", {
                    widgets: [
                        box("deep", 0, 10),
                    ],
                }),
            ],
        }),

        // the variant of the container changes
        widget("ContainerWidget", ["variants", "container_rules"], 20, 250, 760, 70, "variants_plain", {
            variantRules: [rule("container_rules", "variants_warn", "variants_state == 1")], widgets: [
                box("follows", 0, 10),
                box("follows_none", 180, 10, { variant: "none" }),
            ],
        }),
    ], [], { style: "variants_page" });
}

export const PAGES = [variantsPage()];
