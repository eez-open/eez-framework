/*
 * eez-framework tests: loading external assets (decompressAssetsData through
 * the BB3+ loader model, ext.h) and what the external assets contain.
 */

#include <stdlib.h>
#include <string.h>

#include <vector>

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/core/alloc.h>
#include <eez/gui/gui.h>
#include <eez/flow/flow.h>

#include "document.h"
#include "harness.h"
#include "test.h"
#include "ext.h"

using namespace eez;
using namespace eez::gui;

namespace {

const int SCPI_ERROR_OUT_OF_DEVICE_MEMORY = -321;
const int SCPI_ERROR_INVALID_BLOCK_DATA = -161;

// the assets data in a heap buffer of the exact size (ASan sees overreads)
struct HeapData {
    uint8_t *data;
    uint32_t size;
    explicit HeapData(const std::vector<uint8_t> &bytes, uint32_t size_) : size(size_) {
        data = (uint8_t *)malloc(size ? size : 1);
        memcpy(data, bytes.data(), size < bytes.size() ? size : bytes.size());
    }
    ~HeapData() {
        ::free(data);
    }
};

Assets *loadFrom(const std::vector<uint8_t> &bytes, uint32_t size, int &err) {
    HeapData heap(bytes, size);
    err = 0;
    return ext::loadExternalAssets(heap.data, heap.size, &err);
}

} // namespace

TEST("external/load: the fwtest_ext assets are LZ4 compressed firmware assets") {
    auto bytes = ext::assetsData();
    auto header = (const Header *)bytes.data();
    CHECK_EQ(header->tag, HEADER_TAG_COMPRESSED);
    CHECK_EQ((int)header->projectMajorVersion, (int)PROJECT_VERSION_V3);
    // 0: LZ4 (the BB3+ module projects have bb3ModuleMetadata and are LZMA, 1)
    CHECK_EQ((int)header->reserved, 0);
}

TEST("external/load: valid data loads as external assets") {
    int err;
    Assets *assets = loadFrom(ext::assetsData(), (uint32_t)ext::assetsData().size(), err);
    REQUIRE(assets != nullptr);
    CHECK_EQ(err, 0);
    CHECK_EQ((int)assets->external, 1);
    CHECK_EQ((int)assets->projectMajorVersion, (int)PROJECT_VERSION_V3);
    CHECK_EQ((int)assets->assetsType, (int)ASSETS_TYPE_FIRMWARE);
    CHECK(assets != g_mainAssets);
    CHECK(assets->settings != nullptr);
    CHECK_EQ((int)assets->settings->displayWidth, 800);
}

TEST("external/load: the items of the external project") {
    Assets *assets = ext::load();
    CHECK_EQ((int)assets->pages.count, (int)fwtest_ext::PAGE_ID_MOD_UW);
    CHECK_EQ((int)assets->styles.count, (int)fwtest_ext::STYLE_ID_MOD_UW_FRAME);
    CHECK_EQ((int)assets->fonts.count, (int)fwtest_ext::FONT_ID_MOD_ICONS);
    CHECK_EQ((int)assets->bitmaps.count, (int)fwtest_ext::BITMAP_ID_MOD_BITMAP);
    REQUIRE(assets->flowDefinition != nullptr);
    // the pages and the two actions
    CHECK_EQ((int)assets->flowDefinition->flows.count, (int)fwtest_ext::PAGE_ID_MOD_UW + 2);
    CHECK_EQ((int)assets->flowDefinition->globalVariables.count, (int)fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_CAUGHT + 1);
    // a project with a master project has the names of its actions and variables
    CHECK_EQ((int)assets->actionNames.count, 2);
    CHECK_EQ((int)assets->variableNames.count, (int)fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_CAUGHT + 1);
}

TEST("external/load: module colors have a value for each master theme") {
    Assets *assets = ext::load();
    REQUIRE(assets->colorsDefinition != nullptr);
    auto &themes = assets->colorsDefinition->themes;
    REQUIRE((int)themes.count == (int)g_mainAssets->colorsDefinition->themes.count);
    CHECK_STR(static_cast<const char *>(themes[0]->name), "Default");
    // mod_fill, mod_line
    CHECK_EQ((int)themes[0]->colors.count, 2);
    // the custom colors: #8040c0 of mod_custom_box and the default style colors
    CHECK(ext::customColorIndex(assets, ext::MOD_CUSTOM_565) != 0);
}

TEST("external/load: own items have negative indexes, master styles their ids") {
    Assets *assets = ext::load();
    auto page = assets->pages[fwtest_ext::PAGE_ID_MOD_MAIN - 1];
    REQUIRE(page != nullptr);
    REQUIRE(page->widgets.count >= 2);
    // mod_main/fill: mod_fill_box of the external project
    CHECK_EQ((int)page->widgets[0]->style, -(int)fwtest_ext::STYLE_ID_MOD_FILL_BOX);
    // mod_main/master_style: ext_m_box of the master project (id 1)
    CHECK_EQ((int)page->widgets[1]->style, (int)STYLE_ID_EXT_M_BOX);
}

TEST("external/load: module colors are 65534 - k, the custom colors follow") {
    Assets *assets = ext::load();
    CHECK_EQ((int)assets->styles[fwtest_ext::STYLE_ID_MOD_FILL_BOX - 1]->backgroundColor, (int)ext::MOD_FILL_INDEX);
    CHECK_EQ((int)assets->styles[fwtest_ext::STYLE_ID_MOD_LINE_BOX - 1]->backgroundColor, (int)ext::MOD_LINE_INDEX);
    uint16_t customIndex = ext::customColorIndex(assets, ext::MOD_CUSTOM_565);
    CHECK(customIndex != 0 && customIndex < ext::MOD_LINE_INDEX);
    CHECK_EQ((int)assets->styles[fwtest_ext::STYLE_ID_MOD_CUSTOM_BOX - 1]->backgroundColor, (int)customIndex);
    // a master color keeps its index
    CHECK_EQ((int)assets->styles[fwtest_ext::STYLE_ID_MOD_MASTER_COLOR_BOX - 1]->backgroundColor, (int)COLOR_ID_BOOKMARK);
}

TEST("external/load: a module font in a style is a negative int8") {
    Assets *assets = ext::load();
    CHECK_EQ((int)assets->styles[fwtest_ext::STYLE_ID_MOD_ICONS_TEXT - 1]->font, -(int)fwtest_ext::FONT_ID_MOD_ICONS);
    CHECK_EQ((int)assets->styles[fwtest_ext::STYLE_ID_DEFAULT - 1]->font, -(int)fwtest_ext::FONT_ID_MOD_ROBOTO);
}

TEST("external/load: two loads are two independent copies") {
    Assets *a = ext::load();
    Assets *b = ext::load();
    REQUIRE(a != b);
    int pageA = ext::pageId(a, fwtest_ext::PAGE_ID_MOD_MAIN);
    int pageB = ext::pageId(b, fwtest_ext::PAGE_ID_MOD_MAIN);
    CHECK(pageA != pageB);
    Assets *resolved = nullptr;
    CHECK_EQ(ext::resolveExternalPageHook(pageB, &resolved), (int)fwtest_ext::PAGE_ID_MOD_MAIN);
    CHECK(resolved == b);
    // the global variables are in the assets memory
    ext::setGlobal(a, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER, Value(5, VALUE_TYPE_INT32));
    CHECK_EQ(ext::getGlobal(b, fwtest_ext::FLOW_GLOBAL_VARIABLE_MOD_COUNTER).getInt(), 0);
}

TEST("external/load: truncated data is rejected") {
    auto bytes = ext::assetsData();
    int err;
    Assets *assets = loadFrom(bytes, (uint32_t)bytes.size() / 2, err);
    CHECK(assets == nullptr);
    CHECK_EQ(err, SCPI_ERROR_INVALID_BLOCK_DATA);
}

TEST("external/load: data one byte short is rejected") {
    auto bytes = ext::assetsData();
    int err;
    Assets *assets = loadFrom(bytes, (uint32_t)bytes.size() - 1, err);
    CHECK(assets == nullptr);
    CHECK_EQ(err, SCPI_ERROR_INVALID_BLOCK_DATA);
}

TEST("external/load: only the header is rejected") {
    auto bytes = ext::assetsData();
    int err;
    Assets *assets = loadFrom(bytes, sizeof(Header), err);
    CHECK(assets == nullptr);
    CHECK_EQ(err, SCPI_ERROR_INVALID_BLOCK_DATA);
}

TEST("external/load: a wrong decompressed size in the header is rejected") {
    auto bytes = ext::assetsData();
    ((Header *)bytes.data())->decompressedSize += 1;
    int err;
    Assets *assets = loadFrom(bytes, (uint32_t)bytes.size(), err);
    CHECK(assets == nullptr);
    CHECK_EQ(err, SCPI_ERROR_INVALID_BLOCK_DATA);
}

TEST("external/load: a corrupt compressed stream is rejected") {
    auto bytes = ext::assetsData();
    memset(bytes.data() + sizeof(Header), 0xFF, bytes.size() - sizeof(Header));
    int err;
    Assets *assets = loadFrom(bytes, (uint32_t)bytes.size(), err);
    CHECK(assets == nullptr);
    CHECK_EQ(err, SCPI_ERROR_INVALID_BLOCK_DATA);
}

TEST("external/load: decompressed data larger than the buffer is rejected") {
    auto bytes = ext::assetsData();
    HeapData heap(bytes, (uint32_t)bytes.size());
    auto header = (const Header *)heap.data;
    uint32_t bufferSize = 64 + header->decompressedSize / 2;
    auto assets = (Assets *)eez::alloc(bufferSize, 0x5e1f0001);
    REQUIRE(assets != nullptr);
    int err = 0;
    CHECK(!decompressAssetsData(heap.data, heap.size, assets, bufferSize, &err));
    CHECK_EQ(err, SCPI_ERROR_OUT_OF_DEVICE_MEMORY);
    eez::free(assets);
}

TEST_BUG("external/load: data shorter than the header is rejected without reading past it", "C14@san") {
    // decompressAssetsData reads the 12 byte header of a 6 byte buffer (ASan)
    auto bytes = ext::assetsData();
    HeapData heap(bytes, 6);
    auto assets = (Assets *)eez::alloc(sizeof(Assets) + 1024, 0x5e1f0002);
    REQUIRE(assets != nullptr);
    int err = 0;
    CHECK(!decompressAssetsData(heap.data, heap.size, assets, sizeof(Assets) + 1024, &err));
    CHECK_EQ(err, SCPI_ERROR_INVALID_BLOCK_DATA);
    eez::free(assets);
}

TEST_BUG("external/load: data without the compressed assets tag is rejected", "C14") {
    auto bytes = ext::assetsData();
    memcpy(bytes.data(), "JUNK", 4);
    int err;
    Assets *assets = loadFrom(bytes, (uint32_t)bytes.size(), err);
    CHECK(assets == nullptr);
}
