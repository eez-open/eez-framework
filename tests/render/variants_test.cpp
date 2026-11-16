/*
 * eez-framework tests: the style variants (projects/parts/variants.py): the
 * variant styles of a style (getVariantStyles), the variant of a widget
 * (fixed, by data, of its container, none), the states of a variant.
 */

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>

#include "document.h"
#include "harness.h"
#include "render_util.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;
using render::rgb;

////////////////////////////////////////////////////////////////////////////////
// native variable and action of the part

namespace {

int g_variantsState;
int g_variantsClicks;

} // namespace

namespace eez {
namespace gui {

void data_variants_state(DataOperationEnum operation, const WidgetCursor &, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_variantsState, VALUE_TYPE_INT32);
    }
}

void action_variants_on_click() {
    g_variantsClicks++;
}

} // namespace gui
} // namespace eez

namespace {

uint32_t px(int x, int y) {
    return harness::pixel(x, y) | 0xFF000000u;
}

const uint32_t BG = rgb(0x20, 0x20, 0x20);
const uint32_t RED = rgb(0xF8, 0, 0);
const uint32_t GREEN = rgb(0, 0xFC, 0);
const uint32_t BLUE = rgb(0, 0, 0xF8);
const uint32_t ORANGE = rgb(0xF8, 0x80, 0);

// the ids of the variants (parts/variants.py)
const int WARN = 1;
const int VOLT = 2;
const int UNUSED = 3;

// the boxes of the page (parts/variants.py), 160 x 50
const int FIXED[] = { 20, 20 };
const int BASE[] = { 200, 20 };
const int VOLT_BOX[] = { 380, 20 };
const int UNUSED_BOX[] = { 560, 20 };
const int RULES[] = { 20, 90 };
const int RULES_FIXED[] = { 200, 90 };
// in the container at 20, 160
const int INHERIT[] = { 20, 170 };
const int NONE[] = { 200, 170 };
const int OWN[] = { 380, 170 };
const int DEEP[] = { 560, 170 };
// in the container at 20, 250
const int FOLLOWS[] = { 20, 260 };
const int FOLLOWS_NONE[] = { 200, 260 };

// the background of a box
uint32_t fill(const int *box) {
    return px(box[0] + 4, box[1] + 4);
}

void show(int state = 0) {
    g_variantsState = state;
    render::showPage(PAGE_ID_VARIANTS_PAGE);
}

void setState(int state) {
    g_variantsState = state;
    harness::frame(3);
}

void press(const int *box) {
    harness::touch(box[0] + 80, box[1] + 25);
    harness::frame(2);
}

void release() {
    harness::release();
    harness::frame(2);
    // the next touch is not an accidental double tap
    harness::frame(25);
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// assets

TEST("render/variants: a style with variant styles has the list of their style ids") {
    const Style *box = getStyleNoVariant(STYLE_ID_VARIANTS_BOX);
    const int16_t *variantStyles = getVariantStyles(box);
    REQUIRE(variantStyles != nullptr);
    // the number of the variants of the project, then a style for each
    CHECK_EQ((int)variantStyles[0], 3);
    CHECK(variantStyles[WARN] != 0);
    CHECK(variantStyles[VOLT] != 0);
    CHECK_EQ((int)variantStyles[UNUSED], 0);
    CHECK(variantStyles[WARN] != variantStyles[VOLT]);

    // a variant style is the base style with the properties of the variant
    const Style *warn = getStyleNoVariant(variantStyles[WARN]);
    CHECK_EQ((int)warn->borderSizeTop, (int)box->borderSizeTop);
    CHECK_EQ((int)warn->color, (int)box->color);
    CHECK(warn->backgroundColor != box->backgroundColor);
    // and has no variants
    CHECK(getVariantStyles(warn) == nullptr);
}

TEST("render/variants: a style without variant styles has no list") {
    CHECK(getVariantStyles(getStyleNoVariant(STYLE_ID_VARIANTS_PLAIN)) == nullptr);
    // a state style
    const Style *box = getStyleNoVariant(STYLE_ID_VARIANTS_BOX);
    REQUIRE(box->stateStyles[STYLE_STATE_PRESSED] != 0);
    CHECK(getVariantStyles(getStyleNoVariant(box->stateStyles[STYLE_STATE_PRESSED])) == nullptr);
}

TEST("render/variants: a variant has the states of the base style, and its own over them") {
    const Style *box = getStyleNoVariant(STYLE_ID_VARIANTS_BOX);
    const int16_t *variantStyles = getVariantStyles(box);
    REQUIRE(variantStyles != nullptr);
    const Style *pressed = getStyleNoVariant(box->stateStyles[STYLE_STATE_PRESSED]);

    // volt has no pressed style of its own: the pressed look of the base
    const Style *volt = getStyleNoVariant(variantStyles[VOLT]);
    REQUIRE(volt->stateStyles[STYLE_STATE_PRESSED] != 0);
    const Style *voltPressed = getStyleNoVariant(volt->stateStyles[STYLE_STATE_PRESSED]);
    CHECK_EQ((int)voltPressed->backgroundColor, (int)pressed->backgroundColor);

    // warn has its own
    const Style *warn = getStyleNoVariant(variantStyles[WARN]);
    REQUIRE(warn->stateStyles[STYLE_STATE_PRESSED] != 0);
    const Style *warnPressed = getStyleNoVariant(warn->stateStyles[STYLE_STATE_PRESSED]);
    CHECK(warnPressed->backgroundColor != pressed->backgroundColor);
    CHECK(warnPressed->backgroundColor != warn->backgroundColor);
}

TEST("render/variants: getStyle gives the style of the variant that is set") {
    const Style *box = getStyleNoVariant(STYLE_ID_VARIANTS_BOX);
    const int16_t *variantStyles = getVariantStyles(box);
    REQUIRE(variantStyles != nullptr);

    CHECK_EQ(getStyleVariant(), 0);
    CHECK(getStyle(STYLE_ID_VARIANTS_BOX) == box);

    CHECK_EQ(setStyleVariant(WARN), 0);
    CHECK(getStyle(STYLE_ID_VARIANTS_BOX) == getStyleNoVariant(variantStyles[WARN]));
    // a style without a style for the variant
    CHECK(getStyle(STYLE_ID_VARIANTS_PLAIN) == getStyleNoVariant(STYLE_ID_VARIANTS_PLAIN));
    CHECK(getStyleNoVariant(STYLE_ID_VARIANTS_BOX) == box);

    setStyleVariant(UNUSED);
    CHECK(getStyle(STYLE_ID_VARIANTS_BOX) == box);
    // not a variant of the project
    setStyleVariant(4);
    CHECK(getStyle(STYLE_ID_VARIANTS_BOX) == box);

    CHECK_EQ(setStyleVariant(0), 4);
    CHECK(getStyle(STYLE_ID_VARIANTS_BOX) == box);
}

TEST("render/variants: a copy of a style has the same properties") {
    Style copy = *getStyleNoVariant(STYLE_ID_VARIANTS_BOX);
    CHECK_EQ((int)copy.backgroundColor, (int)getStyleNoVariant(STYLE_ID_VARIANTS_BOX)->backgroundColor);
}

////////////////////////////////////////////////////////////////////////////////
// widgets

TEST("render/variants: golden") {
    show();
    CHECK_GOLDEN("variants_page", render::captureFrame());
}

TEST("render/variants: a widget is drawn in its fixed variant") {
    show();
    CHECK_EQ(fill(FIXED), ORANGE);
    CHECK_EQ(fill(BASE), BG);
    CHECK_EQ(fill(VOLT_BOX), BLUE);
    // the style has no style for the variant
    CHECK_EQ(fill(UNUSED_BOX), BG);
    // no variant is left set after the frame
    CHECK_EQ(getStyleVariant(), 0);
}

TEST("render/variants: rules give the variant by data") {
    show();
    CHECK_EQ(fill(RULES), BG);
    setState(1);
    CHECK_EQ(fill(RULES), ORANGE);
    setState(2);
    CHECK_EQ(fill(RULES), BLUE);
    setState(3);
    CHECK_EQ(fill(RULES), BG);
    CHECK_EQ(render::compareWithFullRedraw("variants_rules"), 0);
}

TEST("render/variants: when no rule applies the widget has its fixed variant") {
    show();
    CHECK_EQ(fill(RULES_FIXED), BLUE);
    setState(1);
    CHECK_EQ(fill(RULES_FIXED), ORANGE);
    // the rule None
    setState(2);
    CHECK_EQ(fill(RULES_FIXED), BG);
    setState(0);
    CHECK_EQ(fill(RULES_FIXED), BLUE);
}

TEST("render/variants: a widget without a variant has the variant of its container") {
    show();
    CHECK_EQ(fill(INHERIT), ORANGE);
    // None: not the variant of the container
    CHECK_EQ(fill(NONE), BG);
    // its own variant
    CHECK_EQ(fill(OWN), BLUE);
    // through a container whose style has no style for the variant
    CHECK_EQ(fill(DEEP), ORANGE);
    // the container after it is not in the variant
    CHECK_EQ(fill(FOLLOWS), BG);
}

TEST("render/variants: a change of the variant of a container draws its children again") {
    show();
    CHECK_EQ(fill(FOLLOWS), BG);
    CHECK_EQ(fill(FOLLOWS_NONE), BG);
    setState(1);
    CHECK_EQ(fill(FOLLOWS), ORANGE);
    CHECK_EQ(fill(FOLLOWS_NONE), BG);
    CHECK_EQ(render::compareWithFullRedraw("variants_container"), 0);
    setState(0);
    CHECK_EQ(fill(FOLLOWS), BG);
    CHECK_EQ(render::compareWithFullRedraw("variants_container_back"), 0);
}

TEST("render/variants: a pressed widget is drawn in the pressed state of its variant") {
    show();

    // the pressed style of the variant
    press(FIXED);
    CHECK_EQ(fill(FIXED), RED);
    CHECK_EQ(fill(BASE), BG);
    release();
    CHECK_EQ(fill(FIXED), ORANGE);
    CHECK(g_variantsClicks > 0);

    // a variant without one: the pressed style of the base style
    press(VOLT_BOX);
    CHECK_EQ(fill(VOLT_BOX), GREEN);
    release();
    CHECK_EQ(fill(VOLT_BOX), BLUE);

    press(BASE);
    CHECK_EQ(fill(BASE), GREEN);
    release();
    CHECK_EQ(fill(BASE), BG);

    // a variant by data
    setState(1);
    press(RULES);
    CHECK_EQ(fill(RULES), RED);
    release();
    CHECK_EQ(fill(RULES), ORANGE);
}

TEST("render/variants: the widget at a point is found with a variant set") {
    show();
    press(FIXED);
    release();
    // the find pass leaves no variant set
    CHECK_EQ(getStyleVariant(), 0);
}
