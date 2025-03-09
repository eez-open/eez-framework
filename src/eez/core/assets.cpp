/*
 * eez-framework
 *
 * MIT License
 * Copyright 2024 Envox d.o.o.
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <eez/conf-internal.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <eez/core/alloc.h>
#include <eez/core/os.h>
#include <eez/core/memory.h>
#include <eez/core/debug.h>
#include <eez/core/assets.h>
#include <eez/flow/flow.h>

#if EEZ_FOR_LVGL_LZ4_OPTION
#include <eez/libs/lz4/lz4.h>
#endif

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
using namespace eez::gui;
#endif

#if OPTION_SCPI
#include <scpi/scpi.h>
#else
#define SCPI_ERROR_OUT_OF_DEVICE_MEMORY -321
#define SCPI_ERROR_INVALID_BLOCK_DATA -161
#endif

namespace eez {

bool g_isMainAssetsLoaded;
Assets *g_mainAssets;
bool g_mainAssetsUncompressed;
Assets *g_externalAssets;

////////////////////////////////////////////////////////////////////////////////

void fixOffsets(Assets *assets);

bool decompressAssetsData(const uint8_t *assetsData, uint32_t assetsDataSize, Assets *decompressedAssets, uint32_t maxDecompressedAssetsSize, int *err) {
#if EEZ_FOR_LVGL_LZ4_OPTION
	uint32_t compressedDataOffset;
	uint32_t decompressedSize;

	auto header = (Header *)assetsData;

	if (header->tag == HEADER_TAG_COMPRESSED) {
		decompressedAssets->projectMajorVersion = header->projectMajorVersion;
		decompressedAssets->projectMinorVersion = header->projectMinorVersion;
        decompressedAssets->assetsType = header->assetsType;

		compressedDataOffset = sizeof(Header);
		decompressedSize = header->decompressedSize;
	} else {
		decompressedAssets->projectMajorVersion = PROJECT_VERSION_V2;
		decompressedAssets->projectMinorVersion = 0;
        decompressedAssets->assetsType = ASSETS_TYPE_RESOURCE;

		compressedDataOffset = 4;
		decompressedSize = header->tag;
	}

// disable warning: offsetof within non-standard-layout type ... is conditionally-supported [-Winvalid-offsetof]
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

	auto decompressedDataOffset = offsetof(Assets, settings);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

	if (decompressedDataOffset + decompressedSize > maxDecompressedAssetsSize) {
		if (err) {
			*err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
		}
		return false;
	}

	int compressedSize = assetsDataSize - compressedDataOffset;

    int decompressResult = LZ4_decompress_safe(
		(const char *)(assetsData + compressedDataOffset),
		(char *)decompressedAssets + decompressedDataOffset,
		compressedSize,
		decompressedSize
	);

	if (decompressResult != (int)decompressedSize) {
		if (err) {
			*err = SCPI_ERROR_INVALID_BLOCK_DATA;
		}
		return false;
	}
	return true;
#else
    EEZ_UNUSED(assetsData);
    EEZ_UNUSED(assetsDataSize);
    EEZ_UNUSED(decompressedAssets);
    EEZ_UNUSED(maxDecompressedAssetsSize);
    *err = -1;
    return false;
#endif
}

static void allocMemoryForDecompressedAssets(const uint8_t *assetsData, uint32_t assetsDataSize, uint8_t *&decompressedAssetsMemoryBuffer, uint32_t &decompressedAssetsMemoryBufferSize) {
    EEZ_UNUSED(assetsDataSize);

// disable warning: offsetof within non-standard-layout type ... is conditionally-supported [-Winvalid-offsetof]
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

	auto decompressedDataOffset = offsetof(Assets, settings);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    auto header = (Header *)assetsData;
    assert (header->tag == HEADER_TAG_COMPRESSED);
    uint32_t decompressedSize = header->decompressedSize;

    decompressedAssetsMemoryBufferSize = decompressedDataOffset + decompressedSize;

    decompressedAssetsMemoryBuffer = (uint8_t *)eez::alloc(decompressedAssetsMemoryBufferSize, 0x587da194);
}

void loadMainAssets(const uint8_t *assets, uint32_t assetsSize) {
    auto header = (Header *)assets;
    if (header->tag == HEADER_TAG) {
        g_mainAssets = (Assets *)(assets + sizeof(uint32_t)/* skip HEADER_TAG*/);
        g_mainAssetsUncompressed = true;
    } else {
#if defined(EEZ_FOR_LVGL) || defined(EEZ_DASHBOARD_API)
        uint8_t *DECOMPRESSED_ASSETS_START_ADDRESS = 0;
        uint32_t MAX_DECOMPRESSED_ASSETS_SIZE = 0;
        allocMemoryForDecompressedAssets(assets, assetsSize, DECOMPRESSED_ASSETS_START_ADDRESS, MAX_DECOMPRESSED_ASSETS_SIZE);
#endif
        g_mainAssets = (Assets *)DECOMPRESSED_ASSETS_START_ADDRESS;
        g_mainAssetsUncompressed = false;
        g_mainAssets->external = false;
        auto decompressedSize = decompressAssetsData(assets, assetsSize, g_mainAssets, MAX_DECOMPRESSED_ASSETS_SIZE, nullptr);
        assert(decompressedSize);
    }
    g_isMainAssetsLoaded = true;
}

void unloadExternalAssets() {
	if (g_externalAssets) {
#if EEZ_OPTION_GUI
		removeExternalPagesFromTheStack();
#endif
		free(g_externalAssets);
		g_externalAssets = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////

#if EEZ_OPTION_GUI

const gui::PageAsset* getPageAsset(int pageId) {
	if (pageId > 0) {
		return g_mainAssets->pages[pageId - 1];
	} else if (pageId < 0) {
		if (g_externalAssets == nullptr) {
			return nullptr;
		}
		return g_externalAssets->pages[-pageId - 1];
	}
	return nullptr;
}

const gui::PageAsset *getPageAsset(int pageId, WidgetCursor& widgetCursor) {
	if (pageId < 0) {
		widgetCursor.assets = g_externalAssets;
		widgetCursor.flowState = flow::getPageFlowState(g_externalAssets, -pageId - 1, widgetCursor);
	} else {
	    widgetCursor.assets = g_mainAssets;
		if (g_mainAssets->flowDefinition) {
			widgetCursor.flowState = flow::getPageFlowState(g_mainAssets, pageId - 1, widgetCursor);
		}
    }
	return getPageAsset(pageId);
}

const gui::Style *getStyle(int styleID) {
	if (styleID > 0) {
		return g_mainAssets->styles[styleID - 1];
	} else if (styleID < 0) {
		if (g_externalAssets == nullptr) {
			return getStyle(STYLE_ID_DEFAULT);
		}
		return g_externalAssets->styles[-styleID - 1];
	}
	return getStyle(STYLE_ID_DEFAULT);
}

const gui::FontData *getFontData(int fontID) {
	if (fontID > 0) {
		return g_mainAssets->fonts[fontID - 1];
	} else if (fontID < 0) {
		if (g_externalAssets == nullptr) {
			return nullptr;
		}
		return g_externalAssets->fonts[-fontID - 1];
	}
	return nullptr;
}

const gui::Bitmap *getBitmap(int bitmapID) {
	if (bitmapID > 0) {
		return g_mainAssets->bitmaps[bitmapID - 1];
	} else if (bitmapID < 0) {
		if (g_externalAssets == nullptr) {
			return nullptr;
		}
		return g_externalAssets->bitmaps[-bitmapID - 1];
	}
	return nullptr;
}

const int getBitmapIdByName(const char *bitmapName) {
    for (uint32_t i = 0; i < g_mainAssets->bitmaps.count; i++) {
		if (strcmp(g_mainAssets->bitmaps[i]->name, bitmapName) == 0) {
            return i + 1;
        }
	}
    return 0;
}

#endif // EEZ_OPTION_GUI

int getThemesCount() {
	return (int)g_mainAssets->colorsDefinition->themes.count;
}

static Theme *getTheme(int i) {
    if (i < 0 || i >= (int)g_mainAssets->colorsDefinition->themes.count) {
        return nullptr;
    }
    return g_mainAssets->colorsDefinition->themes[i];
}

const char *getThemeName(int i) {
    auto theme = getTheme(i);
    if (!theme) {
	    return "";
    }
    return static_cast<const char *>(theme->name);
}

uint32_t getThemeColorsCount(int themeIndex) {
    auto theme = getTheme(themeIndex);
    if (!theme) {
	    return 0;
    }
	return theme->colors.count;
}

const uint16_t *getThemeColors(int themeIndex) {
    auto theme = getTheme(themeIndex);
    if (!theme) {
        static uint16_t *g_themeColors = { 0 };
	    return g_themeColors;
    }
	return static_cast<uint16_t *>(theme->colors.items);
}

const uint16_t *getColors() {
	return static_cast<uint16_t *>(g_mainAssets->colorsDefinition->colors.items);
}

int getExternalAssetsMainPageId() {
	return -1;
}

#if EEZ_OPTION_GUI

const char *getActionName(const WidgetCursor &widgetCursor, int16_t actionId) {
	if (actionId == 0) {
		return nullptr;
	}

	if (actionId < 0) {
		actionId = -actionId;
	}
	actionId--;

	if (!widgetCursor.assets) {
		return "";
	}

	return widgetCursor.assets->actionNames[actionId];
}

int16_t getDataIdFromName(const WidgetCursor &widgetCursor, const char *name) {
	if (!widgetCursor.assets) {
		return 0;
	}

	for (uint32_t i = 0; i < widgetCursor.assets->variableNames.count; i++) {
		if (strcmp(widgetCursor.assets->variableNames[i], name) == 0) {
			return -((int16_t)i + 1);
		}
	}
	return 0;
}

#endif // EEZ_OPTION_GUI

} // namespace eez
