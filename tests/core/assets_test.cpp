/*
 * eez-framework tests: core/assets (decompression, loading, lookups)
 */

#include <stdlib.h>
#include <string.h>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/core/memory.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>
#include <eez/gui/hooks.h>

#include "document.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;

namespace {

const int ERR_OUT_OF_DEVICE_MEMORY = -321;
const int ERR_INVALID_BLOCK_DATA = -161;

uint32_t settingsOffset() {
    return (uint32_t)((uint8_t *)&g_mainAssets->settings - (uint8_t *)g_mainAssets);
}

const Header *testHeader() {
    return (const Header *)gui::assets;
}

uint32_t decompressedBufferSize() {
    return settingsOffset() + testHeader()->decompressedSize;
}

// a heap copy of the test assets, so reads past the end are reported
uint8_t *assetsCopy() {
    uint8_t *copy = (uint8_t *)malloc(sizeof(gui::assets));
    memcpy(copy, gui::assets, sizeof(gui::assets));
    return copy;
}

// Assets with one item in the given list, in one heap block that ends with the
// item offsets: a read of the second item is past the block (AddressSanitizer
// reports it). The item points to the string "name".
template <typename T>
Assets *assetsWithOneItem(ListOfAssetsPtr<T> Assets::*list, const char **itemStr = nullptr) {
    size_t strSize = 8;
    size_t size = sizeof(Assets) + strSize + sizeof(int32_t);
    uint8_t *block = (uint8_t *)calloc(1, size);
    Assets *assets = (Assets *)block;

    char *str = (char *)(block + sizeof(Assets));
    strcpy(str, "name");
    int32_t *items = (int32_t *)(block + sizeof(Assets) + strSize);
    items[0] = (int32_t)((uint8_t *)str - (uint8_t *)&items[0]);

    // ListOfAssetsPtr: uint32_t count, then the offset of the items array
    ListOfAssetsPtr<T> &l = assets->*list;
    l.count = 1;
    uint8_t *itemsField = (uint8_t *)&l + sizeof(uint32_t);
    int32_t offset = (int32_t)((uint8_t *)items - itemsField);
    memcpy(itemsField, &offset, sizeof(offset));

    if (itemStr) {
        *itemStr = str;
    }
    return assets;
}

Assets *g_externalAssets;
int g_externalPageIndex;

int resolveExternalPage(int pageId, Assets **assets) {
    (void)pageId;
    *assets = g_externalAssets;
    return g_externalPageIndex;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// main assets

TEST("core/assets: the test assets are compressed v3 firmware assets") {
    CHECK_EQ(testHeader()->tag, HEADER_TAG_COMPRESSED);
    CHECK_EQ((int)testHeader()->projectMajorVersion, (int)PROJECT_VERSION_V3);
    CHECK_EQ((int)testHeader()->assetsType, (int)ASSETS_TYPE_FIRMWARE);
}

TEST("core/assets: loadMainAssets decompressed into a buffer of their size") {
    REQUIRE(g_mainAssets != nullptr);
    CHECK(g_mainAssetsAreMutable);
    CHECK_EQ(g_mainAssets->reserved, 0u);
    CHECK_EQ((int)g_mainAssets->external, 0);
    CHECK_EQ((int)g_mainAssets->projectMajorVersion, (int)PROJECT_VERSION_V3);
    CHECK_EQ((int)g_mainAssets->assetsType, (int)ASSETS_TYPE_FIRMWARE);
    CHECK_EQ((int)g_mainAssets->settings->displayWidth, (int)DISPLAY_WIDTH);
    CHECK_EQ((int)g_mainAssets->settings->displayHeight, (int)DISPLAY_HEIGHT);
}

TEST("core/assets: loadMainAssets of uncompressed assets uses them in place") {
    alignas(8) static uint8_t buffer[64];
    uint32_t tag = HEADER_TAG;
    memcpy(buffer, &tag, sizeof(tag));
    loadMainAssets(buffer, sizeof(buffer));
    CHECK(g_mainAssets == (Assets *)(buffer + sizeof(uint32_t)));
    CHECK(!g_mainAssetsAreMutable);
}

////////////////////////////////////////////////////////////////////////////////
// decompressAssetsData

TEST("core/assets: decompressAssetsData into a new buffer") {
    uint32_t size = decompressedBufferSize();
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    int err = 12345;
    REQUIRE(decompressAssetsData(gui::assets, sizeof(gui::assets), (Assets *)buffer, size, &err));
    // err is only set on failure
    CHECK_EQ(err, 12345);

    Assets *assets = (Assets *)buffer;
    CHECK_EQ((int)assets->projectMajorVersion, (int)PROJECT_VERSION_V3);
    CHECK_EQ((int)assets->projectMinorVersion, (int)testHeader()->projectMinorVersion);
    CHECK_EQ((int)assets->assetsType, (int)ASSETS_TYPE_FIRMWARE);
    CHECK_EQ((int)assets->settings->displayWidth, (int)DISPLAY_WIDTH);
    CHECK_EQ(assets->styles.count, g_mainAssets->styles.count);
    CHECK_EQ(assets->pages.count, g_mainAssets->pages.count);
    CHECK_EQ(assets->fonts.count, g_mainAssets->fonts.count);

    // the offsets are relative: the items are inside the new buffer
    const Style *style = assets->styles[0];
    CHECK((const uint8_t *)style > buffer && (const uint8_t *)style < buffer + size);
    CHECK(memcmp(style, g_mainAssets->styles[0], sizeof(Style)) == 0);
    ::free(buffer);
}

TEST("core/assets: decompressAssetsData into a buffer that is too small") {
    uint32_t size = decompressedBufferSize() - 1;
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    int err = 0;
    CHECK(!decompressAssetsData(gui::assets, sizeof(gui::assets), (Assets *)buffer, size, &err));
    CHECK_EQ(err, ERR_OUT_OF_DEVICE_MEMORY);
    ::free(buffer);
}

TEST("core/assets: decompressAssetsData of truncated data") {
    uint32_t size = decompressedBufferSize();
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    uint8_t *data = assetsCopy();
    int err = 0;
    CHECK(!decompressAssetsData(data, sizeof(gui::assets) - 100, (Assets *)buffer, size, &err));
    CHECK_EQ(err, ERR_INVALID_BLOCK_DATA);
    err = 0;
    CHECK(!decompressAssetsData(data, sizeof(Header) + 1, (Assets *)buffer, size, &err));
    CHECK_EQ(err, ERR_INVALID_BLOCK_DATA);
    ::free(data);
    ::free(buffer);
}

TEST("core/assets: decompressAssetsData of only a header") {
    uint32_t size = decompressedBufferSize();
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    uint8_t *data = assetsCopy();
    int err = 0;
    CHECK(!decompressAssetsData(data, sizeof(Header), (Assets *)buffer, size, &err));
    CHECK_EQ(err, ERR_INVALID_BLOCK_DATA);
    ::free(data);
    ::free(buffer);
}

TEST("core/assets: decompressAssetsData of corrupt compressed data") {
    uint32_t size = decompressedBufferSize();
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    uint8_t *data = assetsCopy();
    memset(data + sizeof(Header), 0xFF, sizeof(gui::assets) - sizeof(Header));
    int err = 0;
    CHECK(!decompressAssetsData(data, sizeof(gui::assets), (Assets *)buffer, size, &err));
    CHECK_EQ(err, ERR_INVALID_BLOCK_DATA);
    ::free(data);
    ::free(buffer);
}

TEST("core/assets: decompressAssetsData with a wrong decompressed size") {
    uint32_t size = decompressedBufferSize() + 16;
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    uint8_t *data = assetsCopy();
    Header *header = (Header *)data;
    int err = 0;
    header->decompressedSize += 1;
    CHECK(!decompressAssetsData(data, sizeof(gui::assets), (Assets *)buffer, size, &err));
    CHECK_EQ(err, ERR_INVALID_BLOCK_DATA);
    err = 0;
    header->decompressedSize -= 2;
    CHECK(!decompressAssetsData(data, sizeof(gui::assets), (Assets *)buffer, size, &err));
    CHECK_EQ(err, ERR_INVALID_BLOCK_DATA);
    ::free(data);
    ::free(buffer);
}

TEST_BUG("core/assets: decompressAssetsData checks the size before it reads the header", "C14@san") {
    // 4 bytes (only the tag): the header is read past the end of the data
    uint8_t *data = (uint8_t *)malloc(4);
    memcpy(data, &HEADER_TAG_COMPRESSED, 4);
    uint32_t size = decompressedBufferSize();
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    int err = 0;
    CHECK(!decompressAssetsData(data, 4, (Assets *)buffer, size, &err));
    ::free(buffer);
    ::free(data);
}

TEST_BUG("core/assets: decompressAssetsData rejects data without the compressed tag", "C14") {
    uint8_t *data = assetsCopy();
    ((Header *)data)->tag = 0x12345678;
    uint32_t size = decompressedBufferSize();
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    int err = 0;
    CHECK(!decompressAssetsData(data, sizeof(gui::assets), (Assets *)buffer, size, &err));
    ::free(buffer);
    ::free(data);
}

////////////////////////////////////////////////////////////////////////////////
// styles, fonts, bitmaps

TEST("core/assets: getStyle of a main assets style") {
    CHECK(getStyle(STYLE_ID_DEFAULT) == g_mainAssets->styles[STYLE_ID_DEFAULT - 1]);
    const Style *style = getStyle(STYLE_ID_ACCENT_BOX);
    REQUIRE(style != nullptr);
    CHECK_EQ((int)style->backgroundColor, (int)COLOR_ID_ACCENT);
    CHECK_EQ((int)style->borderColor, (int)COLOR_ID_TEXT);
    CHECK_EQ((int)style->borderSizeTop, 2);
    CHECK_EQ((int)style->borderSizeLeft, 2);
    CHECK_EQ((int)style->borderRadiusTLX, 8);
    // inherited from "default"
    CHECK_EQ((int)style->font, (int)FONT_ID_ROBOTO_CONDENSED);
    CHECK_EQ((int)style->color, (int)COLOR_ID_TEXT);
}

TEST("core/assets: getStyle of 0 and of external ids without external assets is the default style") {
    const Style *defaultStyle = getStyle(STYLE_ID_DEFAULT);
    CHECK(getStyle(0) == defaultStyle);
    Assets *saved = g_widgetCursor.assets;
    g_widgetCursor.assets = nullptr;
    CHECK(getStyle(-1) == defaultStyle);
    g_widgetCursor.assets = g_mainAssets;
    CHECK(getStyle(-1) == defaultStyle);
    g_widgetCursor.assets = saved;
}

TEST("core/assets: getStyle of an external style out of range is the default style") {
    const Style *defaultStyle = getStyle(STYLE_ID_DEFAULT);
    Assets *external = assetsWithOneItem(&Assets::styles);
    g_widgetCursor.assets = external;
    CHECK((const void *)getStyle(-1) == (const void *)((uint8_t *)external + sizeof(Assets)));
    CHECK(getStyle(-2) == defaultStyle);
    CHECK(getStyle(-100) == defaultStyle);
}

TEST("core/assets: getFontData") {
    const FontData *font = getFontData(FONT_ID_ROBOTO_CONDENSED);
    REQUIRE(font != nullptr);
    CHECK(font->ascent > 0);
    CHECK(font->encodingStart <= 'A');
    CHECK(font->encodingEnd >= 'z');
    CHECK(getFontData(0) == nullptr);
    g_widgetCursor.assets = nullptr;
    CHECK(getFontData(-1) == nullptr);
    g_widgetCursor.assets = g_mainAssets;
    CHECK(getFontData(-1) == nullptr);
}

TEST("core/assets: getFontData of an external font out of range is null") {
    Assets *external = assetsWithOneItem(&Assets::fonts);
    g_widgetCursor.assets = external;
    CHECK(getFontData(-1) != nullptr);
    CHECK(getFontData(-2) == nullptr);
}

TEST("core/assets: getBitmap of 0 and of external ids without external assets") {
    CHECK(getBitmap(0) == nullptr);
    g_widgetCursor.assets = nullptr;
    CHECK(getBitmap(-1) == nullptr);
    CHECK_EQ(getBitmapIdByName("missing"), 0);
}

TEST("core/assets: getBitmap of an external bitmap out of range is null") {
    Assets *external = assetsWithOneItem(&Assets::bitmaps);
    g_widgetCursor.assets = external;
    CHECK(getBitmap(-1) != nullptr);
    CHECK(getBitmap(-2) == nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// themes and colors

TEST("core/assets: themes") {
    REQUIRE(getThemesCount() >= 1);
    CHECK_STR(getThemeName(0), "Default");
    CHECK_STR(getThemeName(-1), "");
    CHECK_STR(getThemeName(getThemesCount()), "");
    CHECK(getThemeColorsCount(0) >= 6);
    CHECK_EQ(getThemeColorsCount(getThemesCount()), 0u);
    CHECK_EQ(getThemeColorsCount(-1), 0u);
    CHECK(getThemeColors(getThemesCount()) == nullptr);
    CHECK(getColors() != nullptr);
}

TEST("core/assets: theme colors are RGB565") {
    const uint16_t *colors = getThemeColors(0);
    REQUIRE(colors != nullptr);
    CHECK_EQ((int)colors[COLOR_ID_BACKGROUND], 0x0000);  // #000000
    CHECK_EQ((int)colors[COLOR_ID_TEXT], 0xFFFF);        // #ffffff
    CHECK_EQ((int)colors[COLOR_ID_ACCENT], 0x2318);      // #2060c0
    CHECK_EQ((int)colors[COLOR_ID_BOOKMARK], 0xFFE0);    // #ffff00
    CHECK_EQ((int)colors[COLOR_ID_BACKDROP], 0x2104);    // #202020
    CHECK_EQ((int)colors[COLOR_ID_ERROR], 0xC000);       // #c00000
}

TEST("core/assets: getColorFromIndex of a theme color") {
    display::ColorRGBA rgba;
    display::getColorRGBAFromIndex(COLOR_ID_ACCENT, &rgba);
    CHECK_EQ((int)rgba.r, 0x20);
    CHECK_EQ((int)rgba.g, 0x60);
    CHECK_EQ((int)rgba.b, 0xC0);
    CHECK_EQ((int)rgba.a, 255);
    display::getColorRGBAFromIndex(COLOR_ID_TEXT, &rgba);
    CHECK_EQ((int)rgba.r, 0xF8);
    CHECK_EQ((int)rgba.g, 0xFC);
    CHECK_EQ((int)rgba.b, 0xF8);
}

////////////////////////////////////////////////////////////////////////////////
// pages, actions, variable names

TEST("core/assets: getPageAsset of main pages") {
    CHECK(getPageAsset(PAGE_ID_MAIN) == g_mainAssets->pages[PAGE_ID_MAIN - 1]);
    CHECK(getPageAsset(PAGE_ID_NUMERIC_KEYPAD) == g_mainAssets->pages[PAGE_ID_NUMERIC_KEYPAD - 1]);
    CHECK(getPageAsset(0) == nullptr);
}

TEST("core/assets: getPageAsset with a widget cursor sets the assets and the flow state") {
    WidgetCursor widgetCursor;
    const PageAsset *page = getPageAsset(PAGE_ID_MAIN, widgetCursor);
    CHECK(page == g_mainAssets->pages[PAGE_ID_MAIN - 1]);
    CHECK(widgetCursor.assets == g_mainAssets);
    CHECK(widgetCursor.flowState != nullptr);
}

TEST("core/assets: getPageAsset of an external page") {
    const char *str;
    g_externalAssets = assetsWithOneItem(&Assets::pages, &str);
    g_externalPageIndex = 1;
    g_hooks.resolveExternalPage = resolveExternalPage;
    CHECK((const void *)getPageAsset(-1) == (const void *)str);
}

TEST_BUG("core/assets: getPageAsset of an external page index out of range", "C14@san") {
    // the index the hook returns is not checked against the page count
    g_externalAssets = assetsWithOneItem(&Assets::pages);
    g_externalPageIndex = 2;
    g_hooks.resolveExternalPage = resolveExternalPage;
    CHECK(getPageAsset(-1) == nullptr);
}

TEST("core/assets: main assets have no action names") {
    // Studio writes the action names only for assets with a master project
    // (external assets, e.g. BB3+ modules)
    CHECK_EQ(g_mainAssets->actionNames.count, 0u);
    WidgetCursor widgetCursor;
    widgetCursor.assets = g_mainAssets;
    CHECK(getActionName(widgetCursor, 0) == nullptr);
    widgetCursor.assets = nullptr;
    CHECK_STR(getActionName(widgetCursor, ACTION_ID_SCROLL), "");
}

TEST("core/assets: getActionName of external assets") {
    WidgetCursor widgetCursor;
    widgetCursor.assets = assetsWithOneItem(&Assets::actionNames);
    CHECK_STR(getActionName(widgetCursor, 1), "name");
    // a negative id (a user action) is the same action
    CHECK_STR(getActionName(widgetCursor, -1), "name");
}

TEST_BUG("core/assets: getActionName of an action id out of range", "C14@san") {
    // no bounds check: reads past the action names
    WidgetCursor widgetCursor;
    widgetCursor.assets = assetsWithOneItem(&Assets::actionNames);
    const char *name = getActionName(widgetCursor, 2);
    CHECK(name == nullptr || *name == 0);
}

TEST("core/assets: getDataIdFromName") {
    WidgetCursor widgetCursor;
    widgetCursor.assets = g_mainAssets;
    auto &names = g_mainAssets->variableNames;
    for (uint32_t i = 0; i < names.count; i++) {
        CHECK_EQ((int)getDataIdFromName(widgetCursor, names[i]), -((int)i + 1));
    }
    CHECK_EQ((int)getDataIdFromName(widgetCursor, "no such variable"), 0);
    widgetCursor.assets = nullptr;
    CHECK_EQ((int)getDataIdFromName(widgetCursor, "anything"), 0);
}
