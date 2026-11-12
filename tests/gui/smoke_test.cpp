/*
 * eez-framework tests: the harness itself (assets, flow, display, frames, touch).
 */

#include <eez/conf-internal.h>
#include <eez/core/assets.h>
#include <eez/gui/gui.h>
#include <eez/gui/display.h>
#include <eez/flow/flow.h>

#include "document.h"
#include "harness.h"
#include "test.h"

using namespace eez;
using namespace eez::gui;

TEST("harness: main assets are loaded with a flow definition") {
    REQUIRE(g_mainAssets != nullptr);
    CHECK(g_mainAssets->flowDefinition != nullptr);
    CHECK(!flow::isFlowStopped());
}

TEST("harness: the main page is active") {
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_MAIN);
}

TEST("harness: frames advance the fake clock") {
    auto start = harness::now();
    auto frames = harness::frameCount();
    harness::frame(10);
    CHECK_EQ(harness::frameCount() - frames, 10);
    CHECK_EQ(harness::now() - start, 10 * harness::FRAME_MS);
}

TEST("harness: the accent box is drawn") {
    // accent_box: 10,60 100x50, background "accent" #2060c0, border 2 px
    // (#2060c0 is (32, 96, 192))
    uint32_t inside = harness::pixel(60, 85);
    CHECK_EQ(inside & 0xFF, 0x20u);
    CHECK_EQ((inside >> 8) & 0xFF, 0x60u);
    CHECK_EQ((inside >> 16) & 0xFF, 0xC0u);
    // outside of every widget: background black
    CHECK_EQ(harness::pixel(400, 300) & 0xFFFFFF, 0u);
}

TEST("harness: a touch on the page and its release") {
    harness::touch(400, 300);
    harness::frame(3);
    harness::release();
    harness::frame(3);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)PAGE_ID_MAIN);
}

TEST("harness: an error toast is shown") {
    harness::appContext()->errorMessage("test error");
    harness::frame(2);
    CHECK_EQ(harness::appContext()->getActivePageId(), (int)INTERNAL_PAGE_ID_TOAST_MESSAGE);
}
