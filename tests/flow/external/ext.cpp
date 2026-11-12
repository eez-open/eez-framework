/*
 * eez-framework tests: external assets, the BB3+ module model (ext.h).
 */

#include "ext.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <eez/core/alloc.h>
#include <eez/gui/gui.h>
#include <eez/flow/flow.h>
#include <eez/flow/hooks.h>
#include <eez/flow/expression.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/components/set_variable.h>

#include "harness.h"
#include "test.h"

// the error of the BB3+ loader when the allocation fails (scpi/scpi.h)
#define SCPI_ERROR_OUT_OF_DEVICE_MEMORY -321

using namespace eez;
using namespace eez::gui;

namespace ext {

////////////////////////////////////////////////////////////////////////////////
// src/bb3/assets.cpp

static const int EXTERNAL_ASSETS_ARRAY_SIZE = 32;
static Assets *g_externalAssetsArray[EXTERNAL_ASSETS_ARRAY_SIZE];

Assets *loadExternalAssets(const uint8_t *assetsData, uint32_t assetsDataSize, int *err) {
    auto header = (Header *)assetsData;

    uint32_t decompressedSize = header->decompressedSize;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
    auto decompressedDataOffset = offsetof(Assets, pages);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    size_t assetsSize = decompressedDataOffset + decompressedSize;
    Assets *assets = (Assets *)alloc(assetsSize, 301);
    if (!assets) {
        if (err) {
            *err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        return nullptr;
    }

    assets->external = true;

    // the BB3+ has an LZMA branch here (header->reserved == 1), the test
    // project is LZ4 (no bb3ModuleMetadata)
    bool result = decompressAssetsData(assetsData, assetsDataSize, assets, (uint32_t)assetsSize, err);
    if (!result) {
        eez::free(assets);
        return nullptr;
    }

    return assets;
}

int getExternalAssetsPageId(Assets *assets, int pageId) {
    int i;

    for (i = 0; i < EXTERNAL_ASSETS_ARRAY_SIZE; i++) {
        if (g_externalAssetsArray[i] == assets) {
            break;
        }
    }

    if (i == EXTERNAL_ASSETS_ARRAY_SIZE) {
        for (i = 0; i < EXTERNAL_ASSETS_ARRAY_SIZE; i++) {
            if (g_externalAssetsArray[i] == nullptr) {
                g_externalAssetsArray[i] = assets;
                break;
            }
        }
    }

    if (i == EXTERNAL_ASSETS_ARRAY_SIZE) {
        return EEZ_PAGE_ID_NONE;
    }

    int pageIndex = pageId - 1;
    if (!(pageIndex < (int)assets->pages.count && assets->pages[pageIndex])) {
        return EEZ_PAGE_ID_NONE;
    }

    return -(((i + 1) << 16) | pageId);
}

int resolveExternalPageHook(int pageId, Assets **assets) {
    assert(pageId < 0);

    int temp = -pageId;
    unsigned externalAssetsArrayIndex = temp >> 16;
    unsigned externalPageId = temp & 0xFFFF;

    if (externalAssetsArrayIndex > 0) {
        externalAssetsArrayIndex--;
        if (externalAssetsArrayIndex < EXTERNAL_ASSETS_ARRAY_SIZE && g_externalAssetsArray[externalAssetsArrayIndex] != nullptr) {
            *assets = g_externalAssetsArray[externalAssetsArrayIndex];

            int pageIndex = externalPageId - 1;
            if (pageIndex < (int)(*assets)->pages.count) {
                PageAsset *page = (*assets)->pages[pageIndex];
                if (page) {
                    return externalPageId;
                }
            }
        }
    } else if (*assets != nullptr && *assets != g_mainAssets) {
        return externalPageId;
    }

    *assets = nullptr;
    return EEZ_PAGE_ID_NONE;
}

void forgetExternalAssets(Assets *assets) {
    for (int i = 0; i < EXTERNAL_ASSETS_ARRAY_SIZE; i++) {
        if (g_externalAssetsArray[i] == assets) {
            g_externalAssetsArray[i] = nullptr;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// src/bb3/flow/flow.cpp

static void setFlowStateError(FlowState *flowState) {
    flowState->error = true;
    for (FlowState *child = flowState->firstChild; child; child = child->nextSibling) {
        setFlowStateError(child);
    }
}

// The flow states of the assets can be inside the flow states of other assets:
// the slot views of the modules are user widgets in the BB3+ pages.
static void setFlowStatesError(FlowState *firstFlowState, Assets *assets) {
    for (FlowState *flowState = firstFlowState; flowState; flowState = flowState->nextSibling) {
        if (flowState->assets == assets) {
            setFlowStateError(flowState);
        } else {
            setFlowStatesError(flowState->firstChild, assets);
        }
    }
}

void setExternalFlowError(Assets *assets) {
    setFlowStatesError(flow::g_firstFlowState, assets);
}

void stopExternalFlow(Assets *assets) {
    setExternalFlowError(assets);
    flow::stop(assets);
}

////////////////////////////////////////////////////////////////////////////////
// test setup

std::vector<uint8_t> assetsData() {
    return std::vector<uint8_t>(fwtest_ext::assets, fwtest_ext::assets + sizeof(fwtest_ext::assets));
}

void installHooks() {
    g_hooks.resolveExternalPage = resolveExternalPageHook;
}

Assets *load() {
    installHooks();
    auto data = assetsData();
    int err = 0;
    Assets *assets = loadExternalAssets(data.data(), (uint32_t)data.size(), &err);
    REQUIRE(assets != nullptr);
    return assets;
}

Assets *setup() {
    Assets *assets = load();
    REQUIRE(flow::start(assets) == 1);
    return assets;
}

void unload(Assets *assets) {
    forgetExternalAssets(assets);
    eez::free(assets);
}

int pageId(Assets *assets, int extPageId) {
    int id = getExternalAssetsPageId(assets, extPageId);
    REQUIRE(id != EEZ_PAGE_ID_NONE);
    return id;
}

void show(int pageId, int frames) {
    harness::appContext()->pushPage(pageId);
    harness::frame(frames);
}

////////////////////////////////////////////////////////////////////////////////
// flow states

FlowState *topLevelFlowState(Assets *assets, int flowIndex) {
    for (auto flowState = flow::g_firstFlowState; flowState; flowState = flowState->nextSibling) {
        if (flowState->assets == assets && flowState->flowIndex == flowIndex) {
            return flowState;
        }
    }
    return nullptr;
}

static void collect(FlowState *first, Assets *assets, std::vector<FlowState *> &result) {
    for (auto flowState = first; flowState; flowState = flowState->nextSibling) {
        if (flowState->assets == assets) {
            result.push_back(flowState);
        }
        collect(flowState->firstChild, assets, result);
    }
}

void collectFlowStates(Assets *assets, std::vector<FlowState *> &result) {
    collect(flow::g_firstFlowState, assets, result);
}

FlowState *findFlowState(Assets *assets, int flowIndex) {
    std::vector<FlowState *> flowStates;
    collectFlowStates(assets, flowStates);
    for (auto flowState : flowStates) {
        if (flowState->flowIndex == flowIndex) {
            return flowState;
        }
    }
    return nullptr;
}

int countFlowStates(Assets *assets, bool wholeTree) {
    if (wholeTree) {
        std::vector<FlowState *> flowStates;
        collectFlowStates(assets, flowStates);
        return (int)flowStates.size();
    }
    int count = 0;
    for (auto flowState = flow::g_firstFlowState; flowState; flowState = flowState->nextSibling) {
        if (flowState->assets == assets) {
            count++;
        }
    }
    return count;
}

static bool evalEntry(FlowState *flowState, int componentIndex, int entryIndex, Value &result) {
    auto component = (flow::SetVariableActionComponent *)flowState->flow->components[componentIndex];
    return flow::evalExpression(flowState, componentIndex, component->entries[entryIndex]->value, result, flow::FlowError::Plain("probe"));
}

static int findProbe(FlowState *flowState, const char *name) {
    REQUIRE(flowState != nullptr);
    std::string label = std::string("probe:") + name;
    for (int i = 0; i < (int)flowState->flow->components.count; i++) {
        auto component = flowState->flow->components[i];
        if (component->type != flow::defs_v3::COMPONENT_TYPE_SET_VARIABLE_ACTION) {
            continue;
        }
        Value labelValue;
        if (evalEntry(flowState, i, 0, labelValue) && labelValue.isString() && label == labelValue.getString()) {
            return i;
        }
    }
    test::fail(__FILE__, __LINE__, "probe " + label + " not found");
    throw test::Abort();
}

bool probe(FlowState *flowState, const char *name, Value &result) {
    int componentIndex = findProbe(flowState, name);
    return evalEntry(flowState, componentIndex, 1, result);
}

void assignProbe(FlowState *flowState, const char *name, const Value &value) {
    int componentIndex = findProbe(flowState, name);
    auto component = (flow::SetVariableActionComponent *)flowState->flow->components[componentIndex];
    Value dstValue;
    REQUIRE(flow::evalAssignableExpression(flowState, componentIndex, component->entries[1]->variable, dstValue, flow::FlowError::Plain("probe")));
    flow::assignValue(flowState, componentIndex, dstValue, value);
}

Value getGlobal(Assets *assets, int index) {
    return flow::getGlobalVariable(assets, index).getValue();
}

void setGlobal(Assets *assets, int index, const Value &value) {
    flow::setGlobalVariable(assets, index, value);
}

////////////////////////////////////////////////////////////////////////////////
// flow errors

static std::vector<ErrorRecord> g_errors;
static void (*g_harnessFlowErrorHook)(FlowState *flowState, int componentIndex, const char *errorMessage);

std::vector<ErrorRecord> &errors() {
    return g_errors;
}

static void onFlowError(FlowState *flowState, int componentIndex, const char *errorMessage) {
    g_errors.push_back({ flowState ? flowState->assets : nullptr, flowState ? (int)flowState->flowIndex : -1, errorMessage ? errorMessage : "" });
    if (g_harnessFlowErrorHook) {
        g_harnessFlowErrorHook(flowState, componentIndex, errorMessage);
    }
}

void recordErrors() {
    if (flow::onFlowErrorHook != onFlowError) {
        g_harnessFlowErrorHook = flow::onFlowErrorHook;
        flow::onFlowErrorHook = onFlowError;
    }
}

void printFlowErrors() {
    for (auto &error : harness::flowErrors()) {
        fprintf(stderr, "flow error: flow %d, component %d: %s\n", error.flowIndex, error.componentIndex, error.message.c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////
// colors

uint16_t customColorIndex(Assets *assets, uint32_t color32) {
    auto colorsDefinition = static_cast<Colors *>(assets->colorsDefinition);
    uint32_t numModuleColors = colorsDefinition->themes.count > 0 ? colorsDefinition->themes[0]->colors.count : 0;
    auto &colors = colorsDefinition->colors;
    for (uint32_t i = 0; i < colors.count; i++) {
        if (colors[i] == color32) {
            return (uint16_t)(65534 - (numModuleColors + i));
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// pixels

uint32_t pixelRgb(int x, int y) {
    return harness::pixel(x, y) & 0xFFFFFF;
}

int countOther(int x, int y, int w, int h, uint32_t color) {
    int count = 0;
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (pixelRgb(i, j) != color) {
                count++;
            }
        }
    }
    return count;
}

} // namespace ext
