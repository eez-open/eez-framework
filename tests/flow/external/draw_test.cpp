/*
 * eez-framework tests: lookups of styles, fonts, bitmaps and colors against the
 * assets of the cursor (g_widgetCursor.assets): negative ids are items of the
 * external assets, positive ids master items, module colors 65534 - k, module
 * font tags U+E002.
 */

#include <stdlib.h>
#include <string.h>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>
#include <eez/gui/font.h>

#include "document.h"
#include "harness.h"
#include "test.h"
#include "ext.h"

using namespace eez;
using namespace eez::gui;

namespace {

Assets *loadIntoCursor() {
    Assets *assets = ext::load();
    g_widgetCursor.assets = assets;
    return assets;
}

uint32_t colorRgb(uint16_t colorIndex) {
    return display::getColorFromIndex(colorIndex) & 0xFFFFFF;
}

// U+E000 (main font), U+E001 (end), U+E002 (external font) in UTF-8
#define TAG_MAIN "\xEE\x80\x80"
#define TAG_END "\xEE\x80\x81"
#define TAG_EXTERNAL "\xEE\x80\x82"
// U+00C3, a glyph of mod_icons (Icons_WaveformS), not of RobotoCondensed
#define GLYPH_C3 "\xC3\x83"

int glyphDx(const FontData *fontData, int32_t encoding) {
    font::Font font(fontData);
    auto glyph = font.getGlyph(encoding);
    return glyph ? glyph->dx : -1;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// styles, fonts, bitmaps

TEST("external/draw: getStyle with a negative id is a style of the cursor assets") {
    Assets *assets = loadIntoCursor();
    CHECK(getStyle(-(int)fwtest_ext::STYLE_ID_MOD_FILL_BOX) == assets->styles[fwtest_ext::STYLE_ID_MOD_FILL_BOX - 1]);
}

TEST("external/draw: getStyle with a positive id is a master style whatever the cursor assets") {
    loadIntoCursor();
    CHECK(getStyle(STYLE_ID_EXT_M_BOX) == g_mainAssets->styles[STYLE_ID_EXT_M_BOX - 1]);
}

TEST("external/draw: getStyle with a negative id beyond the external styles is the default style") {
    Assets *assets = loadIntoCursor();
    CHECK(getStyle(-((int)assets->styles.count + 1)) == getStyle(STYLE_ID_DEFAULT));
}

TEST("external/draw: getStyle with a negative id and the main assets in the cursor is the default style") {
    loadIntoCursor();
    g_widgetCursor.assets = g_mainAssets;
    CHECK(getStyle(-(int)fwtest_ext::STYLE_ID_MOD_FILL_BOX) == getStyle(STYLE_ID_DEFAULT));
}

TEST("external/draw: getFontData with a negative id is a font of the cursor assets") {
    Assets *assets = loadIntoCursor();
    auto fontData = getFontData(-(int)fwtest_ext::FONT_ID_MOD_ICONS);
    CHECK(fontData == assets->fonts[fwtest_ext::FONT_ID_MOD_ICONS - 1]);
    CHECK(glyphDx(fontData, 0xC3) > 0);
    CHECK(getFontData(-((int)assets->fonts.count + 1)) == nullptr);
}

TEST("external/draw: getBitmap with a negative id is a bitmap of the cursor assets") {
    Assets *assets = loadIntoCursor();
    auto bitmap = getBitmap(-(int)fwtest_ext::BITMAP_ID_MOD_BITMAP);
    REQUIRE(bitmap != nullptr);
    CHECK_EQ((int)bitmap->w, 16);
    CHECK_EQ((int)bitmap->h, 16);
    CHECK_STR(static_cast<const char *>(bitmap->name), "mod_bitmap");
    CHECK(getBitmap(-((int)assets->bitmaps.count + 1)) == nullptr);
}

// D7(b): module assets built against another master project carry master ids
// past the lists of the main assets; the lookups read past the lists (the ids
// count + 1 ... count + 8 are checked, so a zero by chance does not hide it)

TEST_BUG("external/draw: a master style id beyond the main styles is the default style", "D7") {
    loadIntoCursor();
    int count = (int)g_mainAssets->styles.count;
    for (int id = count + 1; id <= count + 8; id++) {
        CHECK(getStyle(id) == getStyle(STYLE_ID_DEFAULT));
    }
}

TEST_BUG("external/draw: a master font id beyond the main fonts has no font", "D7") {
    loadIntoCursor();
    int count = (int)g_mainAssets->fonts.count;
    for (int id = count + 1; id <= count + 8; id++) {
        CHECK(getFontData(id) == nullptr);
    }
}

TEST_BUG("external/draw: a master bitmap id beyond the main bitmaps has no bitmap", "D7") {
    // with no bitmaps in the main assets the list has no items (null): crash
    loadIntoCursor();
    int count = (int)g_mainAssets->bitmaps.count;
    for (int id = count + 1; id <= count + 8; id++) {
        CHECK(getBitmap(id) == nullptr);
    }
}

////////////////////////////////////////////////////////////////////////////////
// colors

TEST("external/draw: module colors 65534 - k resolve against the cursor assets") {
    loadIntoCursor();
    CHECK_EQ(colorRgb(ext::MOD_FILL_INDEX), ext::MOD_FILL);
    CHECK_EQ(colorRgb(ext::MOD_LINE_INDEX), ext::MOD_LINE);
}

TEST("external/draw: a custom color of the external assets comes after the module colors") {
    Assets *assets = loadIntoCursor();
    uint16_t customIndex = ext::customColorIndex(assets, ext::MOD_CUSTOM_COLOR32);
    REQUIRE(customIndex != 0);
    CHECK_EQ(colorRgb(customIndex), ext::MOD_CUSTOM);
}

TEST("external/draw: a module color with alpha keeps its alpha") {
    loadIntoCursor();
    CHECK_EQ(display::getColorFromIndex(ext::MOD_FILL_A128_INDEX), 0x8080C040u);
    CHECK(!display::isColorOpaque(ext::MOD_FILL_A128_INDEX));
    CHECK(display::isColorOpaque(ext::MOD_FILL_INDEX));
}

TEST("external/draw: an external color index past the module and custom colors is 0") {
    Assets *assets = loadIntoCursor();
    uint32_t numColors = assets->colorsDefinition->themes[0]->colors.count + assets->colorsDefinition->colors.count;
    CHECK_EQ(display::getColorFromIndex((uint16_t)(65534 - numColors)), 0u);
}

TEST("external/draw: a selected theme past the external themes uses the first one") {
    loadIntoCursor();
    g_selectedThemeIndex = 7;
    CHECK_EQ(colorRgb(ext::MOD_FILL_INDEX), ext::MOD_FILL);
}

TEST("external/draw: a master color index with external assets in the cursor") {
    loadIntoCursor();
    CHECK_EQ(colorRgb(COLOR_ID_ACCENT), ext::ACCENT);
}

////////////////////////////////////////////////////////////////////////////////
// font tags

TEST("external/draw: a U+E002 font tag selects a font of the cursor assets") {
    loadIntoCursor();
    font::Font font(getFontData(-(int)fwtest_ext::FONT_ID_MOD_ROBOTO));
    int dx = glyphDx(getFontData(-(int)fwtest_ext::FONT_ID_MOD_ICONS), 0xC3);
    REQUIRE(dx > 0);
    // U+E002, then the code point of the font index: 2 is mod_icons (-2)
    CHECK_EQ(display::measureStr(TAG_EXTERNAL "\x02" GLYPH_C3, -1, font), dx);
    // without the tag: no such glyph in mod_roboto
    CHECK_EQ(display::measureStr(GLYPH_C3, -1, font), 0);
}

TEST("external/draw: a U+E002 font tag with the main assets in the cursor falls back to the default font") {
    loadIntoCursor();
    g_widgetCursor.assets = g_mainAssets;
    font::Font font(getFontData(FONT_ID_ROBOTO_CONDENSED));
    CHECK_EQ(display::measureStr(TAG_EXTERNAL "\x02" GLYPH_C3, -1, font), 0);
}

TEST("external/draw: the font index at the end of a text with a U+E002 font tag") {
    loadIntoCursor();
    CHECK_EQ(display::getFontIndexAtEnd(TAG_EXTERNAL "\x02" GLYPH_C3, -1, 0), -(int)fwtest_ext::FONT_ID_MOD_ICONS);
    CHECK_EQ(display::getFontIndexAtEnd(TAG_EXTERNAL "\x02" GLYPH_C3 TAG_END "A", -1, 0), 0);
}

TEST("external/draw: a U+E002 font tag past the external fonts uses the default font") {
    loadIntoCursor();
    font::Font font(getFontData(-(int)fwtest_ext::FONT_ID_MOD_ROBOTO));
    int dx = glyphDx(font.fontData, 'A');
    REQUIRE(dx > 0);
    CHECK_EQ(display::measureStr(TAG_EXTERNAL "\x09" "A", -1, font), dx);
}

TEST("external/draw: a U+E000 font tag past the main fonts uses the default font") {
    loadIntoCursor();
    font::Font font(getFontData(-(int)fwtest_ext::FONT_ID_MOD_ROBOTO));
    int dx = glyphDx(font.fontData, 'A');
    REQUIRE(dx > 0);
    CHECK_EQ(display::measureStr(TAG_MAIN "\x09" "A", -1, font), dx);
}

TEST("external/draw: a U+E002 font tag at the end of a text does not read past the terminator") {
    // the NUL is not a font index (D2: handleFontTag read it as the index and
    // went on after it)
    loadIntoCursor();
    font::Font font(getFontData(-(int)fwtest_ext::FONT_ID_MOD_ROBOTO));
    static const char source[] = "ab" TAG_EXTERNAL;
    char *text = (char *)malloc(sizeof(source));
    memcpy(text, source, sizeof(source));
    int width = display::measureStr(text, -1, font);
    CHECK(width == glyphDx(font.fontData, 'a') + glyphDx(font.fontData, 'b'));
    ::free(text);
}
