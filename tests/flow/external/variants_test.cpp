/*
 * eez-framework tests: the style variants in external assets (a module page):
 * a module style with looks for the variants of the master project
 * (make_fwtest_ext.mjs, mod_variant_box), the variant of a widget and of its
 * container, a master style without a look for the variant.
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

TEST("external/variants: a module style without a variant has its own look") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // mod_variants/base 10,10 100x50: mod_fill
    CHECK_EQ(ext::pixelRgb(60, 35), ext::MOD_FILL);
}

TEST("external/variants: a module style in a variant of the master project") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // mod_variants/warn 120,10: the look for variants_warn, mod_line
    CHECK_EQ(ext::pixelRgb(170, 35), ext::MOD_LINE);
    // mod_variants/volt 230,10: the look for variants_volt, the custom color
    CHECK_EQ(ext::pixelRgb(280, 35), ext::MOD_CUSTOM);
}

TEST("external/variants: a module style without a look for the variant is drawn as usual") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // mod_variants/unused 340,10: variants_unused, mod_fill
    CHECK_EQ(ext::pixelRgb(390, 35), ext::MOD_FILL);
}

TEST("external/variants: the children of a container follow its variant") {
    Assets *assets = ext::setup();
    showModVariants(assets);
    // the container 10,80 has variants_volt: mod_variants/inherit at 10,90
    CHECK_EQ(ext::pixelRgb(60, 115), ext::MOD_CUSTOM);
    // mod_variants/none at 120,90: no variant
    CHECK_EQ(ext::pixelRgb(170, 115), ext::MOD_FILL);
    // mod_variants/master at 230,90: ext_m_box of fwtest has no look for it
    CHECK_EQ(ext::pixelRgb(280, 115), ext::ACCENT);
}
