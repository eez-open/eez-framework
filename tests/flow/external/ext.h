/*
 * eez-framework tests: external assets, the BB3+ module model.
 *
 * The BB3+ loads the assets of a module next to its main assets and runs the
 * module flows inside the BB3+ (src/bb3/assets.cpp, src/bb3/gui/app_context.cpp,
 * src/bb3/module/module.cpp, src/bb3/flow/flow.cpp). ext.cpp has copies of the
 * parts of that code the framework depends on: the loader (LZ4 branch of
 * loadExternalAssets), the external page ids (getExternalAssetsPageId) and the
 * resolveExternalPage hook, setExternalFlowError and stopExternalFlow.
 *
 * The external assets are projects/fwtest_ext (make_fwtest_ext.py), a module
 * project with projects/fwtest as its master project. Every test starts from
 * the main-only state of the harness, so a test calls setup() first.
 */

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/core/value.h>
#include <eez/flow/private.h>

#include "../../projects/fwtest_ext/generated/ext_assets.h"

namespace ext {

using eez::Assets;
using eez::Value;
using eez::flow::FlowState;

////////////////////////////////////////////////////////////////////////////////
// the BB3+ model

// src/bb3/assets.cpp loadExternalAssets (LZ4 branch): the assets in a block of
// the framework heap (eez::alloc), external = true. nullptr and *err on error.
Assets *loadExternalAssets(const uint8_t *assetsData, uint32_t assetsDataSize, int *err);

// src/bb3/assets.cpp getExternalAssetsPageId: -(((i + 1) << 16) | pageId),
// i is the index of the assets in the table of external assets
int getExternalAssetsPageId(Assets *assets, int pageId);

// src/bb3/assets.cpp resolveExternalPageHook (the g_hooks.resolveExternalPage)
int resolveExternalPageHook(int pageId, Assets **assets);

// removes the assets from the table of external assets (unloadScriptExternalAssets)
void forgetExternalAssets(Assets *assets);

// src/bb3/flow/flow.cpp
void setExternalFlowError(Assets *assets);
void stopExternalFlow(Assets *assets);

////////////////////////////////////////////////////////////////////////////////
// test setup

// the assets data of fwtest_ext copied to a heap buffer of the exact size
// (ASan sees reads past its end)
std::vector<uint8_t> assetsData();

// sets g_hooks.resolveExternalPage
void installHooks();

// installHooks() and loads fwtest_ext (REQUIRE)
Assets *load();

// load() and flow::start
Assets *setup();

// frees the assets after forgetExternalAssets
void unload(Assets *assets);

// the external page id of a page of fwtest_ext (fwtest_ext::PAGE_ID_...)
int pageId(Assets *assets, int extPageId);

// pushes the page and runs frames
void show(int pageId, int frames = 3);

////////////////////////////////////////////////////////////////////////////////
// flow states

// the flow state of a page in g_firstFlowState (top level)
FlowState *topLevelFlowState(Assets *assets, int flowIndex);

// a flow state with these assets and flow index anywhere in the tree
FlowState *findFlowState(Assets *assets, int flowIndex);

// number of flow states with these assets: top level only or the whole tree
int countFlowStates(Assets *assets, bool wholeTree);

// all flow states with these assets in the tree
void collectFlowStates(Assets *assets, std::vector<FlowState *> &result);

// evaluates the expression of the probe "probe:<name>" (a SetVariable
// component, see projects/parts/external.py) in the flow state
bool probe(FlowState *flowState, const char *name, Value &result);

// assigns the value to the variable of entry 1 of the probe (an assignable
// expression evaluated in the flow state)
void assignProbe(FlowState *flowState, const char *name, const Value &value);

Value getGlobal(Assets *assets, int index);
void setGlobal(Assets *assets, int index, const Value &value);

////////////////////////////////////////////////////////////////////////////////
// flow errors with the assets of the flow state (the harness records them too)

struct ErrorRecord {
    Assets *assets;
    int flowIndex;
    std::string message;
};

std::vector<ErrorRecord> &errors();

// chains onFlowErrorHook: records the error, then calls the harness hook
void recordErrors();

// prints the flow errors the harness recorded (shown with --verbose and on failures)
void printFlowErrors();

////////////////////////////////////////////////////////////////////////////////
// pixels of the last presented frame

// RGB of a pixel (0x00BBGGRR)
uint32_t pixelRgb(int x, int y);

// number of pixels in the rectangle that are not of this RGB color
int countOther(int x, int y, int w, int h, uint32_t color);

// colors of fwtest (master) and fwtest_ext
static const uint32_t BLACK = 0x000000;
static const uint32_t ACCENT = 0xC06020;        // #2060c0
static const uint32_t BOOKMARK = 0x00FCF8;      // #ffff00 in RGB565 (#f8fc00)
static const uint32_t MOD_FILL = 0x80C040;      // #40c080
static const uint32_t MOD_LINE = 0x0040C0;      // #c04000
static const uint32_t MOD_CUSTOM = 0xC04080;    // #8040c0
static const uint32_t MOD_BITMAP = 0x40C080;    // #80c040

// the color indexes of fwtest_ext: module colors 65534 - k, then the custom colors
static const uint16_t MOD_FILL_INDEX = 65534;
static const uint16_t MOD_LINE_INDEX = 65533;

// RGB565 of the custom color #8040c0
static const uint16_t MOD_CUSTOM_565 = 0x8218;

// the color index of a custom color of the external assets (65534 - number of
// module colors - index in the custom colors), 0 if it is not there
uint16_t customColorIndex(Assets *assets, uint16_t rgb565);

} // namespace ext
