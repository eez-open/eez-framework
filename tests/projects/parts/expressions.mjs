/*
 * eez-framework
 *
 * MIT License
 * Copyright 2026 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// Expressions part: the "expressions" page with one SetVariable component per
// case of expression_cases.mjs (tests/expressions/expressions_test.cpp).
//
// Entry 0 of a case component sets the label "case:<n>", entry 1 the case
// expression. Studio numbers the components in the order it meets them while
// building, so the test finds a case by its label. All of them hang on the @seqout
// of a "page open" event that never fires (the page is never shown), so the flow
// doesn't run them by itself: the test evaluates the entry expressions directly.

import { CASES } from "../expression_cases.mjs";
import { component, line, oid, page, variable } from "../fwlib.mjs";

function expressionsPage() {
    const components = [component("OnEventActionComponent", ["expr", "on_event"], { event: "page_open" })];
    const lines = [];
    CASES.forEach((case_, i) => {
        const key = ["expr", i];
        components.push(component("SetVariableActionComponent", key, {
            left: 200, top: i * 80, width: 400, height: 70,
            entries: [
                { objID: oid(...key, "label"), variable: "label", value: `"case:${i}"` },
                { objID: oid(...key, "case"), variable: "result", value: case_[0] },
            ],
        }));
        lines.push(line(["expr", "on_event"], "@seqout", key, "@seqin"));
    });
    return page("expressions", components, lines, {
        localVariables: ["label", "result"].map(name => variable(name, "any", null, { key: ["expr", "var"] })),
    });
}

export const PAGES = [expressionsPage()];
