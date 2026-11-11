/*
 * eez-framework tests: external pages (pages of external assets on the page
 * stack, BB3+ external page ids) and how their styles, colors, fonts and
 * bitmaps are resolved against the external assets.
 */

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/flow/flow.h>

#include "document.h"
#include "harness.h"
#include "test.h"
#include "ext.h"

using namespace eez;
using namespace eez::gui;

namespace {

int showModMain(Assets *assets) {
    int pageId = ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    ext::show(pageId);
    return pageId;
}

} // namespace

TEST("external/pages: an external page id combines the assets slot and the page") {
    Assets *assets = ext::load();
    int pageId = ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    CHECK(pageId < 0);
    // the first slot of the table
    CHECK_EQ(pageId, -((1 << 16) | (int)fwtest_ext::PAGE_ID_MOD_MAIN));
    Assets *resolved = nullptr;
    CHECK_EQ(ext::resolveExternalPageHook(pageId, &resolved), (int)fwtest_ext::PAGE_ID_MOD_MAIN);
    CHECK(resolved == assets);
}

TEST("external/pages: getPageAsset resolves an external page id through the hook") {
    Assets *assets = ext::load();
    int pageId = ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_MAIN);
    CHECK(getPageAsset(pageId) == assets->pages[fwtest_ext::PAGE_ID_MOD_MAIN - 1]);
    WidgetCursor widgetCursor;
    CHECK(getPageAsset(pageId, widgetCursor) == assets->pages[fwtest_ext::PAGE_ID_MOD_MAIN - 1]);
    CHECK(widgetCursor.assets == assets);
}

TEST("external/pages: a page id of an external page in the cursor assets resolves without the slot") {
    // inside external pages a layout id is -pageId (Studio's negative page index)
    Assets *assets = ext::load();
    WidgetCursor widgetCursor;
    widgetCursor.assets = assets;
    CHECK(getPageAsset(-(int)fwtest_ext::PAGE_ID_MOD_UW, widgetCursor) == assets->pages[fwtest_ext::PAGE_ID_MOD_UW - 1]);
    CHECK(widgetCursor.assets == assets);
}

TEST("external/pages: an external page is shown on top of the main page") {
    Assets *assets = ext::setup();
    int pageId = showModMain(assets);
    CHECK_EQ(harness::appContext()->getActivePageId(), pageId);
    CHECK(harness::appContext()->isPageOnStack(PAGE_ID_MAIN));
    harness::appContext()->popPage();
    harness::frame(2);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_MAIN);
}

TEST("external/pages: an external style with a module color") {
    Assets *assets = ext::setup();
    showModMain(assets);
    // mod_main/fill 10,10 100x50, mod_fill_box: mod_fill #40c080
    CHECK_EQ(ext::pixelRgb(60, 35), ext::MOD_FILL);
}

TEST("external/pages: a master style in an external page") {
    Assets *assets = ext::setup();
    showModMain(assets);
    // mod_main/master_style 120,10 100x50, ext_m_box of fwtest: accent #2060c0
    CHECK_EQ(ext::pixelRgb(170, 35), ext::ACCENT);
}

TEST("external/pages: an external style with a custom color") {
    Assets *assets = ext::setup();
    showModMain(assets);
    // mod_main/custom 230,10 100x50, mod_custom_box: #8040c0
    CHECK_EQ(ext::pixelRgb(280, 35), ext::MOD_CUSTOM);
}

TEST("external/pages: an external style with a master color") {
    Assets *assets = ext::setup();
    showModMain(assets);
    // mod_main/master_color 340,10 100x50, mod_master_color_box: bookmark #ffff00
    CHECK_EQ(ext::pixelRgb(390, 35), ext::BOOKMARK);
}

TEST("external/pages: an external bitmap") {
    Assets *assets = ext::setup();
    showModMain(assets);
    // mod_main/bitmap 450,10 16x16: mod_bitmap, #80c040
    CHECK_EQ(ext::pixelRgb(458, 18), ext::MOD_BITMAP);
    CHECK_EQ(ext::countOther(450, 10, 16, 16, ext::MOD_BITMAP), 0);
}

TEST("external/pages: a module font from an external style") {
    Assets *assets = ext::setup();
    showModMain(assets);
    // mod_main/icons 10,80 200x40: U+00C3 in mod_icons (a glyph of Icons_WaveformS)
    CHECK(ext::countOther(10, 80, 200, 40, ext::BLACK) > 100);
}

TEST("external/pages: a module font from a U+E002 font tag") {
    Assets *assets = ext::setup();
    showModMain(assets);
    // mod_main/tag 220,80 200x40: <font name='mod_icons'>U+00C3</font> in mod_roboto
    CHECK(ext::countOther(220, 80, 200, 40, ext::BLACK) > 100);
}

TEST("external/pages: without the module font the glyph is not drawn") {
    Assets *assets = ext::setup();
    showModMain(assets);
    // mod_main/no_glyph 430,80 200x40: U+00C3 in mod_roboto, which has no such glyph
    CHECK_EQ(ext::countOther(430, 80, 200, 40, ext::BLACK), 0);
}

TEST("external/pages: the page flow state has the external assets") {
    Assets *assets = ext::setup();
    showModMain(assets);
    auto flowState = ext::topLevelFlowState(assets, fwtest_ext::PAGE_ID_MOD_MAIN - 1);
    REQUIRE(flowState != nullptr);
    CHECK(!flowState->isAction);
    CHECK(flowState->parentFlowState == nullptr);
    CHECK(flowState->flow == assets->flowDefinition->flows[fwtest_ext::PAGE_ID_MOD_MAIN - 1]);
}

TEST("external/pages: the page open event runs in the external page flow") {
    Assets *assets = ext::setup();
    showModMain(assets);
    CHECK_EQ(ext::getGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_OPENED).getInt(), 1);
    // the main global variables are not touched
    CHECK(harness::flowErrors().empty());
}

TEST("external/pages: the page flow state stays after the page is closed") {
    // as the flow states of the main pages (review P2): kept for the session
    Assets *assets = ext::setup();
    showModMain(assets);
    harness::appContext()->popPage();
    harness::frame(3);
    CHECK(ext::topLevelFlowState(assets, fwtest_ext::PAGE_ID_MOD_MAIN - 1) != nullptr);
    // shown again: the same flow state, the open event again
    showModMain(assets);
    CHECK_EQ(ext::countFlowStates(assets, false), 1);
    CHECK_EQ(ext::getGlobal(assets, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_OPENED).getInt(), 2);
}

TEST_BUG("external/pages: an external page of unloaded assets on the stack does not crash", "G16") {
    // getPageAsset returns nullptr, updatePage reads page->flags
    Assets *assets = ext::setup();
    showModMain(assets);
    ext::forgetExternalAssets(assets);
    harness::frame(2);
    CHECK(true);
}

TEST_BUG("external/pages: a negative page id with the default resolveExternalPage hook gives no page", "C14") {
    // the default hook returns page 0 with the main assets, getPageAsset then
    // reads pages[-1]: the Settings object before the list (800, 480) taken
    // as an offset
    CHECK(getPageAsset(-1) == nullptr);
}
