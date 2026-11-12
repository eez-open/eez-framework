# eez-framework tests

Tests of the EEZ-GUI and EEZ Flow parts of eez-framework (not LVGL, not the
Dashboard API). They run on Linux (WSL) with g++, AddressSanitizer and
UndefinedBehaviorSanitizer, with the configuration of the BB3+ on CM5:
`EEZ_PLATFORM_CM5_RASPI`, 800x480, 32 bpp, SDL (headless), the GUI loop on the
test thread.

## Running

```sh
tests/run.sh                      # build and run everything
tests/run.sh expressions          # only tests whose name contains "expressions"
tests/run.sh --verbose "core/"    # also print the output of passed tests
tests/run.sh --list               # list the tests
tests/run.sh --no-sanitize        # plain build
tests/run.sh --tsan tsan          # data race tests (ThreadSanitizer)
```

Needs `cmake`, `g++`, `libsdl2-dev` and `libsdl2-image-dev`. The build goes to
`~/.cache/eez-framework-tests`. `EEZ_FW_SRC=<path to eez-framework/src>` tests
another framework copy.

Every test runs in its own forked process, started from the state after
`harness::init()`. A crash, a timeout (20 s, `--timeout=S`) or a sanitizer
report fails only that test. Results:

| | |
|---|---|
| `PASS` | passed |
| `FAIL` | failed (exit code 1) |
| `XFAIL` | a known bug (`TEST_BUG`) failed, as expected |
| `XPASS` | a known bug passed: it is fixed, remove the `TEST_BUG` marker (counts as a failure) |

## Layout

| Folder | |
|---|---|
| `harness/` | test runner (`test.h`), the application around the framework (`harness.h`, `app.cpp`), fake clock |
| `conf/` | `eez-framework-conf.h` of the tests |
| `projects/` | test project generator and the generated assets (see below) |
| `core/` | core/: Value, util, unit, assets, alloc |
| `expressions/` | expressions and built-in functions (cases in `projects/expression_cases.py`) |
| `flow/` | flow engine and components |
| `gui/` | pages, events, touch, keypad, toasts, the GUI thread |
| `widgets/` | widgets |
| `render/` | drawing: display, draw, raster, fonts, images |

## Writing tests

```cpp
#include "test.h"
#include "harness.h"

TEST("area/file: what is checked") {
    CHECK(...);  CHECK_EQ(a, b);  CHECK_STR(a, b);  CHECK_NEAR(a, b, eps);
    REQUIRE(...);  // stops the test
}

TEST_BUG("area/file: what should happen", "G1") {
    // a known bug: the id of the finding in the eez-framework review
}
```

- A test checks the correct behaviour. When the framework does something else
  today because of a known bug, mark it with `TEST_BUG` and the finding id. A
  behaviour that is odd but not a bug is pinned as it is, with a comment.
- One bug per `TEST_BUG`, and keep such tests small, so the expected failure
  is the bug and nothing else.
- The ids are those of the eez-framework review. Ids `NEW-<area>-<n>` are bugs
  found while writing the tests; they are in the review too.
- Tests that hang or crash because of a bug are fine: the runner isolates them.
- The harness: `harness::frame(n)` runs n GUI frames (16 ms of the fake clock
  each, flow ticks included), `harness::advance(ms)` moves the clock,
  `harness::touch(x, y)` / `harness::release()` set the touch screen state,
  `harness::pixel(x, y)` reads the presented frame, `harness::flowErrors()`,
  `stopScriptCount()` and `logMessages()` record what the flow reported.
- A known bug that only AddressSanitizer or UBSan show (use after free, a read
  past a buffer, an overflow that happens to work) gets the suffix `@san` on its
  id (`TEST_BUG(name, "C14@san")`): in the `--no-sanitize` build such a test
  passes and is still counted as a known bug. The reference run is the default
  (ASan) build.

## Golden images (render/)

`CHECK_GOLDEN(name, image)` (`render/render_util.h`) compares an image with
`render/golden/<name>.png` (R, G, B may differ by 3, up to 8 pixels).
`EEZ_TESTS_UPDATE_GOLDEN=1 tests/run.sh render/` writes the baselines instead;
a missing baseline is a failure. On a mismatch the actual, expected and diff
images are written to `<build dir>/render-failures/<name>/`. The baselines pin
today's output, including visible known bugs: when such a bug is fixed, update
the baseline after looking at the new image.

## The test project

`projects/make_fwtest.py` writes `projects/fwtest/fwtest.eez-project` from the
parts in `projects/parts/` (`base.py`: what the framework needs and the main page;
`expressions.py`: the expression cases; one part per test area). Names of pages,
actions, variables, styles and colors are global in the project, so a part
prefixes them with its area (`flow_`, `gui_`, `w_`, `render_`). Every item gets
Studio tags (`fwlib.tags`): the area and topics found in its name (`errors`,
`keypad`, `popups`, ...), so `tag:flow` in Studio's search finds the flow items.

```powershell
pwsh tests/projects/build_projects.ps1
```

runs `make_fwtest.py` and builds the project with EEZ Studio (headless, from
`C:\Work\eez\studio`), which writes `projects/fwtest/generated/document.h` and
`document.cpp`. These generated files are committed, so the tests build without
Studio. The tests use the generated ids (`PAGE_ID_*`, `ACTION_ID_*`, ...).

Studio numbers the flow components in the order it meets them while building,
not in file order: find a component by a property (see
`expressions/expressions_test.cpp`), not by its position.

### The external project

`projects/make_fwtest_ext.py` writes `projects/fwtest_ext/fwtest_ext.eez-project`,
a module project with `fwtest` as its master project (like a BB3+ module project):
its own pages, styles, fonts, bitmaps and colors get negative indexes in its
assets, the master styles it uses (with an `id`, `parts/external.py`) keep their
ids. `build_projects.ps1` builds it after `fwtest` into
`projects/fwtest_ext/generated/ext_assets.h` and `ext_assets.cpp` (namespace
`fwtest_ext`), which `flow/external/ext_assets_data.cpp` compiles. The tests in
`flow/external/` load these assets like the BB3+ loads module assets
(`flow/external/ext.h`).
