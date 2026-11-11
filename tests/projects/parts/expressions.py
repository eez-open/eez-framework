"""Expressions part: the "expressions" page with one SetVariable component per
case of expression_cases.py (tests/expressions/expressions_test.cpp).

Entry 0 of a case component sets the label "case:<n>", entry 1 the case
expression. Studio numbers the components in the order it meets them while
building, so the test finds a case by its label. All of them hang on the @seqout
of a "page open" event that never fires (the page is never shown), so the flow
doesn't run them by itself: the test evaluates the entry expressions directly.
"""

from expression_cases import CASES
from fwlib import component, line, oid, page, variable


def _expressions_page():
    components = [component("OnEventActionComponent", ("expr", "on_event"), event="page_open")]
    lines = []
    for i, case in enumerate(CASES):
        key = ("expr", i)
        components.append(component(
            "SetVariableActionComponent", key, left=200, top=i * 80, width=400, height=70,
            entries=[
                {"objID": oid(*key, "label"), "variable": "label", "value": "\"case:%d\"" % i},
                {"objID": oid(*key, "case"), "variable": "result", "value": case[0]},
            ],
        ))
        lines.append(line(("expr", "on_event"), "@seqout", key, "@seqin"))
    return page(
        "expressions", components, lines,
        local_variables=[variable(name, "any", key=("expr", "var")) for name in ("label", "result")]
    )


PAGES = [_expressions_page()]
