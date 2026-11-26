/*
 * eez-framework tests: the style variants in external assets (a module page):
 * a module style with variant styles for the variants of the master project
 * and of the module (make_fwtest_ext.mjs, mod_variant_box), a master style the
 * module adds a variant style to (ext_m_var_box), the variant of a widget and
 * of its container, a master style without a variant style for the variant.
 */

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>

#include "document.h"
#include "harness.h"
#include "test.h"
#include "ext.h"

using namespace eez;
using namespace eez::gui;

namespace {

void showModVariants(Assets *assets) {
    ext::show(ext::pageId(assets, fwtest_ext::PAGE_ID_MOD_VARIANTS));
}

} // namespace

TEST("external/variants: a module style without a variant is drawn as it is") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // mod_variants/base 10,10 100x50: mod_fill
    CHECK_EQ(ext::pixelRgb(60, 35), ext::MOD_FILL);
}

TEST("external/variants: a module style in a variant of the master project") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // mod_variants/warn 120,10: the variant style for variants_warn, mod_line
    CHECK_EQ(ext::pixelRgb(170, 35), ext::MOD_LINE);
    // mod_variants/volt 230,10: the variant style for variants_volt, the custom color
    CHECK_EQ(ext::pixelRgb(280, 35), ext::MOD_CUSTOM);
}

TEST("external/variants: a module style without a variant style for the variant is drawn as it is") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // mod_variants/unused 340,10: variants_unused, mod_fill
    CHECK_EQ(ext::pixelRgb(390, 35), ext::MOD_FILL);
}

TEST("external/variants: a module style in a variant of the module") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // mod_variants/module_var 450,10: mod_var (id 128), bookmark
    CHECK_EQ(ext::pixelRgb(500, 35), ext::BOOKMARK);
}

TEST("external/variants: a master style in a variant of the module has the variant style the module adds") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // mod_variants/master_var 560,10: ext_m_var_box in mod_var, mod_line
    CHECK_EQ(ext::pixelRgb(610, 35), ext::MOD_LINE);
    // mod_variants/master_plain 670,10: ext_m_var_box without a variant, accent
    CHECK_EQ(ext::pixelRgb(720, 35), ext::ACCENT);
}

TEST("external/variants: the children of a container with a variant of the module") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // the container 10,160 has mod_var: mod_variants/mod_inherit at 10,170
    CHECK_EQ(ext::pixelRgb(60, 195), ext::BOOKMARK);
    // mod_variants/mod_master at 120,170: the variant style of ext_m_var_box the module adds
    CHECK_EQ(ext::pixelRgb(170, 195), ext::MOD_LINE);
    // mod_variants/mod_warn at 230,170: its own master variant, mod_line
    CHECK_EQ(ext::pixelRgb(280, 195), ext::MOD_LINE);
}

TEST("external/variants: a master style the module adds a variant style to is a style of the module") {
    Assets *assets = ext::load();
    // the widget style of mod_variants/master_plain (a copy of ext_m_var_box
    // in the module assets) has the variant style in its list, after the
    // variants of the master project
    const PageAsset *page = assets->pages[fwtest_ext::PAGE_ID_MOD_VARIANTS - 1];
    const Widget *masterPlain = nullptr;
    for (uint32_t i = 0; i < page->widgets.count; i++) {
        const Widget *widget = page->widgets[i];
        if (widget->x == 670 && widget->y == 10) {
            masterPlain = widget;
        }
    }
    REQUIRE(masterPlain != nullptr);
    CHECK(masterPlain->style < 0);
    g_widgetCursor.assets = assets;
    const Style *style = getStyleNoVariant(masterPlain->style);
    CHECK(getVariantStyleId(style, STYLE_VARIANT_EXTERNAL_FIRST) != 0);
    CHECK_EQ(getVariantStyleId(style, STYLE_VARIANT_EXTERNAL_FIRST + 1), 0);
}

TEST("external/variants: the children of a container follow its variant") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // the container 10,80 has variants_volt: mod_variants/inherit at 10,90
    CHECK_EQ(ext::pixelRgb(60, 115), ext::MOD_CUSTOM);
    // mod_variants/none at 120,90: no variant
    CHECK_EQ(ext::pixelRgb(170, 115), ext::MOD_FILL);
    // mod_variants/master at 230,90: ext_m_box of fwtest has no variant style for it
    CHECK_EQ(ext::pixelRgb(280, 115), ext::ACCENT);
}
