"""External part: the items of the main project that the external assets tests
use (tests/flow/external). The external project is projects/fwtest_ext
(make_fwtest_ext.py), a module project with this project as its master
project, like a BB3+ module project with the BB3+ project.

- ext_m_box, ext_m_text: master styles used by the external project. A module
  project can use only the master styles that have an id, and the id is the
  index of the style in the main assets, so these two take the ids 1 and 2
  (every other style of the main project comes after them).
- ext_host: a main page with a dynamic user widget (data ext_layout) that shows
  an external layout, like the BB3+ slot views show the module layouts.
- ext_uw_main: a main user widget that external pages show through a dynamic
  user widget.

The probe components (SetVariable, entry 0 "probe:<name>", entry 1 the
expression) hang on the page_close event: the tests evaluate their expressions
in the flow state of the page or user widget, or assign to the variable of
entry 1 (flow/external/ext.cpp).
"""

from fwlib import component, line, oid, page, rectangle, style, variable, widget

STYLES = [
    style("ext_m_box", id=1, backgroundColor="accent"),
    style("ext_m_text", id=2, color="bookmark", alignHorizontal="left"),
]

GLOBALS = [
    # layout id of the dynamic user widget of ext_host (an external page id)
    variable("ext_layout", "integer", "0"),
    # the Start component of ext_uw_main increments it
    variable("ext_uw_started", "integer", "0"),
]


def probes(prefix, cases, left=600):
    """Probe components: SetVariable components on the page_close event. A case
    is (name, expression) or (name, expression, variable of entry 1)."""
    event_key = (prefix, "probe_event")
    components = [component("OnEventActionComponent", event_key, left=left, top=0, event="page_close")]
    lines = []
    for i, case in enumerate(cases):
        name, expression = case[0], case[1]
        target = case[2] if len(case) > 2 else "probe_result"
        key = (prefix, "probe", name)
        components.append(component(
            "SetVariableActionComponent", key, left=left + 200, top=i * 80, width=300, height=70,
            entries=[
                {"objID": oid(*key, "label"), "variable": "probe_label", "value": "\"probe:%s\"" % name},
                {"objID": oid(*key, "value"), "variable": target, "value": expression},
            ],
        ))
        lines.append(line(event_key, "@seqout", key, "@seqin"))
    local_variables = [variable(name, "any", key=(prefix, "probe_var")) for name in ("probe_label", "probe_result")]
    return components, lines, local_variables


def user_widget(key, left, top, width, height, style_="default", data=None, page_name=""):
    props = {
        "userWidgetPageName": page_name,
        "userPropertyValues": {"objID": oid(key, "values"), "values": {}},
    }
    if data:
        props["data"] = data
    return widget("UserWidgetWidget", key, left, top, width, height, style_, **props)


def _ext_host():
    probe_components, probe_lines, probe_locals = probes("ext_host", [
        ("active", "Flow.isPageActive()"),
        # the tests set ext_layout through this one
        ("layout", "ext_layout", "ext_layout"),
        ("uw_started", "ext_uw_started"),
    ])
    return page(
        "ext_host",
        [
            user_widget("ext_host/uw", 0, 0, 200, 100, data="ext_layout"),
            rectangle("ext_host/after", 300, 0, 100, 50, "ext_m_box"),
        ] + probe_components,
        probe_lines,
        local_variables=probe_locals,
    )


def _ext_uw_main():
    probe_components, probe_lines, probe_locals = probes("ext_uw_main", [("active", "Flow.isPageActive()")])
    start = ("ext_uw_main", "start")
    inc = ("ext_uw_main", "inc")
    return page(
        "ext_uw_main",
        [
            rectangle("ext_uw_main/box", 0, 0, 100, 50, "ext_m_box"),
            component("StartActionComponent", start, left=300, top=0, width=100, height=40),
            component(
                "SetVariableActionComponent", inc, left=300, top=60, width=300, height=70,
                entries=[{"objID": oid(*inc, "entry"), "variable": "ext_uw_started", "value": "ext_uw_started + 1"}],
            ),
        ] + probe_components,
        [line(start, "@seqout", inc, "@seqin")] + probe_lines,
        local_variables=probe_locals,
        width=200, height=100, isUsedAsUserWidget=True,
    )


PAGES = [_ext_host()]

USER_WIDGETS = [_ext_uw_main()]
